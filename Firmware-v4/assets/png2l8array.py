#!/usr/bin/env python3

from PIL import Image
import argparse

parser = argparse.ArgumentParser("png2l8array")
parser.add_argument("-i", "--input", type=str, help="Input PNG file", required=True)
parser.add_argument("-n", "--name", type=str, help="array name", required=True)
args = parser.parse_args()

im = Image.open(args.input).convert("RGB")
d = im.getdata()

print("#include <stdint.h>")
print("")
print("const uint8_t " + args.name + "[] = {", end='')

for i in range(48000):
    r = float(d[i][0])
    g = float(d[i][1])
    b = float(d[i][2])

    l = round((r + g + b) / 3.0)

    if i % im.width == 0:
        print("")
        print("\t", end='')
    
    print("{:3d}".format(l) + ", ", end='')

print("}")
print("")

