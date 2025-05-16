#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
public:
  // Конструктор. Инициализирует указатели, настройки и генератор случайных чисел
  Logic(Board* board, Config* config) : board(board), config(config)
  {
    rand_eng = std::default_random_engine(
      !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
    scoring_mode = (*config)("Bot", "BotScoringType");
    optimization = (*config)("Bot", "Optimization");
  }

  /**
   * Находит последовательность лучших ходов для заданного цвета
   * Возвращает вектор ходов, ведущих к наилучшей оценке позиции
   */
  vector<move_pos> find_best_turns(const bool color)
  {
    next_best_state.clear();
    next_move.clear();

    // Начинаем поиск с корневого состояния
    find_first_best_turn(board->get_board(), color, -1, -1, 0);

    // Собираем последовательность лучших ходов
    vector<move_pos> best_sequence;
    int current_state = 0;
    while (current_state != -1 && next_move[current_state].x != -1)
    {
      best_sequence.push_back(next_move[current_state]);
      current_state = next_best_state[current_state];
    }

    return best_sequence;
  }

  // Найти все ходы для фигуры по цвету (используется текущая доска)
  void find_turns(const bool color)
  {
    find_turns(color, board->get_board());
  }

  // Найти все ходы для фигуры по координатам (используется текущая доска)
  void find_turns(const POS_T x, const POS_T y)
  {
    find_turns(x, y, board->get_board());
  }

  // Все найденные ходы
  vector<move_pos> turns;

  // Есть ли хотя бы один бой
  bool have_beats;

  // Глубина поиска (для ИИ)
  int Max_depth;

private:
  // Выполняет ход на копии доски
  vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
  {
    if (turn.xb != -1)
      mtx[turn.xb][turn.yb] = 0;
    if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
      mtx[turn.x][turn.y] += 2;
    mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
    mtx[turn.x][turn.y] = 0;
    return mtx;
  }

  // Оценивает доску для бота
  double calc_score(const vector<vector<POS_T>>& mtx, const bool first_bot_color) const
  {
    double w = 0, wq = 0, b = 0, bq = 0;
    for (POS_T i = 0; i < 8; ++i)
    {
      for (POS_T j = 0; j < 8; ++j)
      {
        w += (mtx[i][j] == 1);
        wq += (mtx[i][j] == 3);
        b += (mtx[i][j] == 2);
        bq += (mtx[i][j] == 4);
        if (scoring_mode == "NumberAndPotential")
        {
          w += 0.05 * (mtx[i][j] == 1) * (7 - i);
          b += 0.05 * (mtx[i][j] == 2) * (i);
        }
      }
    }
    if (!first_bot_color)
    {
      swap(b, w);
      swap(bq, wq);
    }
    if (w + wq == 0)
      return INF;
    if (b + bq == 0)
      return 0;
    int q_coef = (scoring_mode == "NumberAndPotential") ? 5 : 4;
    return (b + bq * q_coef) / (w + wq * q_coef);
  }

  /**
  * Находит первый лучший ход и строит дерево возможных продолжений
  * Рекурсивно оценивает все возможные варианты
  */
  double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color,
    const POS_T x, const POS_T y, size_t state,
    double alpha = -1)
  {
    // Инициализация нового состояния
    next_best_state.push_back(-1);
    next_move.emplace_back(-1, -1, -1, -1);

    double best_score = -1;
    bool is_initial_state = (state == 0);

    // Находим все возможные ходы для текущей позиции
    if (!is_initial_state)
      find_turns(x, y, mtx);

    auto current_turns = turns;
    bool current_has_beats = have_beats;

    // Если нет обязательных взятий и это не начальное состояние
    if (!current_has_beats && !is_initial_state)
    {
      return find_best_turns_rec(mtx, !color, 0, alpha);
    }

    // Перебираем все возможные ходы
    for (auto& turn : current_turns)
    {
      size_t next_state = next_move.size();
      double score;

      if (current_has_beats)
      {
        // Продолжаем серию взятий
        score = find_first_best_turn(make_turn(mtx, turn), color,
          turn.x2, turn.y2, next_state, best_score);
      }
      else
      {
        // Оцениваем позицию после хода
        score = find_best_turns_rec(make_turn(mtx, turn), !color, 0, best_score);
      }

      // Обновляем лучший ход, если нашли лучше
      if (score > best_score)
      {
        best_score = score;
        next_best_state[state] = current_has_beats ? static_cast<int>(next_state) : -1;
        next_move[state] = turn;
      }
    }

    return best_score;
  }

  /**
   * Рекурсивная функция поиска с альфа-бета отсечением
   * Оценивает позицию на заданной глубине
   */
  double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color,
    const size_t depth, double alpha, double beta = INF + 1,
    const POS_T x = -1, const POS_T y = -1)
  {
    // Базовый случай - достигнута максимальная глубина
    if (depth == Max_depth)
    {
      return calc_score(mtx, (depth % 2 == color));
    }

    // Находим возможные ходы
    if (x != -1)
      find_turns(x, y, mtx);
    else
      find_turns(color, mtx);

    auto current_turns = turns;
    bool current_has_beats = have_beats;

    // Обработка окончания серии взятий
    if (!current_has_beats && x != -1)
    {
      return find_best_turns_rec(mtx, !color, depth + 1, alpha, beta);
    }

    // Если нет возможных ходов
    if (turns.empty())
      return (depth % 2 ? 0 : INF);

    double min_score = INF + 1;
    double max_score = -1;

    // Перебираем все возможные ходы
    for (auto& turn : current_turns)
    {
      double score = 0.0;

      if (!current_has_beats && x == -1)
      {
        // Обычный ход, меняем цвет
        score = find_best_turns_rec(make_turn(mtx, turn), !color,
          depth + 1, alpha, beta);
      }
      else
      {
        // Продолжаем серию взятий тем же цветом
        score = find_best_turns_rec(make_turn(mtx, turn), color,
          depth, alpha, beta, turn.x2, turn.y2);
      }

      // Обновляем минимальную и максимальную оценки
      min_score = min(min_score, score);
      max_score = max(max_score, score);

      // Альфа-бета отсечение
      if (depth % 2)
        alpha = max(alpha, max_score);
      else
        beta = min(beta, min_score);

      if (optimization != "O0" && alpha >= beta)
        return (depth % 2 ? max_score + 1 : min_score - 1);
    }

    return (depth % 2 ? max_score : min_score);
  }

  // Находит все возможные ходы для заданного цвета
  void find_turns(const bool color, const vector<vector<POS_T>>& mtx)
  {
    vector<move_pos> res_turns;
    bool have_beats_before = false;
    for (POS_T i = 0; i < 8; ++i)
    {
      for (POS_T j = 0; j < 8; ++j)
      {
        if (mtx[i][j] && mtx[i][j] % 2 != color)
        {
          find_turns(i, j, mtx);
          if (have_beats && !have_beats_before)
          {
            have_beats_before = true;
            res_turns.clear();
          }
          if ((have_beats_before && have_beats) || !have_beats_before)
          {
            res_turns.insert(res_turns.end(), turns.begin(), turns.end());
          }
        }
      }
    }
    turns = res_turns;
    shuffle(turns.begin(), turns.end(), rand_eng);
    have_beats = have_beats_before;
  }

  // Находит возможные ходы для фигуры по координатам
  void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>>& mtx)
  {
    turns.clear();
    have_beats = false;
    POS_T type = mtx[x][y];
    switch (type)
    {
    case 1:
    case 2:
      for (POS_T i = x - 2; i <= x + 2; i += 4)
      {
        for (POS_T j = y - 2; j <= y + 2; j += 4)
        {
          if (i < 0 || i > 7 || j < 0 || j > 7)
            continue;
          POS_T xb = (x + i) / 2, yb = (y + j) / 2;
          if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
            continue;
          turns.emplace_back(x, y, i, j, xb, yb);
        }
      }
      break;
    default:
      for (POS_T i = -1; i <= 1; i += 2)
      {
        for (POS_T j = -1; j <= 1; j += 2)
        {
          POS_T xb = -1, yb = -1;
          for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
          {
            if (mtx[i2][j2])
            {
              if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                break;
              xb = i2;
              yb = j2;
            }
            if (xb != -1 && xb != i2)
            {
              turns.emplace_back(x, y, i2, j2, xb, yb);
            }
          }
        }
      }
      break;
    }
    if (!turns.empty())
    {
      have_beats = true;
      return;
    }
    switch (type)
    {
    case 1:
    case 2:
    {
      POS_T i = ((type % 2) ? x - 1 : x + 1);
      for (POS_T j = y - 1; j <= y + 1; j += 2)
      {
        if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
          continue;
        turns.emplace_back(x, y, i, j);
      }
      break;
    }
    default:
      for (POS_T i = -1; i <= 1; i += 2)
      {
        for (POS_T j = -1; j <= 1; j += 2)
        {
          for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
          {
            if (mtx[i2][j2])
              break;
            turns.emplace_back(x, y, i2, j2);
          }
        }
      }
      break;
    }
  }

  // Генератор случайных чисел
  default_random_engine rand_eng;

  // Режим оценки позиции
  string scoring_mode;

  // Режим оптимизации
  string optimization;

  // Ходы, ведущие к лучшему результату
  vector<move_pos> next_move;

  // Переходы между состояниями
  vector<int> next_best_state;

  // Указатель на доску
  Board* board;

  // Указатель на конфиг
  Config* config;
};
