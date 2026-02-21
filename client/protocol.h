/* chathider protocol — синхронно с server/protocol.h */

#pragma once

enum ChathiderPacketID : unsigned char {
    ID_CHATHIDER = 250
};

enum ChathiderAction : unsigned char {
    ACTION_SET_CHAT_STATUS = 1,
    ACTION_QUIT_GAME = 2,
};

#define ID_KEY_PRESSED 251       /* client->server: пакет [251, key] */
#define ID_LAYOUT_CHANGED 253    /* client->server: пакет [253, layout0, layout1] */

#define CHATHIDER_RPC_KEY_PRESSED    220
#define CHATHIDER_RPC_LAYOUT_CHANGED 222
