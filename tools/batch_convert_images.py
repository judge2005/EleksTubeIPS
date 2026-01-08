from PIL import Image, ImageChops, ImageEnhance
from io import BytesIO
import cairosvg
import random, math
import argparse
import struct, os, sys, glob, pathlib

def create_header(width, height):
    # Create an RGB565 BMP file header

    total_header_size = 14 + 40 # V3 len = 40 bytes
    palette_size = 0
    match args.bpp:
        case 16:
            if args.alpha == "none" :
                total_header_size = 14 + 40 + 12 # V3 len = 52 bytes
            else:
                total_header_size = 14 + 40 + 16 # V3 len + alpha = 56 bytes
        case 4:
            palette_size = 64
        case 2:
            palette_size = 16
        case 1:
            palette_size = 8
            
    bitmap_size = ((args.bpp * width + 31) >> 5) * 4 * height

    file_size = total_header_size + palette_size + bitmap_size
    
    print("input file size=" + str(bitmap_size) + ", output file size=" + str(file_size))

    # struct.pack: I=4 bytes, H=2 bytes, lower case=signed, '<'=little-endian
    # BMP header: Magic (2 bytes), file size, 2 ignored values, bitmap offset
    header = struct.pack('<H 3I', 0x4D42, file_size, 0, total_header_size + palette_size)

    # DIB V3 header: header size, px width, px height, num of color planes, bpp, compression method,
    # bitmap data size, horizontal resolution, vertical resolution, number of colors in palette, number of important colors used
    # Few of these matter, so there are a bunch of default/"magic" numbers here...
    dib_header = struct.pack('<I 2i H H I I 2i', 40, width, height, 1, args.bpp, 0, file_size, 0x0B13, 0x0B13)

    match args.bpp:
        case 16:
            if args.alpha == "none" :
                print("rgb565")
                dib_header = struct.pack('<I 2i H H I I 2i', 52, width, height, 1, args.bpp, 3, file_size, 0x0B13, 0x0B13)
                # Zero size palette, zero important colors
                dib_header += struct.pack('<2I', 0, 0)
                # RGB565 masks
                dib_header += struct.pack('<3i', 0xf800, 0x07e0, 0x001f)
            elif args.alpha == "argb1555" :
                print("argb1555")
                dib_header = struct.pack('<I 2i H H I I 2i', 56, width, height, 1, args.bpp, 3, file_size, 0x0B13, 0x0B13)
                # Zero size palette, zero important colors
                dib_header += struct.pack('<2I', 0, 0)
                # RGB565 masks
                dib_header += struct.pack('<4i', 0x7c00, 0x03e0, 0x001f, 0x8000)
            else:
                print("argb4444")
                dib_header = struct.pack('<I 2i H H I I 2i', 56, width, height, 1, args.bpp, 3, file_size, 0x0B13, 0x0B13)
                # Zero size palette, zero important colors
                dib_header += struct.pack('<2I', 0, 0)
                # RGB565 masks
                dib_header += struct.pack('<4i', 0x0f00, 0x00f0, 0x000f, 0xf000)
        case 4:
            dib_header += struct.pack('<2I', 16, 16)
        case 2:
            dib_header += struct.pack('<2I', 4, 4)
        case 1:
            dib_header += struct.pack('<2I', 2, 2)

    header += dib_header

    return header

max_pixel = {
    2: 0x7f,
    4: 0x3f,
    8: 0x1f,
    16: 0x0f,
    32: 0x07,
    64: 0x03,
    128: 0x01
}

def scale_pixel(p, factor, dither = True):
    if args.dither and dither :
        val = p // factor
        if val != max_pixel[factor] :
            rem = p % factor
            prob = rem / factor
            if random.random() < prob :
                val = val + 1
        # val = int(math.floor(float(p) * (256.0 - factor) / 255.0 / float(factor) + random.random()))
    else:
        val = p // factor

    return val

