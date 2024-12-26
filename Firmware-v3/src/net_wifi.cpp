#include "osnet.h"
#include "INIReader.h"

static std::vector<std::pair<std::string, std::string>> known_nets;
static kernel_time last_update;

std::vector<std::pair<std::string, std::string>> net_get_known_networks()
{
    if(last_update.is_valid() && clock_cur() < (last_update + kernel_time::from_ms(5)))
    {
        return known_nets;
    }

    last_update = clock_cur();

    INIReader ini("/etc/wifi.secrets");
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
