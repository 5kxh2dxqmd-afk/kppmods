#!/usr/bin/env python3
"""Develop one UI package inside a running Kindle process over SSH."""

import argparse
import io
import json
import pathlib
import shlex
import subprocess
import sys
import tarfile
import tempfile
import time
import uuid

SOURCE = pathlib.Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("command", choices=["start", "push", "watch", "stop"])
parser.add_argument("package", nargs="?", type=pathlib.Path)
parser.add_argument("--host", required=True)
parser.add_argument("--port", type=int, default=22)
parser.add_argument("--identity", type=pathlib.Path)
parser.add_argument("--known-hosts", type=pathlib.Path)
args = parser.parse_args()
if args.command in ("push", "watch") and not args.package:
    parser.error("push/watch requires a source package directory")
quote = shlex.quote
work = tempfile.TemporaryDirectory(prefix="kppmods-device-")
workspace = pathlib.Path(work.name)
ssh = [
    "ssh",
    "-o",
    "BatchMode=yes",
    "-o",
    "ConnectTimeout=10",
    "-o",
    "ControlMaster=auto",
    "-o",
    "ControlPath=" + str(workspace / "ssh"),
    "-o",
    "ControlPersist=30",
    "-p",
    str(args.port),
]
if args.identity:
    ssh += ["-i", str(args.identity)]
if args.known_hosts:
    ssh += [
        "-o",
        "UserKnownHostsFile=" + str(args.known_hosts),
        "-o",
        "StrictHostKeyChecking=accept-new",
    ]
ssh += [args.host]


def run(command, data=None):
    return subprocess.check_output(ssh + [command], input=data)


def upload(data, path):
    temporary = path + ".upload"
    run(
        "cat > "
        + quote(temporary)
        + " && chmod 644 "
        + quote(temporary)
        + " && mv "
        + quote(temporary)
        + " "
        + quote(path),
        data,
    )


def manage(command):
    run("/var/run/eink-sdk/usr/bin/kppmod " + quote(command))


def start():
    run("mkdir -p " + quote(remote) + " && chmod 755 " + quote(remote))
    subprocess.run(
        [
            "node",
            str(SOURCE / "tools/build/build-host.cjs"),
            str(workspace / "host.js"),
        ],
        check=True,
    )
    upload((workspace / "host.js").read_bytes(), remote + "/host.js")
    manage("development-start")
    print("Development session started. UI updates can use push or watch.")


def stop():
    manage("development-stop")
    print("Restored installed packages.")


def push():
    started = time.monotonic()
    package = args.package.resolve()
    manifest = json.loads((package / "manifest.json").read_text())
    config = json.loads(run("/var/run/eink-sdk/usr/bin/kppmod configuration"))
    if not config.get("development"):
        raise RuntimeError("Start a development session first")
    installed = next(
        (item for item in config["packages"] if item["id"] == manifest["id"]),
        None,
    )
    if not installed:
        raise RuntimeError("Install/enable the package before starting development")
    if installed.get("ui_runtime"):
        raise RuntimeError(
            "Host and shared toolkit changes require normal package deployment"
        )
    for field in [
        "native",
        "modules",
        "needs",
        "ui_runtime",
        "actions",
        "settingsSchema",
    ]:
        if (manifest.get(field) or None) != (installed.get(field) or None):
            raise RuntimeError(
                field + " changed; apply the package normally and restart development"
            )
    if len(manifest.get("ui", [])) != 1:
        raise RuntimeError("Live development requires one UI entry per package")
    revision = uuid.uuid4().hex
    destination = remote + "/packages/" + manifest["id"] + "/" + revision
    output = workspace / "entry.js"
    subprocess.run(
        [
            "node",
            str(SOURCE / "tools/build/build-extension.cjs"),
            str(package / manifest["ui"][0]),
            manifest["id"],
            str(output),
            str(package),
        ],
        check=True,
    )
    archive = io.BytesIO()
    with tarfile.open(fileobj=archive, mode="w") as tar:

        def exclude(info):
            if any(
                part in ("node_modules", ".git")
                for part in pathlib.PurePosixPath(info.name).parts
            ):
                return None
            info.mode = 0o755 if info.isdir() or info.mode & 0o111 else 0o644
            return info

        tar.add(package, arcname=".", filter=exclude)
    run(
        "mkdir -p "
        + quote(destination)
        + " && chmod 755 "
        + quote(remote + "/packages")
        + " "
        + quote(remote + "/packages/" + manifest["id"])
        + " "
        + quote(destination)
        + " && tar xf - -C "
        + quote(destination),
        archive.getvalue(),
    )
    metadata = {
        "options": manifest.get("options") or {},
        "apps": manifest.get("apps") or [],
        "title": manifest.get("title") or manifest["id"],
    }
    metadata["root"] = destination
    update = {
        "revision": revision,
        "package": manifest["id"],
        "sourceFile": destination + "/entry.js",
        "metadata": metadata,
    }
    source = output.read_bytes()
    if len(source) > 2 * 1024 * 1024:
        raise RuntimeError("Development bundle exceeds 2 MiB")
    upload(source, update["sourceFile"])
    upload(json.dumps(update).encode(), messages + "/update.json")
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        text = run("cat " + quote(messages + "/result.json") + " 2>/dev/null || true")
        result = json.loads(text) if text else {}
        if result.get("revision") == revision:
            elapsed = round((time.monotonic() - started) * 1000)
            if not result.get("accepted"):
                raise RuntimeError(result.get("error", "Update rejected"))
            print(
                f"{manifest['id']}: applied in {elapsed} ms (device apply {result['applyMs']} ms); component state reset",
                flush=True,
            )
            return
        time.sleep(0.1)
    raise RuntimeError(
        "No acknowledgement; inspect the runtime diagnostics and KPP UI errors"
    )


def fingerprint():
    return sorted(
        (str(path), path.stat().st_mtime_ns, path.stat().st_size)
        for path in args.package.rglob("*")
        if path.is_file()
        and not any(part in ("node_modules", ".git") for part in path.parts)
    )


try:
    status = json.loads(run("/var/run/eink-sdk/usr/bin/kppmod status"))
    current = status["config"]
    root = current["root"]
    remote = root + "/development"
    messages = "/var/run/kppmods/development"
    if (
        args.command == "start"
        and current.get("development")
        and not status["pending_jobs"]
    ):
        raise RuntimeError("A development session exists; stop it first")
    if args.command == "start":
        start()
    elif args.command == "stop":
        stop()
    elif args.command == "push":
        push()
    else:
        previous = None
        print(
            "Watching source. Ctrl-C stops watching; use stop to end the device session."
        )
        while True:
            current = fingerprint()
            if current != previous:
                previous = current
                try:
                    push()
                except (
                    RuntimeError,
                    subprocess.CalledProcessError,
                    ValueError,
                    OSError,
                ) as error:
                    print("Update failed:", error, flush=True)
            time.sleep(0.25)
except KeyboardInterrupt:
    pass
except (RuntimeError, subprocess.CalledProcessError, ValueError, OSError) as error:
    print("Development error:", error, file=sys.stderr)
    sys.exit(1)
finally:
    subprocess.run(
        ssh[:-1] + ["-O", "exit", args.host],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    work.cleanup()
