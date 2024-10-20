#include <stm32h7rsxx.h>

#include "screen.h"
#include "pins.h"
#include "clocks.h"
#include "process.h"
#include "scheduler.h"

#include "osmutex.h"
#include "gk_conf.h"

SRAM4_DATA static Spinlock s_scrbuf;
SRAM4_DATA static void *scr_bufs[2] = { 0, 0 };
SRAM4_DATA static int scr_cbuf = 0;

[[maybe_unused]] static screen_hardware_scale _sc_h = x1, _sc_v = x1;

SRAM4_DATA static Spinlock s_scrbuf_overlay;
SRAM4_DATA static void *scr_bufs_overlay[2] = { 0, 0 };
SRAM4_DATA static int scr_cbuf_overlay = 0;

SRAM4_DATA static int scr_brightness = 0;

SRAM4_DATA Condition scr_vsync;

SRAM4_DATA volatile bool screen_flip_in_progress = false;

static const constexpr pin LTDC_CLK_SELECT { GPIOC, 7 };

static unsigned int screen_get_pitch(unsigned int pf)
{
    unsigned int ret;

    switch(pf)
    {
        case 0:
            ret = 2560;
            break;
        case 1:
            ret = 1920;
            break;
        case 2:
        case 3:
        case 4:
        case 7:
            ret = 1280;
            break;
        case 5:
        case 6:
            ret = 640;
            break;
        default:
            ret = 0;        
    }

    if(_sc_h != x1)
        ret /= 2;
    
    return ret;
}

void *screen_get_frame_buffer(bool back_buf)
{
    CriticalGuard cg(s_scrbuf);
    if(back_buf)
    {
        return scr_bufs[scr_cbuf & 0x1];
    }
    else
    {
        return scr_bufs[(scr_cbuf + 1) & 0x1];
    }
}

void *screen_get_overlay_frame_buffer(bool back_buf)
{
    CriticalGuard cg(s_scrbuf_overlay);
    if(back_buf)
    {
        return scr_bufs_overlay[scr_cbuf_overlay & 0x1];
    }
    else
    {
        return scr_bufs_overlay[(scr_cbuf_overlay + 1) & 0x1];
    }
}

void *screen_flip_overlay(void **old_buf, bool visible, int alpha)
{
    CriticalGuard cg(s_scrbuf_overlay);
    scr_cbuf_overlay++;
    int wbuf = scr_cbuf_overlay & 0x1;
    int rbuf = wbuf ? 0 : 1;
    LTDC_Layer2->CFBAR = (uint32_t)(uintptr_t)scr_bufs_overlay[rbuf];
    if(visible)
    {
        LTDC_Layer2->CR |= LTDC_LxCR_LEN;
    }
    else
    {
        LTDC_Layer2->CR &= ~LTDC_LxCR_LEN;
    }
    if(alpha >= 0 && alpha < 256)
    {
        LTDC_Layer2->CACR = (unsigned int)alpha;
    }
    screen_flip_in_progress = true;
    __DMB();
    LTDC->SRCR = LTDC_SRCR_VBR;
    if(old_buf)
    {
        *old_buf = scr_bufs_overlay[rbuf];
    }
    return scr_bufs_overlay[wbuf];
}

void *screen_flip(void **old_buf)
{
    CriticalGuard cg(s_scrbuf);
    scr_cbuf++;
    int wbuf = scr_cbuf & 0x1;
    int rbuf = wbuf ? 0 : 1;
    auto scr_pf = focus_process->screen_pf;
    if(_sc_v == x1)
    {
        // direct from screen buffer
        LTDC_Layer1->CFBAR = (uint32_t)(uintptr_t)scr_bufs[rbuf];
    }
    else
    {
        // via GFXMMU
        LTDC_Layer1->CFBAR = (rbuf == 0) ? GFXMMU_VIRTUAL_BUFFER0_BASE :
            GFXMMU_VIRTUAL_BUFFER1_BASE;
    }
    LTDC_Layer1->PFCR = scr_pf;
    auto line_length = screen_get_pitch(scr_pf);
    unsigned int pitch;
    if(_sc_v == x1)
        pitch = line_length;
    else
        pitch = 4096;       // GFXMMU pitch
    LTDC_Layer1->CFBLR = (pitch << LTDC_LxCFBLR_CFBP_Pos) |
        ((line_length + 7) << LTDC_LxCFBLR_CFBLL_Pos);
    if(scr_bufs[rbuf])
    {
        LTDC_Layer1->CR |= LTDC_LxCR_LEN;
    }
    else
    {
        LTDC_Layer1->CR &= ~LTDC_LxCR_LEN;
    }
    screen_flip_in_progress = true;
    __DMB();
    LTDC->SRCR = LTDC_SRCR_VBR;
    if(old_buf)
    {
        *old_buf = scr_bufs[rbuf];
    }
    return scr_bufs[wbuf];
}

