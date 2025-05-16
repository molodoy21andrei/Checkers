#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// Класс для обработки пользовательского ввода (мышь, окно)
class Hand
{
public:
  // Конструктор, принимающий указатель на игровую доску
  Hand(Board* board) : board(board)
  {
  }

  // Ожидает и обрабатывает клик мыши, возвращает:
  tuple<Response, POS_T, POS_T> get_cell() const
  {
    SDL_Event windowEvent;  // Событие SDL
    Response resp = Response::OK;  // Реакция по умолчанию
    int x = -1, y = -1;    // Абсолютные координаты клика
    int xc = -1, yc = -1;   // Координаты клетки на доске

    // Бесконечный цикл обработки событий
    while (true)
    {
      if (SDL_PollEvent(&windowEvent))  // Получаем событие
      {
        switch (windowEvent.type)
        {
        case SDL_QUIT:  // Закрытие окна
          resp = Response::QUIT;
          break;

        case SDL_MOUSEBUTTONDOWN:  // Клик мыши
          x = windowEvent.motion.x;
          y = windowEvent.motion.y;

          // Преобразуем абсолютные координаты в координаты клетки
          xc = int(y / (board->H / 10) - 1);
          yc = int(x / (board->W / 10) - 1);

          // Обработка специальных зон:
          // 1. Левая верхняя клетка (-1,-1) - кнопка "Назад"
          if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
          {
            resp = Response::BACK;
          }
          // 2. Правая верхняя клетка (-1,8) - кнопка "Реванш"
          else if (xc == -1 && yc == 8)
          {
            resp = Response::REPLAY;
          }
          // 3. Клик по игровой доске (0-7, 0-7)
          else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
          {
            resp = Response::CELL;
          }
          else  // Клик вне значимых зон
          {
            xc = -1;
            yc = -1;
          }
          break;

        case SDL_WINDOWEVENT:  // Изменение размера окна
          if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
          {
            board->reset_window_size();  // Пересчитываем размеры
            break;
          }
        }

        // Если произошло значимое событие - выходим из цикла
        if (resp != Response::OK)
          break;
      }
    }
    return { resp, xc, yc };  // Возвращаем результат
  }

  // Ожидает любое действие пользователя
  Response wait() const
  {
    SDL_Event windowEvent;
    Response resp = Response::OK;

    while (true)
    {
      if (SDL_PollEvent(&windowEvent))
      {
        switch (windowEvent.type)
        {
        case SDL_QUIT:  // Закрытие окна
          resp = Response::QUIT;
          break;

        case SDL_WINDOWEVENT_SIZE_CHANGED:  // Изменение размера
          board->reset_window_size();
          break;

        case SDL_MOUSEBUTTONDOWN:  // Клик мыши
        {
          int x = windowEvent.motion.x;
          int y = windowEvent.motion.y;
          // Проверяем, была ли нажата кнопка "Реванш"
          int xc = int(y / (board->H / 10) - 1);
          int yc = int(x / (board->W / 10) - 1);
          if (xc == -1 && yc == 8)
            resp = Response::REPLAY;
        }
        break;
        }

        if (resp != Response::OK)
          break;
      }
    }
    return resp;
  }

private:
  Board* board;  // Указатель на игровую доску
};