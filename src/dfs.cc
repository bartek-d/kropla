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
#include <span>

#include "simplegame.h"

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
    // std::cout << "discovery at start:\n"
    //         << coord.showColouredBoard(discovery) << std::endl;
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
      //       << coord.showColouredBoard(discovery) << std::endl;

    pti fakeSource = 0;
    low[fakeSource] = discovery[fakeSource] = seq.size();
    seq.push_back(fakeSource);

    dfsAP(game, left_top, fakeSource);
    assert(checkInvariants(game, left_top, bottom_right));
}

std::vector<Enclosure> OnePlayerDfs::findAllEnclosures()
{
    std::vector<Enclosure> encls;
    encls.reserve(aps.size());
    auto& marks = low;
    for (const auto& ap : aps)
    {
        auto border = findBorder(ap);
        std::vector<pti> interior(seq.begin() + ap.seq0, seq.begin() + ap.seq1);
        if (interior.size() > 6)
        {
            // std::cout << coord.showColouredBoard(marks) << std::endl;
            for (auto el : border) marks[el] = -1;
            for (auto el : interior) marks[el] = -1;
            //		std::cout << coord.showColouredBoard(marks) <<
            //std::endl;

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
                        std::cout << coord.showColouredBoard(marks)
                                  << std::endl;
                        std::cout << "AP: " << coord.showPt(ap.where)
                                  << " seq: " << ap.seq0 << " - " << ap.seq1
                                  << std::endl;
                        std::cout << "Wnetrze: ";
                        for (auto el : interior)
                            std::cout << " " << coord.showPt(el);
                        std::cout << "\nBrzeg: ";
                        for (auto el : border)
                            std::cout << " " << coord.showPt(el);
                        std::cout << std::endl;
                        throw;
                    };
                }
            }
            // cleanup
            for (auto p : border) marks[p] = 0;
            for (auto p : interior) marks[p] = 0;
        }
        // add enclosure
        encls.emplace_back(std::move(interior), std::move(border));
    }
    return encls;
}

bool OnePlayerDfs::checkInvariants(const SimpleGame& game, pti left_top, pti bottom_right) const
{
  /*
    discovery:
       -1 == player dot or territory
      > 0 == opponent's dot or empty point outside player's territory
        0 == outside RECTANGLE
      discovery.size() == coord.getSize()
    seq:
       seq[0] == 0 -- fake source
       seq[1], ..., seq[N] -- subsequently discovered points
      Invariants:
       discovery[seq[k]] == k
       discovery.size() < coord.getSize()
    low:
      == 0 for player dot, territory, or outside RECTANGLE
      > 0  for opp's dot or empty point outside player's territory
      Used only to find aps, and then used as a buffer.
      low.size() == coord.getSize()
  */
  const auto x1 = coord.x[left_top];
  const auto y1 = coord.y[left_top];
  const auto x2 = coord.x[bottom_right];
  const auto y2 = coord.y[bottom_right];
  auto insideRectangle = [=](auto i) { return (coord.x[i] >= x1 && coord.x[i] <= x2 && coord.y[i] >= y1 && coord.y[i] <= y2); };
  auto ourDotOrOutside = [&](pti u)
    {
        auto what = game.whoseDotMarginAt(u);
        return (what == SimpleGame::MASK_DOT || what == player);
    };
  //std::cerr << "Player: " << player << '\n' << coord.showBoard(game.worm) << std::endl;
    
  long int sum = 0;
  for (int i = 0; i < coord.getSize(); ++i)
    {
      if (!insideRectangle(i))
	{
	  if (discovery[i] != 0)
	    {
	      std::cerr << "discovery[" << coord.showPt(i) << "] != 0\n" << coord.showColouredBoard(discovery)
                                  << std::endl; 
	      return false;
	    }
	  if (low[i] != 0) {
	      std::cerr << "low[" << coord.showPt(i) << "] != 0\n" << coord.showColouredBoard(low)
                                  << std::endl; 

	    return false;
	  }
	}
      else
	{
	  if (i != left_top)  // left_top is exceptional
	    {
	      if (discovery[i] == 0) {
		std::cerr << "discovery[" << coord.showPt(i) << "] == 0\n" << coord.showColouredBoard(discovery)
			  << std::endl; 
		return false;
	      }
	      if (discovery[i] > 0 && ourDotOrOutside(i)) {
		std::cerr << "discovery[" << coord.showPt(i) << "] > 0\n" << coord.showColouredBoard(discovery)
			  << std::endl; 
		return false;
	      }
	      if (low[i] > 0 && ourDotOrOutside(i)) {
		std::cerr << "low[" << coord.showPt(i) << "] > 0\n" << coord.showColouredBoard(low)
			  << std::endl; 
		return false;
	      }
	    }
	  if (discovery[i] > static_cast<pti>(seq.size())) {
	    return false;
	  }
	  if (discovery[i] > 0) sum += discovery[i];
	}
    }
  long int expected_sum = seq.size() * (seq.size() - 1) / 2;
  if (sum != expected_sum) {
    std::cerr << "Sum: " << sum << " seq.size() == " << seq.size() << std::endl;
    std::cerr << "discovery:\n" << coord.showColouredBoard(discovery)
	      << std::endl; 
    return false;
  }
  for (std::size_t i = 0; i < seq.size(); ++i)
    {
      if (seq[i] < 0 || seq[i] >= coord.getSize()) return false;
      if (discovery[seq[i]] != static_cast<int>(i)) return false;
    }
  if (seq[0] != 0) return false;
  return true;
}
