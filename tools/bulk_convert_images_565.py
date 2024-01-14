from PIL import Image, ImageChops

import argparse
import struct, os, sys, glob

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
    diff = ImageChops.add(diff, diff, 2.0, -100)
    bbox = diff.getbbox(alpha_only=False)
    if bbox:
        return im.crop(bbox)
    else:
        return im
    
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
    "-t",
    "--trim",
    dest="trim",
    action="store_true",
    help="Trim input image to content"
)

parser.add_argument('file',nargs='+')

args = parser.parse_args()

if len(args.file) > 0:
    for file in args.file:
        print(file)
        image = Image.open(file)
        if args.trim:
            image = trim(image)
        new_image = Image.new("RGBA", image.size, args.background_color)
        new_image.paste(image, (0, 0), image)
        downscaled_image = new_image.convert('RGB')
        pixels = list(downscaled_image.getdata())
        # print pixels
        
        with open(file.replace("png", "bmp"), 'wb') as f:
            f.write(create_header(downscaled_image.width, downscaled_image.height));
            write_bin(f, pixels, downscaled_image.width, downscaled_image.height)
else:
    print("You must specify one or more PNG files to convert")