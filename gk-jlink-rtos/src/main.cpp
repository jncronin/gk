#define _CRT_SECURE_NO_WARNINGS 1
#include "rtos-plugin.h"

#include <cstring>
#include <cstdio>

rtos_plugin_server_api_t rapi;

int RTOS_Init (const rtos_plugin_server_api_t* api, uint32_t core)
{
    rapi = *api;
    rapi.output("gk RTOS plugin loaded\n");
    return 1;
}

uint32_t RTOS_GetVersion()
{
    return 100;
}

static rtos_plugin_symbols_t required_symbols[] =
{
    { "s", 0 },
    { nullptr }
};

rtos_plugin_symbols_t *RTOS_GetSymbols()
{
    return required_symbols;
}

uint32_t RTOS_GetNumThreads()
{
    return 0;
}

uint32_t RTOS_GetThreadId(uint32_t index)
{
    return 0;
}

uint32_t RTOS_GetCurrentThreadId()
{
    return 0;
}

int RTOS_GetThreadDisplay(char *out_description, uint32_t thread_id)
{
    strncpy(out_description, "test_thread", 256);
    return (int)strlen(out_description);
}

int RTOS_GetThreadReg(char *out_hex_value, uint32_t reg_index,
    uint32_t thread_id)
{
    sprintf(out_hex_value, "%8x", 0xdeadbeef);
    return 0;
}

int RTOS_GetThreadRegList(char *out_hex_values, uint32_t thread_id)
{
    sprintf(out_hex_values, "%8x", 0xdeadbeef);
    return 0;
}

int RTOS_SetThreadReg(char *hex_value, uint32_t reg_index, uint32_t thread_id)
{
    return 0;
}

int RTOS_SetThreadRegList(char *hex_values, uint32_t thread_id)
{
    return 0;
}

int RTOS_UpdateThreads()
{
    return 1;
}
