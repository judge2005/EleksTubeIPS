from PIL import Image, ImageChops

import random, math
import argparse
import struct, os, sys, glob, pathlib

def create_header(width, height):
    # Create an RGB565 BMP file header

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

def scale_pixel(p, factor):
    if args.dither:
        val = int(math.floor(float(p) * (256.0 - factor) / 255.0 / float(factor) + random.random()))
    else:
        val = math.floor(p/factor)

    return val

def write_bin(f, pixel_list, width, height):
    for row in range(height-1, -1, -1):
        for col in range(0, width, 1) :
            r = scale_pixel(pixel_list[row*width + col][0], 8)
            g = scale_pixel(pixel_list[row*width + col][1], 4)
            b = scale_pixel(pixel_list[row*width + col][2], 8)
            c = (r << 11) | (g << 5) | b
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

def scale(img):
    if image.size[0] == 135 and image.size[1] == 240:   # unchanged
        return img
    img_width = img.size[0]
    img_height = img.size[1]
    wratio = 135.0 / float(img_width)
    hratio = 240.0 / float(img_height)
    ratio = min(wratio, hratio)  
    img = img.resize((int(img_width * ratio), int(img_height * ratio)), Image.Resampling.LANCZOS)
    return img

def process(image, output_file):
    if (args.scale):
        image = scale(image)
    if args.trim:
        image = trim(image)
    if image.has_transparency_data:
        new_image = Image.new("RGBA", image.size, args.background_color)
        new_image.paste(image, (0, 0), image)
        image = new_image.convert('RGB')

    if args.bpp == 16:
        pixels = list(image.getdata())        
        with open(output_file, 'wb') as f:
            f.write(create_header(image.width, image.height))
            write_bin(f, pixels, image.width, image.height)
    if args.bpp < 16:
        image = image.quantize(colors=1 << args.bpp, kmeans=128, method=Image.MAXCOVERAGE)
        image.save(output_file);
    if args.bpp > 16:
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
            out = pathlib.Path(file).stem + "_" + str(row) + "_" + str(col) + "." + args.output
            box = (left, top, left+tw, top+th)
            process(img.crop(box), out)

parser = argparse.ArgumentParser(
    description="Batch convert image files, including output to 16 bit RGB565 format (default!)"
)

parser.add_argument(
    "-b",
    "--background",
    dest="background_color",
    default="BLACK",
    help="background color used when downsampling images with an alpha channel, e.g WHITE or BLACK - defaults to BLACK"
)

parser.add_argument(
    "-o",
    "--output",
    dest="output",
    default="bmp",
    type=str.lower,
    help="output file type - defaults to bmp"
)

parser.add_argument(
    "-p",
    "--bpp",
    dest="bpp",
    type=int,
    default=16,
    choices=[1, 8, 16, 24, 32],
    help="bits per pixel - defaults to 16"
)

parser.add_argument(
    "-d",
    "--dither",
    dest="dither",
    action="store_true",
    help="use dithering when downsampling"
)

parser.add_argument(
    "-c",
    "--cols",
    dest="cols",
    type=int,
    default=1,
    help="number of columns to split input into - defaults to 1"
)

parser.add_argument(
    "-r",
    "--rows",
    dest="rows",
    type=int,
    default=1,
    help="number of rows to split input into - defaults to 1"
)

parser.add_argument(
    "-t",
    "--trim",
    dest="trim",
    action="store_true",
    help="trim input image to content - applied after scaling if used"
)

parser.add_argument(
    "-f",
    "--fuzzy",
    dest="fuzzy",
    action="store_true",
    help="use a fuzzy trim when trimming, used in conjuction with -t"
)

parser.add_argument(
    "-i",
    "--info",
    dest="info",
    action="store_true",
    help="only show image info"
)

parser.add_argument(
    "-s",
    "--scale",
    dest="scale",
    action="store_true",
    help="scale the input/tile to 135x240 maintaining aspect ratio"
)

parser.add_argument('file',nargs='+')

args = parser.parse_args()

if len(args.file) > 0:
    if args.bpp == 16 and args.output != 'bmp':
        print("16 bit output is only supported if the out file type is BMP")
        exit(1)

    for file in args.file:
        image = Image.open(file)
        print(file + ", colors=" + str(len(set(image.getdata()))) + ", width=" + str(image.width) + ", height=" + str(image.height))
        if not args.info:
            if args.rows > 1 and args.cols > 1:
                tile(file)
            else:
                output_file = pathlib.Path(file).stem + "." + args.output
                process(image, output_file)
else:
    print("You must specify one or more files to convert")
    exit(1)