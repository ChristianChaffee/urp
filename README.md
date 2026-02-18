# chathider

## Клиент (chathider.asi)

ASI-плагин для SA-MP 0.3.7 R3. При входе на сервер выводит в чат: `chathider.asi loaded`

**Сборка** (MSYS2 MinGW32):
```bash
./build.sh
```

**Установка:** скопируй `chathider.asi` в папку GTA SA. Нужен ASI loader (CLEO, Mod Loader).

---

## Сервер (chathider.so)

Плагин для SA-MP 0.3.7 R2 Linux. Native: `SetChatStatus(playerid, bool:status)` — пока пустая.

**Сборка** (Linux, 32-bit):
```bash
cd server && ./build.sh
```

**Установка:** скопируй `chathider.so` в `plugins/`, в server.cfg: `plugins chathider`

**Pawn:** подключи `#include <chathider>`
