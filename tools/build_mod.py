"""Build a paired MHGU 1.4.0 NSO + IPS32 from a local updated ExeFS dump."""
import argparse
import ctypes
import ctypes.util
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

ROOT = Path(__file__).resolve().parents[1]
MAIN_ID = bytes.fromhex("FB08F1D20FD1204FE7070672777F869B").ljust(32, b"\0")
SUBSDK0_ID = bytes.fromhex("C2774FA4CACE9ECC6A712BFC839214C19515F36E").ljust(32, b"\0")
TEXT_SHA = "09ff3ff102cf7aa14c9be6be8b7fa70304ba7ab57ccd1215b17f7fb106428df0"
# Each tuple specifies the original destination and all of its matching retail
# call sites that participate in this mod. The callee's entry remains intact.
HOOKS = {
    "mhgu_end_frame": (0x86A3A0, (0xBBD930, 0xBBE77C)),
    "mhgu_input_update": (0x3DE064, (0x3DDF78,)),
    "mhgu_contact_dispatch": (0x170968, (0x16C938,)),
    "mhgu_damage_commit": (0x9D6F8, (0x171B70, 0x1733B8, 0x174208, 0x175300, 0x17777C)),
}
# Retail EndRecording wrapper is a single straight-line tail call. Its C
# equivalent lives in adapter.c. Replacing this specific wrapper frees its
# interior for three one-word relays *within* the NSO text segment. Astris
# discards IPS writes to the page-alignment gap after the section.
END_FRAME = 0x86A3A0
END_FRAME_BODY = bytes.fromhex("0c109fe5780190e501109fe7001091e511ff2fe1d473fd00")
RELAY = {"mhgu_contact_dispatch": END_FRAME + 4, "mhgu_damage_commit": END_FRAME + 8,
         "mhgu_input_update": END_FRAME + 12}


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def nso(path, expected):
    raw = path.read_bytes()
    if raw[:4] != b"NSO0" or raw[64:96] != expected:
        raise ValueError(f"{path}: expected MHGU Switch v1.4.0 {expected.hex()}")
    end = sum(struct.unpack_from("<III", raw, 0x34))
    return raw, (end + 4095) & ~4095


def retail_text(raw):
    offset, _, size = struct.unpack_from("<III", raw, 0x10)
    text = raw[offset:offset + u32(raw, 0x60)]
    if u32(raw, 12) & 1:
        name = ctypes.util.find_library("lz4") or "/opt/homebrew/opt/lz4/lib/liblz4.dylib"
        lib = ctypes.CDLL(name)
        out = ctypes.create_string_buffer(size)
        if lib.LZ4_decompress_safe(text, out, len(text), size) != size:
            raise ValueError("Cannot decode retail .text")
        text = out.raw
    if hashlib.sha256(text).hexdigest() != TEXT_SHA:
        raise ValueError("Retail .text does not match the validated MHGU v1.4.0 build")
    return text


