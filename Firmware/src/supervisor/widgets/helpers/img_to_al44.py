#!/usr/bin/env python3

import PIL
import PIL.Image
import sys

img = PIL.Image.open(sys.argv[1])
pix = img.load()

w = img.size[0]
h = img.size[1]

cga_cols = [
    [ 0, 0, 0],
    [ 0, 0, 0xaa ],
    [ 0, 0xaa, 0 ],
    [ 0, 0xaa, 0xaa ],
    [ 0xaa, 0, 0 ],
    [ 0xaa, 0, 0xaa ],
    [ 0xaa, 0x55, 0 ],
    [ 0xaa, 0xaa, 0xaa ],
    [ 0x55, 0x55, 0x55 ],
    [ 0x55, 0x55, 0xff ],
    [ 0x55, 0xff, 0x55 ],
    [ 0x55, 0xff, 0xff ],
    [ 0xff, 0x55, 0x55 ],
    [ 0xff, 0x55, 0xff ],
    [ 0xff, 0xff, 0x55 ],
    [ 0xff, 0xff, 0xff ]
]

def get_nearest(c):
    cur_nearest = -1
    cur_nearest_val = 0xffffffff

    for i in range(0, 16):
        cval = (c[0] - cga_cols[i][0]) * (c[0] - cga_cols[i][0]) + \
            (c[1] - cga_cols[i][1]) * (c[1] - cga_cols[i][1]) + \
            (c[2] - cga_cols[i][2]) * (c[2] - cga_cols[i][2])
        if cval < cur_nearest_val:
            cur_nearest = i
            cur_nearest_val = cval
    
    return cur_nearest

for y in range(0, h):
    for x in range(0, w):
        cp = pix[x,y]
        cc = get_nearest(cp[0:3])
        ca = round(cp[3] / 16)
        if ca >= 16:
            ca = 15
        
        cv = (ca << 4) + cc
        print("0x" + format(cv, '02x') + ", ", end='')
    
    print("")

