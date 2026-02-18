/*
 * Хук RakClient::Receive для приёма пакетов chathider (ID 250).
 * По аналогии с chandling — pattern scan + detour.
 */

#define WIN32_LEAN_AND_MEAN
#include "packet_hook.h"
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <d3d9.h>
#include <d3dx9.h>
#include "../SAMP-API/include/sampapi/sampapi.h"
#include "../SAMP-API/include/sampapi/0.3.7-R3-1/CChat.h"
#include "../SAMP-API/include/sampapi/0.3.7-R3-1/CNetGame.h"
#include "../server/lib/raknet/BitStream.h"
#include "../server/lib/raknet/raknet.h"
#include "protocol.h"

using namespace sampapi::v037r3;

/* RakNet Packet — минимальная структура (data, length) */
#pragma pack(push, 1)
struct SampPacket {
    unsigned short playerIndex;
    unsigned int   binaryAddress;
    unsigned short port;
    unsigned int   length;
    unsigned int   bitSize;
    unsigned char* data;
    unsigned char  deleteData;
};
#pragma pack(pop)

typedef SampPacket* (__thiscall* tReceive)(void* rakClient);

static tReceive g_OriginalReceive = nullptr;
static void* g_TrampolineMem = nullptr;
static bool g_HookInstalled = false;

/* Видимость чата от сервера: -1 = нет (клиент свободен), 0 = скрыть, 1 = показать */
static volatile long g_ChatVisibleRequested = -1;
static CRITICAL_SECTION g_ChatVisibleCS;

/* CChat, CInput — R3-1 offsets из SAMP-API */
#define SAMP_R3_RENDER_OFFSET     0x671C0  /* CChat::Render — только чат */
#define SAMP_R3_SWITCHMODE_OFFSET 0x60B50
#define SAMP_R3_GETMODE_OFFSET    0x60B40
#define SAMP_R3_INPUT_OPEN_OFFSET 0x68D10  /* CInput::Open — T и F6 вызывают это */

typedef void (__thiscall* tChatRender)(void* chat);
typedef void (__thiscall* tChatSwitchMode)(void* chat);
typedef int  (__thiscall* tChatGetMode)(void* chat);
typedef void (__thiscall* tInputOpen)(void* input);

static tChatRender g_OriginalRender = nullptr;
static tChatSwitchMode g_OriginalSwitchMode = nullptr;
static tChatGetMode g_OriginalGetMode = nullptr;
static tInputOpen g_OriginalInputOpen = nullptr;
static void* g_RenderTrampolineMem = nullptr;
static void* g_SwitchModeTrampolineMem = nullptr;
static void* g_GetModeTrampolineMem = nullptr;
static void* g_InputOpenTrampolineMem = nullptr;

static bool mem_match(const unsigned char* data, const unsigned char* pattern, const char* mask)
{
    for (; *mask; ++mask, ++data, ++pattern)
        if (*mask == 'x' && *data != *pattern)
            return false;
    return true;
}

static DWORD find_pattern(DWORD base, DWORD size, const unsigned char* pattern, const char* mask)
{
    for (DWORD i = 0; i < size; ++i)
        if (mem_match((unsigned char*)(base + i), pattern, mask))
            return base + i;
    return 0;
}

/* Вызывается вместо RakClient::Receive. Обрабатываем ID_CHATHIDER и «съедаем» пакет. */
static SampPacket* __fastcall hooked_receive(void* rakClient)
{
    /* g_OriginalReceive указывает на трамплин (оригинальные 5 байт + JMP) */
    SampPacket* pkt = g_OriginalReceive(rakClient);

    if (pkt && pkt->data && pkt->length >= 3 &&
        pkt->data[0] == ID_CHATHIDER &&
        pkt->data[1] == ACTION_SET_CHAT_STATUS)
    {
        unsigned char status = pkt->data[2];
        EnterCriticalSection(&g_ChatVisibleCS);
        g_ChatVisibleRequested = status ? 1 : 0;
        LeaveCriticalSection(&g_ChatVisibleCS);
        if (!status)
        {
            CChat* pChat = RefChat();
            if (pChat) pChat->m_bRedraw = TRUE; /* форсируем вызов Render в следующем кадре */
        }
        return nullptr; /* съедаем пакет, не передаём в SAMP */
    }
    return pkt;
}

/* Пусто — видимость применяется через хуки Draw/SwitchMode */
void flush_pending_messages()
{
    (void)0;
}

/* Очистить текстуру чата — чтобы скрыть мгновенно (иначе кэш держится до нового сообщения) */
static void clear_chat_texture(CChat* pChat)
{
    if (!pChat || !pChat->m_bRenderToSurface || !pChat->m_pRenderToSurface ||
        !pChat->m_pSurface || !pChat->m_pDevice)
        return;
    if (SUCCEEDED(pChat->m_pRenderToSurface->BeginScene(pChat->m_pSurface, NULL)))
    {
        pChat->m_pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
        pChat->m_pRenderToSurface->EndScene(D3DX_FILTER_NONE);
    }
}

