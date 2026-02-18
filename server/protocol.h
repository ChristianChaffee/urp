/* chathider protocol — packet IDs and actions (like chandling) */

#pragma once

/* Custom packet ID — not used by SA-MP */
enum ChathiderPacketID : unsigned char {
    ID_CHATHIDER = 250
};

/* Action bytes for ID_CHATHIDER */
enum ChathiderAction : unsigned char {
    ACTION_SET_CHAT_STATUS = 1,
    ACTION_KEY_PRESSED = 2      /* client->server: [ID_CHATHIDER, ACTION_KEY_PRESSED, key] */
};

/* RPC ID для client->server (OnKeyPressed). Вне диапазона SA-MP (до ~177). */
#define CHATHIDER_RPC_KEY_PRESSED 220
