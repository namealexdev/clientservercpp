#!/bin/bash
# stop_all_mync.sh - Принудительная остановка ВСЕХ процессов mync

echo "Останавливаем все процессы mync..."

# Находим и убиваем все процессы mync
pids=$(pgrep -f "mync localhost")
if [ -n "$pids" ]; then
    echo "Найдены процессы: $pids"
    kill -TERM $pids 2>/dev/null
    sleep 2
    # Принудительная остановка оставшихся
    pids=$(pgrep -f "mync localhost")
    if [ -n "$pids" ]; then
        echo "Принудительная остановка: $pids"
        kill -KILL $pids 2>/dev/null
    fi
fi

# Удаляем все временные каналы
echo "Удаляем временные файлы..."
rm -f /tmp/pipe_*.* 2>/dev/null
rm -f /tmp/mync_clients_*.pid 2>/dev/null
rm -f /tmp/mync_clients_*.state 2>/dev/null

echo "Очистка завершена"