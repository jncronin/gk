/*
 * This file is part of the µOS++ distribution.
 *   (https://github.com/micro-os-plus)
 * Copyright (c) 2017 Liviu Ionescu.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The plug-in client API definitions (the `RTOS_*` functions) and the
 * GDB server API definitions are compatible with the SEGGER GDBServer
 * RTOS Plug-in SDK API definitions.
 *
 * All IP rights, title and interest in the GDBServer RTOS Plug-in SDK
 * are and shall at all times remain with SEGGER.
 *
 * Copyright (c) 2004-2016 SEGGER Microcontroller GmbH & Co. KG
 * Internet: www.segger.com        Support:  support@segger.com
 */

#ifndef SEGGER_JLINK_SDK_RTOS_PLUGIN_H_
#define SEGGER_JLINK_SDK_RTOS_PLUGIN_H_

// ----------------------------------------------------------------------------
// Plain C definitions.
// Unless explicitly allowed by conditional compilation,
// no C++ should be used whatsoever. A test case will warn about this.

#include <stdint.h>
#include <stddef.h>

// TODO: check/update for mingw-w64.
#ifdef WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

// ----------------------------------------------------------------------------

// J-Link specific definitions.
#define JLINK_CORE_CORTEX_M0        0x060000FF
#define JLINK_CORE_CORTEX_M1        0x010000FF
#define JLINK_CORE_CORTEX_M3        0x030000FF
#define JLINK_CORE_CORTEX_M4        0x0E0000FF
#define JLINK_CORE_CORTEX_M7        0x0E0100FF

// ----------------------------------------------------------------------------

