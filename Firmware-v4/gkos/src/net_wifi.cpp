#include "osnet.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#include "INIReader.h"
#pragma GCC diagnostic pop
#include "syscalls_int.h"
#include <unistd.h>
#include <fcntl.h>
#include <limits>
#include <algorithm>

static std::vector<std::pair<std::string, std::string>> known_nets;
static kernel_time last_update;

#define MYFGETS_BUFLEN 512
static char myfgets_buffer[MYFGETS_BUFLEN];

static char *myfgets(char *buf, int num, void *str)
{
    auto blen = std::min(MYFGETS_BUFLEN, num);

    auto br = deferred_call(syscall_read, (int)(intptr_t)str, myfgets_buffer, blen - 1).first;
    if(br <= 0)
        return nullptr;
    myfgets_buffer[blen-1] = 0;

    auto first_nline = strchr(myfgets_buffer, '\n');
    if(first_nline)
    {
        // truncate the returned data and backtrack the file pointer
        first_nline[1] = 0;

        auto backtrack_by = br - (first_nline - myfgets_buffer) - 1;
        deferred_call(syscall_lseek, (int)(intptr_t)str, -backtrack_by, SEEK_CUR);
    }

    strncpy(buf, myfgets_buffer, num);
    return buf;
}

std::vector<std::pair<std::string, std::string>> net_get_known_networks()
{
    if(kernel_time_is_valid(last_update) && clock_cur() < (last_update + kernel_time_from_ms(500)))
    {
        return known_nets;
    }

    last_update = clock_cur();

    auto fd = deferred_call(syscall_open, "/etc/wifi.secrets", O_RDONLY, 0).first;
    if(fd < 0)
    {
        klog("net: couldn't open /etc/wifi.secrets\n");
        return known_nets;
    }

    INIReader ini(myfgets, (void *)fd);
    close(fd);

    if(ini.ParseError() != 0)
    {
        klog("net: couldn't open /etc/wifi.secrets: %d\n", ini.ParseError());
        return known_nets;
    }

    known_nets.clear();

    for(const auto &sect : ini.Sections())
    {
        auto ssid = ini.Get(sect, "SSID", "");
        auto psk = ini.Get(sect, "secret", "");

        if(ssid != "" && psk != "")
        {
            known_nets.push_back(std::make_pair(ssid, psk));
            klog("net: read ssid: %s, secret: %s\n", ssid.c_str(), psk.c_str());
        }
    }

    return known_nets;
}

const std::vector<WifiNetInterface::wifi_network> &WifiNetInterface::ListNetworks() const
{
    return networks;
}

int WifiNetInterface::IdleTask()
{
    if(active && !connected && !connecting && !scan_in_progress &&
        (!kernel_time_is_valid(last_scan_time) || (clock_cur() >= last_scan_time + kernel_time_from_ms(5000))))
    {
        // run a scan
        last_scan_time = clock_cur();
        scan_in_progress = true;
        DoScan();
    }

    if(active && !connected && scan_in_progress)
    {
        // timeout scans
        if(kernel_time_is_valid(last_scan_time) &&
            (clock_cur() >= last_scan_time + kernel_time_from_ms(5000)))
        {
            klog("net: wifi scan failed\n");
            last_scan_time = clock_cur();
            scan_in_progress = false;
        }
    }

    if(active && !connected && !scan_in_progress && !connecting)
    {
        connecting = true;
        try_connect_networks.clear();
        cur_try_connect_network = 0;
        last_try_connect_time = kernel_time_invalid();

        auto _known_nets = net_get_known_networks();

        for(const auto &cur_net : _known_nets)
        {
            const auto &ssid = cur_net.first;
            const auto &pwd = cur_net.second;

            wifi_network wn{};
            bool found = false;

            /* If we have a recent scan, try and find the best channel for this ssid, else
                try all channels if not found on scan (it may be a hidden ssid) */
            for(const auto &cur_wn : networks)
            {
                if(cur_wn.ssid == ssid)
                {
                    if(found)
                    {
                        if(cur_wn.rssi < wn.rssi)
                            continue;
                    }
                    found = true;
                    wn = cur_wn;
                    wn.password = pwd;
                }
            }

            if(!found)
            {
                wn.ch = -1;
                wn.rssi = std::numeric_limits<decltype(wn.rssi)>::min();
                wn.ssid = ssid;
                wn.password = pwd;
            }

            try_connect_networks.push_back(wn);
        }

        // sort the networks based on rssi high to low
        std::sort(try_connect_networks.begin(), try_connect_networks.end(),
            [](const auto &a, const auto &b) { return a.rssi > b.rssi; });

        if(try_connect_networks.empty())
        {
            // no known networks
            connecting = false;
        }
    }

    if(active && !connected && connecting)
    {
        // try and connect
        if(!kernel_time_is_valid(last_try_connect_time) ||
            clock_cur() >= last_try_connect_time + kernel_time_from_ms(500))
        {
            if(cur_try_connect_network >= try_connect_networks.size())
            {
                // reached the end of the list - fail
                connecting = false;
            }
            else
            {
                const auto &try_network = try_connect_networks[cur_try_connect_network];
                cur_try_connect_network++;
                last_try_connect_time = clock_cur();

                Connect(try_network);
            }
        }
    }

    return 0;
}

int WifiNetInterface::DoScan()
{
    klog("wifi: DoScan() requested on generic wifi interface\n");
    return -1;
}

int WifiNetInterface::Connect(const wifi_network &wn)
{
    klog("wifi: Connect(%s) requested on generic wifi interface\n", wn.ssid.c_str());
    return -1;
}

std::string WifiNetInterface::DeviceType() const
{
    return "wifi";
}
