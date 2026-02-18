/*
 * chathider.so — плагин для SA-MP 0.3.7 R2 Linux
 * Native: SetChatStatus(playerid, bool:status)
 * Callback: OnKeyPressed(playerid, key)
 *
 * Как в chandling-svr: клиент шлёт сырой пакет [ID_CHATHIDER, ACTION_KEY_PRESSED, key].
 * Хук GetPacketID: для пакетов 250 обрабатываем и возвращаем 0xFF, чтобы сервер считал
 * пакет "неизвестным" и не срабатывала защита "Packet was modified".
 *
 * Требуется сервер с PLUGIN_DATA_RAKSERVER и PLUGIN_DATA_CALLPUBLIC_GM.
 */

#include "plugincommon.h"
#include "amx/amx.h"
#include "protocol.h"
#include "lib/raknet/raknet.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

static void *pAMXFunctions = 0;
static void **ppPluginData = 0;
static CCRakServer *pRakServer = 0;

/* CallPublic GM: int (*)(const char *name, const char *format, ...); "ii" = playerid, key */
typedef int (*CallPublicGM_t)(const char *name, const char *format, ...);

static void ensure_rak_server(void)
{
    if (pRakServer || !ppPluginData)
        return;
    typedef void *(*GetRakServerFn)(void);
    GetRakServerFn pfn = (GetRakServerFn)ppPluginData[PLUGIN_DATA_RAKSERVER];
    if (pfn)
        pRakServer = (CCRakServer *)pfn();
}

static void invoke_OnKeyPressed(int playerid, int key)
{
    if (!ppPluginData)
        return;
    CallPublicGM_t pfn = (CallPublicGM_t)ppPluginData[PLUGIN_DATA_CALLPUBLIC_GM];
    if (pfn)
        pfn("OnKeyPressed", "ii", playerid, key);
}

/* --- FindPattern (по образцу chandling-svr / SAMP_AC_v2) --- */
static int memory_compare(const unsigned char *data, const unsigned char *pattern, const char *mask)
{
    for (; *mask; ++mask, ++data, ++pattern)
        if (*mask == 'x' && *data != *pattern)
            return 0;
    return 1;
}

static unsigned long get_exe_base_and_size(unsigned long *out_size)
{
    char buf[512];
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f)
        return 0;
    while (fgets(buf, sizeof(buf), f))
    {
        unsigned long start, end;
        char perms[8];
        if (sscanf(buf, "%lx-%lx %4s", &start, &end, perms) < 3)
            continue;
        if (strchr(perms, 'x') && strchr(perms, 'r'))
        {
            char *path = strchr(buf, '/');
            if (path && strstr(path, ".so") == NULL)
            {
                *out_size = end - start;
                fclose(f);
                return start;
            }
        }
    }
    fclose(f);
    return 0;
}

static unsigned long FindPattern(const char *pattern, const char *mask)
{
    unsigned long size = 0;
    unsigned long address = get_exe_base_and_size(&size);
    if (!address || !size)
        return 0;
    for (unsigned long i = 0; i < size; i++)
    {
        if (memory_compare((const unsigned char *)(address + i), (const unsigned char *)pattern, mask))
            return address + i;
    }
    return 0;
}

/* --- GetPacketID hook (как в chandling: возвращаем 0xFF для своих пакетов) --- */
typedef unsigned char (*GetPacketID_t)(Packet* p);
#define GETPACKETID_HOOK_LEN 6
static GetPacketID_t g_original_GetPacketID = 0;
static void *g_trampoline = 0;

static unsigned char __attribute__((cdecl)) hooked_GetPacketID(Packet *p)
{
    unsigned char ret = g_original_GetPacketID(p);
    if (!p || !p->data || p->length < 3)
        return ret;
    if (ret == (unsigned char)ID_CHATHIDER && p->data[1] == (unsigned char)ACTION_KEY_PRESSED)
    {
        int playerid = (int)p->playerIndex;
        int key = (int)p->data[2];
        invoke_OnKeyPressed(playerid, key);
        return 0xFF;
    }
    return ret;
}

static unsigned long g_FUNC_GetPacketID = 0;

