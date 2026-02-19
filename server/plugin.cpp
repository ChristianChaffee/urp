/*
 * chathider.so — как KeyListener (https://github.com/CyberMor/keylistener)
 * Клиент: пакет [244, key] через BitStream, Send(MEDIUM_PRIORITY, RELIABLE_ORDERED).
 * Сервер: хук GetRakServer — в момент первого вызова патчим vtable Receive (слот 11),
 *         в Receive съедаем пакеты 244. Слоты под Linux: 11, 13.
 * 
 * Переписано на основе samp-ptl (https://github.com/katursis/samp-ptl)
 */

#include "ptl.h"
#include "protocol.h"
#include "lib/raknet/raknet.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <memory>

using namespace ptl;

/* KeyListener Linux vtable offsets */
#define RAKSERVER_RECEIVE_INDEX      11
#define RAKSERVER_DEALLOCATE_INDEX  13

typedef Packet* (*ReceiveFn)(CCRakServer*);
typedef void (*DeallocatePacketFn)(CCRakServer*, Packet*);
static ReceiveFn g_original_Receive = 0;
static DeallocatePacketFn g_original_DeallocatePacket = 0;

typedef void* (*GetRakServerFn)(void);
#define GETRAKSERVER_HOOK_LEN 5
static unsigned char g_orig_getrakserver_bytes[GETRAKSERVER_HOOK_LEN];
static unsigned long g_GetRakServer_addr = 0;
static int g_hook_installed = 0;

/* GetPacketID hook */
#define GETPACKETID_HOOK_LEN 6
static unsigned long g_GetPacketID_addr = 0;
static unsigned char g_orig_getpacketid_bytes[GETPACKETID_HOOK_LEN];
static void *g_getpacketid_trampoline = 0;
static int g_getpacketid_hook_installed = 0;

typedef unsigned char (*GetPacketID_t)(Packet*);

/* RPC */
#define RAKSERVER_REGISTER_RPC_INDEX 31
static int g_rpc_key_registered = 0;

static CCRakServer *pRakServer = 0;

/* Forward declarations */
static void* GetRakServerHook(void);
static Packet* hooked_Receive(CCRakServer* srv);
static unsigned char hooked_GetPacketID(Packet *p);
static void ChathiderRPCKeyPressed(RPCParameters *p);
static void invoke_OnKeyPressed(int playerid, int key);

class Script : public AbstractScript<Script> {
 public:
  std::shared_ptr<Public> onKeyPressed_;

  /* SA-MP: FILTERSCRIPT=0 для gamemode, 1 для filterscript. PTL ожидает переменную "true = gamemode". */
  const char *VarIsGamemode() { return "FILTERSCRIPT"; }

  bool IsGamemode() const {
    /* Инвертируем: в SA-MP FILTERSCRIPT=0 значит gamemode */
    return !AbstractScript<Script>::IsGamemode();
  }

  bool OnLoad() {
    /* Создаём Public для Chathider_OnKeyPressed только в gamemode */
    if (IsGamemode()) {
      onKeyPressed_ = MakePublic("Chathider_OnKeyPressed", true);
      if (onKeyPressed_ && onKeyPressed_->Exists()) {
        Log("Chathider_OnKeyPressed found in gamemode");
      } else {
        Log("Chathider_OnKeyPressed not found in gamemode");
      }
    }
    return true;
  }

  /* Native: SetChatStatus(playerid, bool:status) */
  cell n_SetChatStatus(int playerid, cell status) {
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

};

class Plugin : public AbstractPlugin<Plugin, Script> {
 public:
  const char *Name() { return "chathider"; }

