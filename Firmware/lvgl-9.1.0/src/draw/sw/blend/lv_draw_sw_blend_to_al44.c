#include "lv_draw_sw_blend_to_argb8888.h"
#if LV_USE_DRAW_SW

// Modified for GK


#define FLOAT_ARITHMETIC 1

#if FLOAT_ARITHMETIC
#include <math.h>
#endif

/* The following from the R source:

pals <- c(0x000000, 0x0000aa, 0x00aa00, 0x00aaaa,
         0xaa0000, 0xaa00aa, 0xaa5500, 0xaaaaaa,
         0x555555, 0x5555ff, 0x55ff55, 0x55ffff,
         0xff5555, 0xff55ff, 0xffff55, 0xffffff)

crgbs <- seq(from=0, to=63)

pal.map <- c(0, 0x55, 0xaa, 0xff)

crgb.to.pal <- function(crgb)
{
  r <- pal.map[bitwAnd(bitwShiftR(crgb, 4), 0x3) + 1]
  g <- pal.map[bitwAnd(bitwShiftR(crgb, 2), 0x3) + 1]
  b <- pal.map[bitwAnd(bitwShiftR(crgb, 0), 0x3) + 1]
  
  return(bitwOr(bitwOr(bitwShiftL(r, 16), bitwShiftL(g, 8)), b))
}

pal.to.palid <- function(pal)
{
  diffs <- sapply(pals, function(cpal)
  {
    r.diff <- bitwShiftR(pal, 16) - bitwShiftR(cpal, 16)
    g.diff <- bitwAnd(bitwShiftR(pal, 8), 0xff) - bitwAnd(bitwShiftR(cpal, 8), 0xff)
    b.diff <- bitwAnd(pal, 0xff) - bitwAnd(cpal, 0xff)
    
    return(sqrt(r.diff * r.diff + g.diff * g.diff + b.diff * b.diff))
  })
  
  return(which.min(diffs))
}

print(paste(sapply(crgbs, function(x) pal.to.palid(crgb.to.pal(x))-1), collapse=","))
*/

static const unsigned char crgb_to_cga[] = 
{
0,0,1,1,0,8,1,9,2,2,3,3,2,10,3,11,0,8,1,9,6,8,8,9,2,8,3,9,10,10,10,11,4,4,5,5,6,6,5,9,6,7,7,7,10,10,7,11,4,12,5,13,6,12,12,13,6,12,7,13,14,14,14,15
};

static const uint32_t cga_palette[] = {
        0x000000, 0x0000aa, 0x00aa00, 0x00aaaa,
        0xaa0000, 0xaa00aa, 0xaa5500, 0xaaaaaa,
        0x555555, 0x5555ff, 0x55ff55, 0x55ffff,
        0xff5555, 0xff55ff, 0xffff55, 0xffffff };

static inline unsigned char a_to_cga(unsigned int a)
{
    return (a / 16) << 4;
}

static inline unsigned char col_to_cga(lv_color_t rgb)
{
    // first, convert rgb to 0:3 -> so 64 possibilites
    unsigned int r = rgb.red / 64;
    unsigned int g = rgb.green / 64;
    unsigned int b = rgb.blue / 64;
    unsigned int crgb = (r << 4) | (g << 2) | b;

    return crgb_to_cga[crgb];
}

int klog(const char *format, ...);

