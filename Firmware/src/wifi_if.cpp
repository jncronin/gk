#include <stm32h7rsxx.h>
#include <bsp/include/nm_bsp.h>
#include <bus_wrapper/include/nm_bus_wrapper.h>
#include <driver/include/m2m_wifi.h>

#include "thread.h"
#include "scheduler.h"
#include "osmutex.h"
#include "osnet.h"
#include "pins.h"
#include "clocks.h"
#include "ossharedmem.h"
#include "wifi_if.h"

SRAM4_DATA static BinarySemaphore wifi_irq;
SRAM4_DATA static Mutex winc_mutex;

static int n_irqs = 0;

// Max transaction size on SPI bus
SRAM4_DATA tstrNmBusCapabilities egstrNmBusCapabilities =
{
	UINT16_MAX
};

void wifi_handler(uint8 eventCode, void *p_eventData);

NET_BSS WincNetInterface wifi_if;

constexpr pin WIFI_CHIP_EN { GPIOF, 5 };
constexpr pin WIFI_NRST { GPIOB, 2 };
constexpr pin WIFI_IRQ { GPIOF, 4 };

constexpr pin WIFI_SPI_PINS[] = {
    { GPIOC, 12, 6 },    // MOSI
    { GPIOC, 11, 6 },      // MISO
    { GPIOA, 4, 6 },     // NSS
    { GPIOC, 10, 6 }      // SCK
};

constexpr pin WIFI_LPTIM_OUT { GPIOD, 15, 3 };

// registered interrupt function
static void (*wifi_irq_function)() = nullptr;


void init_wifi()
{
    WIFI_CHIP_EN.set_as_output();
    WIFI_NRST.set_as_output();
    WIFI_IRQ.set_as_input(pin::PullUp);
    for(const auto &p : WIFI_SPI_PINS)
    {
        p.set_as_af();
    }
    WIFI_LPTIM_OUT.set_as_af();

    /* Set up LPTIM5 for ~32.768 kHz
        Clocked off PLL2S = 32MHz so can divide 976 and PLL half that
         - gives error of 576 ppm
    */
    RCC->APB4RSTR = RCC_APB4RSTR_LPTIM5RST;
    (void)RCC->APB4RSTR;
    RCC->APB4RSTR = 0;
    (void)RCC->APB4RSTR;
    RCC->APB4ENR |= RCC_APB4ENR_LPTIM5EN;
    (void)RCC->APB4ENR;
    LPTIM5->CFGR = 0;
    LPTIM5->CR = LPTIM_CR_ENABLE;
    LPTIM5->ARR = 975U;
    LPTIM5->CCR1 = (976U/2) -1U;
    LPTIM5->CR = LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT;

    /* WIFI is connected to SPI3 clocked at 240 MHz
        Max SPI clock is 48 MHz
        
        */
    RCC->APB1RSTR1 = RCC_APB1RSTR1_SPI3RST;
    (void)RCC->APB1RSTR1;
    RCC->APB1RSTR1 = 0;
    (void)RCC->APB1RSTR1;
    RCC->APB1ENR1 |= RCC_APB1ENR1_SPI3EN;
    (void)RCC->APB1ENR1;

    SPI3->CR1 = 0UL;
    SPI3->CFG1 = (2UL << SPI_CFG1_MBR_Pos) |
        (0UL << SPI_CFG1_FTHLV_Pos) |
        (7UL << SPI_CFG1_DSIZE_Pos);
    SPI3->CFG2 = (0x0UL << SPI_CFG2_MSSI_Pos) |   // 0 cycles between SS low and 1st data
        (0x0UL << SPI_CFG2_MIDI_Pos) |            // 0 cycles between each data frame
        SPI_CFG2_SSOE |
        SPI_CFG2_SSOM |
        SPI_CFG2_MASTER |
        SPI_CFG2_AFCNTR |
        0;
    SPI3->CR2 = 0;

    // set wifi irq (GPIOF4), edge
    RCC->APB4ENR |= RCC_APB4ENR_SBSEN;
    (void)RCC->APB4ENR;
    SBS->EXTICR[1] = (SBS->EXTICR[1] & ~SBS_EXTICR2_PC_EXTI4_Msk) |
        (5U << SBS_EXTICR2_PC_EXTI4_Pos);

    EXTI->FTSR1 |= EXTI_FTSR1_FT4;
    EXTI->RTSR1 &= ~EXTI_RTSR1_RT4;
    EXTI->IMR1 |= EXTI_IMR1_IM4;

    wifi_if.connected = WincNetInterface::WIFI_UNINIT;

    //NVIC_EnableIRQ(EXTI4_IRQn);
}

