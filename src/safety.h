/********************************************************************************************************
 kropla -- a program to play Kropki; file safety.cc.
    Copyright (C) 2021 Bartek Dyda,
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

#include <tuple>
#include <map>
#include <vector>

class Game;

class Safety {
public:
  struct MoveDescription {
    pti move;
    pti who;
    bool operator<(const MoveDescription& other) const { return std::tie(move, who) < std::tie(other.move, other.who); }
    bool operator==(const MoveDescription& other) const { return std::tie(move, who) == std::tie(other.move, other.who); }
  };
  using MoveSuggestions = std::map<MoveDescription, pti>;
  Safety();
  struct Info {
    float saf[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float& getPlayersDir(int who, int dir)
    {
      return saf[2*who + dir];
    }
    float getPlayersDir(int who, int dir) const
    {
      return saf[2*who + dir];
    }
    float getSum() const { return saf[0] + saf[1] + saf[2] + saf[3]; }
  };
  void init(Game* game);
  float getSafetyOf(pti p) const { return safety[p].getSum(); }
  MoveSuggestions getMovesInfo(Game* game) const;
  void computeSafety(Game* game);
  void updateAfterMove(Game* game);
  void updateAfterMoveWithoutAnyChangeToSafety();
  const MoveSuggestions& getCurrentlyAddedSugg() const;
  const MoveSuggestions& getPreviouslyAddedSugg() const;
  bool isDameFor(int who, pti where) const;
private:
  void resetSafety();
  void initSafetyForMargin(Game* game, pti p, pti v, pti n, int direction_is_clockwise);
  void markMoveForBoth(MoveSuggestions& sugg, pti where, pti value) const;
  void getMovesInfoForMargin(Game* game, MoveSuggestions& sugg, pti p, pti v, pti n, int v_is_clockwise) const;
  std::vector<Info> safety{};
  MoveSuggestions currentMoveSugg{};
  MoveSuggestions justAddedMoveSugg{};
  MoveSuggestions prevAddedMoveSugg{};
};
