#include <stm32h7xx.h>
#include <bsp/include/nm_bsp.h>
#include <bus_wrapper/include/nm_bus_wrapper.h>
#include <driver/include/m2m_wifi.h>

#include "thread.h"
#include "scheduler.h"
#include "osmutex.h"
#include "osnet.h"
#include "pins.h"
#include "clocks.h"
#include "conf_winc.h"
#include "ossharedmem.h"

#include "SEGGER_RTT.h"

extern Spinlock s_rtt;

SRAM4_DATA static BinarySemaphore wifi_irq;
SRAM4_DATA static Mutex winc_mutex;

// Max transaction size on SPI bus
SRAM4_DATA tstrNmBusCapabilities egstrNmBusCapabilities =
{
	UINT16_MAX
};

static void wifi_handler(uint8 eventCode, void *p_eventData);

class WincNetInterface : public NetInterface
{
    protected:
        HwAddr hwaddr;

        std::vector<std::string> good_net_list, cur_net_list;
        void begin_scan();
        bool scan_in_progress = false;
        friend void *wifi_task(void *);
        friend void wifi_handler(uint8 eventCode, void *p_eventData);

    public:
        const HwAddr &GetHwAddr() const;
        bool GetLinkActive() const;
        int SendEthernetPacket(char *buf, size_t n, const HwAddr &dest, uint16_t ethertype,
            bool release_buffer);

        enum state { WIFI_UNINIT = 0, WIFI_DISCONNECTED, WIFI_CONNECTING, WIFI_AWAIT_IP, WIFI_CONNECTED };
        state         connected = WIFI_UNINIT;

        std::vector<std::string> ListNetworks() const;
};

NET_BSS WincNetInterface wifi_if;

constexpr pin WIFI_WAKE { GPIOD, 11 };
constexpr pin WIFI_CHIP_EN { GPIOD, 12 };
constexpr pin WIFI_NRST { GPIOD, 13 };
constexpr pin WIFI_IRQ { GPIOI, 1 };

constexpr pin WIFI_SPI_PINS[] = {
    { GPIOB, 5, 5 },    // MOSI
    { GPIOG, 9, 5 },      // MISO
    { GPIOG, 10, 5 },     // NSS
    { GPIOG, 11, 5 }      // SCK
};

// registered interrupt function
static void (*wifi_irq_function)() = nullptr;


void init_wifi()
{
    WIFI_WAKE.set_as_output();
    WIFI_CHIP_EN.set_as_output();
    WIFI_NRST.set_as_output();
    WIFI_IRQ.set_as_input();
    for(const auto &p : WIFI_SPI_PINS)
    {
        p.set_as_af();
    }

    /* WIFI is connected to SPI1 clocked at 192 MHz
        Max SPI clock is 48 MHz
        
        */
    RCC->APB2RSTR = RCC_APB2RSTR_SPI1RST;
    (void)RCC->APB2RSTR;
    RCC->APB2RSTR = 0;
    (void)RCC->APB2RSTR;
    RCC->APB2ENR = RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;

    SPI1->CR1 = 0UL;
    SPI1->CFG1 = (1UL << SPI_CFG1_MBR_Pos) |
        (0UL << SPI_CFG1_FTHLV_Pos) |
        (7UL << SPI_CFG1_DSIZE_Pos);
    SPI1->CFG2 = SPI_CFG2_AFCNTR |
        SPI_CFG2_SSOE |
        SPI_CFG2_MASTER;

    // set wifi irq (GPIOI1), falling edge
    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC->APB4ENR;
    SYSCFG->EXTICR[0] &= SYSCFG_EXTICR1_EXTI1_Msk;
    SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI1_PI;

    EXTI->RTSR1 &= ~EXTI_RTSR1_TR1;
    EXTI->FTSR1 |= EXTI_FTSR1_TR1;
    EXTI->IMR1 |= EXTI_IMR1_IM1;

    wifi_if.connected = WincNetInterface::WIFI_UNINIT;

    NVIC_EnableIRQ(EXTI1_IRQn);
}