void wifi_connect()
{
    auto cur_nets = net_get_known_networks();

    for(const auto &cur_net : cur_nets)
    {
        wifi_if.connected = WincNetInterface::WIFI_CONNECTING;
        const auto &ssid = cur_net.first;
        const auto &pwd = cur_net.second;

        tuniM2MWifiAuth wauth = { 0 };
        strncpy((char *)wauth.au8PSK, pwd.c_str(), M2M_MAX_PSK_LEN - 1);
        wauth.au8PSK[M2M_MAX_PSK_LEN - 1] = 0;

        auto ret = m2m_wifi_connect(const_cast<char *>(ssid.c_str()), ssid.length(),
            M2M_WIFI_SEC_WPA_PSK, &wauth, M2M_WIFI_CH_13);

        if(ret != M2M_SUCCESS)
        {
            klog("wifi: m2m_wifi_connect_psk failed %i\n", ret);
            wifi_if.connected = WincNetInterface::WIFI_DISCONNECTED;
        }
    }
}

void nm_bsp_register_isr(tpfNmBspIsr pfIsr)
{
    wifi_irq_function = pfIsr;
}

void nm_bsp_interrupt_ctrl(uint8 ctrl)
{
    if(ctrl)
        NVIC_EnableIRQ(EXTI4_IRQn);
    else
        NVIC_DisableIRQ(EXTI4_IRQn);
}

void nm_bsp_sleep(uint32 msec)
{
    if(scheduler_running())
        Block(clock_cur() + kernel_time::from_ms(msec));
    else
        delay_ms(msec);
}

sint8 nm_bus_init(void *)
{
    // bus reset
    WIFI_CHIP_EN.clear();
    WIFI_NRST.clear();
    nm_bsp_sleep(100);
    WIFI_CHIP_EN.set();
    nm_bsp_sleep(100);
    WIFI_NRST.set();
    nm_bsp_sleep(100);

    klog("net: wifi bus init\n");

    return 0;
}

sint8 nm_bus_deinit()
{
    return 0;
}

sint8 nm_spi_rw(uint8* p_txBuf, uint8* p_rxBuf, uint16 txrxLen);
sint8 nm_bus_ioctl(uint8 u8Cmd, void* pvParameter)
{
	sint8 s8Ret = 0;
	switch(u8Cmd)
	{
		case NM_BUS_IOCTL_RW: {
			tstrNmSpiRw *pstrParam = (tstrNmSpiRw *)pvParameter;
			s8Ret = nm_spi_rw(pstrParam->pu8InBuf, pstrParam->pu8OutBuf, pstrParam->u16Sz);
		}
		break;

		default:
			s8Ret = -1;
		M2M_ERR("invalid ioclt cmd\n");
			break;
	}

	return s8Ret;
}

extern "C" void EXTI4_IRQHandler(void)
{
#ifdef DEBUG_WIFI
    itm_tstamp();
    ITM_SendChar('W');
    ITM_SendChar('I');
#endif
    //rtt_printf_wrapper("WIFI IRQ\n");
    if(wifi_irq_function)
    {
        n_irqs++;
        wifi_irq_function();
    }

    EXTI->PR1 = EXTI_PR1_PR4;
    wifi_irq.Signal();
    __DMB();
}

static void wifi_poll()
{
    m2m_wifi_handle_events(nullptr);
}

