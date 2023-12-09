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

#pragma once

#include "board.h"
#include "bvector.hpp"

void clearLastGoodReplies();

class History
{
    stdb::vector<pti> history;

    static const pti HISTORY_TERR =
        0x4000;  // this is OR-ed with history[...] to denote that someone
                 // played inside own terr or encl
    static const pti HISTORY_ENCL_BORDER =
        0x2000;  // this is OR-ed with history[...] to denote that someone
                 // played on the border of their encl, possibly enclosing sth
    static const pti HISTORY_ENCL_MOVE =
        0x1000;  // apart from the move, there was also an enclosure

    static const pti history_move_MASK =
        ~(HISTORY_ENCL_BORDER | HISTORY_TERR | HISTORY_ENCL_MOVE);

    void saveGoodReplyAt(int i, int who) const;
    void forgetReplyAt(int i, int who) const;

   public:
    History();
    void push_back(pti ind, bool terr, bool encl_border);
    void setEnclosureInLastMove();

    pti getLast() const;
    pti getLastButOne() const;
    std::size_t size() const;
    pti get(int i) const;
    bool isInEnclBorder(int i) const;
    bool isInTerrWithAtari(int i) const;
    bool isEnclosure(int i) const;

    pti getLastGoodReplyFor(int who) const;

    void updateGoodReplies(int lastWho, float abs_value);
};
