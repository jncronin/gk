#include "process.h"
#include "cm33_interface.h"
#include "cm33_data.h"
#include "vmem.h"
#include "screen.h"

/* OLED images are a 48x48 image offset within the 128x64 output image starting at coordinates
    (80, 8).
    
    The image itself is arranged in 8x 128 byte pages each representing a row 8 pixels high.
    Thus, the actual 48x48 image represents 6 pages.

    Each byte within a page represents a 8 pixel high column, with LSB at the top.

    The OLED only supports 1 = white and 0 = black
*/

static inline uint8_t icol_to_bit(unsigned int pf_from, uint32_t from)
{
    uint32_t a = 0, r = 0, g = 0, b = 0;

    switch(pf_from)
    {
        case GK_PIXELFORMAT_ARGB8888:
            b = from & 0xffU;
            g = (from >> 8) & 0xffU;
            r = (from >> 16) & 0xffU;
            a = (from >> 24) & 0xffU;
            break;
        case GK_PIXELFORMAT_XRGB8888:
            b = from & 0xffU;
            g = (from >> 8) & 0xffU;
            r = (from >> 16) & 0xffU;
            a = 255;
            break;
        case GK_PIXELFORMAT_ABGR8888:
            r = from & 0xffU;
            g = (from >> 8) & 0xffU;
            b = (from >> 16) & 0xffU;
            a = (from >> 24) & 0xffU;
            break;
        case GK_PIXELFORMAT_RGBA8888:
            a = from & 0xffU;
            b = (from >> 8) & 0xffU;
            g = (from >> 16) & 0xffU;
            r = (from >> 24) & 0xffU;
            break;
        case GK_PIXELFORMAT_BGRA8888:
            a = from & 0xffU;
            r = (from >> 8) & 0xffU;
            g = (from >> 16) & 0xffU;
            b = (from >> 24) & 0xffU;
            break;
        case GK_PIXELFORMAT_RGB888:
            a = 255;
            b = from & 0xffU;
            g = (from >> 8) & 0xffU;
            r = (from >> 16) & 0xffU;
            break;
        case GK_PIXELFORMAT_RGB565:
            a = 255;
            b = (from & 0x1fU) << 3;
            g = ((from >> 5) & 0x3fU) << 2;
            r = ((from >> 11) & 0x1fU) << 3;
            break;
        case GK_PIXELFORMAT_RGB565A8:
            a = (from >> 16) & 0xffU;
            b = (from & 0x1fU) << 3;
            g = ((from >> 5) & 0x3fU) << 2;
            r = ((from >> 11) & 0x1fU) << 3;
            break;
        case GK_PIXELFORMAT_BGR565:
            a = 255;
            r = (from & 0x1fU) << 3;
            g = ((from >> 5) & 0x3fU) << 2;
            b = ((from >> 11) & 0x1fU) << 3;
            break;
        case GK_PIXELFORMAT_RGB8:
            a = 255;
            r = from & 0xffU;
            b = from & 0xffU;
            g = from & 0xffU;
            break;
        default:
            break;
    }

    r *= a;
    g *= a;
    b *= a;

    auto ret_val = r + g + b;

    /* ret_val is here scaled to 0 - 3 * 255 * 255 = 195075
        To allow full red alone to be interpreted as a '1' for the oled, we set the threshold
        to 255*255 = 65025 */
    return (ret_val >= 65025) ? 0x1u : 0x0u;
}

static int update_oled_buffer(void *buf, const Process::oled_t *odata)
{
    if(!buf)
        return -1;
    
    for(auto i = 0u; i < 1024u/4; i++)
    {
        *(uint32_t *)((uintptr_t)buf + i * 4) = 0;
    }

    auto cfile = odata->file;

    if(!cfile)
    {
        return 0;
    }
    if(cfile->GetType() != FileType::FT_DMABuf)
    {
        return -1;
    }

    auto paddr = ((DMABufFile *)cfile.get())->GetMem();
    if(!paddr.valid || !paddr.base)
        return -1;
    auto vaddr = PMEM_TO_VMEM(paddr.base);
    auto bpp = screen_get_bpp_for_pf(odata->pf);
    if(!bpp)
        return -1;
    if(odata->h * odata->stride > paddr.length)
        return -1;

    uint8_t *out_buf = (uint8_t *)buf;

    for(auto ipage = 0u; ipage < (GK_OLED_H / 8u); ipage++)
    {
        auto opage = ipage + 1u;
        for(auto icol = 0u; icol < GK_OLED_W; icol++)
        {
            uint8_t cur_byte = 0;
            auto ocol = icol + 80u;

            for(auto ibit = 0u; ibit < 8u; ibit++)
            {
                auto iy = ipage * 8 + ibit;
                uint8_t bit_val;

                if(icol >= odata->w || iy >= odata->h)
                {
                    // outside input image - encode black
                    bit_val = 0u;
                }
                else
                {
                    // lookup from the image
                    uint32_t ic = *(uint32_t *)(vaddr + iy * odata->stride + icol * bpp);
                    bit_val = icol_to_bit(odata->pf, ic);
                }

                cur_byte |= bit_val << ibit;
            }

            out_buf[opage * 128 + ocol] = cur_byte;
        }
    }

    return 0;
}

int Process::ShowOled(int scr_id)
{
    if(scr_id < 0 || scr_id >= GK_OLED_COUNT)
        return -1;

    CriticalGuard cg(oleds[scr_id].sl);
    if(update_oled_buffer(cm33_get_oled_buffer(scr_id), &oleds[scr_id]) != 0)
        return -1;
    if(cm33_set_oled_scr(scr_id, oleds[scr_id].file != nullptr) != 0)
        return -1;
    cg.unlock();

    return cm33_set_oled(true);
}

int Process::ShowOleds()
{
    bool one_enabled = false;

    for(auto scr_id = 0u; scr_id < GK_OLED_COUNT; scr_id++)
    {
        CriticalGuard cg(oleds[scr_id].sl);
        if(update_oled_buffer(cm33_get_oled_buffer(scr_id), &oleds[scr_id]) == 0 &&
            cm33_set_oled_scr(scr_id, oleds[scr_id].file != nullptr) == 0 &&
            oleds[scr_id].file != nullptr)
        {
            one_enabled = true;
        }
    }

    return cm33_set_oled(one_enabled);
}
