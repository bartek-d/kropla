/********************************************************************************************************
 kropla -- a program to play Kropki; file dfs.h -- depth first search.
    Copyright (C) 2025 Bartek Dyda,
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
#include <vector>

#include "board.h"

class SimpleGame;

struct APInfo
{
    pti where;
    pti seq0;
    pti seq1;
};

struct OnePlayerDfs
{
    std::vector<pti> low;
    std::vector<pti> discovery;
    std::vector<pti> seq;
    std::vector<APInfo> aps;

    pti player;  // whose player it is, that is, whose enclosures we search

    void AP(const SimpleGame& game, pti left_top, pti bottom_right);
    std::vector<pti> findBorder(const APInfo& ap);

   private:
    void dfsAP(const SimpleGame& game, pti source, pti parent);
};
