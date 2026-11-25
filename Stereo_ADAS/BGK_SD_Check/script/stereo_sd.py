"""Pack rectified image pairs for the RA6E2 and extract its disparity maps."""

import argparse
from pathlib import Path
import random
import struct

from PIL import Image, ImageFilter


WIDTH = HEIGHT = 128
PIXELS = WIDTH * HEIGHT
SECTOR = 512
HEADER = struct.Struct("<8sHHI")
INPUT_MAGIC = b"R6ST0001"
OUTPUT_MAGIC = b"R6DP0001"
IMAGE_TYPES = {".png", ".jpg", ".jpeg", ".bmp", ".pgm", ".tif", ".tiff"}


def indexed_images(directory, side):
    images = {}
    for path in sorted(directory.iterdir()):
        if not path.is_file() or path.suffix.lower() not in IMAGE_TYPES:
            continue
        stem = path.stem
        if stem.endswith("_" + side):
            stem = stem[: -(len(side) + 1)]
        if stem in images:
            raise ValueError(f"Duplicate {side} image ID {stem!r} in {directory}")
        images[stem] = path
    return images


def reference_pairs(directory):
    return [(directory / f"frame_{number}_left.png", directory / f"frame_{number}_right.png")
            for number in range(1, 6)]


def paired_images(left_directory, right_directory):
    left = indexed_images(left_directory, "left")
    right = indexed_images(right_directory, "right")
    if left.keys() != right.keys():
        raise ValueError(f"Unpaired image IDs. Missing from left: {sorted(right.keys() - left.keys())}; "
                         f"missing from right: {sorted(left.keys() - right.keys())}")
    return [(left[key], right[key]) for key in sorted(left)]


def load_pair(left_path, right_path):
    with Image.open(left_path) as left, Image.open(right_path) as right:
        if left.size != right.size:
            raise ValueError(f"Image sizes differ: {left_path} and {right_path}")
        return tuple(image.convert("L").resize((WIDTH, HEIGHT), Image.Resampling.BOX)
                     for image in (left, right))


