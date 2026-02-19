/* chathider protocol — синхронно с server/protocol.h */

#pragma once

enum ChathiderPacketID : unsigned char {
    ID_CHATHIDER = 250
};

enum ChathiderAction : unsigned char {
    ACTION_SET_CHAT_STATUS = 1,
};

#define ID_KEY_PRESSED 251   /* client->server: пакет [251, key], как chandling */

#define CHATHIDER_RPC_KEY_PRESSED 220
