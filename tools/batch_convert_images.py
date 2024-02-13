from PIL import Image, ImageChops, ImageEnhance
from io import BytesIO
import cairosvg
import random, math
import argparse
import struct, os, sys, glob, pathlib

def create_header(width, height):
    # Create an RGB565 BMP file header

    total_header_size = 14 + 40 # V3 len = 40 bytes
    total_header_size = 14 + 40 + 12 # V3 len = 40 bytes
    bitmap_size = ((16 * width + 31) >> 5) * 4 * height
    file_size = total_header_size + bitmap_size
    
    print("input file size=" + str(bitmap_size) + ", output file size=" + str(file_size));

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

def write_rgb565(f, byte_list, width, height):
    for row in range(height-1, -1, -1):
        for col in range(0, width * 2, 2) :
            c =(byte_list[row*width*2 + col]) | (byte_list[row*width*2 + col + 1]  << 8)
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

def resize(img, width, height):
    if image.size[0] == width and image.size[1] == height:   # unchanged
        return img
    img_width = img.size[0]
    img_height = img.size[1]
    wratio = width / float(img_width)
    hratio = height / float(img_height)
    ratio = min(wratio, hratio)  
    img = img.resize((int(img_width * ratio), int(img_height * ratio)), Image.Resampling.LANCZOS)
    return img

def add_to_pixels(img):
    pixels = img.load() 
    
    # Extracting the width and height 
    # of the image: 
    width, height = img.size 
    for i in range(width): 
        for j in range(height):
            c = pixels[i,j];
            pixels[i,j] = (c[0] + args.add, c[1] + args.add, c[2] + args.add)

def invert_image(img):
    pixels = img.load() 
    
    # Extracting the width and height 
    # of the image: 
    width, height = img.size 
    for i in range(width): 
        for j in range(height):
            c = pixels[i,j];
            pixels[i,j] = (c[0] ^ 0xff, c[1] ^ 0xff, c[2] ^ 0xff)

def process(image, output_file):
    if args.scale:
        image = resize(image, 135, 240)
    if args.scale_icon != -1:
        image = resize(image, args.scale_icon, args.scale_icon)
    if args.trim:
        image = trim(image)
    if image.has_transparency_data:
        new_image = Image.new("RGBA", image.size, args.background_color)
        new_image.paste(image, (0, 0), image)
        image = new_image.convert('RGB')

    if args.add != 0:
        add_to_pixels(image)

    if args.brightness != 1:
        image = ImageEnhance.Brightness(image).enhance(args.brightness)

    if args.contrast != 1:
        image = ImageEnhance.Contrast(image).enhance(args.contrast)

    if args.invert:
        invert_image(image)

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

def svg2image(file):
    out = BytesIO()
    cairosvg.svg2png(url=file, write_to=out)
    return Image.open(out)

parser = argparse.ArgumentParser(
    description="Batch convert image files, including output to 16 bit RGB565 format (default!)",
    add_help=False
)

parser.add_argument(
    "-?",
    "--help",
    action="help"
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
     "--brightness",
    dest="brightness",
    type=float,
    default=1,
    help="Modify the brightness(!) >1.0 increases brightness, 1.0 = original brightness, 0.0 = black"
)

parser.add_argument(
     "--add",
    dest="add",
    type=int,
    default=0,
    help="Add a value to each pixel, use a negative value to subtract from each pixel"
)

parser.add_argument(
     "--contrast",
    dest="contrast",
    type=float,
    default=1,
    help="Modify the contrast >1.0 increases contrast, 1.0 = original contrasy, 0.0 = grey"
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
    "-w",
    "--width",
    dest="width",
    type=int,
    default=1,
    help="width of input binary file in pixels (RGB565 without header)"
)

parser.add_argument(
    "-h",
    "--height",
    dest="height",
    type=int,
    default=1,
    help="height of input binary file (RGB565 without header)"
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
    "--invert",
    dest="invert",
    action="store_true",
    help="Invert colors"
)

parser.add_argument(
    "-s",
    "--scale",
    dest="scale",
    action="store_true",
    help="scale the input/tile to 135x240 maintaining aspect ratio"
)

parser.add_argument(
    "--scale_icon",
    dest="scale_icon",
    type=int,
    default=-1,
    help="scale the input/tile to valxval maintaining aspect ratio. Default is to not scale"
)

parser.add_argument('file',nargs='+')

args = parser.parse_args()

if len(args.file) > 0:
    if args.bpp == 16 and args.output != 'bmp':
        print("16 bit output is only supported if the out file type is BMP")
        exit(1)

    for file in args.file:
        fileType = pathlib.Path(file).suffix;
        if fileType == '.bin':
            with open(file, 'rb') as inf:
                pixels = list(inf.read())
                output_file = pathlib.Path(file).stem + "." + args.output
                with open(output_file, 'wb') as outf:
                    outf.write(create_header(args.width, args.height))
                    write_rgb565(outf, pixels, args.width, args.height)
        else:
            if fileType == '.svg':
                image = svg2image(file)
            else:
                image = Image.open(file)
                
            print(file + ", colors=" + str(len(set(image.getdata()))) + ", width=" + str(image.width) + ", height=" + str(image.height))
            if not args.info:
                if args.rows > 1 or args.cols > 1:
                    tile(file)
                else:
                    output_file = pathlib.Path(file).stem + "." + args.output
                    process(image, output_file)

else:
    print("You must specify one or more files to convert")
    exit(1)