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
  computeSafety(game);
}

void
Safety::computeSafety(Game& game)
{
  initSafetyForMargin(game, coord.ind(0, 1), coord.E, coord.N, 1);
  initSafetyForMargin(game, coord.ind(coord.wlkx-1, 1), coord.W, coord.N, 0);
  initSafetyForMargin(game, coord.ind(1, 0), coord.S, coord.W, 0);
  initSafetyForMargin(game, coord.ind(1, coord.wlky-1), coord.N, coord.W, 1);
  initSafetyForMargin(game, coord.ind(0, coord.wlky-2), coord.E, coord.S, 0);
  initSafetyForMargin(game, coord.ind(coord.wlkx-1, coord.wlky-2), coord.W, coord.S, 1);
  initSafetyForMargin(game, coord.ind(coord.wlkx-2, coord.wlky-1), coord.N, coord.E, 0);
  initSafetyForMargin(game, coord.ind(coord.wlkx-2, 0), coord.S, coord.E, 1);
}

void
Safety::initSafetyForMargin(Game& game, pti p, pti v, pti n, int direction_is_clockwise)
{
  float current_safety[2] = {0.75f, 0.75f};
  for (; coord.dist[p] >= 0; p += v) {
    auto whoseDot = game.whoseDotMarginAt(p);
    if (whoseDot) {
      auto hard_safety = game.getSafetyOf(p);
      if (hard_safety >= 2) {
	current_safety[whoseDot - 1] = 1.0f;
	current_safety[2 - whoseDot] = 0.0f;
      }
      else {
	if (current_safety[whoseDot - 1])
	  safety[p].getPlayersDir(whoseDot-1, direction_is_clockwise) = current_safety[whoseDot - 1];
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


Safety::MoveSuggestions
Safety::getMovesInfo(Game& game) const
{
  MoveSuggestions sugg;
  getMovesInfoForMargin(game, sugg, coord.ind(1, 1), coord.E, coord.N, 1);
  getMovesInfoForMargin(game, sugg, coord.ind(1, 1), coord.S, coord.W, 0);
  getMovesInfoForMargin(game, sugg, coord.ind(coord.wlkx-2, 1), coord.S, coord.E, 1);
  getMovesInfoForMargin(game, sugg, coord.ind(1, coord.wlky-2), coord.E, coord.S, 0);
  return sugg;
}

void
Safety::markMoveForBoth(Safety::MoveSuggestions& sugg, pti where, pti value) const
{
  sugg[MoveDescription{where, 1}] = value;
  sugg[MoveDescription{where, 2}] = value;
}

void
Safety::getMovesInfoForMargin(Game& game, Safety::MoveSuggestions& sugg, pti p, pti v, pti n, int v_is_clockwise) const
{
  constexpr pti bad_move = -10;
  constexpr pti good_move = 10;
  //  constexpr pti interesting_move = 5;

  for (; coord.dist[p] > 0; p += v) {
    auto whoseDot = game.whoseDotMarginAt(p);
    if (whoseDot) {
      auto whoseAtTheEdge = game.whoseDotMarginAt(p + n);
      auto hard_safety = game.getSafetyOf(p);
      auto soft_safety = getSafetyOf(p);
      if (hard_safety + soft_safety >= 2.0f) {
	if (whoseAtTheEdge == 0) {
	  markMoveForBoth(sugg, p+n, bad_move);
	}
	continue;
      }
      if (hard_safety == 1 and soft_safety == 0.0f and whoseAtTheEdge == 0) {
	markMoveForBoth(sugg, p + n, good_move);
	// other defending moves?
	if (game.whoseDotMarginAt(p + v) == 0 and game.whoseDotMarginAt(p + n + v) == 0) {
	  sugg[MoveDescription{p + v, whoseDot}] = good_move;
	  sugg[MoveDescription{p + n + v, whoseDot}] = good_move;
	}
	if (game.whoseDotMarginAt(p - v) == 0 and game.whoseDotMarginAt(p + n - v) == 0) {
	  sugg[MoveDescription{p - v, whoseDot}] = good_move;
	  sugg[MoveDescription{p + n - v, whoseDot}] = good_move;
	}
	continue;
      }
      if (hard_safety == 0 and soft_safety >= 0.75f and soft_safety <= 1.0f) {
	if (game.whoseDotMarginAt(p + v) == 0 and game.whoseDotMarginAt(p + n + v) == 0
	    and safety[p].getPlayersDir(whoseDot - 1, 1 - v_is_clockwise) >= 0.75f) {
	  markMoveForBoth(sugg, p + v, good_move);
	}
	if (game.whoseDotMarginAt(p - v) == 0 and game.whoseDotMarginAt(p + n - v) == 0
	    and safety[p].getPlayersDir(whoseDot - 1, v_is_clockwise) >= 0.75f) {
	  markMoveForBoth(sugg, p - v, good_move);
	}
	continue;
      }
      if (hard_safety == 0 and soft_safety == 0.5f) {
	if (game.whoseDotMarginAt(p + v) == 0 and game.whoseDotMarginAt(p + n + v) == 0) {
	  markMoveForBoth(sugg, p + n + v, good_move);
	}
	if (game.whoseDotMarginAt(p - v) == 0 and game.whoseDotMarginAt(p + n - v) == 0) {
	  markMoveForBoth(sugg, p + n - v, good_move);
	}
	continue;
      }
    }
  }
}

void
Safety::updateAfterMove(Game& game)
{
  computeSafety(game);
}
