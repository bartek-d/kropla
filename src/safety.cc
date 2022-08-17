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

#include <cassert>

#include "board.h"
#include "game.h"

namespace
{
constexpr int add_this_to_make_old = 10000;
constexpr int bigger_than_this_means_old = 5000;
}  // namespace

Safety::Safety()
{
    safety.resize(coord.getSize());
    move_value.resize(coord.getSize(), {0, 0});
}

void Safety::init(Game* game)
{
    safety.resize(coord.getSize());
    move_value.resize(coord.getSize(), {0, 0});
    computeSafety(game, getUpdateValueForAllMargins());
    findMoveValues(game);
    justAddedMoveSugg = {};
    prevAddedMoveSugg = {};
}

bool Safety::computeSafety(Game* game, int what_to_update)
{
    bool something_changed =
        (what_to_update ==
         getUpdateValueForAllMargins());  // if we update everything, then too
                                          // much changed to make some savings
    if (what_to_update & 1)
    {  // top
        initSafetyForMargin(game, coord.ind(0, 1), coord.E, coord.N, 1,
                            something_changed);
        initSafetyForMargin(game, coord.ind(coord.wlkx - 1, 1), coord.W,
                            coord.N, 0, something_changed);
    }
    if (what_to_update & 2)
    {  // right
        initSafetyForMargin(game, coord.ind(coord.wlkx - 2, coord.wlky - 1),
                            coord.N, coord.E, 0, something_changed);
        initSafetyForMargin(game, coord.ind(coord.wlkx - 2, 0), coord.S,
                            coord.E, 1, something_changed);
    }
    if (what_to_update & 4)
    {  // bottom
        initSafetyForMargin(game, coord.ind(0, coord.wlky - 2), coord.E,
                            coord.S, 0, something_changed);
        initSafetyForMargin(game, coord.ind(coord.wlkx - 1, coord.wlky - 2),
                            coord.W, coord.S, 1, something_changed);
    }
    if (what_to_update & 8)
    {  // left
        initSafetyForMargin(game, coord.ind(1, 0), coord.S, coord.W, 0,
                            something_changed);
        initSafetyForMargin(game, coord.ind(1, coord.wlky - 1), coord.N,
                            coord.W, 1, something_changed);
    }
    return something_changed;
}

void Safety::initSafetyForMargin(Game* game, pti p, pti v, pti n,
                                 int direction_is_clockwise,
                                 bool& something_changed)
{
    float current_safety[2] = {0.75f, 0.75f};
    int previousDot = -1;
    int localHardSafety = 0;
    bool checkIfLocalHardSafetyShouldBecome1 = false;
    float old_values[2];
    for (int count = 0; coord.dist[p] >= 0; p += v, ++count)
    {
        if (count > 1)
        {
            for (int d = 0; d <= 1; ++d)
            {
                old_values[d] =
                    safety[p].getPlayersDir(d, direction_is_clockwise);
                safety[p].getPlayersDir(d, direction_is_clockwise) = {};
            }
        }
        auto whoseDot = game->whoseDotMarginAt(p);
        if (whoseDot)
        {
            // find local hard safety
            if (whoseDot == previousDot)
            {
                if (checkIfLocalHardSafetyShouldBecome1 and
                    game->whoseDotMarginAt(p + n) == 0)
                {
                    checkIfLocalHardSafetyShouldBecome1 = false;
                    localHardSafety = 1;
                }
            }
            else
            {
                previousDot = whoseDot;
                localHardSafety = game->getSafetyOf(p);
                if (localHardSafety == 1)
                {
                    checkIfLocalHardSafetyShouldBecome1 =
                        (game->whoseDotMarginAt(p + n) != 0);
                    if (checkIfLocalHardSafetyShouldBecome1)
                        localHardSafety = 0;
                }
                else
                {
                    checkIfLocalHardSafetyShouldBecome1 = false;
                    if (localHardSafety >= 2)
                    {
                        current_safety[whoseDot - 1] = 1.0f;
                        current_safety[2 - whoseDot] = 0.0f;
                    }
                }
            }
            // update soft safety, if needed
            if (localHardSafety < 2)
            {
                if (count == 1)
                {
                    // at the corner we have hard safety, so we cannot
                    // double-count it as soft
                    current_safety[whoseDot - 1] = 0.0f;
                }
                else
                {
                    if (current_safety[whoseDot - 1])
                    {
                        safety[p].getPlayersDir(whoseDot - 1,
                                                direction_is_clockwise) =
                            current_safety[whoseDot - 1];
                    }
                    auto whoseNextDot = game->whoseDotMarginAt(p + v);
                    if (whoseNextDot == 0)
                    {
                        current_safety[whoseDot - 1] =
                            std::min(current_safety[whoseDot - 1] +
                                         0.5f * localHardSafety,
                                     1.0f);
                    }
                }
                current_safety[2 - whoseDot] = 0.0f;
            }
        }
        else if (count > 0)
        {
            previousDot = -1;
            auto whoseAtTheEdge = game->whoseDotMarginAt(p + n);
            if (whoseAtTheEdge)
            {
                current_safety[whoseAtTheEdge - 1] = 1.0f;
                current_safety[2 - whoseAtTheEdge] = 0.0f;
            }
        }
        if (count > 1 and not something_changed)
        {
            for (int d = 0; d <= 1; ++d)
            {
                if (old_values[d] !=
                    safety[p].getPlayersDir(d, direction_is_clockwise))
                    something_changed = true;
            }
        }
    }
}

