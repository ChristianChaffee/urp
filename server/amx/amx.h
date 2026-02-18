/* Pawn AMX - minimal types for plugin */

#ifndef AMX_H_INCLUDED
#define AMX_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AMX_NATIVE_CALL
#define AMXAPI

#if defined(__LP64__) || defined(_LP64)
typedef int64_t cell;
#else
typedef int32_t cell;
#endif

struct tagAMX;

typedef cell (AMX_NATIVE_CALL *AMX_NATIVE)(struct tagAMX *amx, cell *params);

typedef struct tagAMX_NATIVE_INFO {
    const char *name;
    AMX_NATIVE func;
} AMX_NATIVE_INFO;

typedef struct tagAMX {
    unsigned char *base;
    unsigned char *data;
    void *callback;
    void *debug;
    cell cip, frm, hea, hlw, stk, stp;
    int flags;
    long usertags[4];
    void *userdata[4];
    int error;
    int paramcount;
    cell pri, alt, reset_stk, reset_hea, sysreq_d;
} AMX;

#define AMX_ERR_NONE 0

#ifdef __cplusplus
}
#endif

#endif
