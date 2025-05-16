#pragma once

enum class Response
{
    OK, // Ход выполнен успешно
    BACK, // Откатить ход назад
    REPLAY, // Перезапустить игру
    QUIT, // Выйти из игры
    CELL // Выбранная клетка
};