void wifi_connect()
{
    auto cur_nets = net_get_known_networks();

    for(const auto &cur_net : cur_nets)
    {
        wifi_if.connected = WincNetInterface::WIFI_CONNECTING;
        const auto &ssid = cur_net.first;
        const auto &pwd = cur_net.second;

        auto ret = m2m_wifi_connect(const_cast<char *>(ssid.c_str()), ssid.length(),
            M2M_WIFI_SEC_WPA_PSK,
            const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(pwd.c_str())), M2M_WIFI_CH_ALL);

        if(ret != M2M_SUCCESS)
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "wifi: m2m_wifi_connect_psk failed %i\n", ret);
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
        NVIC_EnableIRQ(EXTI1_IRQn);
    else
        NVIC_DisableIRQ(EXTI1_IRQn);
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
    nm_bsp_sleep(1);
    WIFI_CHIP_EN.set();
    nm_bsp_sleep(10);
    WIFI_NRST.set();
    nm_bsp_sleep(1);

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

extern "C" void EXTI1_IRQHandler(void)
{
#ifdef DEBUG_WIFI
    itm_tstamp();
    ITM_SendChar('W');
    ITM_SendChar('I');
#endif
    //rtt_printf_wrapper("WIFI IRQ\n");
    if(wifi_irq_function)
        wifi_irq_function();

    wifi_irq.Signal();
    EXTI->PR1 = EXTI_PR1_PR1;
    (void)EXTI->PR1;
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
        SPI1->CFG1 &= ~SPI_CFG1_RXDMAEN;
        SPI1->CFG1 &= ~SPI_CFG1_TXDMAEN;
        while(SPI1->SR & SPI_SR_RXNE)
            (void)SPI1->DR;
        
        // Program DMA - ch4
        DMA2->LIFCR = DMA_LIFCR_CDMEIF0 |
            DMA_LIFCR_CTEIF0 |
            DMA_LIFCR_CHTIF0 |
            DMA_LIFCR_CTCIF0;
        DMA2_Stream0->CR = 0;
        DMA2_Stream0->PAR = (uint32_t)(uintptr_t)&SPI1->DR;
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
        DMA2_Stream1->PAR = (uint32_t)(uintptr_t)&SPI1->DR;
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

        SPI1->CR2 |= SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN;

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
        SPI1->CFG1 &= ~SPI_CFG1_RXDMAEN & ~SPI_CFG1_TXDMAEN;
        SPI1->CR2 = txrxLen;
        SPI1->CR1 = SPI_CR1_SPE;
        SPI1->CR1 = SPI_CR1_SPE | SPI_CR1_CSTART;

        {
            SharedMemoryGuard(p_txBuf, txrxLen, true, false);
            {
                SharedMemoryGuard(p_rxBuf, txrxLen, false, true);

                for(uint16_t i = 0; i < txrxLen; i++)
                {
                    uint8_t tb, rb;
                    tb = (p_txBuf) ? p_txBuf[i] : 0;
                    while(!(SPI1->SR & SPI_SR_TXP));
                    *reinterpret_cast<volatile uint8_t *>(&SPI1->TXDR) = tb;
                    while(!(SPI1->SR & SPI_SR_RXP));
                    rb = *reinterpret_cast<volatile uint8_t *>(&SPI1->RXDR);
                    if(p_rxBuf)
                        p_rxBuf[i] = rb;
                }
            }
        }

        while(!(SPI1->SR & SPI_SR_EOT));
        SPI1->IFCR = 0xff8U;
        SPI1->CR1 = 0;

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

            auto ctrlbuf = reinterpret_cast<tstrM2mIpCtrlBuf *>(pvCtrlBuf);
#ifdef DEBUG_WIFI
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "wifi: rx packet %d/%d @ %x\n",
                    ctrlbuf->u16DataSize, ctrlbuf->u16RemainigDataSize,
                    pvMsg);
                for(unsigned int i = 0; i < ctrlbuf->u16DataSize; i++)
                {
                    SEGGER_RTT_printf(0, "%02X ", reinterpret_cast<char *>(pvMsg)[i]);
                }
                SEGGER_RTT_printf(0, "\n");
            }
#endif

            if(ctrlbuf->u16RemainigDataSize == 0)
            {
                net_inject_ethernet_packet(reinterpret_cast<char *>(pvMsg) - pkt_offset,
                    ctrlbuf->u16DataSize + pkt_offset, &wifi_if);
                
                auto newbuf = net_allocate_pbuf(PBUF_SIZE);
                if(!newbuf)
                {
                    /* Drop priority to let net thread run, then increase again
                        TODO: have scheduler honour new priority */
                    GetCurrentThreadForCore()->base_priority--;
                    while(!newbuf)
                    {
                        Yield();
                        newbuf = net_allocate_pbuf(PBUF_SIZE);
                    }
                    GetCurrentThreadForCore()->base_priority++;
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
            rtt_printf_wrapper("wifi: eth: %u\n", msgType);
            break;
    }
}

