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

#include "game.h"

class Safety {
public:
  struct MoveSuggestions {
    pti move;
    pti who;
    pti value;
    bool operator==(const MoveSuggestions& other) const
    { return move == other.move and who == other.who and value == other.value; }
  };
  Safety(Game& game);
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
  float getSafetyOf(pti p) const { return safety[p].getSum(); }
  std::vector<MoveSuggestions> getMovesInfo(Game& game) const;
private:
  void initSafetyForMargin(Game& game, pti p, pti v, pti n, int direction_is_clockwise);
  void getMovesInfoForMargin(Game& game, std::vector<MoveSuggestions>& sugg, pti p, pti v, pti n, int v_is_clockwise) const;
  std::vector<Info> safety;

};
