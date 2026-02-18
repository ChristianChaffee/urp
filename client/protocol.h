/* chathider protocol — синхронно с server/protocol.h */

#pragma once

enum ChathiderPacketID : unsigned char {
    ID_CHATHIDER = 250
};

enum ChathiderAction : unsigned char {
    ACTION_SET_CHAT_STATUS = 1,
    ACTION_KEY_PRESSED = 2
};

#define CHATHIDER_RPC_KEY_PRESSED 220
