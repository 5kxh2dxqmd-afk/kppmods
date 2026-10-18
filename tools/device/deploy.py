#!/usr/bin/env python3
"""Install local IPKs on a device through its SDK opkg command."""

import argparse
import pathlib
import shlex
import subprocess
import uuid

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("packages", nargs="+", type=pathlib.Path)
p.add_argument("--host", required=True)
p.add_argument("--port", type=int, default=22)
p.add_argument("--identity", type=pathlib.Path)
p.add_argument("--known-hosts", type=pathlib.Path)
a = p.parse_args()
options = ["-o", "BatchMode=yes"]
if a.identity:
    options += ["-i", str(a.identity)]
if a.known_hosts:
    options += ["-o", "UserKnownHostsFile=" + str(a.known_hosts)]
ssh = ["ssh", *options, "-p", str(a.port), a.host]
staging = "/tmp/kppmods-ipk-" + uuid.uuid4().hex
subprocess.run(ssh + ["mkdir -p " + staging], check=True)
try:
    files = []
    for i, package in enumerate(a.packages):
        if package.suffix != ".ipk" or not package.is_file():
            p.error("Expected .ipk files")
        destination = staging + "/" + str(i) + ".ipk"
        subprocess.run(
            [
                "scp",
                "-O",
                *options,
                "-P",
                str(a.port),
                str(package),
                a.host + ":" + destination,
            ],
            check=True,
        )
        files.append(destination)
    subprocess.run(
        ssh
        + [
            "KPPMOD_DEFER_RELOAD=1 /var/local/kindle-opkg/run opkg install "
            + " ".join(map(shlex.quote, files))
        ],
        check=True,
    )
    subprocess.run(ssh + ["/var/run/eink-sdk/usr/bin/kppmods-start"], check=True)
    subprocess.run(ssh + ["sync"], check=True)
finally:
    subprocess.run(ssh + ["rm -rf " + staging], check=False)
