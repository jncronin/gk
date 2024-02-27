#include <osnet.h>
#include <cstring>

static char multicast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
const HwAddr HwAddr::multicast = HwAddr(multicast_addr);

HwAddr::HwAddr(const char *addr)
{
    memcpy(b, addr, 6);
}

std::string HwAddr::ToString() const
{
    char buf[18];
    ToString(buf);
    return std::string(buf);
}

void HwAddr::ToString(char *buf) const
{
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
        b[0], b[1], b[2], b[3], b[4], b[5]);
}

bool HwAddr::operator==(const HwAddr &other)
{
    for(int i = 0; i < 6; i++)
    {
        if(b[i] != other.b[i])
            return false;
    }
    return true;
}
