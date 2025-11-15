#!/bin/bash

python ../../tools/batch_convert_images.py -p 16 -d -t -f --alpha argb4444 sunny.png
python ../../tools/batch_convert_images.py -p 16 -d -t -f --alpha argb4444 sunny_night.png
python ../../tools/batch_convert_images.py -p 16 -d -t -f --alpha argb4444 cloudy1.png
python ../../tools/batch_convert_images.py -p 16 -d -t -f --alpha argb4444 cloudy1_night.png
python ../../tools/batch_convert_images.py -p 16 -d -t -f --alpha argb4444 cloudy2.png
python ../../tools/batch_convert_images.py -p 16 -d -t -f --alpha argb4444 cloudy2_night.png
python ../../tools/batch_convert_images.py -p 16 -d -t -f --alpha argb4444 cloudy4.png
python ../../tools/batch_convert_images.py -p 16 -d -t -f --alpha argb4444 cloudy4_night.png
python ../../tools/batch_convert_images.py -p 16 -d -t -f --alpha argb4444 shower2.png
python ../../tools/batch_convert_images.py -p 16 -d -t -f --alpha argb4444 shower2_night.png
python ../../tools/batch_convert_images.py -p 16 -d -t -f --alpha argb4444 shower3.png
python ../../tools/batch_convert_images.py -p 16 -d -t -f --alpha argb4444 tstorm3.png
python ../../tools/batch_convert_images.py -p 16 -d -t -f --alpha argb4444 snow5.png
python ../../tools/batch_convert_images.py -p 16 -d -t -f --alpha argb4444 fog.png
python ../../tools/batch_convert_images.py -p 16 -d -t -f --alpha argb4444 fog_night.png


cp sunny.bmp 01d.bmp
cp sunny_night.bmp 01n.bmp
cp cloudy1.bmp 02d.bmp
cp cloudy1_night.bmp 02n.bmp
cp cloudy2.bmp 03d.bmp
cp cloudy2_night.bmp 03n.bmp
cp cloudy4.bmp 04d.bmp
cp cloudy4_night.bmp 04n.bmp
cp shower2.bmp 09d.bmp
cp shower2_night.bmp 09n.bmp
cp shower3.bmp 10d.bmp
cp shower3.bmp 10n.bmp
cp tstorm3.bmp 11d.bmp
cp tstorm3.bmp 11n.bmp
cp snow5.bmp 13d.bmp
cp snow5.bmp 13n.bmp
cp fog.bmp 50d.bmp
cp fog_night.bmp 50n.bmp

#On mac have to manually remove extended attributes from files using
#xattrs -c running in mac terminal app. Sigh
#tar cvfz maxclassic.tar.gz [0-9]*.bmp
