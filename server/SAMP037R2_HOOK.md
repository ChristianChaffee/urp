# Проверка бинарника samp03svr для хука GetRakServer

## Что сделать

1. Положи бинарник **samp03svr** (Linux 32-bit) в папку `server/` или укажи полный путь к нему.

2. В WSL выполни:
   ```bash
   cd /mnt/f/urp/server   # или путь, где лежит серверный проект
   python3 find_getrakserver.py samp03svr
   ```
   Если бинарник в другом месте:
   ```bash
   python3 find_getrakserver.py /path/to/samp03svr
   ```

3. Результат:
   - **Pattern found** — паттерн KeyListener подходит, можно включить хук:
     - при запуске сервера: `export CHATHIDER_HOOK_GETRAK=1`
     - затем как обычно запускать samp03svr.
   - **Pattern NOT found** — для этой сборки 0.3.7 R2 нужен другой паттерн. Тогда пришли дизассемблированный файл:
     ```bash
     objdump -d samp03svr > disasm.txt
     ```
     Пришли начало `disasm.txt` (первые 50–100 KB) или весь файл — подберу паттерн по коду GetRakServer.

## Откуда взять samp03svr

Обычно это файл из архива сервера SA-MP 0.3.7 (Linux). Имя может быть `samp03svr` или с суффиксом. Важно: именно **тот бинарник**, который реально запускается на сервере (тот же билд, что и при «зависании»).
