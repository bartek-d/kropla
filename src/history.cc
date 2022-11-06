/********************************************************************************************************
 kropla -- a program to play Kropki; file history.h.
    Copyright (C) 2022 Bartek Dyda,
    email: bartekdyda (at) protonmail (dot) com

    Some parts are inspired by Pachi http://pachi.or.cz/
      by Petr Baudis and Jean-loup Gailly

    This file is part of Kropla.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************************************************/

#include "history.h"

History::History() : history{0, 0} {}

void History::push_back(pti ind, bool terr, bool encl_border)
{
    history.push_back(ind | (terr ? HISTORY_TERR : 0) |
                      (encl_border ? HISTORY_ENCL_BORDER : 0));
}

void History::setEnclosureInLastMove()
{
  history.back() |= HISTORY_ENCL_MOVE;
}

pti History::getLast() const { return history.back() & history_move_MASK; }

pti History::getLastButOne() const { return get(history.size() - 2); }

std::size_t History::size() const { return history.size(); }

pti History::get(int i) const { return history[i] & history_move_MASK; }

bool History::isInEnclBorder(int i) const
{
    return history[i] & HISTORY_ENCL_BORDER;
}

bool History::isInTerrWithAtari(int i) const
{
    return history[i] & HISTORY_TERR;
}