def synthetic_pair(number):
    """Render aligned cameras with a textured background and three shifted foreground planes."""
    rng = random.Random(number)
    background_width = WIDTH + 15
    background = Image.frombytes("L", (background_width, HEIGHT),
                                 bytes(rng.getrandbits(8) for _ in range(background_width * HEIGHT)))
    background = background.filter(ImageFilter.GaussianBlur(0.65)).point(lambda val: 20 + val // 2)
    base = rng.randrange(0, 4)
    left = background.crop((0, 0, WIDTH, HEIGHT))
    right = background.crop((base, 0, base + WIDTH, HEIGHT))
    for disparity in (rng.randrange(4, 8), rng.randrange(8, 12), rng.randrange(12, 16)):
        width, height = rng.randrange(25, 55), rng.randrange(21, 47)
        x, y = rng.randrange(disparity, WIDTH - width + 1), rng.randrange(8, HEIGHT - height + 1)
        levels = rng.randrange(20, 95)
        patch = Image.frombytes("L", (width, height),
                                bytes(levels + rng.randrange(160) for _ in range(width * height)))
        patch = patch.filter(ImageFilter.GaussianBlur(0.45))
        left.paste(patch, (x, y))
        right.paste(patch, (x - disparity, y))
    return left, right


def write_pair(output, pair):
    left_bytes, right_bytes = (image.tobytes() for image in pair)
    for start in range(0, PIXELS, WIDTH):
        output.write(left_bytes[start:start + WIDTH])
        output.write(right_bytes[start:start + WIDTH])


def write_header(output, count):
    output.write(HEADER.pack(INPUT_MAGIC, WIDTH, HEIGHT, count).ljust(SECTOR, b"\0"))


def validate_count(count):
    if not count:
        raise ValueError("No stereo pairs found")
    if SECTOR + 2 * PIXELS * count > 0xFFFFFFFF:
        raise ValueError("Dataset exceeds the FAT32 file size limit. Pack it in batches.")


def pack(args):
    if args.reference:
        if args.right:
            raise ValueError("Use either --reference or --left/--right")
        pairs = reference_pairs(args.reference)
    else:
        if not args.right:
            raise ValueError("--left requires --right")
        pairs = paired_images(args.left, args.right)
    validate_count(len(pairs))

    # Exclusive creation protects any existing dataset on the card.
    output = args.output.open("xb")
    try:
        with output:
            write_header(output, len(pairs))
            for paths in pairs:
                write_pair(output, load_pair(*paths))
    except BaseException:
        args.output.unlink()
        raise
    print(f"Packed {len(pairs)} stereo pairs into {args.output}")


def prepare(args):
    if args.synthetic < 0:
        raise ValueError("--synthetic must be nonnegative")
    kitti = (paired_images(args.kitti / "image_2", args.kitti / "image_3") if args.kitti else [])
    upstream = reference_pairs(args.reference) if args.reference else []
    count = len(kitti) + len(upstream) + args.synthetic
    validate_count(count)
    left_dir = args.output / "LEFT"
    right_dir = args.output / "RIGHT"
    args.output.mkdir(parents=True)
    left_dir.mkdir()
    right_dir.mkdir()
    sources = (
        ((f"KITTI Stereo 2015 {left.name}", load_pair(left, right)) for left, right in kitti),
        ((f"Reference demo {left.name}", load_pair(left, right)) for left, right in upstream),
        ((f"Synthetic scene seed {number}", synthetic_pair(number)) for number in range(args.synthetic)),
    )
    # Publish the MCU input only when all the paired previews and manifest have been written.
    packed = args.output / "STEREO.TMP"
    with packed.open("xb") as output, (args.output / "FRAMES.TXT").open("x", encoding="ascii") as index:
        write_header(output, count)
        index.write("Frame ID | Source\n")
        number = 0
        for source in sources:
            for description, pair in source:
                number += 1
                name = f"F{number:07d}"
                pair[0].save(left_dir / (name + ".PNG"))
                pair[1].save(right_dir / (name + ".PNG"))
                write_pair(output, pair)
                index.write(f"{name} | {description}\n")
    packed.rename(args.output / "STEREO.BIN")
    print(f"Prepared {count} pairs ({len(kitti)} KITTI, {len(upstream)} reference, "
          f"{args.synthetic} synthetic) in {args.output}")


def extract(args):
    with args.input.open("rb") as source:
        header = source.read(SECTOR)
        if len(header) != SECTOR:
            raise ValueError("Incomplete disparity header")
        magic, width, height, count = HEADER.unpack_from(header)
        disparities = header[HEADER.size]
        if (magic, width, height, disparities) != (OUTPUT_MAGIC, WIDTH, HEIGHT, 16):
            raise ValueError("Unsupported disparity file format")
        if not count or args.input.stat().st_size != SECTOR + count * PIXELS:
            raise ValueError("Disparity file size does not match its frame count")
        args.output.mkdir(parents=True)
        scale = bytes(min(255, value * 255 // (disparities - 1)) for value in range(256))
        for number in range(1, count + 1):
            frame = source.read(PIXELS)
            if len(frame) != PIXELS or max(frame) >= disparities:
                raise ValueError(f"Invalid disparity values in frame {number}")
            image = Image.frombytes("L", (WIDTH, HEIGHT), frame.translate(scale))
            image.save(args.output / f"disparity_{number:05d}.png")
    print(f"Extracted {count} grayscale disparity maps into {args.output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(required=True)
    packer = commands.add_parser("pack", help="write a dataset from rectified stereo pairs")
    origin = packer.add_mutually_exclusive_group(required=True)
    origin.add_argument("--reference", type=Path, help="reference repository checkout with five demo pairs")
    origin.add_argument("--left", type=Path, help="directory of left images")
    packer.add_argument("--right", type=Path, help="directory of matching right images")
    packer.add_argument("--output", type=Path, default=Path("STEREO.BIN"))
    packer.set_defaults(run=pack)
    staging = commands.add_parser("prepare", help="prepare an SD-card payload with paired PNGs and an index")
    staging.add_argument("--kitti", type=Path, help="KITTI training directory with image_2 and image_3")
    staging.add_argument("--reference", type=Path, help="reference checkout with five example pairs")
    staging.add_argument("--synthetic", type=int, default=256, help="number of generated pairs")
    staging.add_argument("--output", type=Path, required=True, help="new staging directory")
    staging.set_defaults(run=prepare)
    extractor = commands.add_parser("extract", help="export an MCU result as grayscale PNGs")
    extractor.add_argument("input", type=Path)
    extractor.add_argument("--output", type=Path, required=True, help="new directory for PNG files")
    extractor.set_defaults(run=extract)
    args = parser.parse_args()
    try:
        args.run(args)
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