void Safety::findMoveValues(Game* game)
{
    markMovesAsOld();
    updateAfterMoveWithoutAnyChangeToSafety();

    auto cornerLT = coord.ind(1, 1);
    auto cornerRT = coord.ind(coord.wlkx - 2, 1);
    auto cornerLB = coord.ind(1, coord.wlky - 2);
    auto cornerRB = coord.ind(coord.wlkx - 2, coord.wlky - 2);
    findMoveValuesForMargin(game, cornerLT, cornerRT + coord.E, coord.E,
                            coord.N, 1);
    findMoveValuesForMargin(game, cornerLT, cornerLB + coord.S, coord.S,
                            coord.W, 0);
    findMoveValuesForMargin(game, cornerRT, cornerRB + coord.S, coord.S,
                            coord.E, 1);
    findMoveValuesForMargin(game, cornerLB, cornerRB + coord.E, coord.E,
                            coord.S, 0);
    removeOldMoves();
}

void Safety::markMoveForBoth(pti where, pti value)
{
    if (value > 0)
    {
        if (move_value[where][0] == 0) justAddedMoveSugg[0].push_back(where);
        if (move_value[where][1] == 0) justAddedMoveSugg[1].push_back(where);
    }
    move_value[where] = {value, value};
}

void Safety::markMoveForPlayer(int who, pti where, pti value)
{
    --who;
    if (value > 0 and move_value[where][who] == 0)
        justAddedMoveSugg[who].push_back(where);
    move_value[where][who] = value;
}

void Safety::markMovesAsOld()
{
    auto saveOnlyGoodMove = [](auto& move_val) {
        if (move_val > 0)
            move_val += add_this_to_make_old;
        else
            move_val = 0;
    };
    for (auto p : coord.edge_points)
    {
        saveOnlyGoodMove(move_value[p][0]);
        saveOnlyGoodMove(move_value[p][1]);
    }
    for (auto p : coord.edge_neighb_points)
    {
        saveOnlyGoodMove(move_value[p][0]);
        saveOnlyGoodMove(move_value[p][1]);
    }
}

void Safety::removeOldMoves()
{
    for (auto p : coord.edge_points)
    {
        if (move_value[p][0] >= bigger_than_this_means_old)
            move_value[p][0] = 0;
        if (move_value[p][1] >= bigger_than_this_means_old)
            move_value[p][1] = 0;
    }
    for (auto p : coord.edge_neighb_points)
    {
        if (move_value[p][0] >= bigger_than_this_means_old)
            move_value[p][0] = 0;
        if (move_value[p][1] >= bigger_than_this_means_old)
            move_value[p][1] = 0;
    }
}

void Safety::findMoveValuesForMargin(Game* game, pti p, pti last_p, pti v,
                                     pti n, int v_is_clockwise)
{
    constexpr pti bad_move = -10;
    constexpr pti good_move = 10;
    //  constexpr pti interesting_move = 5;

    for (; p != last_p; p += v)
    {
        auto whoseDot = game->whoseDotMarginAt(p);
        if (whoseDot)
        {
            auto whoseAtTheEdge = game->whoseDotMarginAt(p + n);
            auto hard_safety = game->getSafetyOf(p);
            auto soft_safety = getSafetyOf(p);
            if (hard_safety + soft_safety >= 2.0f)
            {
                if (whoseAtTheEdge == 0)
                {
                    markMoveForBoth(p + n, bad_move);
                }
                continue;
            }
            if (hard_safety == 1 and soft_safety == 0.0f and
                whoseAtTheEdge == 0)
            {
                markMoveForBoth(p + n, good_move);
                // other defending moves?
                if (game->whoseDotMarginAt(p + v) == 0 and
                    game->whoseDotMarginAt(p + n + v) == 0)
                {
                    markMoveForPlayer(whoseDot, p + v, good_move);
                    markMoveForPlayer(whoseDot, p + n + v, good_move);
                }
                if (game->whoseDotMarginAt(p - v) == 0 and
                    game->whoseDotMarginAt(p + n - v) == 0)
                {
                    markMoveForPlayer(whoseDot, p - v, good_move);
                    markMoveForPlayer(whoseDot, p + n - v, good_move);
                }
                continue;
            }
            if (hard_safety == 0 and soft_safety >= 0.75f and
                soft_safety <= 1.0f)
            {
                if (game->whoseDotMarginAt(p + v) == 0 and
                    game->whoseDotMarginAt(p + n + v) == 0 and
                    safety[p].getPlayersDir(whoseDot - 1, 1 - v_is_clockwise) >=
                        0.75f)
                {
                    markMoveForBoth(p + v, good_move);
                }
                if (game->whoseDotMarginAt(p - v) == 0 and
                    game->whoseDotMarginAt(p + n - v) == 0 and
                    safety[p].getPlayersDir(whoseDot - 1, v_is_clockwise) >=
                        0.75f)
                {
                    markMoveForBoth(p - v, good_move);
                }
                continue;
            }
            if (hard_safety == 0 and soft_safety == 0.5f)
            {
                if (game->whoseDotMarginAt(p + v) == 0 and
                    game->whoseDotMarginAt(p + n + v) == 0)
                {
                    markMoveForBoth(p + n + v, good_move);
                }
                if (game->whoseDotMarginAt(p - v) == 0 and
                    game->whoseDotMarginAt(p + n - v) == 0)
                {
                    markMoveForBoth(p + n - v, good_move);
                }
                continue;
            }
        }
    }
}

