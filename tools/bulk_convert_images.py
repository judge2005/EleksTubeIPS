from PIL import Image, ImageChops

import argparse
import struct, os, sys, glob, pathlib

def create_header(width, height):
    """ Function used to create headers when changing them is necessary, e.g. when image dimensions change.
        This function creates both a BMP header and a V3 DIB header, with some values left as defaults (e.g. pixels/meter) """

    total_header_size = 14 + 40 # V3 len = 40 bytes
    total_header_size = 14 + 40 + 12 # V3 len = 40 bytes
    bitmap_size = ((16 * width + 31) >> 5) * 4 * height
    file_size = total_header_size + bitmap_size
    
    # BMP header: Magic (2 bytes), file size, 2 ignored values, bitmap offset
    header = struct.pack('<H 3I', 0x4D42, file_size, 0, total_header_size)

    # DIB V3 header: header size, px width, px height, num of color planes, bpp, compression method,
    # bitmap data size, horizontal resolution, vertical resolution, number of colors in palette, number of important colors used
    # Few of these matter, so there are a bunch of default/"magic" numbers here...
    header += struct.pack('<I 2i H H I I 2i 2I', 40, width, height, 1, 16, 3, file_size, 0x0B13, 0x0B13, 0, 0)

    # RGB565 masks
    header += struct.pack('<3I', 0x0000f800, 0x000007e0, 0x0000001f)

    return header

def write_bin(f, pixel_list, width, height):
    for row in range(height-1, -1, -1):
        for col in range(0, width, 1) :
            r = (pixel_list[row*width + col][0] & 0xF8)
            g = (pixel_list[row*width + col][1] & 0xFC)
            b = (pixel_list[row*width + col][2] & 0xF8)
            c = (r << 8) | (g << 3) | (b >> 3)
            f.write(struct.pack('<H', c))
        if (width % 2) == 1:
            f.write(struct.pack('<H', 0))

def trim(im):
    bg = Image.new(im.mode, im.size, im.getpixel((0,0)))
    diff = ImageChops.difference(im, bg)
    if (args.fuzzy):
        diff = ImageChops.add(diff, diff, 2.0, -100)
    bbox = diff.getbbox(alpha_only=False)
    if bbox:
        return im.crop(bbox)
    else:
        return im

def process(image, output_file):
    if args.trim:
        image = trim(image)
    if image.has_transparency_data:
        new_image = Image.new("RGBA", image.size, args.background_color)
        new_image.paste(image, (0, 0), image)
        image = new_image.convert('RGB')

    if args.depth == 16:
        pixels = list(image.getdata())        
        with open(output_file, 'wb') as f:
            f.write(create_header(image.width, image.height))
            write_bin(f, pixels, image.width, image.height)
    if args.depth < 16:
        image = image.quantize(colors=1 << args.depth, kmeans=128, method=Image.MAXCOVERAGE)
        image.save(output_file);
    if args.depth > 16:
        image.save(output_file);
    
    print(output_file + ", colors=" + str(len(set(image.getdata()))) + ", width=" + str(image.width) + ", height=" + str(image.height))

def tile(file):
    img = Image.open(file)
    w, h = img.size
    th = h / args.rows
    tw = w / args.cols
    for row in range(0, args.rows, 1):
        for col in range(0, args.cols, 1) :
            left = col * tw
            top = row * th
            out = pathlib.Path(file).stem + "_" + str(row) + "_" + str(col) + ".bmp"
            box = (left, top, left+tw, top+th)
            process(img.crop(box), out)

parser = argparse.ArgumentParser(
    description="Bulk convert PNG to RGB565 BMP"
)

parser.add_argument(
    "-b",
    "--background",
    dest="background_color",
    default="BLACK",
    help="e.g WHITE or BLACK - defaults to BLACK"
)

parser.add_argument(
    "-d",
    "--depth",
    dest="depth",
    type=int,
    default=16,
    choices=[1, 8, 16, 24, 32],
    help="bits per pixel"
)

parser.add_argument(
    "-c",
    "--cols",
    dest="cols",
    type=int,
    default=1,
    help="number of columns to split input into"
)

parser.add_argument(
    "-r",
    "--rows",
    dest="rows",
    type=int,
    default=1,
    help="number of rows to split input into"
)

parser.add_argument(
    "-t",
    "--trim",
    dest="trim",
    action="store_true",
    help="Trim input image to content"
)

parser.add_argument(
    "-f",
    "--fuzzy",
    dest="fuzzy",
    action="store_true",
    help="Use a fuzzy trim when trimming, used in conjuction with -t"
)

parser.add_argument(
    "-i",
    "--info",
    dest="info",
    action="store_true",
    help="Only show image info"
)

parser.add_argument('file',nargs='+')

args = parser.parse_args()

if len(args.file) > 0:
    for file in args.file:
        image = Image.open(file)
        print(file + ", colors=" + str(len(set(image.getdata()))) + ", width=" + str(image.width) + ", height=" + str(image.height))
        if not args.info:
            if args.rows > 1 and args.cols > 1:
                tile(file)
            else:
                output_file = pathlib.Path(file).stem + ".bmp"
                process(image, output_file)
else:
    print("You must specify one or more PNG files to convert")