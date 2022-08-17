/********************************************************************************************************
 kropla -- a program to play Kropki; file group_neighbours.cc.
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

#include "group_neighbours.h"

#include "game.h"

GroupNeighbours::GroupNeighbours(Game& game, std::vector<uint8_t>& neighbours,
                                 pti group_id, pti forbidden_point,
                                 uint8_t mask, int who)
{
    for (int i = coord.first; i <= coord.last; ++i)
        if (game.whoseDotMarginAt(i) == 0 and
            not coord.isInNeighbourhood(i, forbidden_point))
        {
            std::array<pti, 4> groups = game.connects[who - 1][i].groups_id;
            addPointIfItIsNeighbour(groups, group_id, neighbours, mask, i);
        }
}

bool GroupNeighbours::isGroupClose(pti group_id) const
{
    return std::find(neighbour_groups.begin(), neighbour_groups.end(),
                     group_id) != neighbour_groups.end();
}

void GroupNeighbours::addPointIfItIsNeighbour(const std::array<pti, 4>& groups,
                                              pti group_id,
                                              std::vector<uint8_t>& neighbours,
                                              uint8_t mask, pti i)
{
    for (int j = 0; j < 4; ++j)
    {
        pti g = groups[j];
        if (g == 0) return;
        if (g == group_id)
        {
            neighbours_list.push_back(i);
            neighbours[i] |= mask;
            addNeighbourGroups(groups, group_id);
        }
    }
}

void GroupNeighbours::addNeighbourGroups(const std::array<pti, 4>& groups,
                                         pti group_id)
{
    for (int j = 0; j < 4; ++j)
    {
        pti g = groups[j];
        if (g == 0) return;
        if (g == group_id) continue;
        if (std::find(neighbour_groups.begin(), neighbour_groups.end(), g) ==
            neighbour_groups.end())
            neighbour_groups.push_back(g);
    }
}