  bool OnLoad() {
    void **ppData = plugin_data_;
    
    /* KeyListener: база основного бинарника по указателю на logprintf */
    unsigned long base = 0, size = 0;
    if (!get_module_info(ppData[PLUGIN_DATA_LOGPRINTF], &base, &size) || size == 0)
      return true;

    /* Поиск GetRakServer */
    static const char kGetRakServerPattern[] =
        "\x04\x24\xFF\xFF\xFF\xFF\x89\x75\xFF\x89\x5D\xFF\xE8\xFF\xFF\xFF\xFF\x89\x04\x24\x89"
        "\xC6\xE8\xFF\xFF\xFF\xFF\x89\xF0\x8B\x5D\xFF\x8B\x75\xFF\x89\xEC\x5D\xC3";
    static const char kGetRakServerMask[] =
        "xx????xx?xx?x????xxxxxx????xxxx?xx?xxxx";

    unsigned long found = FindPattern(base, size, kGetRakServerPattern, kGetRakServerMask);
    if (found == 0)
      return true;
    g_GetRakServer_addr = found - 7;

    if (*(unsigned char *)g_GetRakServer_addr != 0x55)
      g_GetRakServer_addr = 0;
    if (g_GetRakServer_addr == 0)
      return true;

    memcpy(g_orig_getrakserver_bytes, (void*)g_GetRakServer_addr, GETRAKSERVER_HOOK_LEN);

    long page_size = sysconf(_SC_PAGESIZE);
    void *page = (void*)(g_GetRakServer_addr & ~(page_size - 1));
    if (mprotect(page, (size_t)page_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
      return true;

    unsigned char *t = (unsigned char *)g_GetRakServer_addr;
    *t++ = 0xE9;
    *(int *)t = (unsigned long)GetRakServerHook - (g_GetRakServer_addr + 5);
    g_hook_installed = 1;

    /* GetPacketID */
    static const char kGetPacketIDPattern[] = "\x55\xB8\x00\x00\x00\x00\x89\xE5\x8B\x55\x00\x85\xD2";
    static const char kGetPacketIDMask[]   = "xx????xxxx?xx";
    unsigned long getpid_addr = FindPattern(base, size, kGetPacketIDPattern, kGetPacketIDMask);
    if (getpid_addr) {
      g_GetPacketID_addr = getpid_addr;
      memcpy(g_orig_getpacketid_bytes, (void*)g_GetPacketID_addr, GETPACKETID_HOOK_LEN);
    }

    /* Сразу патчим vtable */
    ensure_rak_server();
    try_patch_receive_vtable();

    this->RegisterNative<&Script::n_SetChatStatus>("SetChatStatus");

    return AbstractPlugin<Plugin, Script>::OnLoad();
  }


 private:
  static int memory_compare(const unsigned char *data, const unsigned char *pattern, const char *mask) {
    for (; *mask; ++mask, ++data, ++pattern)
      if (*mask == 'x' && *data != *pattern)
        return 0;
    return 1;
  }

  static int get_module_info(const void *address, unsigned long *out_base, unsigned long *out_size) {
    Dl_info info;
    if (dladdr(address, &info) == 0 || info.dli_fbase == 0)
      return 0;
    struct stat buf;
    if (stat(info.dli_fname, &buf) != 0 || buf.st_size <= 0)
      return 0;
    *out_base = (unsigned long)info.dli_fbase;
    *out_size = (unsigned long)buf.st_size;
    return 1;
  }

  static unsigned long FindPattern(unsigned long base, unsigned long size, const char *pattern, const char *mask) {
    size_t pattern_len = strlen(mask);
    if (pattern_len == 0 || size < pattern_len)
      return 0;
    for (unsigned long i = 0; i + pattern_len <= size; i++) {
      if (memory_compare((const unsigned char *)(base + i), (const unsigned char *)pattern, mask))
        return base + i;
    }
    return 0;
  }

 public:
  void ensure_rak_server(void) {
    if (pRakServer)
      return;
    void **ppData = plugin_data_;
    if (!ppData)
      return;
    GetRakServerFn pfn = (GetRakServerFn)ppData[PLUGIN_DATA_RAKSERVER];
    if (pfn)
      pRakServer = (CCRakServer *)pfn();
  }

  void try_patch_receive_vtable(void) {
    if (g_original_Receive != 0 || pRakServer == 0)
      return;
    void **vtable = *(void***)pRakServer;
    if (!vtable)
      return;
    g_original_Receive = (ReceiveFn)vtable[RAKSERVER_RECEIVE_INDEX];
    g_original_DeallocatePacket = (DeallocatePacketFn)vtable[RAKSERVER_DEALLOCATE_INDEX];
    if (!g_original_Receive || !g_original_DeallocatePacket)
      return;
    long page_size = sysconf(_SC_PAGESIZE);
    void* page = (void*)((uintptr_t)vtable & ~(page_size - 1));
    if (mprotect(page, (size_t)page_size, PROT_READ | PROT_WRITE) == 0)
      vtable[RAKSERVER_RECEIVE_INDEX] = (void*)hooked_Receive;
    register_rpc_key_pressed();
  }

  void register_rpc_key_pressed(void) {
    if (g_rpc_key_registered || !pRakServer)
      return;
    
    void **vtable = *(void***)pRakServer;
    if (!vtable)
      return;
    
    typedef void (*RegisterRPC_t)(void*, int*, void (*)(RPCParameters*));
    RegisterRPC_t reg = (RegisterRPC_t)vtable[RAKSERVER_REGISTER_RPC_INDEX];
    if (!reg)
      return;
    
    static int rpc_id = CHATHIDER_RPC_KEY_PRESSED;
    reg((void*)pRakServer, &rpc_id, ChathiderRPCKeyPressed);
    g_rpc_key_registered = 1;
  }

  void install_getpacketid_hook(void) {
    if (!g_GetPacketID_addr || g_getpacketid_hook_installed || g_getpacketid_trampoline)
      return;
    void *tr = mmap(0, 16, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (tr == MAP_FAILED)
      return;
    unsigned char *tp = (unsigned char *)tr;
    memcpy(tp, g_orig_getpacketid_bytes, GETPACKETID_HOOK_LEN);
    tp += GETPACKETID_HOOK_LEN;
    *tp++ = 0xE9;
    *(int *)tp = (g_GetPacketID_addr + GETPACKETID_HOOK_LEN) - ((unsigned long)tr + 11);
    if (mprotect(tr, 16, PROT_READ | PROT_EXEC) != 0) {
      munmap(tr, 16);
      return;
    }
    g_getpacketid_trampoline = tr;

    long page_size = sysconf(_SC_PAGESIZE);
    void *page = (void*)(g_GetPacketID_addr & ~(page_size - 1));
    if (mprotect(page, (size_t)page_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
      return;
    unsigned char *patch = (unsigned char *)g_GetPacketID_addr;
    *patch++ = 0xE9;
    *(int *)patch = (unsigned long)hooked_GetPacketID - (g_GetPacketID_addr + 5);
    patch += 4;
    *patch = 0x90;
    g_getpacketid_hook_installed = 1;
    Instance().Log("GetPacketID hook installed (packet %d -> 0xFF)", ID_KEY_PRESSED);
  }
};

/* Вызов колбэка Chathider_OnKeyPressed */
static void invoke_OnKeyPressed(int playerid, int key) {
  Plugin::EveryScript([playerid, key](const std::shared_ptr<Script> &script) {
    if (!script->IsGamemode())
      return true;
    if (!script->onKeyPressed_)
      script->onKeyPressed_ = script->MakePublic("Chathider_OnKeyPressed", true);
    if (script->onKeyPressed_ && script->onKeyPressed_->Exists())
      script->onKeyPressed_->Exec(playerid, key);
    return false;
  });
}

/* GetRakServer hook */
static void* GetRakServerHook(void) {
  if (pRakServer != 0)
    return pRakServer;

  long page_size = sysconf(_SC_PAGESIZE);
  if (g_GetRakServer_addr) {
    void* page = (void*)(g_GetRakServer_addr & ~(page_size - 1));
    if (mprotect(page, (size_t)page_size, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
      memcpy((void*)g_GetRakServer_addr, g_orig_getrakserver_bytes, GETRAKSERVER_HOOK_LEN);
      g_hook_installed = 0;
    }
  }
  void* rakserver = ((GetRakServerFn)g_GetRakServer_addr)();
  if (rakserver == 0)
    return 0;
  pRakServer = (CCRakServer*)rakserver;
  void** vtable = *(void***)rakserver;
  if (!vtable)
    return rakserver;
  g_original_Receive = (ReceiveFn)vtable[RAKSERVER_RECEIVE_INDEX];
  g_original_DeallocatePacket = (DeallocatePacketFn)vtable[RAKSERVER_DEALLOCATE_INDEX];
  if (!g_original_Receive || !g_original_DeallocatePacket)
    return rakserver;
  page_size = sysconf(_SC_PAGESIZE);
  void* page = (void*)((uintptr_t)vtable & ~(page_size - 1));
  if (mprotect(page, (size_t)page_size, PROT_READ | PROT_WRITE) == 0)
    vtable[RAKSERVER_RECEIVE_INDEX] = (void*)hooked_Receive;
  /* Регистрируем RPC и хук GetPacketID */
  Plugin::Instance().register_rpc_key_pressed();
  Plugin::Instance().install_getpacketid_hook();
  return rakserver;
}

/* Receive hook */
static Packet* hooked_Receive(CCRakServer* srv) {
  if (!g_original_Receive)
    return 0;
  
  Packet* packet = g_original_Receive(srv);
  while (packet != 0 && packet->data != 0 && packet->length >= 2) {
    unsigned char id = packet->data[0];
    if (id != (unsigned char)ID_KEY_PRESSED)
      return packet;
    int playerid = (int)packet->playerIndex;
    int key = (int)packet->data[1];
    invoke_OnKeyPressed(playerid, key);
    if (g_original_DeallocatePacket)
      g_original_DeallocatePacket(srv, packet);
    packet = g_original_Receive(srv);
  }
  return packet;
}

/* GetPacketID hook */
static unsigned char hooked_GetPacketID(Packet *p) {
  unsigned char ret;
  if (g_getpacketid_trampoline) {
    GetPacketID_t orig = (GetPacketID_t)g_getpacketid_trampoline;
    ret = orig(p);
  } else if (g_GetPacketID_addr) {
    GetPacketID_t orig = (GetPacketID_t)(g_GetPacketID_addr);
    ret = orig(p);
  } else {
    return 0;
  }
  if (!p || !p->data || p->length < 2)
    return ret;
  if (ret == (unsigned char)ID_KEY_PRESSED) {
    invoke_OnKeyPressed((int)p->playerIndex, (int)p->data[1]);
    return 0xFF;
  }
  return ret;
}

/* RPC handler */
static void ChathiderRPCKeyPressed(RPCParameters *p) {
  if (!pRakServer || !p || !p->input || p->numberOfBitsOfData < 8)
    return;
  int playerid = pRakServer->GetIndexFromPlayerID(p->sender);
  if (playerid < 0)
    return;
  invoke_OnKeyPressed(playerid, (int)p->input[0]);
}

/* Plugin exports */
PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports() {
  return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppData) {
  return Plugin::DoLoad(ppData);
}

PLUGIN_EXPORT void PLUGIN_CALL Unload() {
  if (g_getpacketid_hook_installed && g_GetPacketID_addr) {
    long page_size = sysconf(_SC_PAGESIZE);
    void *page = (void*)(g_GetPacketID_addr & ~(page_size - 1));
    if (mprotect(page, (size_t)page_size, PROT_READ | PROT_WRITE | PROT_EXEC) == 0)
      memcpy((void*)g_GetPacketID_addr, g_orig_getpacketid_bytes, GETPACKETID_HOOK_LEN);
    g_getpacketid_hook_installed = 0;
    if (g_getpacketid_trampoline) {
      munmap(g_getpacketid_trampoline, 16);
      g_getpacketid_trampoline = 0;
    }
  }
  if (g_hook_installed && g_GetRakServer_addr) {
    long page_size = sysconf(_SC_PAGESIZE);
    void *page = (void*)(g_GetRakServer_addr & ~(page_size - 1));
    mprotect(page, (size_t)page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
    memcpy((void*)g_GetRakServer_addr, g_orig_getrakserver_bytes, GETRAKSERVER_HOOK_LEN);
    g_hook_installed = 0;
  }
  pRakServer = 0;
  Plugin::DoUnload();
}

PLUGIN_EXPORT void PLUGIN_CALL AmxLoad(AMX *amx) {
  Plugin::DoAmxLoad(amx);
  /* После загрузки AMX регистрируем RPC и хук GetPacketID */
  Plugin::Instance().ensure_rak_server();
  Plugin::Instance().try_patch_receive_vtable();
  Plugin::Instance().register_rpc_key_pressed();
  Plugin::Instance().install_getpacketid_hook();
}

PLUGIN_EXPORT void PLUGIN_CALL AmxUnload(AMX *amx) {
  Plugin::DoAmxUnload(amx);
}
