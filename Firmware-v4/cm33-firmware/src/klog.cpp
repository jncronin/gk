#include "logger.h"
#include "message.h"
#include "interface/cm33_data.h"

class cm33_outputter : public logger_printf_outputter
{
    ssize_t output(const void *buf, size_t count)
    {
        ssize_t ret = 0;
        const uint8_t *b = (const uint8_t *)buf;

        while (count--)
        {
            uint32_t msg = CM33_DK_MSG_LOG | (uint32_t)*b++;
            send_message(msg);
            ret++;
        }

        return ret;
    }
};

int klog(const char *format, ...)
{
    cm33_outputter oput;

    va_list args;
    va_start(args, format);

    auto ret = logger_vprintf(oput, format, args);
    
    va_end(args);

    send_message(CM33_DK_MSG_LOGEND);

    return ret;
}
