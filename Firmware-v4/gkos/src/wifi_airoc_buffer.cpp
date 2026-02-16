#include "cy_network_buffer.h"
#include "logger.h"
#include <cstdlib>
#include "osnet.h"

const size_t cy_header = NET_SIZE_ETHERNET_HEADER + NET_SIZE_WIFI_HEADER;
const size_t cy_footer = NET_SIZE_ETHERNET_FOOTER;

struct cybuf
{
    Spinlock sl{};
    size_t off;
    size_t len;
    size_t tot_size;
    bool is_malloced = false;
};

whd_result_t cy_host_buffer_get(whd_buffer_t* buffer, whd_buffer_dir_t direction,
                                uint16_t size, uint32_t timeout_ms)
{
    auto act_size = size + sizeof(cybuf) + NET_SIZE_ETHERNET_HEADER + NET_SIZE_WIFI_HEADER + NET_SIZE_ETHERNET_FOOTER;
    auto pbuf = net_allocate_pbuf(act_size);
    auto cb = reinterpret_cast<cybuf *>(pbuf);
    if(!pbuf)
    {
        klog("net: host_buffer_get failed for size %u\n", size);

        pbuf = (char *)malloc(act_size);
        if(!pbuf)
        {
            return WHD_WLAN_NOMEM;
        }
        else
        {
            cb = reinterpret_cast<cybuf *>(pbuf);
            *cb = cybuf();
            cb->is_malloced = true;
        }
    }
    else
    {
        *cb = cybuf();
    }

    cb->tot_size = act_size;
    cb->len = size;
    cb->off = sizeof(cybuf) + NET_SIZE_ETHERNET_HEADER + NET_SIZE_WIFI_HEADER;

    *buffer = (void *)cb;

    //klog("net: buffer %p: get (off: %lu, len: %lu, tot_size: %lu)\n",
    //    cb, cb->off, cb->len, cb->tot_size);

    return WHD_SUCCESS;
}

void cy_buffer_release(whd_buffer_t buffer, whd_buffer_dir_t direction)
{
    auto cb = reinterpret_cast<cybuf *>(buffer);
    CriticalGuard cg(cb->sl);
    //klog("net: buffer %p: release (off: %lu, len: %lu, tot_size: %lu)\n",
    //    cb, cb->off, cb->len, cb->tot_size);

    if(cb->is_malloced)
    {
        cg.unlock();
        free(cb);
    }
    else
    {
        cg.unlock();
        net_deallocate_pbuf((char *)cb);
    }
}

uint8_t* cy_buffer_get_current_piece_data_pointer(whd_buffer_t buffer)
{
    auto cb = reinterpret_cast<cybuf *>(buffer);
    CriticalGuard cg(cb->sl);
    //klog("net: buffer %p: get_ptr (off: %lu, len: %lu, tot_size: %lu)\n",
    //    cb, cb->off, cb->len, cb->tot_size);

    return (uint8_t *)buffer + cb->off;
}

whd_result_t cy_buffer_set_size(whd_buffer_t buffer, uint16_t size)
{
    auto cb = reinterpret_cast<cybuf *>(buffer);
    CriticalGuard cg(cb->sl);
    //klog("net: buffer %p: set_size (off: %lu, len: %lu, tot_size: %lu)\n",
    //    cb, cb->off, cb->len, cb->tot_size);

    if((cb->off + size) > cb->tot_size)
    {
        // cannot allocate here.  TODO: reallocate and copy
        klog("cy_buffer_set_size failed for new size %u\n", size);
        return WHD_RTOS_ERROR;
    }
    else
    {
        cb->len = size;
    }

    //klog("net: buffer %p: new_size (off: %lu, len: %lu, tot_size: %lu)\n",
    //    cb, cb->off, cb->len, cb->tot_size);

    return WHD_SUCCESS;
}

whd_result_t cy_buffer_add_remove_at_front(whd_buffer_t* buffer, int32_t add_remove_amount)
{
    auto pcb = reinterpret_cast<cybuf **>(buffer);
    auto cb = *pcb;
    CriticalGuard cg(cb->sl);
    //klog("net: buffer %p: add_remove_at_front (off: %lu, len: %lu, tot_size: %lu)\n",
    //    cb, cb->off, cb->len, cb->tot_size);

    if(add_remove_amount > 0)
    {
        // decrease the space at front i.e. move offset forward
        auto decr_amount = (size_t)add_remove_amount;
        if(decr_amount > cb->len)
        {
            decr_amount = cb->len;
        }
        cb->off += decr_amount;
        cb->len -= decr_amount;
        //klog("net: buffer %p: new_front (off: %lu, len: %lu, tot_size: %lu)\n",
        //    cb, cb->off, cb->len, cb->tot_size);

        return WHD_SUCCESS;
    }
    else
    {
        auto incr_amount = (size_t)(-add_remove_amount);
        // increase the space at front, i.e. move offset backwards, increase len
        if(incr_amount > (cb->off - sizeof(cybuf)))
        {
            klog("cy_buffer_add_remove_at_front: add_remove_amount (-%u) too negative\n",
                incr_amount);
            return WHD_RTOS_ERROR;
        }
        else
        {
            cb->off -= incr_amount;
            cb->len += incr_amount;
            //klog("net: buffer %p: new_front (off: %lu, len: %lu, tot_size: %lu)\n",
            //    cb, cb->off, cb->len, cb->tot_size);

            return WHD_SUCCESS;
        }
    }
}

uint16_t cy_buffer_get_current_piece_size(whd_buffer_t buffer)
{
    auto cb = reinterpret_cast<cybuf *>(buffer);
    CriticalGuard cg(cb->sl);
    //klog("net: buffer %p: get_piece_size (off: %lu, len: %lu, tot_size: %lu)\n",
    //    cb, cb->off, cb->len, cb->tot_size);

    return (uint16_t)cb->len;
}

void cy_network_process_ethernet_data(whd_interface_t interface, whd_buffer_t buffer)
{
    klog("cy_network_process_ethernet_data\n");
}
