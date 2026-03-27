#!/bin/bash

# Проверка, что мы находимся в git-репозитории
if ! git rev-parse --is-inside-work-tree > /dev/null 2>&1; then
    echo "Ошибка: текущая папка не является git-репозиторием."
    exit 1
fi

# Добавляем все изменения
git add .

# Создаём коммит с сообщением "w man"
git commit -m "w man"

# Получаем изменения из удалённого репозитория и сливаем
git pull origin main

# Если pull прошёл успешно, отправляем изменения
if [ $? -eq 0 ]; then
    git push origin main
else
    echo "Ошибка при pull. Push не выполнен."
    exit 1
fi