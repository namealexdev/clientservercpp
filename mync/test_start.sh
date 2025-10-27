#!/bin/bash

# --- КОНФИГУРАЦИЯ ---
# Количество клиентов (по умолчанию 10, можно передать как аргумент)
count=${1:-10}
PROGRAM_CMD="./mync localhost 5202"
# Директория для логов
LOG_DIR="./mync_logs"
# --- /КОНФИГУРАЦИЯ ---

# Создаем директорию для логов если не существует
mkdir -p "$LOG_DIR"

# Файл для отслеживания состояния
STATE_FILE="/tmp/mync_clients_$$.state"
echo "RUNNING" > "$STATE_FILE"

echo "Начало: Запуск $count клиентов..."

# Создаем именованные каналы
PIPES=()
for i in $(seq 1 $count); do
    PIPE_NAME="/tmp/pipe_$i.$$"
    rm -f "$PIPE_NAME" 2>/dev/null
    mkfifo "$PIPE_NAME"
    PIPES+=("$PIPE_NAME")
done

# Запускаем клиентов
CLIENT_PIDS=()
for i in $(seq 1 $count); do
    PIPE_NAME="/tmp/pipe_$i.$$"
    
    # Создаем лог файл с именем client_<PID>.log (пока неизвестен PID, создадим временное имя)
    TEMP_LOG_FILE="$LOG_DIR/client_$$_$i.tmp"
    
    # Запускаем клиента в фоне с перенаправлением ввода из канала и вывода в лог
    $PROGRAM_CMD < "$PIPE_NAME" > "$TEMP_LOG_FILE" 2>&1 &
    CLIENT_PID=$!
    
    # Сохраняем PID
    CLIENT_PIDS+=($CLIENT_PID)
    
    # Переименовываем лог файл с использованием PID клиента
    FINAL_LOG_FILE="$LOG_DIR/client_$CLIENT_PID.log"
    mv "$TEMP_LOG_FILE" "$FINAL_LOG_FILE" 2>/dev/null
    
    echo "Клиент $i запущен (PID: $CLIENT_PID, лог: $FINAL_LOG_FILE)"
done

# Даем время клиентам запуститься
sleep 2

# Запускаем источник данных в отдельной подсессии
(
    trap "exit" TERM
    while [ -f "$STATE_FILE" ]; do
        # Генерируем данные небольшими порциями
        dd if=/dev/urandom bs=64K count=1 2>/dev/null
    done
) | tee "${PIPES[@]}" > /dev/null &
SOURCE_PID=$!

echo "Источник данных запущен (PID: $SOURCE_PID)"
echo "=================================================="
echo "Все $count клиентов работают"
echo "PID основного скрипта: $$"
echo "Логи сохраняются в: $LOG_DIR/client_<PID>.log"
echo "Для остановки выполните: kill $$"
echo "=================================================="

# Функция очистки
cleanup() {
    echo "Начинаем остановку..."
    
    # Помечаем что нужно остановиться
    rm -f "$STATE_FILE" 2>/dev/null
    
    # Даем команду на остановку источника
    if kill -0 $SOURCE_PID 2>/dev/null; then
        kill -TERM $SOURCE_PID 2>/dev/null
        sleep 1
        # Принудительно если нужно
        kill -KILL $SOURCE_PID 2>/dev/null 2>/dev/null
    fi
    
    # Останавливаем клиентов
    for pid in "${CLIENT_PIDS[@]}"; do
        if kill -0 $pid 2>/dev/null; then
            kill -TERM $pid 2>/dev/null
            sleep 0.5
            kill -KILL $pid 2>/dev/null 2>/dev/null
        fi
    done
    
    # Удаляем каналы
    for pipe in "${PIPES[@]}"; do
        rm -f "$pipe" 2>/dev/null
    done
    
    # Удаляем временные лог файлы
    rm -f "$LOG_DIR"/*.tmp 2>/dev/null/mync_logs
    
    echo "Все процессы остановлены"
    exit 0
}

# Регистрируем обработчики
trap cleanup EXIT INT TERM

# Ждем сигнала остановки
while [ -f "$STATE_FILE" ]; do
    sleep 1
done