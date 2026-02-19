# Сборка chathider.so под старый glibc в WSL (без Docker)

Запускай в **основном** Ubuntu в WSL (тот, где у тебя работают apt и g++), не в Ubuntu-20.04.

## 1. Поставить debootstrap и создать chroot с Ubuntu 20.04

```bash
sudo apt-get update
sudo apt-get install -y debootstrap

sudo debootstrap focal /opt/ubuntu2004 http://archive.ubuntu.com/ubuntu
```

Подожди 2–5 минут, пока скачается и распакуется база.

## 2. Примонтировать проект в chroot

```bash
sudo mkdir -p /opt/ubuntu2004/mnt/urp-server
sudo mount --bind /mnt/f/urp/server /opt/ubuntu2004/mnt/urp-server
```

## 3. Зайти в chroot и поставить компилятор

```bash
sudo chroot /opt/ubuntu2004
```

Внутри chroot (приглашение будет что-то вроде `root@...:/#`):

```bash
apt-get update
apt-get install -y g++-multilib
```

## 4. Собрать плагин

Всё ещё внутри chroot:

```bash
cd /mnt/urp-server
./build.sh
```

Проверь, что появился файл:

```bash
ls -la chathider.so
```

## 5. Выйти и отмонтировать

Выйти из chroot:
```bash
exit
```

В обычном WSL:
```bash
sudo umount /opt/ubuntu2004/mnt/urp-server
```

Готовый `chathider.so` лежит в `F:\urp\server\` (собран под glibc 2.31).

---

Повторные сборки: шаги 2–4 (mount → chroot → build), затем exit и umount. Chroot в `/opt/ubuntu2004` создаётся один раз.
