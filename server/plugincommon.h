/* SA-MP Plugin SDK - plugincommon.h */

#pragma once

#define SAMP_PLUGIN_VERSION 0x0200

#ifdef __cplusplus
#define PLUGIN_EXTERN_C extern "C"
#else
#define PLUGIN_EXTERN_C
#endif

#if defined(LINUX) || defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#define PLUGIN_CALL
#define PLUGIN_EXPORT PLUGIN_EXTERN_C __attribute__((visibility("default")))
#elif defined(_WIN32) || defined(WIN32)
#define PLUGIN_CALL __stdcall
#define PLUGIN_EXPORT PLUGIN_EXTERN_C
#else
#error "Define LINUX or WIN32"
#endif

enum SUPPORTS_FLAGS {
    SUPPORTS_VERSION = SAMP_PLUGIN_VERSION,
    SUPPORTS_VERSION_MASK = 0xffff,
    SUPPORTS_AMX_NATIVES = 0x10000,
    SUPPORTS_PROCESS_TICK = 0x20000
};

enum PLUGIN_DATA_TYPE {
    PLUGIN_DATA_LOGPRINTF = 0x00,
    PLUGIN_DATA_AMX_EXPORTS = 0x10,
    PLUGIN_DATA_CALLPUBLIC_FS = 0x11,
    PLUGIN_DATA_CALLPUBLIC_GM = 0x12,
    /* Extended (samp-plugin-sdk / open.mp style); stock 0.3.7 may not provide RAKSERVER */
    PLUGIN_DATA_RAKSERVER = 0xE2,  /* CCRakServer* (*)() — function returning RakServer */
};

enum PLUGIN_AMX_EXPORT {
    PLUGIN_AMX_EXPORT_Register = 33,
};