def arm_branch(site, target, link):
    delta = target - site - 8
    if delta % 4 or not -(1 << 25) <= delta < (1 << 25):
        raise ValueError(f"Unreachable ARM branch {site:#x} -> {target:#x}")
    return (0xEB000000 if link else 0xEA000000) | (delta // 4 & 0xffffff)


def package_ips(text, module_offset, symbols):
    if text[END_FRAME:END_FRAME + len(END_FRAME_BODY)] != END_FRAME_BODY:
        raise ValueError("Unexpected EndRecording dispatch implementation")
    records = {END_FRAME: arm_branch(END_FRAME, module_offset + symbols["mhgu_end_frame"], False)}
    for name, (original, sites) in HOOKS.items():
        entry = module_offset + symbols[name]
        bridge = RELAY.get(name)
        if bridge is not None:
            records[bridge] = arm_branch(bridge, entry, False)
            entry = bridge
        for site in sites:
            if u32(text, site) != arm_branch(site, original, True):
                raise ValueError(f"Unexpected retail call at {site:#x}")
            records[site] = arm_branch(site, entry, True)
    data = bytearray(b"IPS32")
    for site, word in sorted(records.items()):
        if not 0 <= site <= len(text) - 4:
            raise ValueError(f"Patch lies outside the NSO's stored code at {site:#x}")
        # NSO-format IPS addresses include the virtual 0x100-byte file header.
        data += struct.pack(">IH", site + 0x100, 4) + struct.pack("<I", word)
    return data + b"EEOF"


def package_nso(elf):
    phoff = u32(elf, 28)
    phsize, count = struct.unpack_from("<HH", elf, 42)
    loads = [struct.unpack_from("<8I", elf, phoff + i * phsize) for i in range(count)]
    loads = [s for s in loads if s[0] == 1]
    if [s[6] for s in loads] != [5, 4, 6]:
        raise ValueError("Unexpected module ELF segments")
    header, payload = bytearray(256), bytearray()
    struct.pack_into("<4sIII", header, 0, b"NSO0", 0, 0, 0x38)
    for i, (_, pos, addr, _, size, _, _, _) in enumerate(loads):
        part = elf[pos:pos + size]
        struct.pack_into("<III", header, 16 + i*16, 256 + len(payload), addr, size)
        struct.pack_into("<I", header, 96 + i*4, size)
        header[160+i*32:192+i*32] = hashlib.sha256(part).digest()
        payload.extend(part)
    struct.pack_into("<I", header, 60, loads[-1][5] - loads[-1][4])
    header[64:96] = hashlib.sha256(payload).digest()
    return header + payload


def link(objects, output):
    path = os.environ.get("LD_LLD") or shutil.which("ld.lld")
    if path:
        linker = [path]
    else:
        rust = subprocess.check_output(["rustc", "--print", "sysroot"], text=True).strip()
        linker = [str(next((Path(rust)/"lib/rustlib").glob("*/bin/rust-lld"))), "-flavor", "gnu"]
    subprocess.run(linker + ["-T", ROOT/"src/module.ld", "--no-undefined", *objects, "-o", output],
                   check=True)


def build(exefs):
    main, main_extent = nso(exefs/"main", MAIN_ID)
    _, middleware_extent = nso(exefs/"subsdk0", SUBSDK0_ID)
    module_offset = main_extent + middleware_extent
    if module_offset != 0x2715000 or (exefs/"subsdk1").exists():
        raise ValueError("This mod requires the standard retail main/subsdk0 layout")
    text = retail_text(main)
    destination = ROOT / "dist" / "MHGU-Damage-Display.zip"
    with tempfile.TemporaryDirectory(prefix="mhgu-build-") as tmp:
        work = Path(tmp)
        objects = []
        flags = ["--target=armv7a-none-eabi", "-marm", "-Os", "-fPIC", "-ffreestanding",
                 "-fno-unwind-tables", "-fno-asynchronous-unwind-tables", "-Wall",
                 "-Wextra", "-Werror", f"-DMHGU_MODULE_OFFSET={module_offset}"]
        for src in ("start.S", "adapter.c", "hud.c", "damage.c", "contact.c"):
            obj = work / (src + ".o")
            subprocess.run(["clang", *flags, "-c", ROOT/"src"/src, "-o", obj], check=True)
            objects.append(obj)
        elf = work/"mod.elf"
        link(objects, elf)
        symbols = {}
        for line in subprocess.check_output(["nm", "-n", elf], text=True).splitlines():
            fields = line.split()
            if len(fields) == 3 and fields[2] in HOOKS:
                symbols[fields[2]] = int(fields[0], 16)
        ips = package_ips(text, module_offset, symbols)
        image = package_nso(elf.read_bytes())
        destination.parent.mkdir(exist_ok=True)
        # Stable entry metadata keeps identical builds byte-for-byte reproducible.
        with tempfile.TemporaryDirectory(dir=destination.parent) as staging:
            package = Path(staging) / destination.name
            with ZipFile(package, "w") as archive:
                for filename, content in (("subsdk1", image), (MAIN_ID.hex().upper() + ".ips", ips)):
                    entry = ZipInfo("MHGU-Damage-Display/exefs/" + filename)
                    entry.external_attr = 0o100644 << 16
                    archive.writestr(entry, content, compress_type=ZIP_DEFLATED)
            package.replace(destination)
    print(destination)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exefs", type=Path, help="local Switch GU v1.4.0 ExeFS directory")
    build(parser.parse_args().exefs)
