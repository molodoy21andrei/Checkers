﻿#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
public:
  Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
  {
    ofstream fout(project_path + "log.txt", ios_base::trunc);
    fout.close();
  }

  // Запуск игры в шашки
  int play()
  {
    // Фиксируем время начала игры
    auto start = chrono::steady_clock::now();

    // Если это повтор игры (REPLAY), перезагружаем настройки и обновляем доску
    if (is_replay)
    {
      logic = Logic(&board, &config);  // Создаём новую игровую логику
      config.reload();                // Обновляем конфигурацию
      board.redraw();                  // Перерисовываем доску
    }
    else
    {
      board.start_draw();  // Первоначальная отрисовка доски
    }

    is_replay = false; // Сбрасываем флаг повтора

    int turn_num = -1;  // Счётчик ходов
    bool is_quit = false;  // Флаг выхода из игры
    const int Max_turns = config("Game", "MaxNumTurns");  // Максимальное число ходов из конфига

    // Основной игровой цикл
    while (++turn_num < Max_turns)
    {
      beat_series = 0;  // Сбрасываем счётчик серии боёв

      // Определяем доступные ходы для текущего игрока (0 — белые, 1 — чёрные)
      logic.find_turns(turn_num % 2);

      // Если ходов нет — завершаем игру
      if (logic.turns.empty())
        break;

      // Устанавливаем уровень сложности бота
      logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));

      // Проверяем, играет ли текущий цвет человек или бот
      if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
      {
        // Ход игрока
        auto resp = player_turn(turn_num % 2);

        if (resp == Response::QUIT)  // Выход из игры
        {
          is_quit = true;
          break;
        }
        else if (resp == Response::REPLAY)  // Перезапуск игры
        {
          is_replay = true;
          break;
        }
        else if (resp == Response::BACK)  // Отмена хода
        {
          // Если предыдущий ход был бота, откатываем два хода
          if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
            !beat_series && board.history_mtx.size() > 2)
          {
            board.rollback();
            --turn_num;
          }

          // Если не было боя, откатываем один ход
          if (!beat_series)
            --turn_num;

          board.rollback();
          --turn_num;
          beat_series = 0;
        }
      }
      else
      {
        // Ход бота
        bot_turn(turn_num % 2);
      }
    }

    // Фиксируем время завершения игры
    auto end = chrono::steady_clock::now();

    // Записываем время игры в лог
    ofstream fout(project_path + "log.txt", ios_base::app);
    fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
    fout.close();

    // Если запрошен перезапуск — начинаем заново
    if (is_replay)
      return play();

    // Если игрок вышел, возвращаем 0
    if (is_quit)
      return 0;

    // Определяем результат игры
    int res = 2; // 2 — ничья или игра не завершена
    if (turn_num == Max_turns)
    {
      res = 0; // Ничья
    }
    else if (turn_num % 2)
    {
      res = 1; // Победа одного из игроков
    }

    // Показываем финальный экран
    board.show_final(res);

    // Ожидаем действие игрока (например, реванш)
    auto resp = hand.wait();

    // Если выбран реванш — перезапускаем игру
    if (resp == Response::REPLAY)
    {
      is_replay = true;
      return play();
    }

    return res; // Возвращаем результат
  }