sint8 nm_spi_rw(uint8* p_txBuf, uint8* p_rxBuf, uint16 txrxLen)
{
    /* Don't support for small packets (polling probably quicker ... but not benchmarked)
        Needs buffers to be valid (not flash, not CCM RAM)

        DMA needs two channels, one for send and one for receive.
        Even if we are only receiving we still need to send data to trigger the SPI clock
        TX needs to be higher priority so it it triggered before RX
    */

    if(!p_txBuf && !p_rxBuf)
        return 0;

#if ENABLE_DMA

#ifdef DEBUG_WIFI
    itm_tstamp();
    ITM_SendChar('W');
    if(p_rxBuf)
        ITM_SendChar('t');
    if(p_txBuf)
        ITM_SendChar('T');
    char buf[32];
    snprintf(buf, 31, "%u", txrxLen);
    buf[31] = 0;
    auto b = &buf[0];
    while(*b)
        ITM_SendChar(*b++);
#endif
    if((txrxLen > 16) &&
        (!p_rxBuf || (uint32_t)(uintptr_t)p_rxBuf >= 0x20000000) &&
        (!p_txBuf || (uint32_t)(uintptr_t)p_txBuf >= 0x20000000))
    {
#ifdef DEBUG_WIFI
        // prove we are using DMA
        ITM_SendChar('\n');
        ITM_SendChar('W');
#endif
        
        // determine memory burst size - requires x4/8/16 byte
        //  transfer size and alignment
        auto pb = (uint32_t)(uintptr_t)p_rxBuf;
        auto pc = (uint32_t)(uintptr_t)p_txBuf;
        uint32_t bsize = 0;
        uint32_t fifo = 0;
        if((!p_rxBuf || ((pb & 0xf) == 0 && (txrxLen & 0xf) == 0)) &&
            (!p_txBuf || ((pc & 0xf) == 0 && (txrxLen & 0xf) == 0)))
        {
            // 16-byte - make as 4x32-bit, full fifo
            bsize = DMA_SxCR_MBURST_0 |
                DMA_SxCR_MSIZE_1;
            fifo = DMA_SxFCR_FTH_Msk |
                DMA_SxFCR_DMDIS;
#ifdef DEBUG_WIFI
            ITM_SendChar('A');
#endif
        }
        else if((!p_rxBuf || ((pb & 0x7) == 0 && (txrxLen & 0x7) == 0)) &&
            (!p_txBuf || ((pc & 0x7) == 0 && (txrxLen & 0x7) == 0)))
        {
            // 8-byte - make as 4x16-bit, half fifo
            bsize = DMA_SxCR_MBURST_0 |
                DMA_SxCR_MSIZE_0;
            fifo = DMA_SxFCR_FTH_0 |
                DMA_SxFCR_DMDIS;
#ifdef DEBUG_WIFI
            ITM_SendChar('B');
#endif
        }
        else if((!p_rxBuf || ((pb & 0x3) == 0 && (txrxLen & 0x3) == 0)) &&
            (!p_txBuf || ((pc & 0x3) == 0 && (txrxLen & 0x3) == 0)))
        {
            // 4-byte - make as 1x32-bit
            bsize = DMA_SxCR_MSIZE_1;
            fifo = DMA_SxFCR_DMDIS;
#ifdef DEBUG_WIFI
            ITM_SendChar('C');
#endif
        }
        else
        {
#ifdef DEBUG_WIFI
            ITM_SendChar('D');
#endif
        }

        uint32_t rxinc = DMA_SxCR_MINC;
        uint32_t txinc = DMA_SxCR_MINC;

        if(!p_rxBuf)
        {
            pb = (uint32_t)(uintptr_t)&dummy[0];
            rxinc = 0;
        }
        if(!p_txBuf)
        {
            pc = (uint32_t)(uintptr_t)&dummy[0];
            txinc = 0;
        }

        // wait for DMA complete
        while(DMA2_Stream0->CR & DMA_SxCR_EN);
#ifdef DEBUG_WIFI
        itm_tstamp();
        ITM_SendChar('W');
        ITM_SendChar('D');
#endif

        // Disable SPI DMA and clear FIFO
        SPI3->CFG1 &= ~SPI_CFG1_RXDMAEN;
        SPI3->CFG1 &= ~SPI_CFG1_TXDMAEN;
        while(SPI3->SR & SPI_SR_RXNE)
            (void)SPI3->DR;
        
        // Program DMA - ch4
        DMA2->LIFCR = DMA_LIFCR_CDMEIF0 |
            DMA_LIFCR_CTEIF0 |
            DMA_LIFCR_CHTIF0 |
            DMA_LIFCR_CTCIF0;
        DMA2_Stream0->CR = 0;
        DMA2_Stream0->PAR = (uint32_t)(uintptr_t)&SPI3->DR;
        DMA2_Stream0->M0AR = pb;
        DMA2_Stream0->NDTR = txrxLen;
        DMA2_Stream0->CR = DMA_SxCR_CHSEL_2 |
            rxinc |
            bsize;
        DMA2_Stream0->FCR = fifo;

        // We also need to transmit data to make the SPI clock tick
        DMA2->LIFCR = DMA_LIFCR_CDMEIF1 |
            DMA_LIFCR_CTEIF1 |
            DMA_LIFCR_CHTIF1 |
            DMA_LIFCR_CTCIF1;
        DMA2_Stream1->CR = 0;
        DMA2_Stream1->PAR = (uint32_t)(uintptr_t)&SPI3->DR;
        DMA2_Stream1->M0AR = pc;
        DMA2_Stream1->NDTR = txrxLen;
        DMA2_Stream1->CR = DMA_SxCR_CHSEL_2 |
            txinc |
            DMA_SxCR_PL_0 |
            bsize |
            DMA_SxCR_DIR_0;
        DMA2_Stream1->FCR = fifo;

        DMA2_Stream1->CR |= DMA_SxCR_EN;
        DMA2_Stream0->CR |= DMA_SxCR_EN;

        SPI3->CR2 |= SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN;

        // wait for DMA complete
        while(DMA2_Stream0->CR & DMA_SxCR_EN);


        //printf("wifi potential DMA rx %d\n", (int)rxLen);
    }
    else
    {
        // not suitable for DMA, however still need to wait
        //  for any ongoing DMA transfer to complete
        while(DMA2_Stream0->CR & DMA_SxCR_EN);
#ifdef DEBUG_WIFI
        itm_tstamp();
        ITM_SendChar('W');
        ITM_SendChar('N');
#endif

#endif
        SPI3->CFG1 &= ~SPI_CFG1_RXDMAEN & ~SPI_CFG1_TXDMAEN;
        SPI3->CR2 = txrxLen;
        SPI3->CR1 = SPI_CR1_SPE;
        SPI3->CR1 = SPI_CR1_SPE | SPI_CR1_CSTART;

        {
            SharedMemoryGuard(p_txBuf, txrxLen, true, false);
            {
                SharedMemoryGuard(p_rxBuf, txrxLen, false, true);

                for(uint16_t i = 0; i < txrxLen; i++)
                {
                    uint8_t tb, rb;
                    tb = (p_txBuf) ? p_txBuf[i] : 0;
                    while(!(SPI3->SR & SPI_SR_TXP));
                    *reinterpret_cast<volatile uint8_t *>(&SPI3->TXDR) = tb;
                    while(!(SPI3->SR & SPI_SR_RXP));
                    rb = *reinterpret_cast<volatile uint8_t *>(&SPI3->RXDR);
                    if(p_rxBuf)
                        p_rxBuf[i] = rb;
                }
            }
        }

        while(!(SPI3->SR & SPI_SR_EOT));
        SPI3->IFCR = 0xff8U;
        SPI3->CR1 = 0;

#if ENABLE_DMA
#ifdef DEBUG_WIFI
        itm_tstamp();
        ITM_SendChar('W');
        ITM_SendChar('n');
#endif
    }
#endif

    return 0;
}

