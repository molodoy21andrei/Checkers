#pragma once
#include <stdlib.h>

// Тип для хранения координат на игровом поле
typedef int8_t POS_T;

/**
 * Структура, описывающая игровой ход в шашках
 * Содержит информацию о перемещении фигуры и возможном взятии
 */
struct move_pos
{
  POS_T x, y;     // Текущие координаты фигуры (ряд, столбец)
  POS_T x2, y2;   // Новые координаты после хода
  POS_T xb = -1, yb = -1;  // Координаты побитой фигуры (если ход - бой)

  // Создание хода без взятия (обычное перемещение)
  move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2)
    : x(x), y(y), x2(x2), y2(y2)
  {
  }

  // Создание хода со взятием (с указанием позиции бьющейся фигуры)
  move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2,
    const POS_T xb, const POS_T yb)
    : x(x), y(y), x2(x2), y2(y2), xb(xb), yb(yb)
  {
  }

  // Проверка идентичности ходов (сравниваются только начальные и конечные позиции)
  bool operator==(const move_pos& other) const
  {
    return x == other.x && y == other.y &&
      x2 == other.x2 && y2 == other.y2;
  }

  // Проверка различия ходов
  bool operator!=(const move_pos& other) const
  {
    return !(*this == other);
  }
};