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

#include <array>
#include <tuple>
#include <vector>

#include "board.h"

class SimpleGame;

class Safety
{
   public:
    using ValueForBoth = std::array<pti, 2>;
    using GoodMoves = std::array<std::vector<pti>, 2>;
    Safety();
    struct Info
    {
        float saf[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float& getPlayersDir(int who, int dir) { return saf[2 * who + dir]; }
        float getPlayersDir(int who, int dir) const
        {
            return saf[2 * who + dir];
        }
        float getSum() const { return saf[0] + saf[1] + saf[2] + saf[3]; }
    };
    void init(const SimpleGame* game);
    float getSafetyOf(pti p) const { return safety[p].getSum(); }
    void updateAfterMove(const SimpleGame* game, int what_to_update,
                         pti last_move = 0);
    void updateAfterMoveWithoutAnyChangeToSafety();
    const GoodMoves& getCurrentlyAddedSugg() const;
    const GoodMoves& getPreviouslyAddedSugg() const;
    const std::vector<ValueForBoth>& getMoveValues() const;
    bool isDameFor(int who, pti where) const;
    int getUpdateValueForAllMargins() const;
    int getUpdateValueForMarginsContaining(pti p) const;

   private:
    void findMoveValues(const SimpleGame* game);
    bool computeSafety(const SimpleGame* game, int what_to_update);
    void initSafetyForMargin(const SimpleGame* game, pti p, pti v, pti n,
                             int direction_is_clockwise,
                             bool& something_changed);
    void markMoveForBoth(pti where, pti value);
    void markMoveForPlayer(int who, pti where, pti value);
    void markMovesAsOld();
    void removeOldMoves();
    void findMoveValuesForMargin(const SimpleGame* game, pti p, pti last_p,
                                 pti v, pti n, int v_is_clockwise);
    bool areThereNoFreePointsAtTheEdgeNearPoint(const SimpleGame* game,
                                                pti p) const;
    std::vector<Info> safety{};
    std::vector<ValueForBoth> move_value{};
    GoodMoves justAddedMoveSugg{};
    GoodMoves prevAddedMoveSugg{};
};