void screen_set_frame_buffer(void *b0, void *b1)
{
    CriticalGuard cg(s_scrbuf);
    if(b0)
        scr_bufs[0] = b0;
    if(b1)
        scr_bufs[1] = b1;
}

void screen_set_overlay_frame_buffer(void *b0, void *b1)
{
    CriticalGuard cg(s_scrbuf_overlay);
    if(b0)
        scr_bufs_overlay[0] = b0;
    if(b1)
        scr_bufs_overlay[1] = b1;
}

void screen_set_overlay_alpha(unsigned int alpha)
{
    LTDC_Layer2->CACR = alpha;
    if(alpha == 0)
    {
        LTDC_Layer2->CR &= ~LTDC_LxCR_LEN;
    }
    else
    {
        LTDC_Layer2->CR |= LTDC_LxCR_LEN;
    }
    LTDC->SRCR = LTDC_SRCR_VBR;
}

static constexpr pin lcd_pins[] = {
    /* LTDC pins */
    { GPIOA, 0, 13 },       // G3
    { GPIOA, 1, 13 },       // G2
    { GPIOA, 2, 13 },       // B7
    { GPIOA, 3, 13 },       // DE
    { GPIOA, 7, 10 },       // R4
    { GPIOA, 8, 13 },       // B6
    { GPIOA, 9, 14 },       // B5
    { GPIOA, 10, 14 },      // B4
    { GPIOA, 11, 13 },      // B3
    { GPIOA, 12, 13 },      // B2
    { GPIOA, 15, 13 },      // R5
    { GPIOB, 4, 13 },       // R3
    { GPIOB, 5, 10 },       // R2
    { GPIOB, 10, 13 },      // G7
    { GPIOB, 11, 13 },      // G6
    { GPIOB, 12, 13 },      // G5
    { GPIOB, 13, 10 },      // G4
    { GPIOE, 11, 11 },      // VSYNC
    { GPIOF, 7, 13 },       // G0
    { GPIOF, 8, 13 },       // G1
    { GPIOF, 9, 13 },       // R0
    { GPIOF, 10, 14 },      // R1
    { GPIOF, 11, 14 },      // B0
    { GPIOG, 0, 13 },       // R7
    { GPIOG, 1, 13 },       // R6
    { GPIOG, 2, 13 },       // HSYNC
    { GPIOG, 13, 13 },      // CLK      // TODO : these are either AF13 or AF14 - no datasheet value
    { GPIOG, 14, 13 },      // B1       // TODO : these are either AF13 or AF14 - no datasheet value

    /* SPI5 to screen */
    { GPIOF, 12, 5 },
    { GPIOF, 13, 5 },
    { GPIOF, 14, 5 },
    { GPIOF, 15, 5 },
};
static constexpr auto n_lcd_pins = sizeof(lcd_pins) / sizeof(pin);

static constexpr pin lcd_reset { GPIOB, 6 };
static constexpr pin lcd_clk_select { GPIOC, 7 };

void SPI_WriteComm(unsigned char i)
{
    uint16_t id = (uint16_t)i;
    while(!(SPI5->SR & SPI_SR_TXP));
    *(volatile uint16_t *)&SPI5->TXDR = id;
}

void SPI_WriteData(unsigned char i)
{
    uint16_t id = (uint16_t)i | 0x100;
    while(!(SPI5->SR & SPI_SR_TXP));
    *(volatile uint16_t *)&SPI5->TXDR = id;
}

