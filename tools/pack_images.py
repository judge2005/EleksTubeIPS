#!/usr/bin/env python3
"""Packs images into the .tar.gz file the clock takes for a clock face, weather icons or a slide show.

    python3 tools/pack_images.py more_faces/my_face              # makes more_faces/my_face/my_face.tar.gz
    python3 tools/pack_images.py -o maxclassic.tar.gz [0-9]*.bmp  # packs just these files

The set takes the archive's name, and the clock looks for each image at the top of the archive as <name>.bmp,
in lower case. Each image is checked the way the clock loads it, so one it can't show is caught here and not
on its displays. Nothing about whoever packs the set goes in the archive: no owner names or IDs, and none of
the extra files macOS's tar adds for extended attributes. Standard library only.
"""
import argparse
import gzip
import io
import os
import struct
import sys
import tarfile

# The display, TFT_WIDTH and TFT_HEIGHT in the firmware
WIDTH, HEIGHT = 135, 240

problems = []


def problem(message):
    problems.append(message)


def check_name(archive):
    """The set's name is the archive's, checked like ImageUnpacker::isValidName() does."""
    base = os.path.basename(archive)
    if not base.endswith(".tar.gz"):
        problem(f"{base}: the clock only takes files whose names end in .tar.gz")
        return
    name = base[:-len(".tar.gz")]
    if not 0 < len(name) <= 25:
        problem(f"{base}: the set's name has to be 1 to 25 characters, and {name!r} has {len(name)}")
    bad = sorted({c for c in name if not " " <= c <= "~" or c in '/\\"'})
    if bad:
        problem(f"{base}: the set's name can't have {' or '.join(repr(c) for c in bad)} in it")


def check_image(name, data):
    """What TFTs::LoadImageIntoBuffer() needs to show the image. Returns its width, height and kind, or None."""
    if data[:2] == b"CK" and len(data) >= 6:
        width, height = struct.unpack_from("<HH", data, 2)
        if not 0 < width <= WIDTH or not 0 < height <= HEIGHT:
            return problem(f"{name} is {width}x{height}, bigger than the display's {WIDTH}x{HEIGHT}")
        if 6 + width * height * 2 > len(data):
            return problem(f"{name} is shorter than its pixels")
        return width, height, "CLK"

    if data[:2] != b"BM" or len(data) < 54:
        return problem(f"{name} isn't a BMP")
    start, header_size, width, height, planes, bits, compression = struct.unpack_from("<IIiiHHI", data, 10)
    if header_size < 40:
        return problem(f"{name} has an old OS/2 header, which the clock can't read")
    if not 0 < width <= WIDTH or height == 0 or abs(height) > HEIGHT:
        return problem(f"{name} is {width}x{abs(height)}, bigger than the display's {WIDTH}x{HEIGHT}")
    if planes != 1:
        return problem(f"{name} has {planes} color planes, and the clock reads only 1")
    if bits not in (1, 2, 4, 8, 16, 24):
        return problem(f"{name} has {bits} bits per pixel, and the clock shows 1, 2, 4, 8, 16 or 24")
    # The clock doesn't decompress. 16-bit images can say where their colors are (BI_BITFIELDS), which isn't
    # compression.
    if compression not in ((0, 3) if bits == 16 else (0,)):
        return problem(f"{name} is compressed, which the clock can't show")
    if bits <= 8:
        colors = struct.unpack_from("<I", data, 46)[0] or 1 << bits
        if colors > 1 << bits:
            return problem(f"{name} has {colors} colors, more than its {bits}-bit pixels can pick from")
    if start + ((bits * width + 31) >> 5) * 4 * abs(height) > len(data):
        return problem(f"{name} is shorter than its pixels")
    return width, abs(height), f"{bits}-bit"


def images(paths):
    """The files to pack, by the name each will have in the archive."""
    files = {}
    for path in paths:
        if os.path.isdir(path):
            names = sorted(n for n in os.listdir(path) if not n.startswith("."))
            for n in names:
                if n.lower().endswith(".bmp") and not n.endswith(".bmp"):
                    problem(f"{n}: the clock looks for names ending in .bmp, in lower case")
            found = [os.path.join(path, n) for n in names
                     if n.endswith(".bmp") and os.path.isfile(os.path.join(path, n))]
            if not found:
                problem(f"{path} has no .bmp files")
        elif os.path.isfile(path):
            if not path.endswith(".bmp"):
                problem(f"{os.path.basename(path)}: the clock looks for names ending in .bmp, in lower case")
                continue
            found = [path]
        else:
            problem(f"{path} doesn't exist")
            continue
        for f in found:
            n = os.path.basename(f)
            if n in files and os.path.abspath(files[n]) != os.path.abspath(f):
                problem(f"{n} is given twice, as {files[n]} and {f}")
            files[n] = f
    return files


def pack(files, archive):
    """Writes the archive the way the clock's own are: ustar, the images at the top, two empty blocks at the end."""
    tar = io.BytesIO()
    with tarfile.open(fileobj=tar, mode="w", format=tarfile.USTAR_FORMAT) as t:
        for name in sorted(files):
            with open(files[name], "rb") as f:
                data = f.read()
            info = tarfile.TarInfo(name)    # Owned by uid and gid 0, with no user or group name
            info.size = len(data)
            info.mode = 0o644
            info.mtime = int(os.path.getmtime(files[name]))
            t.addfile(info, io.BytesIO(data))
        end = t.offset
    # Python pads the tar out to 10 KB past the two empty blocks that end it, which the clock's own sets don't
    # have. The clock stops reading at those blocks anyway.
    data = tar.getvalue()[:end + 2 * tarfile.BLOCKSIZE]
    with open(archive, "wb") as f:
        f.write(gzip.compress(data, compresslevel=9, mtime=0))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("paths", nargs="+", metavar="PATH", help="images, or a folder whose .bmp files to pack")
    parser.add_argument("-o", "--output", metavar="FILE",
                        help="the archive to write, by default <folder>/<folder's name>.tar.gz for one folder")
    args = parser.parse_args()

    if args.output:
        archive = args.output
    elif len(args.paths) == 1 and os.path.isdir(args.paths[0]):
        archive = os.path.join(args.paths[0], os.path.basename(os.path.abspath(args.paths[0])) + ".tar.gz")
    else:
        parser.error("say where to write the archive with -o")

    check_name(archive)
    files = images(args.paths)
    found = []
    for name, path in sorted(files.items()):
        with open(path, "rb") as f:
            image = check_image(name, f.read())
        if image:
            found.append(image)

    if problems:
        for message in problems:
            print(message, file=sys.stderr)
        sys.exit(1)

    pack(files, archive)
    sizes = {(w, h) for w, h, _ in found}
    size = "%dx%d" % sizes.pop() if len(sizes) == 1 else "up to %dx%d" % tuple(map(max, zip(*sizes)))
    kinds = sorted({kind for _, _, kind in found}, key=lambda k: (len(k), k))
    kinds = " and ".join([", ".join(kinds[:-1]), kinds[-1]] if len(kinds) > 1 else kinds)
    # Each file takes whole 4 KB blocks of the clock's filesystem once unpacked
    unpacked = sum(-(-os.path.getsize(p) // 4096) * 4096 for p in files.values())
    count = f"{len(files)} image{'s' * (len(files) != 1)}"
    print(f"{archive}: {count}, {size}, {kinds}. {os.path.getsize(archive):,} bytes, and about"
          f" {unpacked // 1024:,} KB more on the clock while it's the set in use.")


if __name__ == "__main__":
    main()
