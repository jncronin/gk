#include "osnet.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#include "INIReader.h"
#pragma GCC diagnostic pop
#include "syscalls_int.h"
#include <unistd.h>
#include <fcntl.h>

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