#if defined(__cplusplus)
extern "C"
{
#endif /* defined(__cplusplus) */

  typedef uint32_t rtos_plugin_target_addr_t;
  typedef uint32_t rtos_plugin_thread_id_t;

  // RTOS symbols.
  typedef struct rtos_plugin_symbols_s
  {
#if defined(__cplusplus)
    // Mandatory definition for template usage.
    typedef rtos_plugin_target_addr_t element_type;
#endif /* defined(__cplusplus) */

    const char *name;
    int optional;
    rtos_plugin_target_addr_t address;
  } rtos_plugin_symbols_t;

  // GDB server functions that can be called by the plug-in.
  typedef struct rtos_plugin_server_api_s
  {
    /**
     * @param [in] p Pointer to the memory block.
     */
    void
    (*free) (void* p);

    /**
     * @param [in] bytes Size of memory block.
     *
     * @return Pointer to the allocated memory block. NULL, if the memory could not be allocated.
     */
    void*
    (*malloc) (size_t bytes);

    /**
     * @param [in] p Pointer to old memory block.
     * @param [in] bytes New size of memory block.
     *
     * @return Pointer to the allocated memory block. NULL, if the memory could not be allocated.
     */
    void*
    (*realloc) (void* p, unsigned bytes);

    /**
     * @brief Output a formatted log message to J-Link GDB server window.
     *
     * @details
     * If a log file is specified, the message will also be printed to
     * the log file.
     */
    void
    (*output) (const char* fmt, ...);

    /**
     * @brief Output a formatted log message to the debug channel.
     *
     * @details
     * Outputs to the debug channel are suppressed in non-debug builds of
     * the J-Link GDB Server.
     */
    void
    (*output_debug) (const char* fmt, ...);

    /**
     * @brief Output a formatted warning message to J-Link GDB server window.
     *
     * @details
     * The line starts with “WARNING: ”. If a log file is specified, the
     * message will also be printed to the log file.
     */
    void
    (*output_warning) (const char* fmt, ...);

    /**
     * @brief Output a formatted error message to J-Link GDB server window.
     *
     * @details
     * The line starts with “ERROR: ”. If a log file is specified, the
     * message will also be printed to the log file.
     */
    void
    (*output_error) (const char* fmt, ...);

    /**
     * @brief Read memory from the target system.
     *
     * @details
     * If necessary, the target CPU is halted in order to read memory.
     *
     * @param [in] addr Target address to read from.
     * @param [out] out_array Pointer to buffer for target memory.
     * @param [in] bytes Number of bytes to read.
     *
     * @retval 0 Reading memory OK.
     * @retval <0 Reading memory failed.
     */
    int
    (*read_byte_array) (rtos_plugin_target_addr_t addr, uint8_t* out_array,
                        size_t bytes);

    /**
     * @brief Read one byte from the target system.
     *
     * @details
     * If necessary, the target CPU is halted in order to read memory.
     *
     * @param [in] addr Target address to read from.
     * @param [out] out_value Pointer to byte.
     *
     * @retval 0 Reading memory OK.
     * @retval <0 Reading memory failed.
     */
    int
    (*read_byte) (rtos_plugin_target_addr_t addr, uint8_t* out_value);

    /**
     * @brief Read two bytes from the target system.
     *
     * @details
     * If necessary, the target CPU is halted in order to read memory.
     *
     * @param [in] addr Target address to read from.
     * @param [out] out_value Pointer to two bytes.
     *
     * @retval 0 Reading memory OK.
     * @retval <0 Reading memory failed.
     */
    int
    (*read_short) (rtos_plugin_target_addr_t addr, uint16_t* out_value);

    /**
     * @brief Read four bytes from the target system.
     *
     * @details
     * If necessary, the target CPU is halted in order to read memory.
     *
     * @param [in] addr Target address to read from.
     * @param [out] out_value Pointer to four bytes.
     *
     * @retval 0 Reading memory OK.
     * @retval <0 Reading memory failed.
     */
    int
    (*read_long) (rtos_plugin_target_addr_t addr, uint32_t* out_value);

    /**
     * @brief Write memory to the target system.
     *
     * @details
     * If necessary, the target CPU is halted in order to read memory.
     *
     * @param [in] addr Target address to read from.
     * @param [in] array Pointer to buffer for target memory.
     * @param [in] bytes Number of bytes to read.
     *
     * @retval 0 Writing memory OK.
     * @retval <0 Writing memory failed.
     */
    int
    (*write_byte_array) (rtos_plugin_target_addr_t addr, const uint8_t* array,
                         size_t bytes);

    /**
     * @brief Write one byte to the target system.
     *
     * @details
     * If necessary, the target CPU is halted in order to read memory.
     *
     * @param [in] addr Target address to read from.
     * @param [in] value Byte to write.
     *
     * @retval 0 Writing memory OK.
     * @retval <0 Writing memory failed.
     */
    void
    (*write_byte) (rtos_plugin_target_addr_t addr, uint8_t value);

    /**
     * @brief Write two bytes to the target system.
     *
     * @details
     * If necessary, the target CPU is halted in order to read memory.
     *
     * @param [in] addr Target address to read from.
     * @param [in] value Bytes to write.
     *
     * @retval 0 Writing memory OK.
     * @retval <0 Writing memory failed.
     */
    void
    (*write_short) (rtos_plugin_target_addr_t addr, uint16_t value);

    /**
     * @brief Write four bytes to the target system.
     *
     * @details
     * If necessary, the target CPU is halted in order to read memory.
     *
     * @param [in] addr Target address to read from.
     * @param [in] value Bytes to write.
     *
     * @retval 0 Writing memory OK.
     * @retval <0 Writing memory failed.
     */
    void
    (*write_long) (rtos_plugin_target_addr_t addr, uint32_t value);

    /**
     * @brief Load two bytes from a memory buffer according to the
     * target endianness.
     *
     * @param [in] p Pointer to memory buffer.
     *
     * @return The converted value.
     */
    uint32_t
    (*load_short) (const uint8_t* p);

    /**
     * @brief Load three bytes from a memory buffer according to the
     * target endianness.
     *
     * @param [in] p Pointer to memory buffer.
     *
     * @return The converted value.
     */
    uint32_t
    (*load_3bytes) (const uint8_t* p);

    /**
     * @brief Load four bytes from a memory buffer according to the
     * target endianness.
     *
     * @param [in] p Pointer to memory buffer.
     *
     * @return The converted value.
     */
    uint32_t
    (*load_long) (const uint8_t* p);

  } rtos_plugin_server_api_t;

  // --------------------------------------------------------------------------

  /**
   * @brief Initializes RTOS plug-in for further usage.
   *
   * @details
   * If the plug-in does not support the CPU architecture, it should
   * return 0. The pointer to the API functions table should be stored
   * locally. API funtions can be used later by the plug-in to perform
   * special functions like reading or writing to target memory.
   *
   * @param [in] api Pointer to API functions table.
   * @param [in] core JLINK_CORE_* constant identifying the target’s core.
   *
   * @retval 0 Initialization failed
   * @retval 1 Initialized successfully.
   */
  EXPORT int
  RTOS_Init (const rtos_plugin_server_api_t* api, uint32_t core);

  /**
   * @brief Get the RTOS plugin version.
   *
   * @details
   * Will be called before any other function. The J-Link GDB server only
   * checks the major version number. The minor version
   * number is printed in the GDB server’s log
   * file but it is not evaluated.
   *
   * @par Parameters
   *  None.
   *
   * @return The plugin version number as unsigned integer:
   *   100 * [major] + [minor].
   */
  EXPORT uint32_t
  RTOS_GetVersion (void);

  /**
   * @brief Get a pointer to the RTOS symbol table.
   *
   * @details
   * The J-Link GDB server tries to find addresses of all of the symbols
   * specified in the RTOS symbol table. If it is found, the address is
   * written into RTOS_SYMBOLS.address, otherwise NULL is written into
   * the address field. If any of the symbols, declared as mandatory,
   * is not found, the plug-in is not being used by the GDB server. So
   * it is ensured for the following functions, that every mandatory
   * symbol has a valid address entry.
   *
   * @par Parameters
   *  None.
   *
   * @return Pointer to the RTOS symbol table.
   */
  EXPORT rtos_plugin_symbols_t*
  RTOS_GetSymbols (void);

  /**
   * @brief Get the number of threads.
   *
   * @details
   * After calling this function, the GDB server will request the
   * thread ID by calling RTOS_GetThreadId() for every thread.
   *
   * @par Parameters
   *  None.
   *
   * @return The number of threads known by the scheduler.
   */
  EXPORT uint32_t
  RTOS_GetNumThreads (void);

  /**
   * @brief Get the ID of the thread by index.
   *
   * @details
   * Index numbers for threads run from 0..[m-1], where m is the
   * number of threads returned by RTOS_GetNumThreads().
   *
   * @param [in] index Index of the thread.
   *
   * @return The ID of the thread.
   */
  EXPORT uint32_t
  RTOS_GetThreadId (uint32_t index);

  /**
   * @brief Get the ID of the currently running thread.
   *
   * @par Parameters
   *  None.
   *
   * @return The ID of the currently running thread.
   */
  EXPORT uint32_t
  RTOS_GetCurrentThreadId (void);

  /**
   * @brief Get the printable thread name.
   *
   * @details
   * The name may contain extra information about the tread’s
   * status (running/suspended, priority, etc.).
   *
   * @param [out] out_description Pointer to the string where the name
   *  has to be copied to; the space reserved for the name is 256 bytes
   *  (including terminating zero).
   * @param [in] thread_id ID of the thread.
   *
   * @return The length of the name string.
   */
  EXPORT int
  RTOS_GetThreadDisplay (char* out_description, uint32_t thread_id);

  /**
   * @brief Get the thread’s register value as HEX string.
   *
   * @details
   * If the register value has to be read directly from the CPU,
   * the function must return a value <0. The register value is then
   * read from the CPU by the GDB server itself.
   *
   * @param [out] out_hex_value Pointer to the string, the value has
   *  to be copied to.
   * @param [in] reg_index Index of the register.
   * @param [in] thread_id ID of the thread.
   *
   * @retval 0 Reading register OK.
   * @retval <0 Reading register failed.
   */
  EXPORT int
  RTOS_GetThreadReg (char* out_hex_value, uint32_t reg_index,
                     uint32_t thread_id);

  /**
   * @brief Get the thread's general registers as HEX string.
   *
   * @details
   * If the register values have to be read directly from the CPU,
   * the function must return a value <0. The register values are then
   * read from the CPU by the GDB server itself.
   *
   * @param [out] out_hex_values Pointer to the string, the values
   *  have to be copied to.
   * @param [in] thread_id ID of the thread.
   *
   * @retval 0 Reading registers OK.
   * @retval <0 Reading register failed.
   */
  EXPORT int
  RTOS_GetThreadRegList (char* out_hex_values, uint32_t thread_id);

  /**
   * @brief Sets the thread’s register to the given HEX string.
   *
   * @details
   * If the register value has to be written directly to the CPU,
   * the function must return a value <0. The register value is then
   * written to the CPU by the GDB server itself.
   *
   * @param [in] hex_value Pointer to the string, containing the
   *  value to write.
   * @param [in] reg_index Index of the register.
   * @param [in] thread_id ID of the thread.
   *
   * @retval 0 Writing register OK.
   * @retval <0 Writing register failed.
   */
  EXPORT int
  RTOS_SetThreadReg (char* hex_value, uint32_t reg_index, uint32_t thread_id);

  /**
   * @brief Set the thread registers to the given HEX string.
   *
   * @details
   * If the register value has to be written directly to the CPU,
   * the function must return a value <0. The register value is then
   * written to the CPU by the GDB server itself.
   *
   * @param [in] hex_values Pointer to the string, containing the
   *  values to write.
   * @param [in] thread_id ID of the thread.
   *
   * @retval 0 Writing registers OK.
   * @retval <0 Writing registers failed.
   */
  EXPORT int
  RTOS_SetThreadRegList (char* hex_values, uint32_t thread_id);

  /**
   * @brief Update plug-in knowledge on target threads.
   *
   * @details
   * For efficiency purposes, the plug-in should read all required
   * information within this function at once, so later requests can
   * be served without further communication to the target.
   *
   * @par Parameters
   *  None.
   *
   * @retval 0 Initialization failed.
   * @retval 1 Initialized successfully.
   */
  EXPORT int
  RTOS_UpdateThreads (void);

// ----------------------------------------------------------------------------

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* SEGGER_JLINK_SDK_RTOS_PLUGIN_H_ */
