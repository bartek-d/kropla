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
#include <set>
#include <span>

#include "enclosure.h"
#include "simplegame.h"

bool ImportantRectangle::checkIfEssential(const SimpleGame& sg, pti ind,
                                          pti player) const
{
    const auto w = sg.worm[ind];
    const auto& d = sg.descr.at(w);
    if (d.dots[player - 1] > 2) return true;  // large worm
    if (d.neighb.empty()) return false;       // 1-dot or 2-dot isolated worm
    if (d.neighb.size() >= 2) return true;    // at least 2 neighbours
    if (d.dots[player - 1] == 2)
        return true;  // 2-dot worm with at least 1 neighbour
    // here 1-dot worm with one neighbour, check if the neighbour is also 1-dot with no other neighbours
    const auto nei = d.neighb[0];
    if (sg.descr.at(nei).dots[player - 1] == 1 &&
        sg.descr.at(nei).neighb.size() == 1)
        return false;
    return true;
}

ImportantRectangle::ImportantRectangle() = default;
ImportantRectangle::ImportantRectangle(pti ind)
{
    left_top = ind;
    bottom_right = ind;
}

bool ImportantRectangle::contains(pti ind) const noexcept
{
    ImportantRectangle just_ind{ind};
    return just_ind <= *this;
}

void ImportantRectangle::initialise(const SimpleGame& sg, pti player)
{
    pti x0 = coord.wlkx;
    pti y0 = coord.wlky;
    pti x1 = -1;
    pti y1 = -1;

    for (pti i = coord.first; i <= coord.last; ++i)
        if (sg.whoseDotMarginAt(i) == player && checkIfEssential(sg, i, player))
        {
            const pti x = coord.x[i];
            const pti y = coord.y[i];
            x0 = std::min<pti>(x0, x);
            y0 = std::min<pti>(y0, y);
            x1 = std::max<pti>(x1, x);
            y1 = std::max<pti>(y1, y);
        }
    if (x1 != -1)
    {
        x0 = std::max<pti>(0, x0 - 1);
        y0 = std::max<pti>(0, y0 - 1);
        x1 = std::min<pti>(coord.wlkx - 1, x1 + 1);
        y1 = std::min<pti>(coord.wlky - 1, y1 + 1);
        left_top = coord.ind(x0, y0);
        bottom_right = coord.ind(x1, y1);
        return;
    }
    left_top = -1;
    bottom_right = -1;
}

void ImportantRectangle::update(const SimpleGame& sg, pti point, pti player)
// at 'point' a new dot of 'player' was put, update the rectangle
{
    if (left_top == coord.first && bottom_right == coord.last) return;
    const pti x = coord.x[point];
    const pti y = coord.y[point];
    pti x0 = left_top != -1 ? coord.x[left_top] : x;
    pti y0 = left_top != -1 ? coord.y[left_top] : y;
    pti x1 = left_top != -1 ? coord.x[bottom_right] : x;
    pti y1 = left_top != -1 ? coord.y[bottom_right] : y;
    if (x >= x0 + 3 && x <= x1 - 3 && y >= y0 + 3 && y <= y1 - 3) return;
    if (!checkIfEssential(sg, point, player)) return;
    // having a new dot at ind may make some other dots essential, those dots may be up to 2 points away
    const pti xstart = std::max<pti>(x - 2, 0);
    const pti xend = std::min<pti>(x + 2, coord.wlkx - 1);
    const pti ystart = std::max<pti>(y - 2, 0);
    const pti yend = std::min<pti>(y + 2, coord.wlky - 1);
    for (pti x = xstart; x <= xend; ++x)
        for (pti y = ystart; y <= yend; ++y)
        {
            pti ind = coord.ind(x, y);
            if (sg.whoseDotMarginAt(ind) == player &&
                checkIfEssential(sg, ind, player))
            {
                x0 = std::min<pti>(x0, x - 1);
                y0 = std::min<pti>(y0, y - 1);
                x1 = std::max<pti>(x1, x + 1);
                y1 = std::max<pti>(y1, y + 1);
            }
        }
    x0 = std::max<pti>(0, x0);
    y0 = std::max<pti>(0, y0);
    x1 = std::min<pti>(coord.wlkx - 1, x1);
    y1 = std::min<pti>(coord.wlky - 1, y1);
    left_top = coord.ind(x0, y0);
    bottom_right = coord.ind(x1, y1);
}

pti ImportantRectangle::getLeftTop() const { return left_top; }

pti ImportantRectangle::getBottomRight() const { return bottom_right; }