static void eth_handler(uint8 msgType, void *pvMsg, void *pvCtrlBuf)
{
    switch(msgType)
    {
        case M2M_WIFI_RESP_ETHERNET_RX_PACKET:
        {
            static unsigned int pkt_offset = 0;

            auto ctrlbuf = reinterpret_cast<tstrM2MDataBufCtrl *>(pvCtrlBuf);
            if(ctrlbuf->u8IfcId != STATION_INTERFACE)
                break;
#ifdef DEBUG_WIFI
            {
                CriticalGuard cg;
                klog("wifi: rx packet %d/%d @ %x\n",
                    ctrlbuf->u16DataSize, ctrlbuf->u16RemainigDataSize,
                    pvMsg);
                for(unsigned int i = 0; i < ctrlbuf->u16DataSize; i++)
                {
                    klog("%02X ", reinterpret_cast<char *>(pvMsg)[i]);
                }
                klog("\n");
            }
#endif

            if(ctrlbuf->u16RemainigDataSize == 0)
            {
                net_inject_ethernet_packet(reinterpret_cast<char *>(pvMsg) - M2M_ETH_PAD_SIZE - 4,
                    ctrlbuf->u16DataSize + pkt_offset, &wifi_if);
                
                auto newbuf = net_allocate_pbuf(PBUF_SIZE);
                if(!newbuf)
                {
                    /* Drop priority to let net thread run, then increase again */
                    auto t = GetCurrentThreadForCore();
                    auto old_prio = t->base_priority;
                    auto new_prio = old_prio ? (old_prio - 1U) : old_prio;
                    if(new_prio)
                        s().ChangePriority(t, old_prio, new_prio);
                    while(!newbuf)
                    {
                        Yield();
                        newbuf = net_allocate_pbuf(PBUF_SIZE);
                    }
                    if(new_prio)
                        s().ChangePriority(t, new_prio, old_prio);
                }
                m2m_wifi_set_receive_buffer(newbuf, PBUF_SIZE);
                pkt_offset = 0;
            }
            else
            {
                pkt_offset += ctrlbuf->u16DataSize;
                m2m_wifi_set_receive_buffer(reinterpret_cast<char *>(pvMsg) + ctrlbuf->u16DataSize,
                    PBUF_SIZE - pkt_offset);
            }
        }
        break;

        default:
            klog("wifi: eth: %u\n", msgType);
            break;
    }
}

