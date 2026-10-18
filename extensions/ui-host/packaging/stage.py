import shutil
import subprocess


def stage(context):
    if not context.options.toolkit or not context.options.toolkit.is_file():
        raise ValueError(
            "ui-host requires --toolkit from tools/build/build-toolkit.cjs"
        )
    subprocess.run(
        [
            "node",
            str(context.repository / "tools/build/build-host.cjs"),
            str(context.output / "host.js"),
        ],
        check=True,
    )
    shutil.copy2(context.options.toolkit, context.output / "toolkit.js")
