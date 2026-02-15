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
#include "enclosure.h"

struct SimpleGame;

class ImportantRectangle
{
    bool checkIfEssential(const SimpleGame& sg, pti ind, pti player) const;
    pti left_top{-1};
    pti bottom_right{-1};

   public:
    void initialise(const SimpleGame& sg, pti player);
    void update(const SimpleGame& sg, pti point, pti player);
    pti getLeftTop() const;
    pti getBottomRight() const;
};

struct APInfo
{
    pti where;
    pti seq0;
    pti seq1;
};

struct OnePlayerDfs
{
    /*
      discovery:
         -1 == player dot or territory
        > 0 == opponent's dot or empty point outside player's territory
          0 == outside RECTANGLE
        discovery.size() == coord.getSize()
        exception: discovery[top-left-corner] == 1, but the corner is irrelevant anyway
      seq:
        seq[0] == 0 -- fake source seq[1], ..., seq[N] -- subsequently discovered points
	Invariants:
	  discovery[seq[k]] == k discovery.size() < coord.getSize()
      low:
        == 0 for player dot, territory, or outside RECTANGLE
        > 0  for opp's dot or empty point outside player's territory
        Exception: low[top-left-corner] > 0, but the corner is irrelevant anyway
        Used only to find aps, and then used as a buffer.
        low.size() == coord.getSize()
     */
    std::vector<pti> low;
    std::vector<pti> discovery;
    std::vector<pti> seq;
    std::vector<APInfo> aps;

    pti player;  // whose player it is, that is, whose enclosures we search

    void AP(const SimpleGame& game, pti left_top, pti bottom_right);
    std::vector<pti> findBorder(const APInfo& ap);
    void findTerritoriesAndEnclosuresInside(const SimpleGame& game,
                                            pti left_top, pti bottom_right);

    std::vector<Enclosure> findAllEnclosures();

    bool checkInvariants(const SimpleGame& game, pti left_top,
                         pti bottom_right) const;

   private:
    void dfsAP(const SimpleGame& game, pti source, pti parent);
    void dfsAPinsideTerr(const SimpleGame& game, pti source, pti parent,
                         pti root);
    void adjustDiscoveryAndAPs(std::size_t previousAPs);
};
