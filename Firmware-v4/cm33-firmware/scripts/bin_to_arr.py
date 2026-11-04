#!/usr/bin/env python3

import argparse

parser = argparse.ArgumentParser("bin_to_arr")
parser.add_argument("-i", "--input", type=str, help="Input bin file", required=True)
parser.add_argument("-o", "--output", type=str, help="output c file", required=True)
args = parser.parse_args()

with open(args.input, "rb") as im, open(args.output, "w") as of:
    ifile = im.read()
    is_first = True
    line_idx = 0
    of.write("{")
    for b in ifile:
        if is_first == False:
            of.write(", ")
        if line_idx == 0:
            of.write("\n\t")
        is_first = False
        of.write("0x%0.2x" % b)
        line_idx = line_idx + 1
        if line_idx == 8:
            line_idx = 0
    of.write("\n}\n")