private:
  void bot_turn(const bool color)
  {
    // Засекаем время начала хода
    auto start = chrono::steady_clock::now();

    // Задержка между ходами бота (из конфига)
    auto delay_ms = config("Bot", "BotDelayMS");

    // Поток для задержки хода бота
    thread th(SDL_Delay, delay_ms);

    // Находим оптимальные ходы для бота
    auto turns = logic.find_best_turns(color);

    // Ожидаем завершения потока с задержкой
    th.join();

    bool is_first = true;
    // Применяем ходы бота
    for (auto turn : turns)
    {
      // Добавляем задержку между последовательными ходами
      if (!is_first)
      {
        SDL_Delay(delay_ms);
      }

      is_first = false;

      // Увеличиваем счётчик боёв, если был захват фигуры
      beat_series += (turn.xb != -1);

      // Выполняем ход на доске
      board.move_piece(turn, beat_series);
    }

    // Фиксируем время завершения хода
    auto end = chrono::steady_clock::now();

    // Логируем время хода бота
    ofstream fout(project_path + "log.txt", ios_base::app);
    fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
    fout.close();
  }

  Response player_turn(const bool color)
  {
    // Подсвечиваем клетки с возможными ходами
    vector<pair<POS_T, POS_T>> cells;
    for (auto turn : logic.turns)
    {
      cells.emplace_back(turn.x, turn.y);
    }
    board.highlight_cells(cells);

    // Инициализация переменных для хранения хода
    move_pos pos = { -1, -1, -1, -1 };
    POS_T x = -1, y = -1;

    // Ожидаем выбор клетки игроком
    while (true)
    {
      auto resp = hand.get_cell();
      if (get<0>(resp) != Response::CELL)
        return get<0>(resp);

      pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) };

      // Проверяем, допустима ли выбранная клетка
      bool is_correct = false;
      for (auto turn : logic.turns)
      {
        if (turn.x == cell.first && turn.y == cell.second)
        {
          is_correct = true;
          break;
        }
        if (turn == move_pos{ x, y, cell.first, cell.second })
        {
          pos = turn;
          break;
        }
      }

      // Если ход корректен — выходим из цикла
      if (pos.x != -1)
        break;

      // Если клетка недопустима — сбрасываем выбор
      if (!is_correct)
      {
        if (x != -1)
        {
          board.clear_active();
          board.clear_highlight();
          board.highlight_cells(cells);
        }
        x = -1;
        y = -1;
        continue;
      }

      // Сохраняем выбранную клетку и подсвечиваем следующие возможные ходы
      x = cell.first;
      y = cell.second;
      board.clear_highlight();
      board.set_active(x, y);

      vector<pair<POS_T, POS_T>> cells2;
      for (auto turn : logic.turns)
      {
        if (turn.x == x && turn.y == y)
        {
          cells2.emplace_back(turn.x2, turn.y2);
        }
      }
      board.highlight_cells(cells2);
    }

    // Очищаем подсветку и выполняем ход
    board.clear_highlight();
    board.clear_active();
    board.move_piece(pos, pos.xb != -1);

    // Если не было боя — завершаем ход
    if (pos.xb == -1)
      return Response::OK;

    // Если был бой — продолжаем серию
    beat_series = 1;
    while (true)
    {
      logic.find_turns(pos.x2, pos.y2); // Проверяем возможные продолжения
      if (!logic.have_beats)
        break;

      // Подсвечиваем клетки для следующих боёв
      vector<pair<POS_T, POS_T>> cells;
      for (auto turn : logic.turns)
      {
        cells.emplace_back(turn.x2, turn.y2);
      }
      board.highlight_cells(cells);
      board.set_active(pos.x2, pos.y2);

      // Ожидаем выбор игрока
      while (true)
      {
        auto resp = hand.get_cell();
        if (get<0>(resp) != Response::CELL)
          return get<0>(resp);

        pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) };

        bool is_correct = false;
        for (auto turn : logic.turns)
        {
          if (turn.x2 == cell.first && turn.y2 == cell.second)
          {
            is_correct = true;
            pos = turn;
            break;
          }
        }

        if (!is_correct)
          continue;

        board.clear_highlight();
        board.clear_active();
        beat_series += 1; // Увеличиваем счётчик серии боёв
        board.move_piece(pos, beat_series);
        break;
      }
    }

    return Response::OK;
  }

private:
  Config config;
  Board board;
  Hand hand;
  Logic logic;
  int beat_series;  // Счётчик серии последовательных боёв
  bool is_replay = false;
};