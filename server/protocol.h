/* chathider protocol — packet IDs and actions */

#pragma once

/* Server->client: скрыть/показать чат */
enum ChathiderPacketID : unsigned char {
    ID_CHATHIDER = 250
};

/* Action bytes for ID_CHATHIDER (server->client) */
enum ChathiderAction : unsigned char {
    ACTION_SET_CHAT_STATUS = 1,
    ACTION_QUIT_GAME = 2,   /* закрыть игру у игрока (ExitProcess на клиенте) */
};

/* Client->server: нажатие клавиши. ID 251 как в chandling — обходим "Packet was modified" через GetPacketID hook */
#define ID_KEY_PRESSED 251       /* пакет [251, key] — 2 байта */
#define ID_LAYOUT_CHANGED 253    /* пакет [253, layout0, layout1] — 3 байта */
#define ID_AFK_STATE 254         /* пакет [254, state] — 2 байта: 1=enter AFK, 0=exit AFK */

/* RPC ID для client->server. Вне диапазона SA-MP (до ~177). */
#define CHATHIDER_RPC_KEY_PRESSED    220  /* BitStream: key */
#define CHATHIDER_RPC_LAYOUT_CHANGED 222  /* BitStream: layout0, layout1 */
#define CHATHIDER_RPC_AFK_STATE      223  /* BitStream: state (1=enter AFK, 0=exit AFK) */
