#!/usr/bin/env python3

from PIL import Image
import argparse

parser = argparse.ArgumentParser("png2argb8888bin")
parser.add_argument("-i", "--input", type=str, help="Input PNG file", required=True)
parser.add_argument("-o", "--output", type=str, help="output binary file", required=True)
args = parser.parse_args()

with Image.open(args.input).convert("RGBA") as im, open(args.output, "wb") as of:
    d = im.getdata()

    for i in range(im.width * im.height):
        r = d[i][0]
        g = d[i][1]
        b = d[i][2]
        a = d[i][3]
        
        of.write(b.to_bytes())
        of.write(g.to_bytes())
        of.write(r.to_bytes())
        of.write(a.to_bytes())