bool ImportantRectangle::operator<=(ImportantRectangle other) const noexcept
// returns true if *this is included in other
{
    if (left_top == -1) return true;
    if (other.left_top == -1) return false;
    const pti x0 = coord.x[left_top];
    const pti y0 = coord.y[left_top];
    const pti x1 = coord.x[bottom_right];
    const pti y1 = coord.y[bottom_right];
    return (coord.x[other.left_top] <= x0 &&
            x1 <= coord.x[other.bottom_right] &&
            coord.y[other.left_top] <= y0 && y1 <= coord.y[other.bottom_right]);
}

bool AnnotatedEncl::isEnclSame(pti other_where, pti other_interior_near_where,
                               pti other_seq_length) const
{
    return where == other_where &&
           interior_near_where == other_interior_near_where &&
           seq_length == other_seq_length;
}

std::vector<pti> OnePlayerDfs::findBorder(const APInfo& ap)
{
    auto& marks = low;
    std::vector<pti> stack;
    stack.reserve(low.size());
    pti leftmost = seq[ap.seq0];
    // mark our dots that are neighbours to some interior points
    for (auto pt : std::span(seq.begin() + ap.seq0, seq.begin() + ap.seq1))
        for (auto nind : coord.nb4)
        {
            pti n = pt + nind;
            if (marks[n] >= 0 && discovery[n] < 0)
            {
                marks[n] = -1;
                leftmost = std::min(leftmost, n);
                // std::cout << coord.showPt(n) << " & ";
                stack.push_back(n);
            }
        }
    if (ap.where)
    {
        stack.push_back(ap.where);
        marks[ap.where] = -1;
        leftmost = std::min(leftmost, ap.where);
    }
    // std::cout << std::endl;
    const auto stack_size = stack.size();
    auto doCleanUp = [&]()
    {
        for (auto p : std::span(stack.begin(), stack.begin() + stack_size))
            marks[p] = 0;
    };
    if (stack_size <= 6)
    {
        // with <=6 potential border elements, all marked dots must be at the
        // border
        doCleanUp();
        return stack;
    }

    // when stack_size > 6, it is possible that some neighbours of interior will
    // not be on border -- traverse the border to find minimal-area enclosure
    const pti before_leftmost = leftmost + coord.NE;
    stack.push_back(before_leftmost);
    stack.push_back(leftmost);
    stack.push_back(coord.findNextOnRight(leftmost, before_leftmost));
    marks[leftmost] = -2;
    marks[before_leftmost] = -2;

    // std::cout << "Marks przed przejsciem brzegu:\n" <<
    // coord.showColouredBoard(marks) << std::endl;

    do
    {
        // check if current point is on the border, if not, take next
        // (clockwise)
        while (marks[stack.back()] >= 0)
        {
            stack.back() =
                coord.findNextOnRight(stack[stack.size() - 2], stack.back());
        }
        // std::cout << "Next dot: " << coord.showPt(stack.back()) << std::endl;
        if (stack.back() == before_leftmost)  // enclosure found
            break;
        if (marks[stack.back()] == -2)
        {
            // this point has been already visited, go back to the last visit of
            // that point and try next neighbour
            pti loop_pt = stack.back();
            stack.pop_back();
            pti prev_pt = stack.back();
            while (stack.back() != loop_pt)
            {
                marks[stack.back()] = -1;
                stack.pop_back();
            }
            // now we are again at loop_pt which has been already unMARKed
            stack.push_back(coord.findNextOnRight(loop_pt, prev_pt));
        }
        else
        {
            // visit the point
            marks[stack.back()] = -2;
            stack.push_back(
                coord.findNextOnRight(stack.back(), stack[stack.size() - 2]));
        }
    } while (stack.back() != before_leftmost);
    doCleanUp();

    /*
    if (std::any_of(marks.begin(), marks.end(), [](auto el) { return el < 0; }))
      {
        std::cout << "Nieposprzatane!\n";
        std::cout << coord.showColouredBoard(marks) << std::endl;
        std::cout << coord.showColouredBoard(discovery) << std::endl;
        std::cout << "AP: " << coord.showPt(ap.where) << " seq: " << ap.seq0 <<
    " - " << ap.seq1 << std::endl; throw 3;
        } */
    // std::cout << "stack_size: " << stack_size
    //           << "  stack.size(): " << stack.size() << std::endl;
    if (2 * stack_size + 1 == stack.size())
    {
        //    std::cout << "Skrot!!\n";
        stack.resize(stack_size);
        return stack;
    }
    stack.pop_back();
    stack.erase(stack.begin(), stack.begin() + stack_size);
    return stack;
}

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

bool OnePlayerDfs::isRectangleTooSmall(pti left_top, pti bottom_right) const
{
    if (left_top == -1) return true;
    if (coord.x[bottom_right] - coord.x[left_top] <= 1 ||
        coord.y[bottom_right] - coord.y[left_top] <= 1)
        return true;
    return false;
}

