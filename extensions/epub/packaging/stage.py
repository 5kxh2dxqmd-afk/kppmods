import json
import shutil
import struct


def check_architecture(path, architecture):
    with path.open("rb") as runtime:
        header = runtime.read(52)
    if (
        len(header) != 52
        or header[:6] != b"\x7fELF\x01\x01"
        or struct.unpack_from("<H", header, 18)[0] != 40
    ):
        raise ValueError(f"EPUB runtime is not a 32-bit ARM ELF: {path}")
    flags = struct.unpack_from("<I", header, 36)[0]
    hard_float = bool(flags & 0x400)
    if hard_float != (architecture == "kindlehf"):
        raise ValueError(f"EPUB runtime does not match {architecture}: {path}")


def stage(context):
    runtime = context.options.epub_runtime
    if not runtime:
        raise ValueError("EPUB requires --epub-runtime")
    abis = []
    for abi in ("abi0", "abi1"):
        if not (runtime / abi).is_dir():
            continue
        for name in ("libkindle-epub-kpp.so", "libkindle-epub.so"):
            check_architecture(runtime / abi / name, context.options.architecture)
        shutil.copytree(runtime / abi, context.output / abi)
        abis.append(abi.removeprefix("abi"))
    if not abis:
        raise ValueError("EPUB runtime has no C++ ABI variants")
    for name in ("kindle-epub-import", "libkindle-epub-framework.so"):
        check_architecture(runtime / name, context.options.architecture)
        shutil.copy2(runtime / name, context.output / name)
    manifest_path = context.output / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    for libraries in manifest["native"].values():
        for library in libraries:
            if isinstance(library, dict):
                library["variants"] = {abi: library["variants"][abi] for abi in abis}
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