extern "C" void os_hook_isr()
{
    // do we need to do anything here?
}

static void wifi_handler(uint8 eventCode, void *p_eventData)
{
    switch(eventCode)
    {
        case M2M_WIFI_RESP_CON_STATE_CHANGED:
            {
                auto state = reinterpret_cast<tstrM2mWifiStateChanged *>(p_eventData);

                switch(state->u8CurrState)
                {
                    case M2M_WIFI_CONNECTED:
                        wifi_if.connected = WincNetInterface::WIFI_AWAIT_IP;
                        rtt_printf_wrapper("WIFI Connected\n");

                        break;

                    case M2M_WIFI_DISCONNECTED:
                        wifi_if.connected = WincNetInterface::WIFI_DISCONNECTED;
                        //socket_wifi_disconnect();
                        rtt_printf_wrapper("WIFI Disconnected %i\n", state->u8ErrCode);

                        break;
                }
            }
            break;

        case M2M_WIFI_RESP_SCAN_RESULT:
            {
                auto sr = reinterpret_cast<tstrM2mWifiscanResult *>(p_eventData);
                auto ssid = std::string((char *)sr->au8SSID);
                wifi_if.cur_net_list.push_back(ssid);
            }
            break;

        case M2M_WIFI_RESP_SCAN_DONE:
            {
                wifi_if.good_net_list = wifi_if.cur_net_list;
                wifi_if.scan_in_progress = false;
            }
            break;

        default:
            // TODO
            printf("WIFI unhandled event %i\n", eventCode);
            break;
    }
    // TODO
    (void)eventCode;
    (void)p_eventData;
}

void *wifi_task(void *p)
{
    (void)p;

    uint64_t last_scan_time = 0ULL;

    while(true)
    {
        wifi_irq.Wait(clock_cur() + kernel_time::from_ms(200ULL));
        {
            //rtt_printf_wrapper("wifi: event\n");
            static WincNetInterface::state old_state = WincNetInterface::WIFI_UNINIT;
            WincNetInterface::state ws = wifi_if.connected;

            if(!wifi_if.scan_in_progress && (!last_scan_time || clock_cur_ms() >= last_scan_time + 5000ULL))
            {
                m2m_wifi_request_scan(M2M_WIFI_CH_ALL);
                wifi_if.scan_in_progress = true;
                last_scan_time = clock_cur_ms();
            }

            if(ws != old_state)
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "WIFI state: %d\n", wifi_if.connected);
                old_state = ws;
            }
            if(ws == WincNetInterface::WIFI_UNINIT)
            {
                tstrWifiInitParam wip;
                memset(&wip, 0, sizeof(wip));
                wip.pfAppWifiCb = wifi_handler;
                wip.strEthInitParam.pfAppWifiCb = wifi_handler;
                wip.strEthInitParam.pfAppEthCb = eth_handler;
                wip.strEthInitParam.u8EthernetEnable = M2M_WIFI_MODE_ETHERNET;
                wip.strEthInitParam.au8ethRcvBuf = reinterpret_cast<uint8 *>(net_allocate_pbuf(PBUF_SIZE));
                if(!wip.strEthInitParam.au8ethRcvBuf)
                {
                    Yield();
                    continue;
                }
                wip.strEthInitParam.u16ethRcvBufSize = sizeof(PBUF_SIZE);
        
                if(m2m_wifi_init(&wip) == M2M_SUCCESS)
                {
                    wifi_if.connected = WincNetInterface::WIFI_DISCONNECTED;

                    uint8 hwaddr[6];
                    if(m2m_wifi_get_mac_address(hwaddr) == M2M_SUCCESS)
                    {
                        wifi_if.hwaddr = HwAddr(reinterpret_cast<char *>(hwaddr));
                        rtt_printf_wrapper("wifi: hwaddr: %s\n", wifi_if.GetHwAddr().ToString().c_str());
                    }
                }
            }
            if(ws == WincNetInterface::WIFI_DISCONNECTED)
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
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "send wifi packet:\n");
        for(unsigned int i = 0; i < n + 14; i++)
            SEGGER_RTT_printf(0, "%02X ", buf[i]);
        SEGGER_RTT_printf(0, "\nchecksum: %8" PRIx32 "\n", crc);
    }

    winc_mutex.lock();
    auto ret = m2m_wifi_send_ethernet_pkt(reinterpret_cast<uint8 *>(buf), n + 18);
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
