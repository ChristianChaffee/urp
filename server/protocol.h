/* chathider protocol — packet IDs and actions */

#pragma once

/* Server->client: скрыть/показать чат */
enum ChathiderPacketID : unsigned char {
    ID_CHATHIDER = 250
};

/* Action bytes for ID_CHATHIDER (server->client) */
enum ChathiderAction : unsigned char {
    ACTION_SET_CHAT_STATUS = 1,
};

/* Client->server: нажатие клавиши. ID 251 как в chandling — обходим "Packet was modified" через GetPacketID hook */
#define ID_KEY_PRESSED 251   /* пакет [251, key] — 2 байта */

/* RPC ID для client->server (OnKeyPressed). Вне диапазона SA-MP (до ~177). */
#define CHATHIDER_RPC_KEY_PRESSED 220
