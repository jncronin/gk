#include "syscalls_int.h"
#include "wifi_airoc_if.h"
#include <memory>

int syscall_wifienable(int en, int *_errno)
{
    extern std::unique_ptr<WifiAirocNetInterface> airoc_if;
    airoc_if->SetActive(en != 0);
    return 0;
}
