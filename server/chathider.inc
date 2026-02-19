/**
 * chathider.inc
 * Native: SetChatStatus(playerid, bool:status)
 * Callback: OnUserChangeLayout(playerid, layout[]) — при смене раскладки клавиатуры на клиенте (layout = "RU", "EN" и т.д.)
 */

#if defined _chathider_included
  #endinput
#endif
#define _chathider_included

native SetChatStatus(playerid, bool:status);

forward OnUserChangeLayout(playerid, layout[]);
