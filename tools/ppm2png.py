"""Convert a screenshot from the scenes (P key) into a PNG for the README.

    ./venv/bin/python tools/ppm2png.py satellite-1234.ppm docs/molniya.png

Downscales to 1600 px wide by default. A Retina framebuffer is ~3400 px across,
which is several megabytes of PNG for no visible benefit at the width GitHub
renders images at.

Optionally crops empty margin first, which most orbit captures have a lot of:

    ./venv/bin/python tools/ppm2png.py shot.ppm docs/out.png --trim
"""

import argparse
import pathlib
import sys

from PIL import Image, ImageChops


def trim_margin(image, background, padding=24):
    """Crop uniform background from the edges, keeping a little padding."""
    mask = ImageChops.difference(image, Image.new("RGB", image.size, background))
    box = mask.getbbox()
    if box is None:
        return image

    left, upper, right, lower = box
    return image.crop((
        max(0, left - padding),
        max(0, upper - padding),
        min(image.width, right + padding),
        min(image.height, lower + padding),
    ))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--width", type=int, default=1600)
    parser.add_argument("--trim", action="store_true",
                        help="crop uniform background from the edges")
    args = parser.parse_args()

    if not args.source.exists():
        sys.exit(f"no such file: {args.source}")

    image = Image.open(args.source).convert("RGB")
    original = image.size

    if args.trim:
        # The scenes' clear colour, as written by glClearColor(0.043, 0.047, 0.055).
        image = trim_margin(image, (11, 12, 14))

    if image.width > args.width:
        height = round(image.height * args.width / image.width)
        image = image.resize((args.width, height), Image.LANCZOS)

    args.destination.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.destination, optimize=True)

    size_kb = args.destination.stat().st_size / 1024
    print(f"{args.source} {original} -> {args.destination} {image.size} "
          f"({size_kb:.0f} KB)")


if __name__ == "__main__":
    main()
