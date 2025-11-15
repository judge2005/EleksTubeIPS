#!/bin/bash

python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 01d.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 01n.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 02d.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 02n.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 03d.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 03n.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 04d.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 04n.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 09d.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 09n.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 10d.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 10n.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 11d.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 11n.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 13d.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 13n.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 50d.svg
python ../../tools/batch_convert_images.py -p 16 -s -d -t -f --alpha argb4444 50n.svg

#On mac have to manually remove extended attributes from files using
#xattrs -c running in mac terminal app. Sigh
#tar cvfz maxclassic.tar.gz [0-9]*.bmp