static void screen_set_timings()
{
    /* Timings from linux for 60 Hz/24 MHz pixel clock:
        Hsync = 16, Vsync = 2
        H back porch = 48, V back porch = 13
        Display = 640x480
        H front porch = 16, V front porch = 5
        Total = 720*500 => gives 66.67 Hz refresh

        We make slightly bigger h front porch, so 800*500 gives 60 Hz refresh
        
        Values accumulate in the register settings (and always -1 at end) */
    
    unsigned int pll3r_divisor;

    if(_sc_h == x1)
    {
        LTDC->SSCR = (1UL << LTDC_SSCR_VSH_Pos) |
            (15UL << LTDC_SSCR_HSW_Pos);
        LTDC->BPCR = (14UL << LTDC_BPCR_AVBP_Pos) |
            (63UL << LTDC_BPCR_AHBP_Pos);
        LTDC->AWCR = (494UL << LTDC_AWCR_AAH_Pos) |
            (703UL << LTDC_AWCR_AAW_Pos);
        LTDC->TWCR = (499UL << LTDC_TWCR_TOTALH_Pos) |
            (799UL << LTDC_TWCR_TOTALW_Pos);
        pll3r_divisor = 19U;
    }
    else
    {
        // only support x2 in hardware - halve all horiz values

        // 16, 64, 704, 800 -> 8, 32, 352, 400
        LTDC->SSCR = (1UL << LTDC_SSCR_VSH_Pos) |
            (7UL << LTDC_SSCR_HSW_Pos);
        LTDC->BPCR = (14UL << LTDC_BPCR_AVBP_Pos) |
            (31UL << LTDC_BPCR_AHBP_Pos);
        LTDC->AWCR = (494UL << LTDC_AWCR_AAH_Pos) |
            (351UL << LTDC_AWCR_AAW_Pos);
        LTDC->TWCR = (499UL << LTDC_TWCR_TOTALH_Pos) |
            (399UL << LTDC_TWCR_TOTALW_Pos);
        
        // halve output clock (actual clock is doubled outside stm32)
        pll3r_divisor = 39U;
    }

    // update PLL3 if required
    if(((RCC->PLL3DIVR1 & RCC_PLL3DIVR1_DIVR_Msk) >> RCC_PLL3DIVR1_DIVR_Pos) != pll3r_divisor)
    {
        //RCC->CR &= ~RCC_CR_PLL3ON;
        //while(RCC->CR & RCC_CR_PLL3RDY);
        RCC->PLLCFGR &= ~RCC_PLLCFGR_PLL3REN;
        RCC->PLL3DIVR1 = (RCC->PLL3DIVR1 & ~RCC_PLL1DIVR1_DIVR_Msk) |
            (pll3r_divisor << RCC_PLL3DIVR1_DIVR_Pos);
        RCC->PLLCFGR |= RCC_PLLCFGR_PLL3REN;
        //RCC->CR |= RCC_CR_PLL3ON;
        //while(!(RCC->CR & RCC_CR_PLL3RDY));
    }

    LTDC_Layer1->WHPCR =
        ((((LTDC->BPCR & LTDC_BPCR_AHBP_Msk) >> LTDC_BPCR_AHBP_Pos) + 1UL) << LTDC_LxWHPCR_WHSTPOS_Pos) |
        ((((LTDC->AWCR & LTDC_AWCR_AAW_Msk) >> LTDC_AWCR_AAW_Pos)) << LTDC_LxWHPCR_WHSPPOS_Pos);
    
    LTDC->SRCR = LTDC_SRCR_VBR;
}