static void PreHooking(void)
{
    /* Паттерны из chandling-svr (Whitetiger SAMP_AC_v2) */
    g_FUNC_GetPacketID = FindPattern("\x55\xB8\x00\x00\x00\x00\x89\xE5\x8B\x55\x00\x85\xD2", "xx????xxxx?xx");
    if (!g_FUNC_GetPacketID)
        g_FUNC_GetPacketID = FindPattern("\xE9\x00\x00\x00\x00\x00\x89\xE5\x8B\x55\x00\x85\xD2\x74\x00\x8B\x52\x00\x0F", "x?????xxxx?xxx?xx?x");
}

static bool InstallGetPacketIDHook(void)
{
    if (!g_FUNC_GetPacketID)
        return false;
    long page_size = sysconf(_SC_PAGESIZE);
    void *page = (void*)(g_FUNC_GetPacketID & ~(page_size - 1));
    if (mprotect(page, (size_t)page_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
        return false;
    g_trampoline = mmap(0, 32, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (g_trampoline == MAP_FAILED)
        return false;
    unsigned char *t = (unsigned char *)g_trampoline;
    unsigned char *src = (unsigned char *)g_FUNC_GetPacketID;
    memcpy(t, src, GETPACKETID_HOOK_LEN);
    t += GETPACKETID_HOOK_LEN;
    *t++ = 0xE9;
    *(int *)t = (g_FUNC_GetPacketID + GETPACKETID_HOOK_LEN) - ((unsigned long)(t + 4));
    g_original_GetPacketID = (GetPacketID_t)g_trampoline;
    t = (unsigned char *)g_FUNC_GetPacketID;
    *t++ = 0xB8;
    *(unsigned long *)t = (unsigned long)hooked_GetPacketID;
    t += 4;
    *t++ = 0xFF;
    *t++ = 0xE0;
    return true;
}

static void UninstallGetPacketIDHook(void)
{
    if (!g_FUNC_GetPacketID || !g_trampoline)
        return;
    long page_size = sysconf(_SC_PAGESIZE);
    void *page = (void*)(g_FUNC_GetPacketID & ~(page_size - 1));
    mprotect(page, (size_t)page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
    unsigned char *t = (unsigned char *)g_FUNC_GetPacketID;
    memcpy(t, g_trampoline, GETPACKETID_HOOK_LEN);
    munmap(g_trampoline, 32);
    g_trampoline = 0;
    g_original_GetPacketID = 0;
}

static cell AMX_NATIVE_CALL n_SetChatStatus(AMX *amx, cell *params)
{
    int playerid = (int)params[1];
    bool status = params[2] != 0;

    ensure_rak_server();
    if (!pRakServer)
        return 0;

    PlayerID pid = pRakServer->GetPlayerIDFromIndex(playerid);
    if (pid.binaryAddress == 0xFFFFFFFF && pid.port == 0xFFFF)
        return 0;

    RakNet::BitStream bs;
    bs.Write((unsigned char)ID_CHATHIDER);
    bs.Write((unsigned char)ACTION_SET_CHAT_STATUS);
    bs.Write((unsigned char)(status ? 1 : 0));

    bool ok = pRakServer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, pid, false);
    return ok ? 1 : 0;
}

static AMX_NATIVE_INFO natives[] = {
    {"SetChatStatus", n_SetChatStatus},
    {0, 0}
};

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports()
{
    return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppData)
{
    ppPluginData = ppData;
    pAMXFunctions = ppData[PLUGIN_DATA_AMX_EXPORTS];
    PreHooking();
    ensure_rak_server();
    return true;
}

PLUGIN_EXPORT void PLUGIN_CALL Unload()
{
    UninstallGetPacketIDHook();
}

typedef int (*amx_Register_t)(AMX *amx, const AMX_NATIVE_INFO *nativelist, int number);

PLUGIN_EXPORT int PLUGIN_CALL AmxLoad(AMX *amx)
{
    ensure_rak_server();
    InstallGetPacketIDHook();
    amx_Register_t amx_Register = (amx_Register_t)((void **)pAMXFunctions)[PLUGIN_AMX_EXPORT_Register];
    return amx_Register(amx, natives, -1);
}

PLUGIN_EXPORT int PLUGIN_CALL AmxUnload(AMX *amx)
{
    (void)amx;
    return AMX_ERR_NONE;
}
