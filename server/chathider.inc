/**
 * chathider.inc
 * Native: SetChatStatus(playerid, bool:status)
 * Callback: OnUserChangeLayout(playerid, layout[]) — при смене раскладки клавиатуры на клиенте (layout = "RU", "EN" и т.д.)
 * Callback: OnPlayerEnterAFK(playerid) — игрок в меню паузы или свернул игру
 * Callback: OnPlayerExitAFK(playerid) — игрок вышел из паузы или развернул игру
 */

#if defined _chathider_included
  #endinput
#endif
#define _chathider_included

native SetChatStatus(playerid, bool:status);
native QuitGameForPlayer(playerid);
native GetUserLayout(playerid, string_return[], size = sizeof(string_return));
native IsPlayerAFK(playerid);

forward OnUserChangeLayout(playerid, layout[]);
forward OnPlayerEnterAFK(playerid);
forward OnPlayerExitAFK(playerid);