extern "C" void os_hook_isr()
{
    // do we need to do anything here?
}

void wifi_handler(uint8 eventCode, void *p_eventData)
{
    switch(eventCode)
    {
        case M2M_WIFI_RESP_CON_STATE_CHANGED:
            {
                auto state = reinterpret_cast<tstrM2mWifiStateChanged *>(p_eventData);

                if(state->u8IfcId == STATION_INTERFACE)
                {
                    switch(state->u8CurrState)
                    {
                        case M2M_WIFI_CONNECTED:
                            wifi_if.connected = WincNetInterface::WIFI_AWAIT_IP;
                            klog("WIFI Connected\n");

                            break;

                        case M2M_WIFI_DISCONNECTED:
                            wifi_if.connected = WincNetInterface::WIFI_DISCONNECTED;
                            //socket_wifi_disconnect();
                            klog("WIFI Disconnected %i\n", state->u8ErrCode);

                            break;
                    }
                }
            }
            break;

        case M2M_WIFI_RESP_SCAN_RESULT:
            {
                auto sr = reinterpret_cast<tstrM2mWifiscanResult *>(p_eventData);
                auto ssid = std::string((char *)sr->au8SSID);
                wifi_if.cur_net_list.push_back(ssid);

                if(wifi_if.cur_net_list.size() >= wifi_if.scan_n_aps)
                {
                    // scan done
                    wifi_if.good_net_list = wifi_if.cur_net_list;
                    wifi_if.scan_in_progress = false;

                    CriticalGuard cg;
                    klog("wifi: scan complete, ssids:\n");
                    for(const auto &cssid : wifi_if.good_net_list)
                        klog(" - %s\n", cssid.c_str());
                    klog("wifi: end of list\n");
                }
                else
                {
                    // request next
                    m2m_wifi_req_scan_result(wifi_if.cur_net_list.size());
                }
            }
            break;

        case M2M_WIFI_RESP_SCAN_DONE:
            {
                auto sd = reinterpret_cast<tstrM2mScanDone *>(p_eventData);
                wifi_if.cur_net_list.clear();
                wifi_if.scan_n_aps = sd->u8NumofCh;

                if(wifi_if.scan_n_aps == 0)
                {
                    wifi_if.scan_in_progress = false;
                    klog("wifi: scan complete, no APs found\n");
                }
                else
                {
                    m2m_wifi_req_scan_result(0);
                }
            }
            break;

        default:
            printf("WIFI unhandled event %i\n", eventCode);
            break;
    }
    (void)eventCode;
    (void)p_eventData;
}

