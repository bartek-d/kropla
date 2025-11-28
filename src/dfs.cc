/********************************************************************************************************
 kropla -- a program to play Kropki; file dfs.cc -- depth first search.
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
#include "dfs.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <map>
#include <memory>

#include "simplegame.h"

void OnePlayerDfs::dfsAP(const SimpleGame& game, pti source, pti parent)
{
    auto u = source;
    const auto saved_disc = discovery[u];
    low[u] = discovery[u] = seq.size();
    seq.push_back(u);

    auto ourDotOrOutside = [&](pti u)
    {
        auto what = game.whoseDotMarginAt(u);
        return (what == SimpleGame::MASK_DOT || what == player);
    };
    auto visitPoint = [&](pti v, pti u)
    {
        if (ourDotOrOutside(v)) return;
        if (discovery[v] < 0)
        {
            const auto time_before_node = static_cast<pti>(seq.size());
            dfsAP(game, v, u);
            if (discovery[u] <= low[v] && game.whoseDotMarginAt(u) == 0)
            {
                // ap found
                aps.push_back(APInfo{.where = u,
                                     .seq0 = time_before_node,
                                     .seq1 = static_cast<pti>(seq.size())});
            }
            low[u] = std::min(low[u], low[v]);
        }
        else  // if (discovery[u] >= 0)
        {
            low[u] = std::min(low[u], discovery[v]);
        }
    };

    const pti offset = -128;
    if (saved_disc < -1)  // edge point, visit other edges
    {
        for (pti v = u + saved_disc - offset; v != parent;)
        {
            visitPoint(v, u);
            if (discovery[v] < 0)
                v += discovery[v] - offset;
            else
                break;
        }
        const pti fakeSource = 0;
        low[u] = low[fakeSource];
    }

    for (auto i = 0u; i < 4u; ++i)
    {
        pti v = u + coord.nb4[i];
        if (v == parent) continue;
        visitPoint(v, u);
    }
}

void OnePlayerDfs::AP(const SimpleGame& game, pti left_top, pti bottom_right)
{
    low = std::vector<pti>(coord.getSize());
    discovery = std::vector<pti>(coord.getSize());
    seq.clear();
    aps.clear();
    pti current_bottom = coord.ind(coord.x[left_top], coord.y[bottom_right]);
    const pti delta = left_top + coord.NE - current_bottom + 2;
    const pti offset = -128;
    // first column on the left
    for (auto i = left_top; i < current_bottom; ++i)
    {
        discovery[i] = offset + coord.S;
    }
    // bottom row
    for (auto i = current_bottom; i < bottom_right; i += coord.E)
        discovery[i] = offset + coord.E;
    // interior
    current_bottom += coord.NE;
    const pti last_inside = bottom_right + coord.NW;
    for (auto i = left_top + coord.SE; i <= last_inside; ++i)
    {
        discovery[i] = -1;
        if (i == current_bottom)
        {
            i += delta;
            current_bottom += coord.E;
        }
    }
    std::cout << "discovery at start:\n"
              << coord.showColouredBoard(discovery) << std::endl;
    // top row
    for (auto i = left_top + coord.E; i <= bottom_right; i += coord.E)
        discovery[i] = offset + coord.W;
    // right column
    for (auto i = bottom_right + delta + coord.W; i <= bottom_right; ++i)
    {
        std::cout << coord.showPt(i) << std::endl;
        discovery[i] = offset + coord.N;
    }
    std::cout << "discovery at start:\n"
              << coord.showColouredBoard(discovery) << std::endl;

    pti fakeSource = 0;
    low[fakeSource] = discovery[fakeSource] = seq.size();
    seq.push_back(fakeSource);

    dfsAP(game, left_top, fakeSource);
}

struct Dfs
{
    std::array<OnePlayerDfs, 2> dfs;
};