/* Хук CChat::Render — рисует только чат; Draw (радар и др.) вызывается отдельно */
static void __fastcall hooked_chat_render(void* chat)
{
    long req;
    EnterCriticalSection(&g_ChatVisibleCS);
    req = g_ChatVisibleRequested;
    LeaveCriticalSection(&g_ChatVisibleCS);
    if (req == 0)
    {
        /* Скрыть — очищаем текстуру и не рисуем (иначе кэш показывается до нового сообщения) */
        clear_chat_texture((CChat*)chat);
        return;
    }
    g_OriginalRender(chat);
}

/* Хук CChat::GetMode — при скрытом чате возвращаем OFF, чтобы F7/другие пути не показывали чат */
static int __fastcall hooked_chat_getmode(void* chat)
{
    long req;
    EnterCriticalSection(&g_ChatVisibleCS);
    req = g_ChatVisibleRequested;
    LeaveCriticalSection(&g_ChatVisibleCS);
    if (req == 0) return 0; /* DISPLAY_MODE_OFF — чат скрыт сервером */
    return g_OriginalGetMode(chat);
}

/* Хук CChat::SwitchMode — блокируем F7, когда сервер скрыл чат */
static void __fastcall hooked_chat_switchmode(void* chat)
{
    long req;
    EnterCriticalSection(&g_ChatVisibleCS);
    req = g_ChatVisibleRequested;
    LeaveCriticalSection(&g_ChatVisibleCS);
    if (req == 0) return; /* скрыто сервером — F7 не должен показывать */
    g_OriginalSwitchMode(chat);
}

/* Хук CInput::Open — блокируем T и F6 при скрытом чате (строка ввода не откроется) */
static void __fastcall hooked_input_open(void* input)
{
    long req;
    EnterCriticalSection(&g_ChatVisibleCS);
    req = g_ChatVisibleRequested;
    LeaveCriticalSection(&g_ChatVisibleCS);
    if (req == 0) return; /* чат скрыт — T/F6 не откроют строку ввода */
    g_OriginalInputOpen(input);
}

#if CHATHIDER_KEYLOG_ENABLED
/* Логирование клавиш через WH_KEYBOARD_LL — не трогает код игры, только когда игрок заспавнен */
static HMODULE g_hKeylogModule = nullptr;
#endif

void set_keylog_module(void* hMod)
{
#if CHATHIDER_KEYLOG_ENABLED
    g_hKeylogModule = (HMODULE)hMod;
#else
    (void)hMod;
#endif
}

#if CHATHIDER_KEYLOG_ENABLED

/* RakClient Send — сырой пакет [ID_CHATHIDER, ACTION_KEY_PRESSED, key], как в chandling (сервер хукает Receive) */
static void send_key_to_server(unsigned int vkCode)
{
    CNetGame* net = RefNetGame();
    if (!net || net->GetState() != CNetGame::GAME_MODE_CONNECTED)
        return;
    void* rak = net->GetRakClient();
    if (!rak) return;

    RakNet::BitStream bs;
    bs.Write((unsigned char)ID_CHATHIDER);
    bs.Write((unsigned char)ACTION_KEY_PRESSED);
    bs.Write((unsigned char)(vkCode & 0xFF));

    typedef bool (__thiscall *SendFn)(void* pThis, void* bitStream, int prio, int reli, int channel);
    void** vtable = *(void***)rak;
    SendFn pSend = (SendFn)vtable[6];
    if (pSend)
        pSend(rak, &bs, 1, 4, 0);
}

