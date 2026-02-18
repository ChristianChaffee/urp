/*
 * chathider.asi — ASI-плагин для SA-MP 0.3.7 R3
 * Принимает RPC SetChatStatus от сервера — скрывает/показывает чат (только чат, не радар)
 *
 * Основа: SAMP-API (BlastHackNet/SAMP-API)
 * https://github.com/BlastHackNet/SAMP-API
 */

#include <windows.h>
#include "../SAMP-API/include/sampapi/sampapi.h"
#include "../SAMP-API/include/sampapi/0.3.7-R3-1/CNetGame.h"
#include "packet_hook.h"

using namespace sampapi::v037r3;

static bool g_PacketHookInstalled = false;

static DWORD WINAPI InitThread(LPVOID)
{
    // Ждём загрузки samp.dll (до ~2 минут)
    for (int i = 0; i < 240; i++)
    {
        Sleep(500);
        if (!GetModuleHandleA("samp.dll")) continue;

        // Ждём инициализации SAMP
        Sleep(2000);

        // Опрашиваем состояние постоянно
        while (1)
        {
            Sleep(300);

            CNetGame* netGame = RefNetGame();
            if (!netGame)
                continue;

            int state = netGame->GetState();
            // CONNECTED (5) = подключён, WAITJOIN (6) = выбор класса / спавн
            if (state == CNetGame::GAME_MODE_CONNECTED ||
                state == CNetGame::GAME_MODE_WAITJOIN)
            {
                if (!g_PacketHookInstalled)
                {
                    install_receive_hook();
                    g_PacketHookInstalled = true;
                }
                flush_pending_messages();  /* применить SetChatStatus от сервера */
            }
        }
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        set_keylog_module(hModule);
        CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
    }
    return TRUE;
}
