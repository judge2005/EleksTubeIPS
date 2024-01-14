from PIL import Image
import glob
import os

# get the current working directory
current_working_directory = os.getcwd()

# print output to the console
print(current_working_directory)
  
for file in glob.glob("*.png"):
    print(file)
    image = Image.open(file)
    new_image = Image.new("RGBA", image.size, "BLACK")
    new_image.paste(image, (0, 0), image)
    new_image.save(file.replace("png", "bmp"))