void *wifi_task(void *p)
{
    (void)p;

    uint64_t last_scan_time = 0ULL;

    while(true)
    {
        auto irq_signalled = wifi_irq.Wait(clock_cur() + kernel_time::from_ms(200ULL));
        {
            if(irq_signalled)
            {
                winc_mutex.lock();
                wifi_poll();
                winc_mutex.unlock();
            }

            //rtt_printf_wrapper("wifi: event\n");
            static WincNetInterface::state old_state = WincNetInterface::WIFI_UNINIT;
            WincNetInterface::state ws = wifi_if.connected;

            if(ws == WincNetInterface::WIFI_DISCONNECTED && !wifi_if.scan_in_progress &&
                (!last_scan_time || clock_cur_ms() >= last_scan_time + 5000ULL))
            {
                m2m_wifi_request_scan(M2M_WIFI_CH_13);
                wifi_if.scan_in_progress = true;
                last_scan_time = clock_cur_ms();
            }

            if(ws != old_state)
            {
                klog("WIFI state: %d\n", wifi_if.connected);
                old_state = ws;
            }
            if(ws == WincNetInterface::WIFI_UNINIT)
            {
                tstrWifiInitParam wip;
                memset(&wip, 0, sizeof(wip));
                wip.pfAppWifiCb = wifi_handler;
                wip.strEthInitParam.pfAppEthCb = eth_handler;
                wip.strEthInitParam.au8ethRcvBuf = reinterpret_cast<uint8 *>(net_allocate_pbuf(PBUF_SIZE));
                if(!wip.strEthInitParam.au8ethRcvBuf)
                {
                    Yield();
                    continue;
                }
                wip.strEthInitParam.u16ethRcvBufSize = PBUF_SIZE;
        
                if(m2m_wifi_init(&wip) == M2M_SUCCESS)
                {
                    NVIC_EnableIRQ(EXTI4_IRQn);
                    wifi_if.connected = WincNetInterface::WIFI_DISCONNECTED;

                    uint8 hwaddr_ap[6], hwaddr_sta[6];
                    if(m2m_wifi_get_mac_address(hwaddr_ap, hwaddr_sta) == M2M_SUCCESS)
                    {
                        wifi_if.hwaddr = HwAddr(reinterpret_cast<char *>(hwaddr_sta));
                        klog("wifi: hwaddr: %s\n", wifi_if.GetHwAddr().ToString().c_str());
                    }
                }
            }
            if(ws == WincNetInterface::WIFI_DISCONNECTED && !wifi_if.scan_in_progress)
            {
                wifi_connect();
            }
            if(ws == WincNetInterface::WIFI_AWAIT_IP)
            {
                if(net_ip_get_address(&wifi_if) == 0UL)
                {
                    net_dhcpc_begin_for_iface(&wifi_if);
                }
                else
                {
                    wifi_if.connected = WincNetInterface::WIFI_CONNECTED;

                    // spawn a ntp thread - TODO: cancel others on wifi disconnect
                    void *net_ntpc_thread(void *);
                    IP4Addr waddr = net_ip_get_address(&wifi_if);
                    extern Process p_net;
                    auto tntp = Thread::Create("ntp", net_ntpc_thread, (void *)waddr.get(), true,
                        GK_PRIORITY_NORMAL, p_net);
                    (void)tntp;
                    s().Schedule(tntp);
                }
            }

            winc_mutex.lock();
            wifi_poll();
            winc_mutex.unlock();
        }
    }
}

const HwAddr &WincNetInterface::GetHwAddr() const
{
    return hwaddr;
}

int WincNetInterface::SendEthernetPacket(char *buf, size_t n, const HwAddr &dest, uint16_t ethertype,
    bool release_buffer)
{
    // decorate packet
    buf -= 14;
    memcpy(&buf[0], dest.get(), 6);
    memcpy(&buf[6], hwaddr.get(), 6);
    *reinterpret_cast<uint16_t *>(&buf[12]) = htons(ethertype);

    // compute crc
    if(n < (64 - 18))
    {
        memset(&buf[14 + n], 0, (64 - 18) - n);
        n = 64 - 18;  // min frame length
    }
    auto crc = net_ethernet_calc_crc(buf, n + 14);
    *reinterpret_cast<uint32_t *>(&buf[n + 14]) = crc;
#if 0
    {
        CriticalGuard cg;
        klog("send wifi packet:\n");
        for(unsigned int i = 0; i < n + 14; i++)
            klog("%02X ", buf[i]);
        klog("\nchecksum: %8" PRIx32 "\n", crc);
    }
#endif

    winc_mutex.lock();
    /* For WINC3000 - data must start in the buffer at offset M2M_ETHERNET_HDR_OFFSET + M2M_ETH_PAD_SIZE.
        These extra fields are not included in the packet size (but do include 18 bytes for ethernet header) */
    auto ret = m2m_wifi_send_ethernet_pkt(reinterpret_cast<uint8 *>(buf) - M2M_ETHERNET_HDR_OFFSET - M2M_ETH_PAD_SIZE,
        n + 18, STATION_INTERFACE);
    winc_mutex.unlock();
    
    if(release_buffer)
    {
        net_deallocate_pbuf(buf);
    }
    if(ret == M2M_SUCCESS)
    {
        return NET_OK;
    }
    else
    {
        return NET_NOROUTE;
    }
}

bool WincNetInterface::GetLinkActive() const
{
    return connected == WincNetInterface::WIFI_CONNECTED ||
        connected == WincNetInterface::WIFI_AWAIT_IP;
}

std::vector<std::string> WincNetInterface::ListNetworks() const
{
    return wifi_if.good_net_list;
}