void init_screen()
{
    for(unsigned int i = 0; i < n_lcd_pins; i++)
    {
        lcd_pins[i].set_as_af();
    }
    lcd_reset.set_as_output();
    LTDC_CLK_SELECT.clear();
    LTDC_CLK_SELECT.set_as_output();

    // Initial set-up is through SPI5, kernel clock 240 MHz off PLL3Q
    RCC->APB2ENR |= RCC_APB2ENR_SPI5EN;
    (void)RCC->APB2ENR;

    SPI5->CR1 = 0;
    SPI5->CFG1 = (8UL << SPI_CFG1_DSIZE_Pos) |  // 9-bit frame - 1st is C/D
        (7UL << SPI_CFG1_MBR_Pos);              // /256 = 937.5 kHz
    SPI5->CFG2 = (3UL << SPI_CFG2_MSSI_Pos) |   // 3 cycles between SS low and 1st data
        (3UL << SPI_CFG2_MIDI_Pos) |            // 3 cycles between each data frame
        SPI_CFG2_MASTER |
        SPI_CFG2_SSOE |
        SPI_CFG2_SSOM;                          // pulse SS between each byte
    SPI5->CR2 = 0;                              // endless mode
    SPI5->CR1 = SPI_CR1_SPE;
    SPI5->CR1 = SPI_CR1_SPE | SPI_CR1_CSTART;

    /* Pulse hardware reset */
    lcd_reset.set();
    delay_ms(100);
    lcd_reset.clear();
    delay_ms(100);
    lcd_reset.set();
    delay_ms(100);

    SPI_WriteComm(0xFF);SPI_WriteData(0x30);
    SPI_WriteComm(0xFF);SPI_WriteData(0x52);
    SPI_WriteComm(0xFF);SPI_WriteData(0x01);  // Page 01
    SPI_WriteComm(0xE3);SPI_WriteData(0x00);  
    SPI_WriteComm(0x40);SPI_WriteData(0x00);
    SPI_WriteComm(0x03);SPI_WriteData(0x40);
    SPI_WriteComm(0x04);SPI_WriteData(0x00);
    SPI_WriteComm(0x05);SPI_WriteData(0x03);
    SPI_WriteComm(0x08);SPI_WriteData(0x00);
    SPI_WriteComm(0x09);SPI_WriteData(0x07);
    SPI_WriteComm(0x0A);SPI_WriteData(0x01);
    SPI_WriteComm(0x0B);SPI_WriteData(0x32);
    SPI_WriteComm(0x0C);SPI_WriteData(0x32);
    SPI_WriteComm(0x0D);SPI_WriteData(0x0B);
    SPI_WriteComm(0x0E);SPI_WriteData(0x00);
    SPI_WriteComm(0x23);SPI_WriteData(0xA2);

    SPI_WriteComm(0x24);SPI_WriteData(0x0c);
    SPI_WriteComm(0x25);SPI_WriteData(0x06);
    SPI_WriteComm(0x26);SPI_WriteData(0x14);
    SPI_WriteComm(0x27);SPI_WriteData(0x14);

    SPI_WriteComm(0x38);SPI_WriteData(0x9C); // vcom_adj
    SPI_WriteComm(0x39);SPI_WriteData(0xA7); // vcom_adj
    SPI_WriteComm(0x3A);SPI_WriteData(0x3a); // vcom_adj

    SPI_WriteComm(0x28);SPI_WriteData(0x40);
    SPI_WriteComm(0x29);SPI_WriteData(0x01);
    SPI_WriteComm(0x2A);SPI_WriteData(0xdf);
    SPI_WriteComm(0x49);SPI_WriteData(0x3C);   
    SPI_WriteComm(0x91);SPI_WriteData(0x57); // pump_ctrl
    SPI_WriteComm(0x92);SPI_WriteData(0x57); // pump_ctrl
    SPI_WriteComm(0xA0);SPI_WriteData(0x55); 
    SPI_WriteComm(0xA1);SPI_WriteData(0x50);
    SPI_WriteComm(0xA4);SPI_WriteData(0x9C);
    SPI_WriteComm(0xA7);SPI_WriteData(0x02);  
    SPI_WriteComm(0xA8);SPI_WriteData(0x01);  
    SPI_WriteComm(0xA9);SPI_WriteData(0x01);  
    SPI_WriteComm(0xAA);SPI_WriteData(0xFC);  
    SPI_WriteComm(0xAB);SPI_WriteData(0x28);  
    SPI_WriteComm(0xAC);SPI_WriteData(0x06);  
    SPI_WriteComm(0xAD);SPI_WriteData(0x06);  
    SPI_WriteComm(0xAE);SPI_WriteData(0x06);  
    SPI_WriteComm(0xAF);SPI_WriteData(0x03);  
    SPI_WriteComm(0xB0);SPI_WriteData(0x08);  
    SPI_WriteComm(0xB1);SPI_WriteData(0x26);  
    SPI_WriteComm(0xB2);SPI_WriteData(0x28);  
    SPI_WriteComm(0xB3);SPI_WriteData(0x28);  
    SPI_WriteComm(0xB4);SPI_WriteData(0x33);  
    SPI_WriteComm(0xB5);SPI_WriteData(0x08);  
    SPI_WriteComm(0xB6);SPI_WriteData(0x26);  
    SPI_WriteComm(0xB7);SPI_WriteData(0x08);  
    SPI_WriteComm(0xB8);SPI_WriteData(0x26); 
    SPI_WriteComm(0xF0);SPI_WriteData(0x00); 
    SPI_WriteComm(0xF6);SPI_WriteData(0xC0);


    SPI_WriteComm(0xFF);SPI_WriteData(0x30);
    SPI_WriteComm(0xFF);SPI_WriteData(0x52);
    SPI_WriteComm(0xFF);SPI_WriteData(0x02); // page 02
    SPI_WriteComm(0xB0);SPI_WriteData(0x0B);
    SPI_WriteComm(0xB1);SPI_WriteData(0x16);
    SPI_WriteComm(0xB2);SPI_WriteData(0x17); 
    SPI_WriteComm(0xB3);SPI_WriteData(0x2C); 
    SPI_WriteComm(0xB4);SPI_WriteData(0x32);  
    SPI_WriteComm(0xB5);SPI_WriteData(0x3B);  
    SPI_WriteComm(0xB6);SPI_WriteData(0x29); 
    SPI_WriteComm(0xB7);SPI_WriteData(0x40);   
    SPI_WriteComm(0xB8);SPI_WriteData(0x0d);
    SPI_WriteComm(0xB9);SPI_WriteData(0x05);
    SPI_WriteComm(0xBA);SPI_WriteData(0x12);
    SPI_WriteComm(0xBB);SPI_WriteData(0x10);
    SPI_WriteComm(0xBC);SPI_WriteData(0x12);
    SPI_WriteComm(0xBD);SPI_WriteData(0x15);
    SPI_WriteComm(0xBE);SPI_WriteData(0x19);              
    SPI_WriteComm(0xBF);SPI_WriteData(0x0E);
    SPI_WriteComm(0xC0);SPI_WriteData(0x16);  
    SPI_WriteComm(0xC1);SPI_WriteData(0x0A);
    SPI_WriteComm(0xD0);SPI_WriteData(0x0C);
    SPI_WriteComm(0xD1);SPI_WriteData(0x17);
    SPI_WriteComm(0xD2);SPI_WriteData(0x14);
    SPI_WriteComm(0xD3);SPI_WriteData(0x2E);   
    SPI_WriteComm(0xD4);SPI_WriteData(0x32);   
    SPI_WriteComm(0xD5);SPI_WriteData(0x3C);  
    SPI_WriteComm(0xD6);SPI_WriteData(0x22);
    SPI_WriteComm(0xD7);SPI_WriteData(0x3D);
    SPI_WriteComm(0xD8);SPI_WriteData(0x0D);
    SPI_WriteComm(0xD9);SPI_WriteData(0x07);
    SPI_WriteComm(0xDA);SPI_WriteData(0x13);
    SPI_WriteComm(0xDB);SPI_WriteData(0x13);
    SPI_WriteComm(0xDC);SPI_WriteData(0x11);
    SPI_WriteComm(0xDD);SPI_WriteData(0x15);
    SPI_WriteComm(0xDE);SPI_WriteData(0x19);                   
    SPI_WriteComm(0xDF);SPI_WriteData(0x10);
    SPI_WriteComm(0xE0);SPI_WriteData(0x17);    
    SPI_WriteComm(0xE1);SPI_WriteData(0x0A);

    SPI_WriteComm(0xFF);SPI_WriteData(0x30);
    SPI_WriteComm(0xFF);SPI_WriteData(0x52);
    SPI_WriteComm(0xFF);SPI_WriteData(0x03);   // page 03
    SPI_WriteComm(0x00);SPI_WriteData(0x2A);
    SPI_WriteComm(0x01);SPI_WriteData(0x2A);
    SPI_WriteComm(0x02);SPI_WriteData(0x2A);
    SPI_WriteComm(0x03);SPI_WriteData(0x2A);
    SPI_WriteComm(0x04);SPI_WriteData(0x61);  
    SPI_WriteComm(0x05);SPI_WriteData(0x80);   
    SPI_WriteComm(0x06);SPI_WriteData(0xc7);   
    SPI_WriteComm(0x07);SPI_WriteData(0x01);  
    SPI_WriteComm(0x08);SPI_WriteData(0x03); 
    SPI_WriteComm(0x09);SPI_WriteData(0x04);
    SPI_WriteComm(0x70);SPI_WriteData(0x22);
    SPI_WriteComm(0x71);SPI_WriteData(0x80);
    SPI_WriteComm(0x30);SPI_WriteData(0x2A);
    SPI_WriteComm(0x31);SPI_WriteData(0x2A);
    SPI_WriteComm(0x32);SPI_WriteData(0x2A);
    SPI_WriteComm(0x33);SPI_WriteData(0x2A);
    SPI_WriteComm(0x34);SPI_WriteData(0x61);
    SPI_WriteComm(0x35);SPI_WriteData(0xc5);
    SPI_WriteComm(0x36);SPI_WriteData(0x80);
    SPI_WriteComm(0x37);SPI_WriteData(0x23);
    SPI_WriteComm(0x40);SPI_WriteData(0x03); 
    SPI_WriteComm(0x41);SPI_WriteData(0x04); 
    SPI_WriteComm(0x42);SPI_WriteData(0x05); 
    SPI_WriteComm(0x43);SPI_WriteData(0x06); 
    SPI_WriteComm(0x44);SPI_WriteData(0x11); 
    SPI_WriteComm(0x45);SPI_WriteData(0xe8); 
    SPI_WriteComm(0x46);SPI_WriteData(0xe9); 
    SPI_WriteComm(0x47);SPI_WriteData(0x11);
    SPI_WriteComm(0x48);SPI_WriteData(0xea); 
    SPI_WriteComm(0x49);SPI_WriteData(0xeb);
    SPI_WriteComm(0x50);SPI_WriteData(0x07); 
    SPI_WriteComm(0x51);SPI_WriteData(0x08); 
    SPI_WriteComm(0x52);SPI_WriteData(0x09); 
    SPI_WriteComm(0x53);SPI_WriteData(0x0a); 
    SPI_WriteComm(0x54);SPI_WriteData(0x11); 
    SPI_WriteComm(0x55);SPI_WriteData(0xec); 
    SPI_WriteComm(0x56);SPI_WriteData(0xed); 
    SPI_WriteComm(0x57);SPI_WriteData(0x11); 
    SPI_WriteComm(0x58);SPI_WriteData(0xef); 
    SPI_WriteComm(0x59);SPI_WriteData(0xf0); 
    SPI_WriteComm(0xB1);SPI_WriteData(0x01); 
    SPI_WriteComm(0xB4);SPI_WriteData(0x15); 
    SPI_WriteComm(0xB5);SPI_WriteData(0x16); 
    SPI_WriteComm(0xB6);SPI_WriteData(0x09); 
    SPI_WriteComm(0xB7);SPI_WriteData(0x0f); 
    SPI_WriteComm(0xB8);SPI_WriteData(0x0d); 
    SPI_WriteComm(0xB9);SPI_WriteData(0x0b); 
    SPI_WriteComm(0xBA);SPI_WriteData(0x00); 
    SPI_WriteComm(0xC7);SPI_WriteData(0x02); 
    SPI_WriteComm(0xCA);SPI_WriteData(0x17); 
    SPI_WriteComm(0xCB);SPI_WriteData(0x18); 
    SPI_WriteComm(0xCC);SPI_WriteData(0x0a); 
    SPI_WriteComm(0xCD);SPI_WriteData(0x10); 
    SPI_WriteComm(0xCE);SPI_WriteData(0x0e); 
    SPI_WriteComm(0xCF);SPI_WriteData(0x0c); 
    SPI_WriteComm(0xD0);SPI_WriteData(0x00); 
    SPI_WriteComm(0x81);SPI_WriteData(0x00); 
    SPI_WriteComm(0x84);SPI_WriteData(0x15); 
    SPI_WriteComm(0x85);SPI_WriteData(0x16); 
    SPI_WriteComm(0x86);SPI_WriteData(0x10); 
    SPI_WriteComm(0x87);SPI_WriteData(0x0a); 
    SPI_WriteComm(0x88);SPI_WriteData(0x0c); 
    SPI_WriteComm(0x89);SPI_WriteData(0x0e);
    SPI_WriteComm(0x8A);SPI_WriteData(0x02); 
    SPI_WriteComm(0x97);SPI_WriteData(0x00); 
    SPI_WriteComm(0x9A);SPI_WriteData(0x17); 
    SPI_WriteComm(0x9B);SPI_WriteData(0x18);
    SPI_WriteComm(0x9C);SPI_WriteData(0x0f);
    SPI_WriteComm(0x9D);SPI_WriteData(0x09); 
    SPI_WriteComm(0x9E);SPI_WriteData(0x0b); 
    SPI_WriteComm(0x9F);SPI_WriteData(0x0d); 
    SPI_WriteComm(0xA0);SPI_WriteData(0x01); 

    SPI_WriteComm(0xFF);SPI_WriteData(0x30);
    SPI_WriteComm(0xFF);SPI_WriteData(0x52);
    SPI_WriteComm(0xFF);SPI_WriteData(0x02);  // page 02
    SPI_WriteComm(0x01);SPI_WriteData(0x01);
    SPI_WriteComm(0x02);SPI_WriteData(0xDA);
    SPI_WriteComm(0x03);SPI_WriteData(0xBA);
    SPI_WriteComm(0x04);SPI_WriteData(0xA8);
    SPI_WriteComm(0x05);SPI_WriteData(0x9A);
    SPI_WriteComm(0x06);SPI_WriteData(0x70);
    SPI_WriteComm(0x07);SPI_WriteData(0xFF);
    SPI_WriteComm(0x08);SPI_WriteData(0x91);
    SPI_WriteComm(0x09);SPI_WriteData(0x90);
    SPI_WriteComm(0x0A);SPI_WriteData(0xFF);
    SPI_WriteComm(0x0B);SPI_WriteData(0x8F);
    SPI_WriteComm(0x0C);SPI_WriteData(0x60);
    SPI_WriteComm(0x0D);SPI_WriteData(0x58);
    SPI_WriteComm(0x0E);SPI_WriteData(0x48);
    SPI_WriteComm(0x0F);SPI_WriteData(0x38);
    SPI_WriteComm(0x10);SPI_WriteData(0x2B);

    SPI_WriteComm(0xFF);SPI_WriteData(0x30);    // page 00
    SPI_WriteComm(0xFF);SPI_WriteData(0x52);
    SPI_WriteComm(0xFF);SPI_WriteData(0x00);   
    SPI_WriteComm(0x36);SPI_WriteData(0x0A);    // MADCTL - BGR, panel flip horizontal, no flip vertical
                                        
    SPI_WriteComm(0x11);SPI_WriteData(0x00);	 //sleep out
    delay_ms( 200 );

    SPI_WriteComm(0x29);SPI_WriteData(0x00);	  //display on
    delay_ms(10);  

    // Set up LTDC
    RCC->APB5ENR |= RCC_APB5ENR_LTDCEN;
    (void)RCC->APB5ENR;

    screen_set_timings();
    
    /* Set all control signals active low */
    LTDC->GCR = 0UL | LTDC_GCR_PCPOL;

    /* Back colour (without any layer enabled) */
    LTDC->BCCR = 0xff00ffUL;

    /* Layers */
    LTDC_Layer1->WHPCR =
        ((((LTDC->BPCR & LTDC_BPCR_AHBP_Msk) >> LTDC_BPCR_AHBP_Pos) + 1UL) << LTDC_LxWHPCR_WHSTPOS_Pos) |
        ((((LTDC->AWCR & LTDC_AWCR_AAW_Msk) >> LTDC_AWCR_AAW_Pos)) << LTDC_LxWHPCR_WHSPPOS_Pos);
    LTDC_Layer1->WVPCR =
        ((((LTDC->BPCR & LTDC_BPCR_AVBP_Msk) >> LTDC_BPCR_AVBP_Pos) + 1UL) << LTDC_LxWVPCR_WVSTPOS_Pos) |
        ((((LTDC->AWCR & LTDC_AWCR_AAH_Msk) >> LTDC_AWCR_AAH_Pos)) << LTDC_LxWVPCR_WVSPPOS_Pos);
    LTDC_Layer1->PFCR = 0UL;        // ARGB8888
    LTDC_Layer1->CACR = 0xffUL;
    LTDC_Layer1->DCCR = 0UL;
    LTDC_Layer1->BFCR = (4UL << LTDC_LxBFCR_BF1_Pos) |
        (5UL << LTDC_LxBFCR_BF2_Pos);       // Use constant alpha for now
    LTDC_Layer1->CFBAR = 0;
    LTDC_Layer1->CFBLR = (2560UL << LTDC_LxCFBLR_CFBP_Pos) |
        (2567UL << LTDC_LxCFBLR_CFBLL_Pos);
    LTDC_Layer1->CFBLNR = 480UL;
    LTDC_Layer1->CR = 0;

    LTDC_Layer2->WHPCR =
        ((((LTDC->BPCR & LTDC_BPCR_AHBP_Msk) >> LTDC_BPCR_AHBP_Pos) + 1UL) << LTDC_LxWHPCR_WHSTPOS_Pos) |
        ((((LTDC->AWCR & LTDC_AWCR_AAW_Msk) >> LTDC_AWCR_AAW_Pos)) << LTDC_LxWHPCR_WHSPPOS_Pos);
    LTDC_Layer2->WVPCR =
        ((((LTDC->BPCR & LTDC_BPCR_AVBP_Msk) >> LTDC_BPCR_AVBP_Pos) + 1UL) << LTDC_LxWVPCR_WVSTPOS_Pos) |
        ((((LTDC->AWCR & LTDC_AWCR_AAH_Msk) >> LTDC_AWCR_AAH_Pos)) << LTDC_LxWVPCR_WVSPPOS_Pos);
    LTDC_Layer2->DCCR = 0UL;
    LTDC_Layer2->CACR = 0xffUL;
    LTDC_Layer2->BFCR = (6UL << LTDC_LxBFCR_BF1_Pos) |
        (7UL << LTDC_LxBFCR_BF2_Pos);
    LTDC_Layer2->PFCR = 6U; // AL44
    LTDC_Layer2->CFBLR = (640UL << LTDC_LxCFBLR_CFBP_Pos) |
        (647UL << LTDC_LxCFBLR_CFBLL_Pos);
    LTDC_Layer2->CFBLNR = 480UL;
    
    // Load lookup table for AL44 CGA text mode colors https://en.wikipedia.org/wiki/Color_Graphics_Adapter
    constexpr uint32_t palette[] = {
        0x000000, 0x0000aa, 0x00aa00, 0x00aaaa,
        0xaa0000, 0xaa00aa, 0xaa5500, 0xaaaaaa,
        0x555555, 0x5555ff, 0x55ff55, 0x55ffff,
        0xff5555, 0xff55ff, 0xffff55, 0xffffff };
    for(unsigned int i = 0; i < sizeof(palette) / sizeof(uint32_t); i++)
    {
        // for al44 replicate clut idx to top two nibbles (rm p. 1242)
        LTDC_Layer2->CLUTWR = (i << 24) | (i << 28) | palette[i];
    }
    LTDC_Layer2->CR = LTDC_LxCR_CLUTEN;
    
    // frame buffers
    memset((void *)0x90000000, 0, 0x400000);
    screen_set_frame_buffer((void *)0x90000000, (void *)0x90200000);
    screen_set_overlay_frame_buffer((void *)0x90180000, (void *)0x90380000);

    /* Enable */
    LTDC->LIPCR = 499;
    LTDC->IER = LTDC_IER_LIE | LTDC_IER_RRIE;
    NVIC_EnableIRQ(LTDC_IRQn);
    LTDC->SRCR = LTDC_SRCR_IMR;
    LTDC->GCR |= LTDC_GCR_LTDCEN;

    // switch on backlight - TIM2 CH4 GPIOA11
    constexpr const pin LED_BACKLIGHT { GPIOA, 5, 1 };
    LED_BACKLIGHT.set_as_af();

    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
    (void)RCC->APB1ENR1;
    /* Timer clocks on APB1 run between 200 and 300 MHz depending on M4 CPU speed
        We want something in the 5-20 kHz range
        For ARR = 4096, we need a prescaler of 6 to give 8.1 kHz to 12.2 kHz */
    TIM2->CCMR1 = (0UL << TIM_CCMR1_CC1S_Pos) |
        TIM_CCMR1_OC1PE |
        (6UL << TIM_CCMR1_OC1M_Pos) |
        (0UL << TIM_CCMR1_CC2S_Pos) |
        TIM_CCMR1_OC2PE |
        (6UL << TIM_CCMR1_OC2M_Pos);
    TIM2->CCMR2 = (0UL << TIM_CCMR2_CC3S_Pos) |
        TIM_CCMR2_OC3PE |
        (6UL << TIM_CCMR2_OC3M_Pos) |
        (0UL << TIM_CCMR2_CC4S_Pos) |
        TIM_CCMR2_OC4PE |
        (6UL << TIM_CCMR2_OC4M_Pos);
    TIM2->CCER = TIM_CCER_CC1E;
    TIM2->PSC = 1UL;    // (ck = ck/(1 + PSC))
    TIM2->ARR = 4096;
    TIM2->CCR1 = 0UL;
    TIM2->CCR2 = 0UL;
    TIM2->CCR3 = 0UL;
    TIM2->CCR4 = 0UL;
    TIM2->BDTR = TIM_BDTR_MOE;
    TIM2->CR1 = TIM_CR1_CEN;

    screen_set_brightness(100);
}

