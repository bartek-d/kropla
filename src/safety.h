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
  Safety(Game& game);
  struct Info {
    float saf[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float& getPlayersDir(int who, int dir)
    {
      return saf[2*who + dir];
    }
    float getSum() { return saf[0] + saf[1] + saf[2] + saf[3]; }
  };
  float getSafetyOf(pti p) { return safety[p].getSum(); }
private:
  void initSafetyForMargin(Game& game, pti p, pti v, pti n);

  std::vector<Info> safety;

};