static LRESULT CALLBACK lowlevel_keyboard_proc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && wParam == WM_KEYUP)
    {
        CNetGame* net = RefNetGame();
        if (net && net->GetState() == CNetGame::GAME_MODE_CONNECTED)
        {
            KBDLLHOOKSTRUCT* kbs = (KBDLLHOOKSTRUCT*)lParam;
            send_key_to_server((unsigned int)kbs->vkCode);
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static DWORD WINAPI keylog_thread(LPVOID)
{
    if (!g_hKeylogModule)
        g_hKeylogModule = GetModuleHandleA("chathider.asi");
    HHOOK hHook = SetWindowsHookExA(WH_KEYBOARD_LL, lowlevel_keyboard_proc, g_hKeylogModule, 0);
    if (!hHook) return 1;
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
        DispatchMessage(&msg);
    UnhookWindowsHookEx(hHook);
    return 0;
}

static void start_keylog_if_enabled()
{
#if CHATHIDER_KEYLOG_ENABLED
    CreateThread(NULL, 0, keylog_thread, NULL, 0, NULL);
#endif
}
#endif

static bool install_single_hook(DWORD target_addr, void* trampoline_mem, void* handler, void** orig_ptr, int trampoline_size)
{
    void* mem = VirtualAlloc(nullptr, (SIZE_T)trampoline_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) return false;

    unsigned char* tramp = (unsigned char*)mem;
    const int hook_len = 6;
    memcpy(tramp, (void*)target_addr, hook_len);
    tramp[hook_len] = 0xE9;
    *(DWORD*)(tramp + hook_len + 1) = (target_addr + hook_len) - ((DWORD)tramp + hook_len + 5);

    DWORD old;
    if (!VirtualProtect((void*)target_addr, hook_len, PAGE_EXECUTE_READWRITE, &old))
        return false;
    *(unsigned char*)target_addr = 0xE9;
    *(DWORD*)(target_addr + 1) = (DWORD)handler - (target_addr + 5);
    VirtualProtect((void*)target_addr, hook_len, old, &old);

    *(void**)orig_ptr = mem;
    *(void**)trampoline_mem = mem;
    return true;
}

bool install_receive_hook()
{
    if (g_HookInstalled) return true;

    InitializeCriticalSection(&g_ChatVisibleCS);

    HMODULE hSamp = GetModuleHandleA("samp.dll");
    if (!hSamp) return false;

    DWORD base = (DWORD)hSamp;
    MODULEINFO mi = {0};
    if (!GetModuleInformation(GetCurrentProcess(), hSamp, &mi, sizeof(mi)))
        return false;

    /* Pattern для RakClient::Receive — точно как в chandling Hooks.cpp */
    const unsigned char pattern[] = {0x6A, 0x04, 0x8B, 0xCE, 0xC7, 0x44, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00};
    const char mask[] = "xxxxxxx?????";  /* 7 фикс + 5 wildcard */
    DWORD found = find_pattern(base, mi.SizeOfImage, pattern, mask);
    if (!found) return false;

    DWORD receive_addr = found - 60; /* как в chandling */
    if (receive_addr < base || receive_addr >= base + mi.SizeOfImage)
        return false; /* адрес вне модуля */

    /* Копируем 6 байт — типичный пролог push ebp; mov ebp,esp; sub esp,xx — 1+2+3=6 */
    const int hook_len = 6;

    g_TrampolineMem = VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_TrampolineMem) return false;

    unsigned char* tramp = (unsigned char*)g_TrampolineMem;
    memcpy(tramp, (void*)receive_addr, hook_len);
    tramp[hook_len] = 0xE9; /* JMP rel32 */
    *(DWORD*)(tramp + hook_len + 1) = (receive_addr + hook_len) - ((DWORD)tramp + hook_len + 5);

    DWORD old;
    if (!VirtualProtect((void*)receive_addr, hook_len, PAGE_EXECUTE_READWRITE, &old))
        return false;

    *(unsigned char*)receive_addr = 0xE9;
    *(DWORD*)(receive_addr + 1) = (DWORD)hooked_receive - (receive_addr + 5);
    VirtualProtect((void*)receive_addr, hook_len, old, &old);

    g_OriginalReceive = (tReceive)g_TrampolineMem;

    /* Хук CChat::Render — скрываем только чат (радар рисуется в Draw, не в Render) */
    DWORD render_addr = base + SAMP_R3_RENDER_OFFSET;
    if (!install_single_hook(render_addr, &g_RenderTrampolineMem, (void*)hooked_chat_render, (void**)&g_OriginalRender, 32))
        return false;

    /* Хук CChat::GetMode — возвращаем OFF при скрытом чате (F7 и др. не откроют) */
    DWORD getmode_addr = base + SAMP_R3_GETMODE_OFFSET;
    if (!install_single_hook(getmode_addr, &g_GetModeTrampolineMem, (void*)hooked_chat_getmode, (void**)&g_OriginalGetMode, 32))
        return false;

    /* Хук CChat::SwitchMode — блокируем F7 при скрытом чате */
    DWORD switch_addr = base + SAMP_R3_SWITCHMODE_OFFSET;
    if (!install_single_hook(switch_addr, &g_SwitchModeTrampolineMem, (void*)hooked_chat_switchmode, (void**)&g_OriginalSwitchMode, 32))
        return false;

    /* Хук CInput::Open — блокируем T и F6 (строка ввода) при скрытом чате */
    DWORD input_open_addr = base + SAMP_R3_INPUT_OPEN_OFFSET;
    if (!install_single_hook(input_open_addr, &g_InputOpenTrampolineMem, (void*)hooked_input_open, (void**)&g_OriginalInputOpen, 32))
        return false;

#if CHATHIDER_KEYLOG_ENABLED
    start_keylog_if_enabled();
#endif

    g_HookInstalled = true;
    return true;
}