void OnePlayerDfs::AP(const SimpleGame& game, pti left_top, pti bottom_right)
{
    aps.clear();
    if (isRectangleTooSmall(left_top, bottom_right)) return;
    low.clear();
    discovery.clear();
    low.resize(coord.getSize(), 0);
    discovery.resize(coord.getSize(), 0);
    seq.clear();
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
    // top row
    for (auto i = left_top + coord.E; i <= bottom_right; i += coord.E)
        discovery[i] = offset + coord.W;
    // right column
    for (auto i = bottom_right + delta + coord.W; i <= bottom_right; ++i)
    {
        // std::cout << coord.showPt(i) << std::endl;
        discovery[i] = offset + coord.N;
    }
    //std::cout << "discovery at start:\n"
    //      << coord.showColouredBoard(discovery) << std::endl;

    pti fakeSource = 0;
    low[fakeSource] = discovery[fakeSource] = seq.size();
    seq.push_back(fakeSource);

    dfsAP(game, left_top, fakeSource);
    assert(checkInvariants(game, left_top, bottom_right));
}

void OnePlayerDfs::dfsAPinsideTerr(const SimpleGame& game, pti source,
                                   pti parent, pti root)
{
    auto u = source;
    low[u] = discovery[u] = seq.size();
    seq.push_back(u);

    auto ourDotAt = [&](pti u)
    {
        auto what = game.whoseDotMarginAt(u);
        return what == player;
    };
    auto visitPoint = [&](pti v, pti u)
    {
        if (ourDotAt(v)) return;
        if (discovery[v] < 0)
        {
            const auto time_before_node = static_cast<pti>(seq.size());
            dfsAPinsideTerr(game, v, u, root);
            if (discovery[u] <= low[v] && u != root &&
                game.whoseDotMarginAt(u) == 0)
            {
                // ap found
                std::cout << "AP inside terr found at " << coord.showPt(u)
                          << " interior size: " << seq.size() - time_before_node
                          << "  seq_where = " << discovery[u]
                          << "  interior: " << time_before_node << " -- "
                          << seq.size() << std::endl;
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

    APInfo last_subtree{.where = root, .seq0 = 0, .seq1 = 0};
    int subtrees_before = 0;
    for (auto i = 0u; i < 4u; ++i)
    {
        pti v = u + coord.nb4[i];
        if (v == parent) continue;
        const auto last_discovery = seq.size();
        visitPoint(v, u);
        if (u == root && game.whoseDotMarginAt(root) == 0 &&
            seq.size() > last_discovery)
        {
            switch (subtrees_before)
            {
                case 0:
                    last_subtree.seq0 = last_discovery;
                    last_subtree.seq1 = seq.size();
                    break;
                case 1:
                    aps.push_back(last_subtree);
                    [[fallthrough]];
                default:
                    last_subtree.seq0 = last_discovery;
                    last_subtree.seq1 = seq.size();
                    aps.push_back(last_subtree);
            }
            ++subtrees_before;
        }
    }
}

void OnePlayerDfs::adjustDiscoveryAndAPs(std::size_t previousAPs)
// For APs in territories, we need that the first AP with some point 'where'
// has interior starting with (where+1). E.g.
//   x x
// x 1 4 x
// x 2 3 x
// x 5 x
//   x
// Here '2' is an AP with interior [5]. This means that its complement
// would be an AP at '2' with interior [1, 3, 4], which is not a sequence of
// consecutive numbers. Therefore, we need to rearrange discovery as follows:
//   x x
// x 1 5 x
// x 2 4 x
// x 3 x
//   x
//
// Another example:
//     x
//   x 4 x
// x 6 3 2 x
//   x 5 1 x
//     x x
// After rearragement:
//     x
//   x 4 x
// x 5 3 2 x
//   x 6 1 x
//     x x
{
    pti last_where = -1;
    std::cout
        << "----------------------------------------------------------------\n";
    const pti endOfTerr = seq.size();
    for (std::size_t a = previousAPs; a < aps.size(); ++a)
    {
        bool rearragement_needed = false;
        pti expected_seq_start{};
        if (aps[a].where != last_where)
        {
            // first AP at this point
            last_where = aps[a].where;
            expected_seq_start = discovery[last_where] + 1;
            rearragement_needed = (aps[a].seq0 != expected_seq_start);
        }
        else
        {
            expected_seq_start = aps[a - 1].seq1;
            rearragement_needed = (aps[a].seq0 != expected_seq_start);
        }
        if (rearragement_needed)
        {
            // rearragement needed
            // are there some other APs with the same point? if so, take the end
            // range1 as the last seq1
            std::cout << "Discovery BEFORE change:\n"
                      << coord.showBoard(discovery) << std::endl;

            auto findLastSeq1WithThisWhere = [&]()
            {
                auto lastSeq1 = aps[a].seq1;
                for (auto j = a + 1; j < aps.size(); ++j)
                {
                    if (aps[j].where != last_where) break;
                    assert(aps[j].seq0 == lastSeq1);
                    lastSeq1 = aps[j].seq1;
                }
                return lastSeq1;
            };
            const pti range1 = findLastSeq1WithThisWhere();
            const pti offsetMinus = aps[a].seq0 - expected_seq_start;
            const pti offsetPlus = range1 - aps[a].seq0;
            const pti range0 = expected_seq_start;
            const pti middle = aps[a].seq0;
            std::cout << "aps[" << a
                      << "]: (seq0, seq1, exp_seq_start) = " << aps[a].seq0
                      << ", " << aps[a].seq1 << ", " << expected_seq_start
                      << '\n';
            std::cout << "ranges: " << range0 << ", " << middle << ", "
                      << range1 << std::endl;
            std::cout << "offsets: -" << offsetMinus << " +" << offsetPlus
                      << std::endl;
            // discovery with values in [range0, range1) have to be changed:
            // those in [range0, middle) by +offsetPlus, those in [middle, seq1) by -offsetMinus
            for (auto i = range0; i < middle; ++i)
            {
                discovery[seq[i]] += offsetPlus;
                seq.push_back(seq[i]);
            }
            for (auto i = middle; i < range1; ++i)
            {
                discovery[seq[i]] -= offsetMinus;
                seq[i - offsetMinus] = seq[i];
            }
            std::copy(seq.begin() + endOfTerr, seq.end(),
                      seq.begin() + (range1 - offsetMinus));
            seq.resize(endOfTerr);
            // now change this and subsequent APs
            auto adjust = [=](pti i) -> pti
            {
                if (i < range0 || i >= range1) return i;
                if (i < middle) return i + offsetPlus;
                return i - offsetMinus;
            };
            std::cout << "Discovery:\n"
                      << coord.showBoard(discovery) << std::endl;
            for (auto k = a; k < aps.size(); ++k)
            {
                std::cout << "aps[" << k
                          << "]: (seq0, seq1, where) = " << aps[k].seq0 << ", "
                          << aps[k].seq1 << ", " << aps[k].where << '\n';
                auto len = aps[k].seq1 - aps[k].seq0;
                aps[k].seq0 = adjust(aps[k].seq0);
                aps[k].seq1 = aps[k].seq0 + len;
                std::cout << "aps[" << k
                          << "]: (seq0, seq1, where) = " << aps[k].seq0 << ", "
                          << aps[k].seq1 << ", " << aps[k].where << '\n';
            }
        }
    }
}

void OnePlayerDfs::markTerritories(const SimpleGame& game,
                                   std::span<pti> in_terr, pti left_top,
                                   pti bottom_right) const
{
    if (isRectangleTooSmall(left_top, bottom_right)) return;
    // note: AP has to be called first
    auto ourDotAt = [&](pti u)
    {
        auto what = game.whoseDotMarginAt(u);
        return what == player;
    };

    const auto height = coord.y[bottom_right] - coord.y[left_top];
    for (auto top_row = left_top; top_row < bottom_right; top_row += coord.E)
        for (pti y = 0; y <= height; ++y)
        {
            pti ind = top_row + y;
            if (discovery[ind] < 0 && !ourDotAt(ind))
            {
                in_terr[ind] = 1;
            }
            else
            {
                in_terr[ind] = 0;
            }
        }
}

void OnePlayerDfs::findTerritoriesAndEnclosuresInside(const SimpleGame& game,
                                                      pti left_top,
                                                      pti bottom_right)
{
    if (isRectangleTooSmall(left_top, bottom_right)) return;
    // note: AP has to be called first
    auto ourDotAt = [&](pti u)
    {
        auto what = game.whoseDotMarginAt(u);
        return what == player;
    };

    const auto height = coord.y[bottom_right] - coord.y[left_top];
    for (auto top_row = left_top; top_row < bottom_right; top_row += coord.E)
        for (pti y = 0; y <= height; ++y)
        {
            pti ind = top_row + y;
            if (discovery[ind] < 0 && !ourDotAt(ind))
            {
                const pti fakeSource = 0;
                const pti startOfTerr = seq.size();
                const auto previousAPs = aps.size();
                dfsAPinsideTerr(game, ind, fakeSource, ind);
                const pti endOfTerr = seq.size();
                // double the APs
                const auto currentAPs = aps.size();

                if (currentAPs > previousAPs)
                {
                    std::sort(
                        aps.begin() + previousAPs, aps.end(),
                        [this](const auto& ap1, const auto& ap2)
                        {
                            return std::tie(discovery[ap1.where], ap1.seq0) <
                                   std::tie(discovery[ap2.where], ap2.seq0);
                        });
                    std::cerr << "-- Adjusting discovery\n";
                    std::cerr << "Before:\n"
                              << coord.showBoard(discovery) << std::endl;
                    std::cerr << "seq: size = " << seq.size() << "\n";
                    for (auto el : seq) std::cerr << coord.showPt(el) << "  ";
                    std::cerr << std::endl;

                    adjustDiscoveryAndAPs(previousAPs);
                    std::cerr << "-- Adjusted discovery\n";
                    std::cerr << "AFTER:\n"
                              << coord.showBoard(discovery) << std::endl;
                    std::cerr << "seq: size = " << seq.size() << "\n";
                    for (auto el : seq) std::cerr << coord.showPt(el) << "  ";
                    std::cerr << std::endl;
                    assert(checkInvariants(game, left_top, bottom_right));
                    pti last_where = aps[previousAPs].where;
                    pti currentStart = discovery[last_where];
                    pti augmentedSeq = startOfTerr;
                    for (std::size_t a = previousAPs; a <= currentAPs; ++a)
                    {
                        if (a == currentAPs || aps[a].where != last_where)
                        {
                            pti currentEnd = aps[a - 1].seq1;
                            // subtrees span [currentStart, currentEnd), and the whole terr is [startOfTerr, endOfTerr)
                            if (currentEnd - currentStart <
                                endOfTerr - startOfTerr)
                            {
                                if (currentStart == startOfTerr)
                                    aps.push_back(APInfo{.where = last_where,
                                                         .seq0 = currentEnd,
                                                         .seq1 = endOfTerr});
                                else if (currentEnd == endOfTerr)
                                    aps.push_back(APInfo{.where = last_where,
                                                         .seq0 = startOfTerr,
                                                         .seq1 = currentStart});
                                else
                                {
                                    // augmentation is needed
                                    if (augmentedSeq < currentStart)
                                    {
                                        for (auto i = augmentedSeq;
                                             i < currentStart; ++i)
                                            seq.push_back(seq[i]);
                                        augmentedSeq = currentStart;
                                    }
                                    aps.push_back(APInfo{
                                        .where = last_where,
                                        .seq0 = currentEnd,
                                        .seq1 = static_cast<pti>(endOfTerr +
                                                                 currentStart -
                                                                 startOfTerr)});
                                }
                            }
                            if (a < currentAPs)
                            {
                                last_where = aps[a].where;
                                currentStart = discovery[last_where];
                            }
                        }
                    }
                }
                // territory AP
                aps.push_back(
                    APInfo{.where = 0, .seq0 = startOfTerr, .seq1 = endOfTerr});
            }
        }
}

Enclosure OnePlayerDfs::findEnclosure(const APInfo& ap)
{
    auto border = findBorder(ap);
    std::vector<pti> interior(seq.begin() + ap.seq0, seq.begin() + ap.seq1);
    if (interior.size() > 6)
    {
        auto& marks = low;

        for (auto el : border) marks[el] = -1;
        for (auto el : interior) marks[el] = -1;

        for (std::size_t i = 0; i < interior.size(); ++i)
        {
            for (auto nind : coord.nb4)
            {
                pti n = interior[i] + nind;
                try
                {
                    if (marks.at(n) >= 0)
                    {
                        marks[n] = -1;
                        interior.push_back(n);
                    }
                }
                catch (...)
                {
                    std::cout << coord.showColouredBoard(discovery)
                              << std::endl;
                    std::cout << coord.showColouredBoard(marks) << std::endl;
                    std::cout << "AP: " << coord.showPt(ap.where)
                              << " seq: " << ap.seq0 << " - " << ap.seq1
                              << std::endl;
                    std::cout << "Wnetrze: ";
                    for (auto el : interior)
                        std::cout << " " << coord.showPt(el);
                    std::cout << "\nBrzeg: ";
                    for (auto el : border) std::cout << " " << coord.showPt(el);
                    std::cout << std::endl;
                    throw;
                };
            }
        }
        // cleanup
        for (auto p : border) marks[p] = 0;
        for (auto p : interior) marks[p] = 0;
    }
    return Enclosure{std::move(interior), std::move(border)};
}

AnnotatedEncl OnePlayerDfs::findAnnotatedEnclosure(const APInfo& ap)
{
    auto encl = findEnclosure(ap);
    return AnnotatedEncl{.encl = std::move(encl),
                         .terr = -1,
                         .dots = -1,
                         .where = ap.where,
                         .interior_near_where = seq[ap.seq0],
                         .seq_length = static_cast<pti>(ap.seq1 - ap.seq0)};
}

template <typename EnclType>
std::vector<EnclType> OnePlayerDfs::findAllEnclosuresT()
{
    if (aps.empty()) return {};
    std::vector<EnclType> encls;
    encls.reserve(aps.size());
    for (const auto& ap : aps)
    {
        if constexpr (std::is_same_v<EnclType, Enclosure>)
        {
            encls.push_back(findEnclosure(ap));
        }
        else if constexpr (std::is_same_v<EnclType, AnnotatedEncl>)
        {
            encls.push_back(findAnnotatedEnclosure(ap));
        }
    }
    return encls;
}

std::vector<Enclosure> OnePlayerDfs::findAllEnclosures()
{
    return findAllEnclosuresT<Enclosure>();
}

std::vector<AnnotatedEncl> OnePlayerDfs::findAllAnnotatedEncl()
{
    return findAllEnclosuresT<AnnotatedEncl>();
}

bool OnePlayerDfs::checkInvariants(const SimpleGame& game, pti left_top,
                                   pti bottom_right) const
{
    if (isRectangleTooSmall(left_top, bottom_right)) return aps.empty();
    const auto x1 = coord.x[left_top];
    const auto y1 = coord.y[left_top];
    const auto x2 = coord.x[bottom_right];
    const auto y2 = coord.y[bottom_right];
    auto insideRectangle = [=](auto i)
    {
        return (coord.x[i] >= x1 && coord.x[i] <= x2 && coord.y[i] >= y1 &&
                coord.y[i] <= y2);
    };
    auto ourDotOrOutside = [&](pti u)
    {
        auto what = game.whoseDotMarginAt(u);
        return (what == SimpleGame::MASK_DOT || what == player);
    };
    // std::cerr << "Player: " << player << '\n' << coord.showBoard(game.worm)
    // << std::endl;

    long int sum = 0;
    for (int i = 0; i < coord.getSize(); ++i)
    {
        if (!insideRectangle(i))
        {
            if (discovery[i] != 0)
            {
                std::cerr << __FUNCTION__ << ":" << __LINE__ << "discovery["
                          << coord.showPt(i) << "] != 0\n"
                          << coord.showColouredBoard(discovery) << std::endl
                          << "leftTop: " << coord.showPt(left_top)
                          << ", bottom right: " << coord.showPt(bottom_right)
                          << std::endl;
                return false;
            }
            if (low[i] != 0)
            {
                std::cerr << __FUNCTION__ << ":" << __LINE__ << "low["
                          << coord.showPt(i) << "] != 0\n"
                          << coord.showColouredBoard(low) << std::endl;

                return false;
            }
        }
        else
        {
            if (i != left_top)  // left_top is exceptional
            {
                if (discovery[i] == 0)
                {
                    std::cerr << __FUNCTION__ << ":" << __LINE__ << "discovery["
                              << coord.showPt(i) << "] == 0\n"
                              << coord.showColouredBoard(discovery)
                              << std::endl;
                    return false;
                }
                if (discovery[i] > 0 && ourDotOrOutside(i))
                {
                    std::cerr << __FUNCTION__ << ":" << __LINE__ << "discovery["
                              << coord.showPt(i) << "] > 0\n"
                              << coord.showColouredBoard(discovery)
                              << std::endl;
                    return false;
                }
                if (low[i] > 0 && ourDotOrOutside(i))
                {
                    std::cerr << __FUNCTION__ << ":" << __LINE__ << "low["
                              << coord.showPt(i) << "] > 0\n"
                              << coord.showColouredBoard(low) << std::endl;
                    return false;
                }
            }
            if (discovery[i] > static_cast<pti>(seq.size()))
            {
                std::cerr << __FUNCTION__ << ":" << __LINE__ << "discovery["
                          << coord.showPt(i) << "] > " << seq.size() << '\n'
                          << coord.showColouredBoard(discovery) << std::endl;
                return false;
                return false;
            }
            if (discovery[i] > 0) sum += discovery[i];
        }
    }
    long int expected_sum = seq.size() * (seq.size() - 1) / 2;
    if (sum > expected_sum)
    {
        std::cerr << __FUNCTION__ << ":" << __LINE__ << "Sum: " << sum
                  << " seq.size() == " << seq.size() << std::endl;
        std::cerr << "discovery:\n"
                  << coord.showColouredBoard(discovery) << std::endl;
        return false;
    }
    std::set<pti> visited;
    for (std::size_t i = 0; i < seq.size(); ++i)
    {
        if (seq[i] < 0 || seq[i] >= coord.getSize())
        {
            std::cerr << __FUNCTION__ << ":" << __LINE__ << " index: " << i
                      << " point: " << coord.showPt(seq[i]) << '\n'
                      << coord.showColouredBoard(discovery) << std::endl;
            std::cerr << "seq: \n";
            for (auto el : seq) std::cerr << coord.showPt(el) << "  ";
            std::cerr << std::endl;
            return false;
        }
        if (!visited.contains(seq[i]) &&
            discovery[seq[i]] != static_cast<int>(i))
        {
            std::cerr << __FUNCTION__ << ":" << __LINE__ << " index: " << i
                      << " point: " << coord.showPt(seq[i]) << '\n'
                      << coord.showColouredBoard(discovery) << std::endl;
            std::cerr << "seq: size = " << seq.size() << "\n";
            for (auto el : seq) std::cerr << coord.showPt(el) << "  ";
            std::cerr << std::endl;
            return false;
        }
        visited.insert(seq[i]);
    }
    if (seq[0] != 0) return false;
    return true;
}

void DfsThreats::init(const SimpleGame& game, int who)
{
    dfs.player = who;
    dfs.AP(game, game.rectangle[who - 1].getLeftTop(),
           game.rectangle[who - 1].getBottomRight());
    in_terr.clear();
    in_terr.resize(coord.getSize(), 0);
    dfs.markTerritories(game, std::span<pti>(in_terr.begin(), in_terr.end()),
                        game.rectangle[who - 1].getLeftTop(),
                        game.rectangle[who - 1].getBottomRight());
    aencls = dfs.findAllAnnotatedEncl();
    in_encl.clear();
    in_encl.resize(coord.getSize(), 0);
    in_border.clear();
    in_border.resize(coord.getSize(), 0);

    for (const auto& en : aencls)
    {
        for (const auto p : en.encl.border) ++in_border[p];
        for (const auto p : en.encl.interior) ++in_encl[p];
    }
}

bool DfsThreats::ourNewDotMayChangeEncls(const SimpleGame& game, pti ind,
                                         int who) const
{
    if (!game.rectangle[who - 1].contains(ind)) return false;
    if (in_border[ind]) return true;

    return true;
}

void DfsThreats::makeEnclosure(const Enclosure& encl, int who)
{
    std::cout << "Making encl at " << coord.showPt(encl.border[0]) << " for "
              << who << std::endl;
    std::cout << __LINE__ << ": Sizes of aencls: " << aencls.size()
              << " aps: " << dfs.aps.size() << std::endl;
    if (who != dfs.player)
    {
        // temporarily mark interior
        constexpr pti term = 14000;
        for (const auto p : encl.interior) in_terr[p] += term;
        constexpr pti invalid_where = -4;
        aencls.erase(
            std::remove_if(
                aencls.begin(), aencls.end(),
                [&encl, this](const auto& aencl)
                {
                    auto isInInterior = [&](pti p)
                    {
                        constexpr pti threshold = 7000;
                        return in_terr[p] > threshold;
                    };

                    if (std::ranges::none_of(aencl.encl.border, isInInterior))
                        return false;
                    for (auto p : aencl.encl.border)
                    {
                        if (isInInterior(p))
                        {
                            std::cout << "Encl at " << coord.showPt(aencl.where)
                                      << " has a border point inside "
                                      << coord.showPt(p) << std::endl;
                            break;
                        }
                    }
                    for (const auto p : aencl.encl.interior)
                    {
                        --in_encl[p];
                    }
                    for (const auto p : aencl.encl.border)
                    {
                        --in_border[p];
                    }
                    // remove corresponding AP
                    for (auto& ap : dfs.aps)
                    {
                        if (aencl.isEnclSame(ap.where, dfs.seq[ap.seq0],
                                             ap.seq1 - ap.seq0))
                        {
                            ap.where = invalid_where;
                            break;
                        }
                    }
                    return true;
                }),
            aencls.end());
        // unmark interior
        for (const auto p : encl.interior) in_terr[p] -= term;
        // remove aps corresponding to removed aencls
        dfs.aps.erase(std::remove_if(dfs.aps.begin(), dfs.aps.end(),
                                     [invalid_where](const auto& ap)
                                     { return (ap.where == invalid_where); }),
                      dfs.aps.end());
        if (std::ranges::all_of(encl.border,
                                [&](pti p) { return in_terr[p] != 0; }))
        {
            for (const auto p : encl.interior)
            {
                in_terr[p] = 1;
            }
        }
        std::cout << __LINE__ << ": Sizes of aencls: " << aencls.size()
                  << " aps: " << dfs.aps.size() << std::endl;
        return;
    }
    // our new dot...
    /*
    if (in_terr[encl.border[0]] == 0)
    {
        // enclosure is not inside our territory, so its inside is not our territory either
        for (const auto p : encl.interior) in_terr[p] = 0;
    }
    */
}

void DfsThreats::placeDot(const SimpleGame& game, pti ind, int who)
{
    if (who != dfs.player)
    {
        dfs.aps.erase(std::remove_if(dfs.aps.begin(), dfs.aps.end(),
                                     [ind, this](const auto& ap)
                                     { return (ap.where == ind); }),
                      dfs.aps.end());
        aencls.erase(std::remove_if(aencls.begin(), aencls.end(),
                                    [ind, this](const auto& aencl)
                                    {
                                        if (aencl.where != ind) return false;
                                        for (const auto p : aencl.encl.interior)
                                        {
                                            --in_encl[p];
                                        }
                                        for (const auto p : aencl.encl.border)
                                        {
                                            --in_border[p];
                                        }
                                        return true;
                                    }),
                     aencls.end());
        return;
    }
    // our new dot...
    if (!ourNewDotMayChangeEncls(game, ind, who)) return;
    // recreate dfs
    dfs.AP(game, game.rectangle[who - 1].getLeftTop(),
           game.rectangle[who - 1].getBottomRight());
    in_terr.clear();
    in_terr.resize(coord.getSize(), 0);
    dfs.markTerritories(game, std::span<pti>(in_terr.begin(), in_terr.end()),
                        game.rectangle[who - 1].getLeftTop(),
                        game.rectangle[who - 1].getBottomRight());
    auto cache = std::move(aencls);
    aencls.clear();
    aencls.reserve(dfs.aps.size());
    constexpr pti invalid_where = -4;
    for (const auto& ap : dfs.aps)
    {
        auto it = std::ranges::find_if(cache,
                                       [&](const auto& ann) {
                                           return ann.isEnclSame(
                                               ap.where, dfs.seq[ap.seq0],
                                               ap.seq1 - ap.seq0);
                                       });
        if (it != cache.end())
        {
            aencls.push_back(std::move(*it));
            it->where = invalid_where;
        }
        else
        {
            auto aencl = dfs.findAnnotatedEnclosure(ap);
            for (const auto p : aencl.encl.border) ++in_border[p];
            for (const auto p : aencl.encl.interior) ++in_encl[p];
            aencls.push_back(std::move(aencl));
        }
    }
    // remove remaining (invalidated) encls from cache
    for (const auto& ae : cache)
    {
        if (ae.where == invalid_where) continue;
        for (const auto p : ae.encl.border) --in_border[p];
        for (const auto p : ae.encl.interior) --in_encl[p];
    }
}

void DfsThreats::makeMove(const SimpleGame& game, const Move& move)
{
    placeDot(game, move.ind, move.who);
    for (const auto& encl : move.enclosures)
    {
        makeEnclosure(*encl, move.who);
    }
}

bool DfsThreats::operator==(const DfsThreats& other) const
{
    if (dfs.player != other.dfs.player)
    {
        std::cout << __FUNCTION__ << ":" << __LINE__ << std::endl;
        return false;
    }
    if (aencls.size() != other.aencls.size())
    {
        std::cout << __FUNCTION__ << ":" << __LINE__ << std::endl;
        return false;
    }

    if (!std::ranges::equal(in_terr, other.in_terr))
    {
        std::cout << __FUNCTION__ << ":" << __LINE__ << std::endl;
        for (std::size_t i = 0; i < in_terr.size(); ++i)
        {
            if (in_terr[i] != other.in_terr[i])
            {
                std::cout << __FUNCTION__ << ":" << __LINE__
                          << " at point: " << coord.showPt(i) << " -- "
                          << "this in_terr: " << in_terr[i]
                          << ", other in_terr: " << other.in_terr[i] << '\n';
            }
        }
        return false;
    }

    if (!std::ranges::equal(in_encl, other.in_encl))
    {
        std::cout << __FUNCTION__ << ":" << __LINE__ << std::endl;
        return false;
    }

    if (!std::ranges::equal(in_border, other.in_border))
    {
        std::cout << __FUNCTION__ << ":" << __LINE__ << std::endl;
        return false;
    }

    for (const auto& ae : other.aencls)
    {
        const auto it = std::ranges::find_if(
            aencls,
            [&](const auto& a)
            {
                return a.where == ae.where &&
                       a.interior_near_where == ae.interior_near_where &&
                       //a.seq_length == ae.seq_length &&
                       std::set<pti>(a.encl.interior.begin(),
                                     a.encl.interior.end()) ==
                           std::set<pti>(ae.encl.interior.begin(),
                                         ae.encl.interior.end()) &&
                       std::set<pti>(a.encl.border.begin(),
                                     a.encl.border.end()) ==
                           std::set<pti>(ae.encl.border.begin(),
                                         ae.encl.border.end());
            });
        if (it == aencls.end())
        {
            std::cout << __FUNCTION__ << ":" << __LINE__ << std::endl;
            std::cout << "Could not find aencl with where = "
                      << coord.showPt(ae.where) << " interior_near_where = "
                      << coord.showPt(ae.interior_near_where)
                      << " seq_length = " << ae.seq_length << std::endl;
            std::cout << coord.showColouredBoard(ae.encl.border) << std::endl;
            return false;
        }
    }
    return true;
}
