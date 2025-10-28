#!/bin/bash

# Упрощенный скрипт
count=${1:-10}
CMD="./mync localhost 5201"
PID_FILE="/tmp/mync_$$.pid"

if [ "$2" = "stop" ]; then
    echo "Остановка клиентов..."
    if [ -f "$PID_FILE" ]; then
        kill $(cat "$PID_FILE") 2>/dev/null
        rm -f "$PID_FILE"
    fi
    echo "Готово"
else
    echo "Запуск $count клиентов..."
    for i in $(seq 1 $count); do
        $CMD > /dev/null 2>&1 &
        echo $! >> "$PID_FILE"
    done
    echo "Готово. Для остановки: $0 $1 stop"
fi