def write_bin(f, image, width, height):
    pixel_list = list(image.getdata())        

    match args.bpp:
        case 16:
            for row in range(height-1, -1, -1):
                for col in range(0, width, 1) :
                    if args.alpha == "none" :
                        r = scale_pixel(pixel_list[row*width + col][0], 8)
                        g = scale_pixel(pixel_list[row*width + col][1], 4)
                        b = scale_pixel(pixel_list[row*width + col][2], 8)
                        c = (r << 11) | (g << 5) | b
                        f.write(struct.pack('<H', c))
                    elif args.alpha == "argb1555" :
                        r = scale_pixel(pixel_list[row*width + col][0], 8)
                        g = scale_pixel(pixel_list[row*width + col][1], 8)
                        b = scale_pixel(pixel_list[row*width + col][2], 8)
                        if len(pixel_list[row*width + col]) == 4 :
                            a = scale_pixel(pixel_list[row*width + col][3], 128)
                        else:
                            a = 1
                        c = (a << 15) | (r << 10) | (g << 5) | b
                        f.write(struct.pack('<H', c))
                    else:
                        r = scale_pixel(pixel_list[row*width + col][0], 16)
                        g = scale_pixel(pixel_list[row*width + col][1], 16)
                        b = scale_pixel(pixel_list[row*width + col][2], 16)
                        if len(pixel_list[row*width + col]) == 4 :
                            a = scale_pixel(pixel_list[row*width + col][3], 16)
                        else:
                            a = 0x0f
                        c = (a << 12) | (r << 8) | (g << 4) | b
                        f.write(struct.pack('<H', c))
                if (width % 2) == 1:
                    f.write(struct.pack('<H', 0))

        case 4: # Image will already be in 'P' mode, i.e. one byte per pixel
            colors = image.getpalette()
            print(colors)
            # Write out the palette
            for color in range(0, len(colors), 3):
                f.write(struct.pack('<I', (colors[color] << 16) + (colors[color+1] << 8) + (colors[color+2])))
            for row in range(height-1, -1, -1):
                byte = 0
                pixels = 0
                bytesToWrite = 4
                for col in range(0, width, 1) :
                    c = pixel_list[row*width + col]
                    byte = byte << 4 | c
                    pixels += 1
                    if pixels == 2:
                        bytesToWrite = (bytesToWrite + 3) % 4
                        pixels = 0
                        f.write(struct.pack('<B', byte))
                        byte = 0
                if pixels != 0:
                    bytesToWrite = (bytesToWrite + 3) % 4
                    f.write(struct.pack('<B', byte << ((2-pixels) * 4)))
                for i in range(0, bytesToWrite):
                    f.write(struct.pack('<B', 0))

        case 2: # Image will already be in 'P' mode, i.e. one byte per pixel
            colors = image.getpalette()
            print(colors)
            # Write out the palette
            for color in range(0, len(colors), 3):
                f.write(struct.pack('<I', (colors[color] << 16) + (colors[color+1] << 8) + (colors[color+2])))
            for row in range(height-1, -1, -1):
                byte = 0
                pixels = 0
                bytesToWrite = 4
                for col in range(0, width, 1) :
                    c = pixel_list[row*width + col]
                    byte = byte << 2 | c
                    pixels += 1
                    if pixels == 4:
                        bytesToWrite = (bytesToWrite + 3) % 4
                        pixels = 0
                        f.write(struct.pack('<B', byte))
                        byte = 0
                if pixels != 0:
                    bytesToWrite = (bytesToWrite + 3) % 4
                    f.write(struct.pack('<B', byte << ((4-pixels) * 2)))
                for i in range(0, bytesToWrite):
                    f.write(struct.pack('<B', 0))

        case 1: # Image will already be in 'P' mode, i.e. one byte per pixel
            colors = image.getpalette()
            print(colors)
            # Do this in reverse order because quantize does some weird stuff
            for color in range(len(colors)-3, -3, -3):
                f.write(struct.pack('<I', (colors[color] << 16) + (colors[color+1] << 8) + (colors[color+2])))
            for row in range(height-1, -1, -1):
                byte = 0
                pixels = 0
                bytesToWrite = 4
                for col in range(0, width, 1) :
                    c = pixel_list[row*width + col]
                    # this looks odd image.quantize seems to invert the pixels
                    byte = byte << 1 | (1-c)
                    pixels += 1
                    if pixels == 8:
                        bytesToWrite = (bytesToWrite + 3) % 4
                        pixels = 0
                        f.write(struct.pack('<B', byte))
                        byte = 0
                if pixels != 0:
                    bytesToWrite = (bytesToWrite + 3) % 4
                    f.write(struct.pack('<B', byte << (8-pixels)))
                for i in range(0, bytesToWrite):
                    f.write(struct.pack('<B', 0))

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
            c = pixels[i,j]
            pixels[i,j] = (c[0] + args.add, c[1] + args.add, c[2] + args.add)

def invert_image(img):
    return ImageChops.invert(img)

def process(image, output_file):
    # if not image.has_transparency_data :
    #     print ("No transparency: " + image.has_transparency_data)
    #     args.alpha = "none"

    if args.scale:
        image = resize(image, 135, 240)
    if args.scale_icon != -1:
        image = resize(image, args.scale_icon, args.scale_icon)
    if args.trim:
        image = trim(image)
    if image.has_transparency_data and args.alpha == "none":
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
        image = invert_image(image)

    if args.bpp < 16:
        image = image.quantize(colors=1 << args.bpp, kmeans=128, method=Image.MAXCOVERAGE)

    if args.bpp == 16 or args.bpp == 4 or args.bpp == 2 or args.bpp == 1:
        with open(output_file, 'wb') as f:
            f.write(create_header(image.width, image.height))
            write_bin(f, image, image.width, image.height)
    else:
        image.save(output_file)
    
    print(output_file + ", colors=" + str(len(set(image.getdata()))) + ", width=" + str(image.width) + ", height=" + str(image.height))

def tile(file):
    img = Image.open(file)
    w, h = img.size
    if args.width >1 or args.height > 1:
        h = args.height
        w = args.width
    th = h / args.rows
    tw = w / args.cols
    for row in range(0, args.rows, 1):
        for col in range(0, args.cols, 1) :
            left = args.xoffset + col * tw
            top = args.yoffset + row * th
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
    choices=[1, 2, 4, 8, 16, 24, 32],
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
    "-x",
    "--xoffset",
    dest="xoffset",
    type=int,
    default=0,
    help="initial x offset when tiling - defaults to 0"
)
parser.add_argument(
    "-y",
    "--yoffset",
    dest="yoffset",
    type=int,
    default=0,
    help="initial y offset when tiling - defaults to 0"
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

parser.add_argument(
    "--alpha",
    dest="alpha",
    type=str.lower,
    default="none",
    help="one of none, argb1555 or argb4444"
)

parser.add_argument('file',nargs='+')

args = parser.parse_args()

if len(args.file) > 0:
    if args.bpp == 16 and args.output != 'bmp':
        print("16 bit output is only supported if the out file type is BMP")
        exit(1)

    for file in args.file:
        print("input file=" + file)
        fileType = pathlib.Path(file).suffix
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