//__attribute__((section(".sram4"))) static bool led_set = false;

extern "C" void LTDC_IRQHandler()
{

    if(LTDC->ISR & LTDC_ISR_LIF)
    {
        scr_vsync.Signal();
    }

    if(LTDC->ISR & LTDC_ISR_RRIF)
    {
        screen_flip_in_progress = false;
    }

    LTDC->ICR = 0xf;
}

static unsigned int pct_to_arr(int pct, unsigned int arr)
{
    // linear for now
    pct = (pct * 90) / 100 + 10;
    return pct * (arr - 1) / 100;
}

void screen_set_brightness(int pct)
{
    if(pct < 0) pct = 0;
    if(pct > 100) pct = 100;
    
    auto arr = pct_to_arr(pct, TIM2->ARR);
    TIM2->CCR1 = arr;
    scr_brightness = pct;
}

int screen_get_brightness()
{
    return scr_brightness;
}

int screen_set_hardware_scale(screen_hardware_scale scale_horiz,
    screen_hardware_scale scale_vert)
{
    _sc_h = scale_horiz;
    _sc_v = scale_vert;

    // vertical scrolling is done by GFXMMU
    if(scale_vert == x2 || scale_vert == x4)
    {
        RCC->AHB5ENR |= RCC_AHB5ENR_GFXMMUEN;
        (void)RCC->AHB5ENR;

        GFXMMU->B0CR = (uint32_t)(uintptr_t)scr_bufs[0];
        GFXMMU->B1CR = (uint32_t)(uintptr_t)scr_bufs[1];

        // All virtual buffers share the same look-up table, dependent upon pixel format
        // blocks are 12 or 16 bytes each - all our pitches, including 24bpp/1920 bytes
        //  are multiples of 16
        unsigned int y_virt = 0;
        int y_phys = 0;
        unsigned int y_virt_incr = scale_vert == x2 ? 2 : 4;

        unsigned int pitch = screen_get_pitch(focus_process->screen_pf);
        unsigned int nblocks = pitch / 16;
        int blocks_used = 0;

        for(; y_virt < 480; y_virt += y_virt_incr)
        {
            for(unsigned int y = 0; y < y_virt_incr; y++)
            {
                auto lutl = (volatile uint32_t *)(GFXMMU_BASE + 0x1000 + 8 * (y_virt + y));
                auto luth = (volatile uint32_t *)(GFXMMU_BASE + 0x1004 + 8 * (y_virt + y));

                *lutl = GFXMMU_LUTxL_EN |
                    (0U << GFXMMU_LUTxL_FVB_Pos) |
                    ((nblocks - 1) << GFXMMU_LUTxL_LVB_Pos);

                // number of blocks used - address of first block
                //*luth = ((uint32_t)(blocks_used - y_phys)) & 0x3ffff;
                *luth = (uint32_t)blocks_used;

            }
            blocks_used += nblocks;

            y_phys += (int)nblocks;
        }

        GFXMMU->CR = GFXMMU_CR_ATE;
    }

    if(scale_horiz == x1)
    {
        LTDC_CLK_SELECT.clear();
    }
    else
    {
        LTDC_CLK_SELECT.set();
    }
    screen_set_timings();

    return 0;
}