bool Safety::areThereNoFreePointsAtTheEdgeNearPoint(Game* game, pti p) const
{
    if (coord.dist[p] == 0)
    {
        // check if there were some good moves nearby
        for (int i = 0; i < 4; ++i)
        {
            pti nb = p + coord.nb4[i];
            if (coord.dist[nb] == 1 and
                (move_value[nb][0] != 0 or move_value[nb][1] != 0))
                return false;
        }
        return true;
    }

    for (int i = 0; i < 4; ++i)
    {
        pti nb = p + coord.nb4[i];
        if (coord.dist[nb] == 0 and game->whoseDotMarginAt(nb) == 0)
            return false;
    }
    return true;
}

void Safety::updateAfterMove(Game* game, int what_to_update, pti last_move)
{
    if (what_to_update == 0)
    {
        updateAfterMoveWithoutAnyChangeToSafety();
        return;
    }
    bool something_changed = computeSafety(game, what_to_update);
    if (not something_changed and last_move and
        game->getSafetyOf(last_move) >= 2 and
        areThereNoFreePointsAtTheEdgeNearPoint(game, last_move))
    {  // it would be possible to optimise also when there are free points, by
       // adding dame by hand / removing now dame moves
        updateAfterMoveWithoutAnyChangeToSafety();
        markMoveForBoth(last_move, 0);
    }
    else
    {
        findMoveValues(game);
    }
    // remove elements of prevAddedMoveSugg that no longer make sense
    prevAddedMoveSugg[0].erase(
        std::remove_if(prevAddedMoveSugg[0].begin(), prevAddedMoveSugg[0].end(),
                       [this, game](pti where) {
                           return (move_value[where][0] <= 0 or
                                   game->whoseDotMarginAt(where) != 0);
                       }),
        prevAddedMoveSugg[0].end());
    prevAddedMoveSugg[1].erase(
        std::remove_if(prevAddedMoveSugg[1].begin(), prevAddedMoveSugg[1].end(),
                       [this, game](pti where) {
                           return (move_value[where][1] <= 0 or
                                   game->whoseDotMarginAt(where) != 0);
                       }),
        prevAddedMoveSugg[1].end());
}

void Safety::updateAfterMoveWithoutAnyChangeToSafety()
{
    prevAddedMoveSugg = std::move(justAddedMoveSugg);
    justAddedMoveSugg = {};
}

const Safety::GoodMoves& Safety::getCurrentlyAddedSugg() const
{
    return justAddedMoveSugg;
}

const Safety::GoodMoves& Safety::getPreviouslyAddedSugg() const
{
    return prevAddedMoveSugg;
}

const std::vector<Safety::ValueForBoth>& Safety::getMoveValues() const
{
    return move_value;
}

bool Safety::isDameFor(int who, pti where) const
{
    assert(who == 1 or who == 2);
    return move_value[where][who - 1] < 0;
}

int Safety::getUpdateValueForAllMargins() const { return 0xf; }

int Safety::getUpdateValueForMarginsContaining(pti p) const
{
    if (p == coord.ind(0, 0) or p == coord.ind(coord.wlkx - 1, 0) or
        p == coord.ind(0, coord.wlky - 1) or
        p == coord.ind(coord.wlkx - 1, coord.wlky - 1))
        return 0;  // ignore corners
    int value = 0;
    int x = coord.x[p];
    int y = coord.y[p];
    if (y <= 1) value |= 1;
    if (x >= coord.wlkx - 2) value |= 2;
    if (y >= coord.wlky - 2) value |= 4;
    if (x <= 1) value |= 8;
    return value;
}
