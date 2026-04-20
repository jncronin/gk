#!/usr/bin/env python3

from PIL import Image
import argparse

parser = argparse.ArgumentParser("png2l8bin")
parser.add_argument("-i", "--input", type=str, help="Input PNG file", required=True)
parser.add_argument("-o", "--output", type=str, help="output binary file", required=True)
args = parser.parse_args()

with Image.open(args.input).convert("RGB") as im, open(args.output, "wb") as of:
    d = im.getdata()

    for i in range(48000):
        r = float(d[i][0])
        g = float(d[i][1])
        b = float(d[i][2])

        l = round((r + g + b) / 3.0)
        
        of.write(l.to_bytes())