#if FLOAT_ARITHMETIC
static inline unsigned char al44_blend(unsigned int src, unsigned int dest)
{
    /* blend as per DMA2D to allow white completely transparent bottom layer to be properly overwritten
    
        amult = afg * abg / 255
        aout = afg + abg - amult
        cout = (cfg * afg + cbg * abg - cbg * amult) / aout */
    uint32_t src_rgb = cga_palette[src & 0xf];
    uint32_t dest_rgb = cga_palette[dest & 0xf];
    float src_a = (float)((src >> 4) & 0xf) / 15.0f;
    float dest_a = (float)((dest >> 4) & 0xf) / 15.0f;
    float amult = src_a * dest_a;
    float aout = src_a + dest_a - amult;

    float src_r = (float)(src_rgb >> 16) / 255.0f;
    float src_g = (float)((src_rgb >> 8) & 0xff) / 255.0f;
    float src_b = (float)(src_rgb & 0xff) / 255.0f;

    float dest_r = (float)(dest_rgb >> 16) / 255.0f;
    float dest_g = (float)((dest_rgb >> 8) & 0xff) / 255.0f;
    float dest_b = (float)(dest_rgb & 0xff) / 255.0f;

    float out_r = (src_r * src_a + dest_r * (dest_a - amult)) / aout;
    float out_g = (src_g * src_a + dest_g * (dest_a - amult)) / aout;
    float out_b = (src_b * src_a + dest_b * (dest_a - amult)) / aout;

    uint32_t out_crgb = ((uint32_t)rintf(out_r * 3.0f) << 4) |
        ((uint32_t)rintf(out_g * 3.0f) << 2) |
        (uint32_t)rintf(out_b * 3.0f);

    unsigned char out_c = crgb_to_cga[out_crgb];
    unsigned char ret = (((unsigned char)rintf(aout * 15.0f) & 0xf) << 4) | out_c;

    //klog("al44_blend: %02x, %02x ((%f,%f,%f),(%f,%f,%f)) to %02x (%f,%f,%f)\n",
    //    src, dest, src_r, src_g, src_b, dest_r, dest_g, dest_b, ret, out_r, out_g, out_b);

    return ret;
}
#else
static inline unsigned char al44_blend(unsigned int src, unsigned int dest)
{
    /* src * src_alpha + dest * (1-dest_alpha) 
        given integer arithmetic tends to round down towards darker colours */
    uint32_t src_rgb = cga_palette[src & 0xf];
    uint32_t dest_rgb = cga_palette[dest & 0xf];
    uint32_t src_a = (src >> 4) & 0xf;

    uint32_t src_r = src_rgb >> 16;
    uint32_t src_g = (src_rgb >> 8) & 0xff;
    uint32_t src_b = src_rgb & 0xff;

    uint32_t dest_r = dest_rgb >> 16;
    uint32_t dest_g = (dest_rgb >> 8) & 0xff;
    uint32_t dest_b = dest_rgb & 0xff;

    uint32_t out_r = ((src_r * src_a) + (dest_r * (16U - src_a))) / 16;
    uint32_t out_g = ((src_g * src_a) + (dest_g * (16U - src_a))) / 16;
    uint32_t out_b = ((src_b * src_a) + (dest_b * (16U - src_a))) / 16;

    uint32_t out_crgb = ((out_r / 64) << 4) | ((out_g / 64) << 2) | (out_b / 64);

    unsigned char out_c = crgb_to_cga[out_crgb];
    return (src & 0xf0) | out_c;
}
#endif

void lv_draw_sw_blend_image_to_al44(_lv_draw_sw_blend_image_dsc_t * dsc)
{
    __asm__ volatile("bkpt \n" ::: "memory");
}

void lv_draw_sw_blend_color_to_al44(_lv_draw_sw_blend_fill_dsc_t * dsc)
{
    int32_t w = dsc->dest_w;
    int32_t h = dsc->dest_h;
    lv_opa_t opa = dsc->opa;
    int32_t mask_stride = dsc->mask_stride;
    int32_t dest_stride = dsc->dest_stride;
    unsigned char *dest_buf = dsc->dest_buf;

    if(!dsc->mask_buf)
    {
        if(opa >= LV_OPA_MAX)
        {
            // don't blend
            unsigned char c = col_to_cga(dsc->color) | 0xf0;

            for(int y = 0; y < h; y++)
            {
                for(int x = 0; x < w; x++)
                {
                    dest_buf[x + y * dest_stride] = c;
                }
            }
        }
        else if(opa >= LV_OPA_MIN)
        {
            unsigned char c = col_to_cga(dsc->color) | a_to_cga(opa);

            for(int y = 0; y < h; y++)
            {
                for(int x = 0; x < w; x++)
                {
                    // blend
                    dest_buf[x + y * dsc->dest_stride] = al44_blend(
                        c, dest_buf[x + y * dsc->dest_stride]);
                }
            }
        }
        else
        {
            // completely transparent rectangles _shouldn't_ get through to this part unless
            //  they are the bottom layer - therefore special case and just write them direct
            //  to the buffer
            unsigned char c = col_to_cga(dsc->color);

            for(int y = 0; y < h; y++)
            {
                for(int x = 0; x < w; x++)
                {
                    dest_buf[x + y * dsc->dest_stride] = c;
                }
            }
        }
    }
    else
    {
        // has mask buffer
        for(int y = 0; y < h; y++)
        {
            for(int x = 0; x < w; x++)
            {
                unsigned char c;
                if(opa < LV_OPA_MIN)
                {
                    c = 0;
                }
                else if(opa >= LV_OPA_MAX)
                {
                    c = col_to_cga(dsc->color) | a_to_cga(dsc->mask_buf[x + y * dsc->mask_stride]);
                }
                else
                {
#if FLOAT_ARITHMETIC
                    float op_a = (float)opa / 255.0f;
                    float mask_a = (float)dsc->mask_buf[x + y * dsc->mask_stride] / 15.0f;
                    unsigned int a = ((unsigned int)rintf((op_a * mask_a) * 15.0f)) << 4;

#else
                    unsigned int opa_a = a_to_cga(opa);
                    unsigned int mask_a = a_to_cga(dsc->mask_buf[x + y * dsc->mask_stride]);
                    unsigned int a = ((opa_a >> 4) * (mask_a >> 4)) & 0xf0;
#endif
                    c = col_to_cga(dsc->color) | a;
                }

                if((c & 0xf0) == 0xf0)
                {
                    dest_buf[x + y * dsc->dest_stride] = c;
                }
                else if(c & 0xf0)
                {
                    // blend
                    dest_buf[x + y * dsc->dest_stride] = al44_blend(
                        c, dest_buf[x + y * dsc->dest_stride]);
                }
            }
        }
    }
}


#endif
