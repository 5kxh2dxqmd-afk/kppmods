import subprocess


def stage(context):
    renderer = context.options.rsvg_convert
    if not renderer:
        raise ValueError("status-items requires rsvg-convert")
    subprocess.run(
        [
            renderer,
            "--width",
            "64",
            "--height",
            "64",
            "--keep-aspect-ratio",
            "--output",
            str(context.output / "assets/apps.png"),
            str(context.source / "assets/apps.svg"),
        ],
        check=True,
    )
