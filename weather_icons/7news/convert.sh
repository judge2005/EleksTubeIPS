#!/bin/bash

python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 01d.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 01n.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 02d.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 02n.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 03d.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 03n.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 04d.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 04n.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 09d.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 09n.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 10d.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 10n.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 11d.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 11n.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 13d.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 13n.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 50d.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -d -t -f --alpha argb4444 50n.svg
python ../../tools/batch_convert_images.py -p 16 --scale_icon 120 -t -f --alpha argb4444 unknown.svg

python ../../tools/pack_images.py -o 7news.tar.gz [0-9]*.bmp unknown.bmp
