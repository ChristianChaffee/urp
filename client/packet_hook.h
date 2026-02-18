#pragma once

#define CHATHIDER_KEYLOG_ENABLED 1  /* 1 = логирование клавиш в чат (только когда заспавнен) */

bool install_receive_hook();
void flush_pending_messages();

void set_keylog_module(void* hMod); /* HMODULE — для WH_KEYBOARD_LL, пустая при KEYLOG=0 */
