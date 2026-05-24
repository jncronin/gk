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

static PMutex m_known_nets;
static std::vector<std::shared_ptr<WifiNetInterface::wifi_network>> known_nets;

int init_wifi()
{
    m_known_nets = MutexList.Create();
    known_nets.clear();
    return 0;
}

std::vector<std::shared_ptr<WifiNetInterface::wifi_network>> net_get_known_networks()
{
    MutexGuard mg(*m_known_nets);
    return known_nets;
}

int net_clear_known_networks()
{
    MutexGuard mg(*m_known_nets);
    known_nets.clear();
    return 0;
}

int net_add_known_network(std::shared_ptr<WifiNetInterface::wifi_network> n)
{
    MutexGuard mg(*m_known_nets);
    known_nets.push_back(n);
    return 0;
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
        try_connect_one = false;
        try_connect_networks.clear();
        cur_try_connect_network = 0;
        last_try_connect_time = kernel_time_invalid();

        auto _known_nets = net_get_known_networks();

        for(const auto &cur_net : _known_nets)
        {
            const auto &ssid = cur_net->ssid;
            const auto &creds = cur_net->credentials;
            const auto &cred_type = cur_net->cred_type;

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
                    wn.cred_type = cred_type;
                    wn.credentials = creds;
                }
            }

            if(!found)
            {
                wn.ch = -1;
                wn.rssi = std::numeric_limits<decltype(wn.rssi)>::min();
                wn.ssid = ssid;
                wn.cred_type = cred_type;
                wn.credentials = creds;
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
            !try_connect_one ||
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
                try_connect_one = true;

                if(Connect(try_network) == NET_TRYAGAIN)
                {
                    try_connect_one = false;
                    cur_try_connect_network--;
                }
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
