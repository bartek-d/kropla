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

#include "safety.h"
#include "board.h"

Safety::Safety(Game& game)
{
  safety.resize(coord.getSize());
  initSafetyForMargin(game, coord.ind(0, 1), coord.E, coord.N);
  initSafetyForMargin(game, coord.ind(coord.wlkx-1, 1), coord.W, coord.N);
  initSafetyForMargin(game, coord.ind(1, 0), coord.S, coord.W);
  initSafetyForMargin(game, coord.ind(1, coord.wlky-1), coord.N, coord.W);
  initSafetyForMargin(game, coord.ind(0, coord.wlky-2), coord.E, coord.S);  
  initSafetyForMargin(game, coord.ind(coord.wlkx-1, coord.wlky-2), coord.W, coord.S);  
  initSafetyForMargin(game, coord.ind(coord.wlkx-2, coord.wlky-1), coord.N, coord.E);  
  initSafetyForMargin(game, coord.ind(coord.wlkx-2, 0), coord.S, coord.E);  
}

void
Safety::initSafetyForMargin(Game& game, pti p, pti v, pti n)
{
  float current_safety[2] = {0.75f, 0.75f};
  const int direction = (v < 0);
  for (; coord.dist[p] >= 0; p += v) {
    auto whoseDot = game.whoseDotMarginAt(p);
    if (whoseDot) {
      auto hard_safety = game.getSafetyOf(p);
      if (hard_safety >= 2) {
	current_safety[whoseDot - 1] = 1.0f;
	current_safety[2 - whoseDot] = 0.0f;
      }
      else {
	safety[p].getPlayersDir(whoseDot-1, direction) = current_safety[whoseDot - 1];
	current_safety[whoseDot - 1] = std::min(current_safety[whoseDot - 1] + 0.5f * hard_safety, 1.0f);
	current_safety[2 - whoseDot] = 0.0f;
      }
    } else {
      auto whoseAtTheEdge = game.whoseDotMarginAt(p + n);
      if (whoseAtTheEdge) {
	current_safety[whoseAtTheEdge - 1] = 1.0f;
	current_safety[2 - whoseAtTheEdge] = 0.0f;
      }
    }
  }
}
