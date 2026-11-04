#!/usr/bin/env python3

from PIL import Image
import argparse

parser = argparse.ArgumentParser("oled_test_img")
parser.add_argument("-i", "--input", type=str, help="Input PNG file", required=True)
parser.add_argument("-o", "--output", type=str, help="output binary file", required=True)
args = parser.parse_args()

with Image.open(args.input).convert("RGB") as im, open(args.output, "wb") as of:
    d = im.getdata()

    for page in range(8):
        for col in range(128):
            cur_val = 0
            for bit in range(8):
                in_y = page * 8 + bit
                in_idx = in_y * 128 + col
                r = float(d[in_idx][0])
                g = float(d[in_idx][1])
                b = float(d[in_idx][2])

                l = round((r + g + b) / 3.0)

                bit_val = 0
                if l > 0.3:
                    bit_val = 1
                cur_val = cur_val | (bit_val << bit)
        
            of.write(cur_val.to_bytes())
