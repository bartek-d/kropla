/********************************************************************************************************
 kropla -- a program to play Kropki; file game.cc -- main file.
    Copyright (C) 2015,2016,2017,2018,2019,2020,2021,2025 Bartek Dyda,
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
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>  // abs()
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>  // ?
#include <map>
#include <memory>  // unique pointer
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>
//#include <exception>
#include <cctype>  // iswhite()
#include <chrono>  // chrono::high_resolution_clock, only to measure elapsed time
#include <stdexcept>

#include "board.h"
#include "command.h"
#include "dfs.h"
#include "enclosure.h"
#include "game.h"
#include "group_neighbours.h"
#include "montecarlo.h"
#include "patterns.h"
#include "sgf.h"
#include "threats.h"

Coord coord(15, 15);

std::chrono::high_resolution_clock::time_point start_time =
    std::chrono::high_resolution_clock::now();  // to measure time, value only
                                                // to use auto
long long debug_nanos = 0;
long long debug_nanos2 = 0;
long long debug_nanos3 = 0;
int debug_previous_count = 0;
thread_local std::default_random_engine Game::engine;
thread_local std::stringstream Game::out;

namespace global
{
const Pattern3 patt3(global::program_path + "patterns.bin", 0);
const Pattern3 patt3_symm(
    {// hane pattern - enclosing hane
     "XOX"
     ".H."
     "?o?",
     "6",  // note: ?O? is contained in other part
           // keima cut
     "?XO"
     "OHX"
     "???",
     "5",
     // 'keima' cut with two dots O on E and W
     "?XO"
     "OHO"
     "...",
     "6",
     //
     "?XO"
     "OHO"
     "O..",
     "4",
     //
     "?XO"
     "OHO"
     "..O",
     "4",
     //
     "?XO"
     "OHO"
     ".O.",
     "4",
     //
     "?X?"
     "OHO"
     "?X?",
     "7",  // more general then just keima cut
           //
     "?X?"
     "OHO"
     "X??",
     "7",  // more general then just keima cut; contains "??X" by symmetry
           // keima/one space jump on the edge
     "?XO"
     "OHx"
     "###",
     "6",
     //
     "?X?"
     "OHO"
     "###",
     "6",
     // keima cut with empty at E
     "?XO"
     "OH."
     "...",
     "5",
     //
     "?XO"
     "OH."
     "X.?",
     "7",
     //
     "?XO"
     "OH."
     "?XX",
     "5",
     //
     "?XO"
     "OH."
     "?XO",
     "7",
     //
     "?XO"
     "OH."
     "?X.",
     "6",
     //
     "?XO"
     "OH."
     "XO?",
     "6",
     //
     "?XO"
     "OH."
     ".OX",
     "2",  // ???
           // bambus moves ... -- TODO
           //
           // cut between diagonal jump
     "?.O"
     ".H."
     "O.?",
     "5",
     //
     "?XO"
     ".H."
     "O.?",
     "6",  // maybe too much?
           //
     "?XO"
     "XH."
     "O.O",
     "4",
     // two keimas
     "X.O"
     "OHX"
     "???",
     "2",
     // cut -- warning: contains some previous shapes
     "oOo"
     ".H."
     "oOo",
     "4",
     //
     "?Oo"
     "XH."
     "?O?",
     "6",  // 'o' in the corner to rule out bambus
           //
     "?O?"
     "XHX"
     "?O?",
     "7",
     //
     "?Oo"
     "XHO"
     "?Oo",
     "5",
     // diagonal cut (contains some previous shapes)
     "XO?"
     "OH?"
     "???",
     "3"});

int komi;
int komi_ratchet;
std::string program_path;
}  // namespace global

/********************************************************************************************************
  Monte Carlo constants.
*********************************************************************************************************/
const constexpr real_t MC_SIMS_EQUIV_RECIPR = 1.0 / 400.0;  // originally 2500!
const constexpr real_t MC_SIMS_ENCL_EQUIV_RECIPR = 1.0 / 20.0;

/********************************************************************************************************
  Worm description class
*********************************************************************************************************/

std::string WormDescr::show() const
{
    std::stringstream out;
    out << "dots={" << dots[0] << ", " << dots[1] << "}, leftmost=" << leftmost
        << ", group_id=" << group_id << ", safety=" << safety;
    return out.str();
}

/********************************************************************************************************
  Some helper functions
*********************************************************************************************************/

void addOppThreatZobrist(std::vector<uint64_t> &v, uint64_t z)
{
    if (v.empty())
    {
        v.push_back(z);
    }
    else if (v.back() == z)
    {
        return;
    }
    else
    {
        auto pos = std::find(v.begin(), v.end(), z);
        if (pos == v.end())
        {
            v.push_back(z);
        }
        else
        {
            std::swap(*pos, v.back());  // to save time on future function calls
        }
    }
}

void removeOppThreatZobrist(std::vector<uint64_t> &v, uint64_t z)
{
    auto pos = std::find(v.begin(), v.end(), z);
    if (pos != v.end())
    {
        *pos = v.back();
        v.pop_back();
    }
}

/********************************************************************************************************
  Diagonal connections class
*********************************************************************************************************/
/*
struct DiagConn {
  pti p1, p2;   // always p1<p2
};

class Connections {
  std::set<DiagConn> diagonal[2];  // for player 1, 2
  // bamboo? other?
};
*/

/********************************************************************************************************
  Movestats class for handling Monte Carlo tree.
*********************************************************************************************************/

const Movestats &Movestats::operator+=(const Movestats &other)
{
    playouts += other.playouts;
    auto other_sum = other.value_sum.load();
    value_sum = value_sum.load() + other_sum;
    return *this;
}

const Movestats &Movestats::operator+=(const NonatomicMovestats &other)
{
    playouts += other.playouts;
    value_sum = value_sum.load() + other.value_sum;
    return *this;
}

Movestats &Movestats::operator=(const Movestats &other)
{
    playouts = other.playouts.load();
    value_sum = other.value_sum.load();
    return *this;
}

Movestats &Movestats::operator=(const NonatomicMovestats &other)
{
    playouts = other.playouts;
    value_sum = other.value_sum;
    return *this;
}

bool Movestats::operator<(const Movestats &other) const
{
    if (other.playouts == 0) return false;
    if (playouts == 0) return true;
    return (value_sum / playouts < other.value_sum / other.playouts);
}

std::string Movestats::show() const
{
    std::stringstream out;
    out << " v=" << (playouts > 0 ? value_sum / playouts : 0.0)
        << " sim=" << playouts;
    return out.str();
}

const NonatomicMovestats &NonatomicMovestats::operator+=(
    const NonatomicMovestats &other)
{
    playouts += other.playouts;
    value_sum += other.value_sum;
    return *this;
}

void NonatomicMovestats::normaliseTo(int32_t N)
{
    if (N < 0) return;
    if (playouts == 0)
    {
        playouts = N;
        value_sum = 0.5f * N;
        return;
    }
    const float norm = static_cast<float>(N) / playouts;
    playouts = N;
    value_sum *= norm;
}

std::string NonatomicMovestats::show() const
{
    std::stringstream out;
    out << " v=" << (playouts > 0 ? value_sum / playouts : 0.0)
        << " sim=" << playouts;
    return out.str();
}

NonatomicMovestats wonSimulations(int n)
{
    return NonatomicMovestats{
        n, static_cast<decltype(NonatomicMovestats::value_sum)>(n)};
}
NonatomicMovestats lostSimulations(int n)
{
    return NonatomicMovestats{n, 0.0f};
}
NonatomicMovestats defaultInitialPriors()
{
    return NonatomicMovestats{30, 15.0f};
}

/********************************************************************************************************
  Treenode class for handling Monte Carlo tree.
*********************************************************************************************************/

Treenode &Treenode::operator=(const Treenode &other)
{
    parent = other.parent;
    children = other.children.load();
    t = other.t;
    amaf = other.amaf;
    prior = other.prior;
    move = other.move;
    flags = other.flags;
    cnn_prob = other.cnn_prob;
    return *this;
}

real_t Treenode::getValue() const
{
    real_t value;
    real_t ucb_term = 0.0;
    if (true)  // parent != this)    // not at root
    {
        const uint32_t N = parent->t.playouts - parent->prior.playouts;
        const uint32_t n = t.playouts - prior.playouts;
        const bool is_root = parent == this;
        const real_t C = is_root ? 0.4 : 0.14;
        ucb_term = C * std::sqrt(std::log(N + 1) / (n + 0.1));
    }
    if (t.playouts > 0 and amaf.playouts > 0)
    {
        const real_t factor = getDepth() <= 2 ? (3.0f - getDepth()) : 1.0f;
        const real_t mc_sims_equiv =
            factor * (move.enclosures.empty() ? MC_SIMS_EQUIV_RECIPR
                                              : MC_SIMS_ENCL_EQUIV_RECIPR);
        real_t beta =
            amaf.playouts / (amaf.playouts + t.playouts +
                             t.playouts * mc_sims_equiv * amaf.playouts);
        value = beta * amaf.value_sum / amaf.playouts +
                (1 - beta) * t.value_sum / t.playouts;
    }
    else
    {
        if (t.playouts > 0)
        {
            value = t.value_sum / t.playouts;
        }
        else
        {
            value = amaf.value_sum / amaf.playouts;
        }
    }
    return value + ucb_term + (isInsideTerrNoAtari() ? -0.02 : 0.0);
}

bool Treenode::operator<(const Treenode &other) const
{
    // return t < other.t;
    return getValue() < other.getValue();
}

const Treenode *Treenode::getBestChild() const
{
    if (children == nullptr) return nullptr;
    int max = 0;
    Treenode *best = nullptr;
    for (Treenode *ch = children; true; ++ch)
    {
        if (ch->t.playouts - ch->prior.playouts > max)
        {
            max = ch->t.playouts - ch->prior.playouts;
            best = ch;
        }
        if (ch->isLast()) break;
    }
    return best;
}

std::string Treenode::show() const
{
    return move.show() + " " + t.show() + ", prior: " + prior.show() +
           ", amaf: " + amaf.show() + ", flags: " + std::to_string(flags) +
           ", cnn: " + std::to_string(cnn_prob);
}

std::string Treenode::showParents() const
{
    if (this->parent == this) return "[root]";
    std::string s;
    for (Treenode *node = const_cast<Treenode *>(this); node->parent != node;
         node = node->parent)
    {
        s = node->move.show() + " " + s;
    }
    return s;
}

std::string Treenode::getMoveSgf() const
{
    return std::string(";") + toString(move.toSgfString());
}

/********************************************************************************************************
  TreenodeAllocator class for thread-safe memory allocation in Monte Carlo.
*********************************************************************************************************/
TreenodeAllocator::TreenodeAllocator()
{
    pools.push_back(new Treenode[pool_size]);
    min_block_size = 3 * coord.wlkx * coord.wlky;
    last_block_start = 0;
    cursor = 0;
}

TreenodeAllocator::~TreenodeAllocator()
{
    std::cerr << "Memory use (Treenode) " << pools.size() << " * " << pool_size
              << " * " << sizeof(Treenode) << ";  in last pool: " << cursor
              << std::endl;
    for (auto &el : pools)
    {
        delete[] el;
    }
    pools.clear();
}

/// Returns pointer to the next (free) element.
Treenode *TreenodeAllocator::getNext()
{
    if (cursor == pool_size)
    {
        assert(last_block_start > 0);  // otherwise our pools are too small
        // reallocate
        Treenode *newt = new Treenode[pool_size];
        std::copy(&pools.back()[last_block_start],
                  &pools.back()[last_block_start] + cursor - last_block_start,
                  newt);
        pools.push_back(newt);
        cursor = pool_size - last_block_start;
        last_block_start = 0;
    }
    //  pools.back()[cursor] = Treenode();
    return &pools.back()[cursor++];
}

/// Returns pointer to the last block and do not change anything
Treenode *TreenodeAllocator::getLastBlockWithoutResetting() const
{
    if (cursor == last_block_start) return nullptr;
    return &pools.back()[last_block_start];
}

/// Returns pointer to the last block, and resets the last block.
Treenode *TreenodeAllocator::getLastBlock()
{
    if (cursor == last_block_start)
        return nullptr;
    else
    {
        Treenode *res = &pools.back()[last_block_start];
        pools.back()[cursor - 1].markAsLast();
        if (last_block_start + min_block_size < pool_size)
        {
            last_block_start = cursor;
        }
        else
        {
            pools.push_back(new Treenode[pool_size]);
            last_block_start = 0;
            cursor = 0;
        }
        return res;
    }
}

void TreenodeAllocator::copyPrevious()
{
    assert(cursor >= last_block_start + 2);
    pools.back()[cursor - 1] = pools.back()[cursor - 2];
}

int TreenodeAllocator::getSize(Treenode *ch)
{
    int n = 0;
    if (ch != nullptr)
    {
        for (;;)
        {
            ++n;
            if (ch->isLast()) break;
            ++ch;
        }
    }
    return n;
}

/********************************************************************************************************
  ListOfPlaces -- helper class for PossibleMoves
*********************************************************************************************************/
/*
class ListOfPlaces {
  std::vector<pti> list;
public:
  ListOfPlaces();
  void addPoint(pti p);
  void removePoint(pti p);
  bool isOnList(pti p) const;
};

ListOfPlaces::ListOfPlaces()
{
  list.reserve(coord.getSize());
}


void
ListOfPlaces::addPoint(pti p)
{
  assert(std::find(list.begin(), list.end(), p) == list.end());
  list.push_back(p);
}

void
ListOfPlaces::removePoint(pti p)
{
  auto pos = std::find(list.begin(), list.end(), p);
  if (pos != list.end()) {
    *pos = list.back();
    list.pop_back();
  }
}

bool
ListOfPlaces::isOnList(pti p) const
{
  return (std::find(list.begin(), list.end(), p) != list.end());
}

*/

/********************************************************************************************************
  PossibleMoves class for keeping track of possible moves
*********************************************************************************************************/

/// This function removes move p from 'lists', but does not update mtype[p].
void PossibleMoves::removeFromList(pti p)
{
    int curr_list = mtype[p] >> PossibleMovesConsts::MASK_SHIFT;
    if (curr_list < 3)
    {
        unsigned int ind = (mtype[p] & PossibleMovesConsts::INDEX_MASK);
        if (ind + 1 < lists[curr_list].size())
        {
            // p is not the last element on the curr_list, so replace it by the
            // last one
            pti last = lists[curr_list].back();
            lists[curr_list][ind] = last;
            mtype[last] = (curr_list << PossibleMovesConsts::MASK_SHIFT) | ind;
        }
        lists[curr_list].pop_back();
    }
}

void PossibleMoves::newDotOnEdge(pti p, EdgeType edge)
{
    int ind, iter, count = 0;
    switch (edge)
    {
        case EdgeType::LEFT:
            if (left)
            {
                ind = coord.ind(0, 1);
                iter = coord.S;
                count = coord.wlky - 2;
                left = false;
            }
            break;
        case EdgeType::RIGHT:
            if (right)
            {
                ind = coord.ind(coord.wlkx - 1, 1);
                iter = coord.S;
                count = coord.wlky - 2;
                right = false;
            }
            break;
        case EdgeType::TOP:
            if (top)
            {
                ind = coord.ind(1, 0);
                iter = coord.E;
                count = coord.wlkx - 2;
                top = false;
            }
            break;
        case EdgeType::BOTTOM:
            if (bottom)
            {
                ind = coord.ind(1, coord.wlky - 1);
                iter = coord.E;
                count = coord.wlkx - 2;
                bottom = false;
            }
            break;
    }
    // change dame moves on the edge to neutral
    for (; count != 0; --count)
    {
        if (ind != p) changeMove(ind, PossibleMovesConsts::NEUTRAL);
        ind += iter;
    }
}

void PossibleMoves::generate()
{
    lists[0].reserve(coord.last);
    lists[1].reserve(coord.last);
    lists[2].reserve(coord.last);
    mtype = std::vector<pti>(coord.getSize(), PossibleMovesConsts::REMOVED);
    for (int i = coord.first; i <= coord.last; ++i)
    {
        if (coord.dist[i] > 0)
        {
            mtype[i] = PossibleMovesConsts::NEUTRAL |
                       lists[PossibleMovesConsts::LIST_NEUTRAL].size();
            lists[PossibleMovesConsts::LIST_NEUTRAL].push_back(i);
        }
        else if (coord.dist[i] == 0)
        {
            mtype[i] = PossibleMovesConsts::DAME |
                       lists[PossibleMovesConsts::LIST_DAME].size();
            lists[PossibleMovesConsts::LIST_DAME].push_back(i);
        }
    }
    left = true;
    top = true;
    right = true;
    bottom = true;
}

/// new_type should be NEUTRAL, DAME, TERRM or REMOVED
void PossibleMoves::changeMove(pti p, int new_type)
{
    if (new_type != PossibleMovesConsts::REMOVED)
    {
        auto curr_type = mtype[p] & PossibleMovesConsts::TYPE_MASK;
        if (curr_type == new_type) return;
        removeFromList(p);
        mtype[p] = lists[new_type >> PossibleMovesConsts::MASK_SHIFT].size() |
                   new_type;
        lists[new_type >> PossibleMovesConsts::MASK_SHIFT].push_back(p);
    }
    else
    {
        removeFromList(p);
        mtype[p] = PossibleMovesConsts::REMOVED;
        // check where is the new dot
        int x = coord.x[p], y = coord.y[p];
        if (x == 1 || (x == 0 and y != 0 and y != coord.wlky - 1))
        {
            newDotOnEdge(p, EdgeType::LEFT);
        }
        else if (x == coord.wlkx - 2 ||
                 (x == coord.wlkx - 1 and y != 0 and y != coord.wlky - 1))
        {
            newDotOnEdge(p, EdgeType::RIGHT);
        }
        if (y == 1 || (y == 0 and x != 0 and x != coord.wlkx - 1))
        {
            newDotOnEdge(p, EdgeType::TOP);
        }
        else if (y == coord.wlky - 2 ||
                 (y == coord.wlky - 1 and x != 0 and x != coord.wlkx - 1))
        {
            newDotOnEdge(p, EdgeType::BOTTOM);
        }
    }
}

/********************************************************************************************************
  InterestingMoves -- class for keeping track of 3 lists of interesting moves,
very similar to PossibleMoves
*********************************************************************************************************/
/// This function removes move p from 'lists', but does not update mtype[p].
void InterestingMoves::removeFromList(pti p)
{
    int curr_list = mtype[p] >> InterestingMovesConsts::MASK_SHIFT;
    if (curr_list < 3)
    {
        unsigned int ind = (mtype[p] & InterestingMovesConsts::INDEX_MASK);
        if (ind + 1 < lists[curr_list].size())
        {
            // p is not the last element on the curr_list, so replace it by the
            // last one
            pti last = lists[curr_list].back();
            lists[curr_list][ind] = last;
            mtype[last] =
                (curr_list << InterestingMovesConsts::MASK_SHIFT) | ind;
        }
        lists[curr_list].pop_back();
    }
}

void InterestingMoves::generate()
{
    lists[0].reserve(coord.last);
    lists[1].reserve(coord.last);
    lists[2].reserve(coord.last);
    mtype = std::vector<pti>(coord.getSize(), InterestingMovesConsts::REMOVED);
}

/// new_type should be MOVE_0, MOVE_1, MOVE_2 or REMOVED
void InterestingMoves::changeMove(pti p, int new_type)
{
    assert(new_type == InterestingMovesConsts::MOVE_0 ||
           new_type == InterestingMovesConsts::MOVE_1 ||
           new_type == InterestingMovesConsts::MOVE_2 ||
           new_type == InterestingMovesConsts::REMOVED);
    assert(p >= coord.first and p <= coord.last);
    if (new_type != InterestingMovesConsts::REMOVED)
    {
        auto curr_type = mtype[p] & InterestingMovesConsts::TYPE_MASK;
        if (curr_type == new_type) return;
        removeFromList(p);
        mtype[p] =
            lists[new_type >> InterestingMovesConsts::MASK_SHIFT].size() |
            new_type;
        lists[new_type >> InterestingMovesConsts::MASK_SHIFT].push_back(p);
    }
    else
    {
        removeFromList(p);
        mtype[p] = InterestingMovesConsts::REMOVED;
    }
}

int InterestingMoves::classOfMove(pti p) const
{
    int curr_list = mtype[p] >> InterestingMovesConsts::MASK_SHIFT;
    return 3 - curr_list;
}

/********************************************************************************************************
  Game class
*********************************************************************************************************/

#ifdef DEBUG_SGF
SgfTree Game::sgf_tree;
#endif

Game::Game(SgfSequence seq, int max_moves, bool must_surround)
    : must_surround{must_surround}
{
    global::komi = 0;
    global::komi_ratchet = 10000;
    auto sz_pos = seq[0].findProp("SZ");
    std::string sz = (sz_pos != seq[0].props.end()) ? sz_pos->second[0] : "";
    if (sz.find(':') == std::string::npos)
    {
        if (sz != "")
        {
            int x = stoi(sz);
            coord.changeSize(x, x);
        }
    }
    else
    {
        std::string::size_type i = sz.find(':');
        int x = stoi(sz.substr(0, i));
        int y = stoi(sz.substr(i + 1));
        coord.changeSize(x, y);
    }
#ifdef DEBUG_SGF
    sgf_tree.changeBoardSize(coord.wlkx, coord.wlky);
#endif
    // reserve memory
    sg.reserveMemory();
    recalculate_list.reserve(coord.wlkx * coord.wlky);
    const pattern3_t empty_point = 0;
    pattern3_value[0] = std::vector<pattern3_val>(coord.getSize(), 0);
    pattern3_value[1] = std::vector<pattern3_val>(coord.getSize(), 0);
    pattern3_at = std::vector<pattern3_t>(coord.getSize(), empty_point);
    threats[0] = AllThreats();
    threats[1] = AllThreats();
    possible_moves.generate();
    interesting_moves.generate();

    // prepare patterns, taken from Pachi (playout/moggy.c)
    // (these may be pre-calculated)
    /*
    global::patt3.generate({
          // hane pattern - enclosing hane
          "XOX"
          ".H."
          "???",  "52",
          // hane pattern - non-cutting hane
          "YO."
          ".H."
          "?.?",  "53",
          // hane pattern - magari 		// 0.32
          "XO?"
          "XH."
          "x.?",  "32",  // TODO: empty triange for O possible, when upper ? ==
    O
          // hane pattern - thin hane 		// 0.22
          "XOO"
          ".H."
          "?.?" "X",   "22",
          // generic pattern - katatsuke or diagonal attachment; similar to
    magari 	// 0.37
          ".Q."
          "YH."
          "...",  "37",   // TODO: is it good? seems like Q should be in corner
    for the diagonal attachment
          // cut1 pattern (kiri) - unprotected cut 	// 0.28
          "XOo"
          "OHo"
          "###",  "28",
          //
          "XO?"
          "OHo"
          "*o*",  "28",
          // cut1 pattern (kiri) - peeped cut 	// 0.21
          "XO?"
          "OHX"
          "???",  "21",
          // cut2 pattern (de) 			// 0.19
          "?X?"
          "OHO"
          "ooo",   "19",
          // cut keima (not in Mogo) 		// 0.82
          "OX?"
          "oHO"   // original was: "?.O", but if ?=O, then it's not a cut
          "?o?",  "52",  // oo? has some pathological tsumego cases
          // side pattern - block side cut 	// 0.20
          "OX?"
          "XHO"
          "###",  "20",
          // different dame moves
          "X?O"
          "XH?"
          "XXX",  "-2000",
          // dame
          "?OO"
          "XHO"
          "XX?",  "-2000",
          // dame
          "X?O"
          "XHO"
          "X?O",  "-2000",
          // dame
          "XX?"
          "XHO"
          "XX?",  "-2000",
          // edge dame
          "X?O"
          "XHO"
          "###",  "-2000",
          // edge dame
          "XX?"
          "XH?"
          "###",  "-2000",
          // edge dame
          "X?O"
          "XHO"
          "###",  "-2000",
          // corner -- always dame
          "#??"
          "#H?"
          "###",  "-2000",
          });
    */
    // global::patt3.showCode();  <-- to precalculate

    for (int i = coord.first; i <= coord.last; ++i)
    {
        if (coord.dist[i] == 0)
        {
            pattern3recalculatePoint(i);
        }
    }
    // process RU, AB, AW properties
    // (...)
    // make moves
    start_time = std::chrono::high_resolution_clock::now();
    replaySgfSequence(seq, max_moves);
}

void Game::replaySgfSequence(SgfSequence seq, int max_moves)
{
    for (auto &node : seq)
    {
        if (node.findProp("B") != node.props.end())
        {
            if (--max_moves < 0) break;
            for (auto &value : node.findProp("B")->second)
            {
                if (value.empty())
                    continue;  // Program kropki gives empty moves sometimes.
                makeSgfMove(value, 1);
            }
            sg.nowMoves = 2;
        }
        if (node.findProp("W") != node.props.end())
        {
            if (--max_moves < 0) break;
            for (auto &value : node.findProp("W")->second)
            {
                if (value.empty())
                    continue;  // Program kropki gives empty moves sometimes.
                makeSgfMove(value, 2);
            }
            sg.nowMoves = 1;
        }
        if (node.findProp("AB") != node.props.end())
        {
            for (auto &value : node.findProp("AB")->second)
            {
                makeSgfMove(value, 1);
            }
        }
        if (node.findProp("AW") != node.props.end())
        {
            for (auto &value : node.findProp("AW")->second)
            {
                makeSgfMove(value, 2);
            }
        }
    }
}

std::vector<pti> Game::findThreats_preDot(pti ind, int who)
// find possible new threats because of the (future) dot of who at [ind]
// Each possible threat is a pair of pti's, first denote a point where to play,
// second gives a code which neighbours may go inside.
{
    std::vector<pti> possible_threats;
    Threat *smallest_terr = nullptr;
    unsigned int smallest_size = coord.maxSize;
    // find smallest territory that [ind] is (if any), return immediately if we
    // are inside an 1-point-territory pool (so we cannot check for
    // ind==thr.where here, because of this side-effect!)
    if (isInTerr(ind, who))
    {
        int nthr = isInTerr(ind, who);
        for (auto &thr : threats[who - 1].threats)
        {
            if (thr.type & ThreatConsts::TERR)
            {
                if (thr.encl->isInInterior(ind))
                {
                    if (thr.encl->interior.size() == 1)
                    {
                        thr.type |= ThreatConsts::TO_REMOVE;
                        return possible_threats;
                    }
                    if (thr.encl->interior.size() < smallest_size)
                    {
                        smallest_terr = &thr;
                        smallest_size = thr.encl->interior.size();
                    }
                    --nthr;
#ifdef NDEBUG
                    if (nthr == 0) break;
#else
                    assert(nthr >= 0);
#endif
                }
            }
        }
    }
    // find groups in the neighbourhood
    std::array<pti, 4> groups = sg.getConnects(who - 1)[ind].groups_id;
    int top = sg.getConnects(who - 1)[ind].count();
    if (top == 0)
    {  // isolated dot cannot pose any threats
        return possible_threats;
    }
    // if inside TERR, then we want to know whether we make a new connection
    // between the border, so that the TERR splits into 2 or more smaller
    if (smallest_terr)
    {
        auto border_group =
            sg.descr.at(sg.worm[smallest_terr->encl->getBorderElement()])
                .group_id;
        int count = 0;
        for (int g = 0; g < top; g++) count += (groups[g] == border_group);
        if (count >= 2)
        {
            int encl_count = 0;
            for (auto &thr : threats[who - 1].threats)
            {
                if (thr.where == ind) encl_count++;
            }
            if (encl_count == count)
            {
                smallest_terr->type |=
                    ThreatConsts::TO_REMOVE;  // territory will split into 2+
                                              // new ones
            }
            else
            {
                // in this case the territory does not split
                // example situation, where O = last dot, the large territory of
                // 'o' is not divided into two, although there is a new smaller
                // one inside
                //   o o o   o o o
                // o       o       o
                // o x     o       o
                // o     o x o     o
                // o       O       o
                //   o           o
                //     o o o o o
                // Hence we leave the threat unchanged.
                /* debug:
                   std::cerr << "count = " << count << ", encl_count = " <<
                   encl_count << std::endl; std::cerr << "last move [" <<
                   sg.nowMoves << "] at " << coord.showPt(sg.getHistory().back()
                   & ~HISTORY_TERR) << std::endl; show(); assert(encl_count ==
                   count);
                */
            }
        }
        else if (count == 1)
        {  // and dots>1)  we could check if there's more than 1 our dot in the
           // neighbourhood, as only then we need to check
            // maybe it's just one dot, then interior will be the same
            int gdots = 0;
            for (int j = 0; j < 8; j++)
            {
                pti nb = ind + coord.nb8[j];
                if (isDotAt(nb) and
                    sg.descr.at(sg.worm[nb]).group_id == border_group)
                    gdots++;
            }
            if (gdots > 1)
                smallest_terr->type |=
                    ThreatConsts::TO_CHECK;  // territory may (will?) have now
                                             // smaller area
        }
    }
    // check our enclosures containing [ind]
    if (isInEncl(ind, who) || isInBorder(ind, who))
    {
        krb::SmallVector<Threat *, 32>::allocator_type::arena_type
            arena_this_encl;
        krb::SmallVector<Threat *, 32> this_encl{arena_this_encl};
        for (auto &thr : threats[who - 1].threats)
        {
            if (thr.type & ThreatConsts::ENCL)
            {
                if (thr.where == ind)
                {
                    thr.type ^=
                        (ThreatConsts::ENCL |
                         ThreatConsts::TERR);  // switch ENCL bit to TERR
                    thr.where = 0;
                    threats[who - 1].changeEnclToTerr(thr);
                    this_encl.push_back(&thr);
                    // thr.zobrist_key does not change
                }
                else if (thr.encl->isInInterior(ind))
                {
                    if (thr.encl->interior.size() == 1)
                    {
                        thr.type |= ThreatConsts::TO_REMOVE;
                    }
                    else
                    {
                        thr.type |= ThreatConsts::TO_CHECK;
                    }
                }
            }
        }
        // check enclosures inside our new territory
        if (!this_encl.empty())
        {
            std::vector<int8_t> this_interior(coord.last + 1, 0);
            for (auto tt : this_encl)
                for (auto i : tt->encl->interior) this_interior[i] = 1;
            for (auto &thr : threats[who - 1].threats)
            {
                if ((thr.type & ThreatConsts::ENCL) and
                    this_interior[thr.where])
                {
                    thr.type |= ThreatConsts::TO_CHECK;
                    // TODO: we could remove thr if it is almost the same (one
                    // of) current enclosure(s)  [almost: thr.interior + [ind]
                    // == curr_encl.interior ]
                }
            }
        }
    }
    // find other points which touch points from found groups, but do not touch
    // the new dot
    if (top >= 2)
    {
        auto debug_time = std::chrono::high_resolution_clock::now();

        std::array<pti, 4> unique_groups = {0, 0, 0, 0};
        int ug = sg.getConnects(who - 1)[ind].getUniqueGroups(unique_groups);
        if (ug >= 2)
        {
            for (int i = coord.first; i <= coord.last; i++)
            {
                if (sg.getConnects(who - 1)[i].groups_id[1] !=
                        0  // this point connects at least 2 groups
                    and !coord.isInNeighbourhood(
                            i, ind))  // and does not touch [ind]
                {
                    // check groups
                    int count = 0;
                    for (int k = 0; k < ug; k++)
                    {
                        for (int j = 0; j < 4; j++)
                            if (sg.getConnects(who - 1)[i].groups_id[j] ==
                                unique_groups[k])
                            {
                                count++;
                                break;
                            }
                    }
                    if (count >= 2)
                    {
                        possible_threats.push_back(i);
                        int w = 0;  // find the code for the neighbourhood
                        for (int j = 7; j >= 0; j--)
                        {
                            pti nb2 = i + coord.nb8[j];
                            if (whoseDotMarginAt(nb2) == who and
                                std::find(unique_groups.begin(),
                                          unique_groups.end(),
                                          sg.descr.at(sg.worm[nb2]).group_id) !=
                                    unique_groups.end())
                            {
                                w |= 1;
                            }
                            else if (whoseDotMarginAt(nb2) == 3)
                            {
                                w |= 3;  // outside the board
                            }
                            w <<= 2;
                        }
                        w >>= 2;
                        possible_threats.push_back(coord.connections_tab[w]);
                    }
                }
            }
        }

        debug_nanos +=
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now() - debug_time)
                .count();
    }
    // check neighbours of [ind]
    for (int i = 0; i < 8; i++)
    {
        pti nb = ind + coord.nb8[i];
        if (sg.getConnects(who - 1)[nb].groups_id[0] != 0)
        {               // there is some dot of who, and we're inside the board
            int w = 0;  // find the code for the neighbourhood
            for (int j = 7; j >= 0; j--)
            {
                pti nb2 = nb + coord.nb8[j];
                if ((nb2 == ind) ||
                    (whoseDotMarginAt(nb2) == who and
                     std::find(groups.begin(), groups.end(),
                               sg.descr.at(sg.worm[nb2]).group_id) !=
                         groups.end()))
                {
                    w |= 1;
                }
                else if (whoseDotMarginAt(nb2) == 3)
                {
                    w |= 3;  // outside the board
                }
                w <<= 2;
            }
            w >>= 2;
            if (coord.connections_tab[w])
            {
                possible_threats.push_back(nb);
                possible_threats.push_back(coord.connections_tab[w]);
            }
        }
    }

#ifndef NDEBUG
    /*
    std::cerr << "Zagrozen: " << possible_threats.size()/2 << " -- ";
    for (int i=0; i< possible_threats.size(); i+=2)
      std::cerr << coord.showPt(possible_threats[i]) << " ";
    std::cerr << std::endl;
    */
#endif

    return possible_threats;
}

/// helper function used in Game::findThreats2moves_preDot
/// @param[in] ind  Last dot.
/// @param[in] nb   Current dot.
/// @param[in] i    such that ind+coord.nb8[i]==nb
/// @param[in] who  Who encloses.
/// @return    A pair [left, right) -- we should iterate over j there and
/// consider nb+coord.nb8[j&7]
std::array<int, 2> Game::findThreats2moves_preDot__getRange(pti ind, pti nb,
                                                            int i,
                                                            int who) const
{
    assert(coord.nb8[0] == coord.NE and coord.nb8[7] == coord.N);
    int shortcutL =
        (whoseDotMarginAt(ind + coord.nb8[i + 7]) == who);  // (i+7)&7
    int shortcutR =
        (whoseDotMarginAt(ind + coord.nb8[i + 1]) == who);  // (i+1)&7
    std::array<int, 2> range;
    if ((i & 1) == 0)
    {
        // diagonal connection between [ind] and [nb]
        if (shortcutL)
        {
            // check also longer (2-dot) shortcut
            range[0] = (whoseDotMarginAt(nb + coord.nb8[i + 7]) == who)
                           ? (i + 10)
                           : (i + 8);
        }
        else
        {
            range[0] = i + 6;
        }
        if (shortcutR)
        {
            // check also longer (2-dot) shortcut
            range[1] = (whoseDotMarginAt(nb + coord.nb8[i + 1]) == who)
                           ? (i + 7)
                           : (i + 9);
        }
        else
        {
            range[1] = i + 11;
        }
    }
    else
    {
        // horizontal or vertical connection between [ind] and [nb]
        if ((shortcutL || shortcutR) and
            (whoseDotMarginAt(nb + coord.nb8[i]) == who))
        {
            // there is a 2-dot shortcut, take empty range
            range[0] = 0;
            range[1] = 0;
        }
        else
        {
            range[0] = shortcutL ? (i + 9) : (i + 7);
            range[1] = shortcutR ? (i + 8) : (i + 10);
        }
    }
    return range;
}

/// Finds up to 4 direct neighbours of [ind] which are closable, i.e.,
/// empty or with opponent dot, and not on the edge of the board
/// @param[in] forbidden1, forbidden2 Typically points at which who will play,
/// hence we do not want to put it into
///                                   the array of closable neighbours. It may
///                                   be e.g. 0 if there are no such points.
/// @return  Array with [0]=neighbours count, [1],... -- neighbours
std::array<int, 5> Game::findClosableNeighbours(pti ind, pti forbidden1,
                                                pti forbidden2, int who) const
{
    std::array<int, 5> res;
    res[0] = 0;
    bool prev_ok = false;
    bool first_ok = false;
    for (int i = 0; i < 4; i++)
    {
        int nb = ind + coord.nb4[i];
        if (whoseDotMarginAt(nb) != who and nb != forbidden1 and
            nb != forbidden2)
        {
            if (coord.dist[nb] >= 1)
            {
                // check if nb is connected with previous nb
                if (i > 0)
                {
                    int connecting = nb + coord.nb4[i - 1];
                    if (prev_ok and whoseDotMarginAt(connecting) != who and
                        connecting != forbidden1 and connecting != forbidden2)
                    {
                        continue;
                    }
                    if (i == 3)
                    {
                        int connecting = nb + coord.nb4[0];
                        if (first_ok and whoseDotMarginAt(connecting) != who and
                            connecting != forbidden1 and
                            connecting != forbidden2)
                        {
                            continue;
                        }
                    }
                }
                res[++res[0]] = nb;
            }
            if (i == 0) first_ok = true;
            prev_ok = true;
        }
        else
            prev_ok = false;
    }
    return res;
}

/// 3 points: p0, p1, p2 should be on the boundary of an enclosure, so check
/// them and take the optimal one. Points p0, p1, p2 do not have to be adjacent.
/// TODO: use connections_tab, like in usual Threats
void Game::addClosableNeighbours(std::vector<pti> &tab, pti p0, pti p1, pti p2,
                                 int who) const
{
    auto cn0 = findClosableNeighbours(p0, p1, p2, who);
    auto cn1 = findClosableNeighbours(p1, p0, p2, who);
    auto cn2 = findClosableNeighbours(p2, p0, p1, who);
    if (cn0[0] <= cn1[0] and cn0[0] <= cn2[0])
    {
        for (int i = 0; i < cn0[0]; i++) tab.push_back(cn0[i + 1]);
        tab.push_back(cn0[0]);
    }
    else if (cn1[0] <= cn2[0])
    {
        for (int i = 0; i < cn1[0]; i++) tab.push_back(cn1[i + 1]);
        tab.push_back(cn1[0]);
    }
    else
    {
        for (int i = 0; i < cn2[0]; i++) tab.push_back(cn2[i + 1]);
        tab.push_back(cn2[0]);
    }
}

std::vector<pti> Game::findThreats2moves_preDot(pti ind, int who)
// find possible new threats in 2 moves because of the (future) dot of who at
// [ind] Each possible threat consists of:
//  points (possibly) to enclose,
//  number of these points to enclose,
//  a pair of pti's, denoting the points to play.
// (The order is so that we may then pop_back two points, count and points to
// enclose).
{
    if (not threats[who - 1].isActiveThreats2m()) return {};
    std::vector<pti> possible_threats;
    // find groups in the neighbourhood
    std::array<pti, 4> groups = sg.getConnects(who - 1)[ind].groups_id;
    int top = sg.getConnects(who - 1)[ind].count();
    if (top == 4) return possible_threats;
    SmallMultiset<pti, 4> connected_groups;
    if (top >= 1)
    {
        for (int i = 0; i < 4; i++)
        {
            pti g = groups[i];
            if (g == 0) break;
            connected_groups.insert(g);
        }
    }
    possible_threats.reserve(64);
    if (top <= 2)
    {
        // here it's possible to put 2 dots in the neighbourhood of [ind] to
        // make an enclosure
        for (int i = 0; i <= 4; i++)
        {
            pti nb = ind + coord.nb8[i];
            if (whoseDotMarginAt(nb) == 0)
            {
                pti other_groups[4];
                int count = 0;
                for (int c = 0; c < 4; c++)
                {
                    auto g = sg.getConnects(who - 1)[nb].groups_id[c];
                    if (g == 0) break;
                    if (!connected_groups.contains(g))
                    {
                        other_groups[count++] = g;
                    }
                }
                if (count)
                {
                    int start_ind, stop_ind;
                    if ((i & 1) == 0)
                    {  // diagonal connection
                        start_ind = i + 2;
                        stop_ind = std::min(i + 6, 7);
                    }
                    else
                    {
                        start_ind = i + 3;
                        stop_ind = std::min(i + 5, 7);
                    }
                    for (int j = start_ind; j <= stop_ind; ++j)
                    {
                        pti nb2 = ind + coord.nb8[j];
                        if (whoseDotMarginAt(nb2) == 0)
                        {
                            for (int c = 0; c < 4; c++)
                            {
                                auto g =
                                    sg.getConnects(who - 1)[nb2].groups_id[c];
                                if (g == 0) break;
                                // we found place nb2, which has group g as a
                                // neighbour, check if g is in other_groups
                                for (int n = 0; n < count; ++n)
                                    if (other_groups[n] == g)
                                    {
                                        addClosableNeighbours(possible_threats,
                                                              ind, nb, nb2,
                                                              who);
                                        possible_threats.push_back(nb);
                                        possible_threats.push_back(nb2);
                                        goto threat_added;
                                    }
                            }
                        threat_added:;
                        }
                    }
                }
            }
        }
    }
    // check 2 dots, the first close to ind, the second close to the first
    if (top >= 1 and top <= 3)
    {
        auto debug_time = std::chrono::high_resolution_clock::now();

        // std::cerr << "ind = " << coord.showPt(ind) << std::endl;
        for (int i = 0; i < 8; i++)
        {
            pti nb = ind + coord.nb8[i];
            if (whoseDotMarginAt(nb) != 0) continue;  // dot or outside
            auto p2 = findThreats2moves_preDot__getRange(ind, nb, i, who);
            // check if nb and ind have a common neighbour, for later use (could
            // be lazy evaluated, but does not seem that slow to do it in this
            // way...)
            bool are_ind_nb_connected = false;
            auto connected_only = connected_groups;
            if ((i & 1) == 0)
            {  // diagonal connection between ind and nb
                auto tnb = ind + coord.nb8[i + 1];  // (i+1)&7
                if (whoseDotMarginAt(tnb) == who)
                {
                    connected_only.remove_one(
                        sg.descr.at(sg.worm[tnb]).group_id);
                }
                else
                {
                    tnb = ind + coord.nb8[i + 7];  // (i+7)&7
                    if (whoseDotMarginAt(tnb) == who)
                    {
                        connected_only.remove_one(
                            sg.descr.at(sg.worm[tnb]).group_id);
                    }
                }
            }
            else
            {  // horizontal or vertical connection between ind and nb
                // first pair of neighbours
                auto tnb = ind + coord.nb8[i + 1];  // (i+1)&7
                if (whoseDotMarginAt(tnb) == who)
                {
                    connected_only.remove_one(
                        sg.descr.at(sg.worm[tnb]).group_id);
                }
                else
                {
                    tnb = ind + coord.nb8[i + 2];  // (i+2)&7
                    if (whoseDotMarginAt(tnb) == who)
                    {
                        connected_only.remove_one(
                            sg.descr.at(sg.worm[tnb]).group_id);
                    }
                }
                // second pair of neighbours
                tnb = ind + coord.nb8[i + 7];  // (i+7)&7
                if (whoseDotMarginAt(tnb) == who)
                {
                    connected_only.remove_one(
                        sg.descr.at(sg.worm[tnb]).group_id);
                }
                else
                {
                    tnb = ind + coord.nb8[i + 6];  // (i+6)&7
                    if (whoseDotMarginAt(tnb) == who)
                    {
                        connected_only.remove_one(
                            sg.descr.at(sg.worm[tnb]).group_id);
                    }
                }
            }
            if (connected_only.empty())
            {
                continue;  // there will be no new threats, because [ind] would
                           // be redundant for a threat with [nb]
            }
            for (int gn = 0; gn < 4; gn++)
            {
                pti g = sg.getConnects(who - 1)[nb].groups_id[gn];
                if (g == 0) break;
                if (connected_groups.contains(g))
                {
                    are_ind_nb_connected = true;
                    break;
                }
            }
            for (int j = p2[0]; j < p2[1]; j++)
            {
                pti nb2 = nb + coord.nb8[j];               // [j & 7];
                if (whoseDotMarginAt(nb2) != 0) continue;  // dot or outside
                auto p3 =
                    findThreats2moves_preDot__getRange(nb, nb2, j & 7, who);
                // std::cerr << "  nb2 = " << coord.showPt(nb2) << ", range = ["
                // << p3[0] << ", " << p3[1] << "]" << std::endl;
                for (int k = p3[0]; k < p3[1]; k++)
                {
                    pti nb3 = nb2 + coord.nb8[k];  // [k & 7];
                    // std::cerr << "    nb3 = " << coord.showPt(nb3) <<
                    // std::endl;
                    if (whoseDotMarginAt(nb3) == who and
                        connected_groups.contains(
                            sg.descr.at(sg.worm[nb3]).group_id))
                    {
                        /*
                        // dla pokazania ustaw kropki na ind, nb, nb2
                        CleanupOneVar<pti> sg.worm_where_cleanup0(&sg.worm[ind],
                        who+4); CleanupOneVar<pti>
                        sg.worm_where_cleanup(&sg.worm[nb], who);
                        CleanupOneVar<pti> sg.worm_where_cleanup2(&sg.worm[nb2],
                        who); show(); std::cerr << "dodaje (ind=" <<
                        coord.showPt(ind) << "): " << coord.showPt(nb) << ", "
                        << coord.showPt(nb2) << " --> " << coord.showPt(nb3) <<
                        std::endl; std::cin.ignore();
                        */
                        // now add closable neighbours, as few as possible
                        if (!are_ind_nb_connected)
                        {  // if (!haveConnection(ind, nb, who)) {
                            int count = 0;
                            if (coord.dist[ind + coord.nb8[i + 7]] >= 1)
                            {
                                possible_threats.push_back(ind +
                                                           coord.nb8[i + 7]);
                                count++;
                            }
                            if (coord.dist[ind + coord.nb8[i + 1]] >= 1)
                            {
                                possible_threats.push_back(ind +
                                                           coord.nb8[i + 1]);
                                count++;
                            }
                            possible_threats.push_back(count);
                        }
                        else if (!haveConnection(nb, nb2, who))
                        {
                            int count = 0;
                            if (coord.dist[nb + coord.nb8[j + 7]] >= 1)
                            {
                                possible_threats.push_back(nb +
                                                           coord.nb8[j + 7]);
                                count++;
                            }
                            if (coord.dist[nb + coord.nb8[j + 1]] >= 1)
                            {
                                possible_threats.push_back(nb +
                                                           coord.nb8[j + 1]);
                                count++;
                            }
                            possible_threats.push_back(count);
                        }
                        else
                        {
                            addClosableNeighbours(possible_threats, ind, nb,
                                                  nb2, who);
                        }
                        possible_threats.push_back(nb);
                        possible_threats.push_back(nb2);
                        /*
                        if (no_thr_expexcted) {
                          // dla pokazania ustaw kropki na ind, nb, nb2
                          CleanupOneVar<pti>
                        sg.worm_where_cleanup0(&sg.worm[ind], who+4);
                        CleanupOneVar<pti> sg.worm_where_cleanup(&sg.worm[nb],
                        who); CleanupOneVar<pti>
                        sg.worm_where_cleanup2(&sg.worm[nb2], who); show();
                        std::cerr << "Nieoczekiwanie dodaje (ind="
                        << coord.showPt(ind) << "): " << coord.showPt(nb) << ",
                        " << coord.showPt(nb2) << " --> " << coord.showPt(nb3)
                        << std::endl; std::cin.ignore();
                        }
                        */
                        break;
                    }
                }
            }
        }

        debug_nanos3 +=
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now() - debug_time)
                .count();
    }

    SmallMultimap<7, 7> pairs_pointCloseToInd_groupIdThatItTouches{};
    if (top > 0 and top < 4)
    {
        pairs_pointCloseToInd_groupIdThatItTouches =
            getEmptyPointsCloseToIndTouchingSomeOtherGroup(connected_groups,
                                                           ind, who);
    }

    if (pairs_pointCloseToInd_groupIdThatItTouches.getNumberOfGroups() > 0 or
        connected_groups.hasAtLeastTwoDistinctElements())
    {
        // If the first part of the above alternative is true, we find threats
        // of type:
        //  a. one point is close to ind and it connects with some other group
        //  G, second point is far and it connects G with a group already
        //  touching ind
        // If the second part of the above alternative is true, we find threats
        // caused by connecting 2 groups:
        //  b. maybe there are 2 moves somewhere connecting these groups (and
        //  thus posing a threat in 2 moves) c. maybe there is a third group
        //  which is of distance 1 to each of the just connected 2 groups
        std::vector<pti> unique_connected_groups =
            connected_groups.getUniqueSet();
        std::vector<uint8_t> neighbours(coord.getSize(), 0);
        std::vector<GroupNeighbours> group_neighb;
        int mask = 1;
        for (pti gid : unique_connected_groups)
        {
            group_neighb.emplace_back(*this, neighbours, gid, ind, mask, who);
            mask <<= 1;
        }
        // the first part, case a.
        for (unsigned first = 0; first < unique_connected_groups.size();
             ++first)
        {
            for (int second = 0;
                 second <
                 pairs_pointCloseToInd_groupIdThatItTouches.getNumberOfGroups();
                 ++second)
            {
                pti second_gr_id =
                    pairs_pointCloseToInd_groupIdThatItTouches.group(second);
                if (not group_neighb[first].isGroupClose(second_gr_id))
                    continue;
                for (auto point_close_to_1 :
                     group_neighb[first].neighbours_list)
                {
                    if (not sg.getConnects(who - 1)[point_close_to_1].contains(
                            second_gr_id))
                        continue;
                    auto number_of_neighb =
                        pairs_pointCloseToInd_groupIdThatItTouches
                            .numberOfElems(second);
                    for (int ind_dot2 = 0; ind_dot2 < number_of_neighb;
                         ++ind_dot2)
                    {
                        pti point2 =
                            pairs_pointCloseToInd_groupIdThatItTouches.elem(
                                second, ind_dot2);
                        if (coord.isInNeighbourhood(
                                point2,
                                point_close_to_1))  // second point is not far:
                                                    // both points touch each
                                                    // other
                            continue;
                        addClosableNeighbours(possible_threats, ind,
                                              point_close_to_1, point2, who);
                        possible_threats.push_back(point_close_to_1);
                        possible_threats.push_back(point2);
                    }
                }
            }
        }
        // the second part, cases b. and c.
        for (unsigned first = 0; first < unique_connected_groups.size() - 1;
             ++first)
        {
            int mask_first = (1 << first);
            for (unsigned second = first + 1;
                 second < unique_connected_groups.size(); ++second)
            {
                int mask_second = (1 << second);
                int mask_both = mask_first | mask_second;
                // check if there are 2 adjacent points from neighbours of
                // exactly one of gr1 and gr2 (case b.)
                for (auto point_close_to_1 :
                     group_neighb[first].neighbours_list)
                {
                    if ((neighbours[point_close_to_1] & mask_second) == 0)
                    {  // point_close_to_1 is not a neighbour of group2
                        for (int n = 0; n < 8; ++n)
                        {
                            int nb = point_close_to_1 + coord.nb8[n];
                            if ((neighbours[nb] & mask_both) == mask_second)
                            {
                                addClosableNeighbours(possible_threats, ind,
                                                      point_close_to_1, nb,
                                                      who);
                                possible_threats.push_back(point_close_to_1);
                                possible_threats.push_back(nb);
                            }
                        }
                    }
                }
                // find other groups that are close to both gr1 and gr2 and add
                // threats (case c.)
                for (auto other_gr_id : group_neighb[first].neighbour_groups)
                {
                    if (not group_neighb[second].isGroupClose(other_gr_id))
                        continue;
                    for (auto point_close_to_1 :
                         group_neighb[first].neighbours_list)
                    {
                        if (not sg.getConnects(who - 1)[point_close_to_1]
                                    .contains(other_gr_id))
                            continue;
                        if (neighbours[point_close_to_1] & mask_second)
                            continue;
                        for (auto point_close_to_2 :
                             group_neighb[second].neighbours_list)
                        {
                            if (not sg.getConnects(who - 1)[point_close_to_2]
                                        .contains(other_gr_id))
                                continue;
                            if (neighbours[point_close_to_2] & mask_first)
                                continue;
                            addClosableNeighbours(possible_threats, ind,
                                                  point_close_to_1,
                                                  point_close_to_2, who);
                            possible_threats.push_back(point_close_to_1);
                            possible_threats.push_back(point_close_to_2);
                        }
                    }
                }
            }
        }
    }

    return possible_threats;
}

SmallMultimap<7, 7> Game::getEmptyPointsCloseToIndTouchingSomeOtherGroup(
    const SmallMultiset<pti, 4> &connected_groups, pti ind, int who) const
{
    constexpr unsigned max_number_of_groups = 7;
    constexpr unsigned max_number_of_neighb_per_group = 7;
    SmallMultimap<max_number_of_groups, max_number_of_neighb_per_group> pairs{};
    for (int i = 0; i < 8; i++)
    {
        pti nb = ind + coord.nb8[i];
        if (whoseDotMarginAt(nb) != 0 ||
            sg.getConnects(who - 1)[nb].groups_id[0] == 0)
            continue;
        std::array<pti, 4> unique_groups = {0, 0, 0, 0};
        int ug = sg.getConnects(who - 1)[nb].getUniqueGroups(unique_groups);
        for (int j = 0; j < ug; j++)
            if (not connected_groups.contains(unique_groups[j]))
            {
                pairs.addPair(unique_groups[j], nb);
            }
    }
    return pairs;
}

void Game::checkThreat_encl(Threat *thr, int who)
/// The function may add a threat, thus invalidating threat list and perhaps
/// also 'thr' pointer.
/// @param[in] who  Whose threat it is (i.e., who makes this enclosure).
{
    thr->type |= ThreatConsts::TO_REMOVE;
    int where = thr->where;
#ifndef NDEBUG
    // std::cerr << "Zagrozenie do usuniecia: " << who << " w " <<
    // coord.showPt(where) << std::endl; std::cerr << thr->show() << std::endl;
#endif
    if (sg.worm[where] == 0)
    {
        CleanupOneVar<pti> worm_where_cleanup(
            &sg.worm[where], who);  //  sg.worm[where] = who;  with restored old
                                    //  value (0) by destructor
        int done[4] = {0, 0, 0, 0};
        for (int j = 0; j < 4; j++)
            if (!done[j])
            {
                pti nb = where + coord.nb4[j];
                if (whoseDotMarginAt(nb) != who and coord.dist[nb] >= 1)
                {
                    Enclosure encl = findEnclosure(nb, sg.MASK_DOT, who);
                    if (!encl.isEmpty() and !encl.isInInterior(where))
                    {
                        const auto zobr = encl.zobristKey(who);
                        // if some next neighbour has been enclosed, mark it as
                        // done
                        for (int k = j + 1; k < 4; k++)
                        {
                            if (encl.isInInterior(where + coord.nb4[k]))
                                done[k] = 1;
                        }
                        Threat *thrfz =
                            threats[who - 1].findThreatZobrist(zobr);
                        if (thrfz == nullptr)
                        {
                            auto tmp = countDotsTerrInEncl(encl, 3 - who);
                            Threat t;
                            t.type = ThreatConsts::ENCL;
                            t.where = where;
                            t.opp_dots = std::get<0>(tmp);
                            t.zobrist_key = zobr;
                            t.terr_points = std::get<1>(tmp);
                            t.hist_size = sg.getHistory().size();
                            t.encl =
                                std::make_shared<Enclosure>(std::move(encl));
                            // std::tie<t.opp_dots, t.terr_points>
                            addThreat(std::move(t), who);
                        }
                        else
                        {
                            thrfz->type &= ~(ThreatConsts::TO_CHECK |
                                             ThreatConsts::TO_REMOVE);
                        }
                    }
                }
            }
        // sg.worm[where] = 0;  by destructor of cleanup
    }
}

bool Game::checkIfThreat_encl_isUnnecessary(Threat *thr, pti ind, int who) const
{
    if (coord.distBetweenPts_1(thr->where, ind) != 1) return false;
    if (not isInTerr(thr->where, who)) return false;
    for (const auto &t : threats[who - 1].threats)
        if (t.type == ThreatConsts::TERR and t.encl->isInBorder(ind) and
            t.encl->isInInterior(thr->where))
        {
            const auto [p1, p2] = t.encl->getNeighbourBorderElements(ind);
            if (coord.distBetweenPts_infty(p1, thr->where) == 1 and
                coord.distBetweenPts_infty(p2, thr->where) == 1)
                return true;
        }
    return false;
}

void Game::checkThreat_terr(Threat *thr, pti p, int who,
                            std::vector<int8_t> *done)
/// Try to enclose point [p] by who.
/// @param[in] thr  Threat with a territory to check, it may happen that the
/// territory found will be the same
///                 and we will just change thr.type (reset REMOVE flag) instead
///                 of saving new one.
{
    Enclosure encl = findEnclosure(p, sg.MASK_DOT, who);
    if (!encl.isEmpty())
    {
        if (done != nullptr)
        {
            for (auto i : encl.interior) (*done)[i] = 1;
        }
        auto zobr = encl.zobristKey(who);
        if (thr and zobr == thr->zobrist_key)
        {
            thr->type &=
                ~(ThreatConsts::TO_CHECK |
                  ThreatConsts::TO_REMOVE);  //  thr->type = ThreatConsts::TERR;
        }
        else
        {
            Threat *tz = threats[who - 1].findThreatZobrist(zobr);
            if (tz != nullptr)
            {
                tz->type &=
                    ~(ThreatConsts::TO_CHECK |
                      ThreatConsts::TO_REMOVE);  // may be redundant, but it's
                                                 // as fast to do as to check
            }
            else
            {
                Threat t;
                t.type = ThreatConsts::TERR;
                t.where = 0;
                t.zobrist_key = zobr;
                auto tmp = countDotsTerrInEncl(encl, 3 - who);
                t.opp_dots = std::get<0>(tmp);
                t.terr_points = std::get<1>(tmp);
                t.hist_size = sg.getHistory().size();
                t.encl = std::make_shared<Enclosure>(std::move(encl));
                addThreat(std::move(t), who);
            }
        }
    }
}

void Game::checkThreats_postDot(std::vector<pti> &newthr, pti ind, int who)
{
    assert(who == 1 || who == 2);
    /*
    // check our old threats  -- already in preDot
    for (auto &thr : threats[who-1].threats) {
      if (thr.where == ind) {
        if (thr.type == ThreatConsts::ENCL) {
          thr.type = ThreatConsts::TERR;
          thr.where = 0;
          // thr.zobrist_key does not change
        }
      }
    }
    */
    // process our threats marked as TO_CHECK
    // for (int who=1; who<=2; who++)   // <-- not needed, because there are no
    // opponent threats markes as TO_CHECK
    // In first pass, we check TERR threats, so that in the second one
    // checkIfThreat_encl_isUnnecessary() works correctly.
    for (unsigned int tn = 0; tn < threats[who - 1].threats.size(); tn++)
    {
        Threat *thr = &threats[who - 1].threats[tn];
        if ((thr->type & ThreatConsts::TO_CHECK) and
            (thr->type & ThreatConsts::TERR))
        {
            thr->type |= ThreatConsts::TO_REMOVE;
            std::vector<int8_t> done(coord.last + 1, 0);
            std::shared_ptr<Enclosure> encl =
                thr->encl;  // thr may be invalidated by adding new threats
                            // in checkThreat_terr
            for (auto p : encl->interior)
                if (!done[p] and whoseDotMarginAt(p) != who)
                {
                    checkThreat_terr(
                        &threats[who - 1].threats[tn], p, who,
                        &done);  // try to enclose p; cannot use *thr
                                 // because it can be invalidated
                }
        }
    }
    // Second pass -- check ENCL threats.
    for (unsigned int tn = 0; tn < threats[who - 1].threats.size(); tn++)
    {
        Threat *thr = &threats[who - 1].threats[tn];
        if (thr->type & ThreatConsts::ENCL)
        {
            const bool just_remove_as_unnecessary =
                checkIfThreat_encl_isUnnecessary(thr, ind, who);
            if (just_remove_as_unnecessary)
                thr->type |= ThreatConsts::TO_REMOVE;
            else if (thr->type & ThreatConsts::TO_CHECK)
            {
                checkThreat_encl(thr, who);
            }
        }
    }
    // remove opponents threats that ((old: that need to put dot at [ind] -- now
    // in placeDot, or )) are marked as to be removed
    removeMarked(3 - who);
    // recalculate number of dots in opponent threats, if needed
    if (isInEncl(ind, 3 - who) || isInTerr(ind, 3 - who))
    {
        for (auto &t : threats[2 - who].threats)
        {
            if (t.encl->isInInterior(ind))
            {
                t.opp_dots++;
                t.terr_points--;
                // singular_dots are already updated in subtractThreat (in
                // removeAtPoint in placeDot) if (threats[2-who].is_in_encl[ind]
                // + threats[2-who].is_in_terr[ind] == 1)
                //  t.singular_dots++;
            }
        }
    }
    // recalculate terr points in our threats, if needed
    if (isInEncl(ind, who) || isInTerr(ind, who))
    {
        for (auto &t : threats[who - 1].threats)
        {
            if (t.encl->isInInterior(ind))
            {
                t.terr_points--;
            }
        }
    }
    // remove our marked threats
    removeMarked(who);
    // check new
    while (!newthr.empty())
    {
        pti interior = newthr.back();
        newthr.pop_back();
        pti where = newthr.back();
        newthr.pop_back();
        if (sg.worm[where] == 0)
        {
            CleanupOneVar<pti> worm_where_cleanup(
                &sg.worm[where], who);  //  worm[where] = who;  with restored
                                        //  old value (0) by destructor
            int last = (interior & 0x1000) ? 1 : 4;
            pti last_pt = 0;
            for (int j = 0; j < last; j++)
            {
                pti pt = where + coord.nb8[(interior + 1) & 7];
                if (pt == last_pt) break;
                last_pt = pt;
                interior >>= 3;

                Enclosure encl = findEnclosure(pt, sg.MASK_DOT, who);
                if (!encl.isEmpty() and !encl.isInInterior(where))
                {
                    /* for debugging */
                    /*
                    show();
                    std::cerr << "Zagrozenie: " << who << " w "
                              << coord.showPt(where) << std::endl;
                    std::cerr << encl.show() << '\n';
                    */
                    auto zobr = encl.zobristKey(who);
                    if (threats[who - 1].findThreatZobrist(zobr) == nullptr)
                    {
                        Threat t;
                        t.type = ThreatConsts::ENCL;
                        t.where = where;
                        t.zobrist_key = zobr;
                        auto tmp = countDotsTerrInEncl(encl, 3 - who);
                        t.opp_dots = std::get<0>(tmp);
                        t.terr_points = std::get<1>(tmp);
                        t.encl = std::make_shared<Enclosure>(std::move(encl));
                        t.hist_size = sg.getHistory().size();
                        addThreat(std::move(t), who);
                    }
                }
            }
            // sg.worm[where] = 0;   this does the destructor of
            // worm_where_cleanup
        }
    }
}

void Game::checkThreat2moves_encl(Threat *thr, pti where0, int who)
/// @param[in] where0  The first dot of the threat.
/// @param[in] who  Whose threat it is (i.e., who makes this enclosure).
{
    thr->type |= ThreatConsts::TO_REMOVE;
    int where = thr->where;
    if (sg.worm[where] == 0 and sg.worm[where0] == 0)
    {
        CleanupOneVar<pti> worm_where_cleanup0(
            &sg.worm[where0], who);  //  sg.worm[where0] = who;  with restored
                                     //  old value (0) by destructor
        CleanupOneVar<pti> worm_where_cleanup(
            &sg.worm[where], who);  //  sg.worm[where] = who;  with restored old
                                    //  value (0) by destructor
        int done[4] = {0, 0, 0, 0};
        Threat t[4];
        for (int j = 0; j < 4; j++)
        {
            t[j].type = 0;
            if (!done[j])
            {
                pti nb = where + coord.nb4[j];
                if (whoseDotMarginAt(nb) != who and coord.dist[nb] >= 1 and
                    thr->encl->isInInterior(nb))
                {
                    t[j].encl = std::make_shared<Enclosure>(
                        findEnclosure(nb, sg.MASK_DOT, who));
                    if (!t[j].encl->isEmpty() and
                        t[j].encl->isInBorder(where) and
                        t[j].encl->isInBorder(where0) and
                        !t[j].encl->checkIfRedundant(where) and
                        !t[j].encl->checkIfRedundant(where0))
                    {  // added in v139
                        t[j].zobrist_key = t[j].encl->zobristKey(who);
                        if (t[j].zobrist_key == thr->zobrist_key)
                        {  // our old threat is still valid --> this is in
                           // addThreat2moves
                            thr->type &= ~ThreatConsts::TO_REMOVE;
                            break;
                        }
                        t[j].type = ThreatConsts::ENCL;
                        t[j].where = where;
                        // if some next neighbour has been enclosed, mark it as
                        // done
                        for (int k = j + 1; k < 4; k++)
                        {
                            if (t[j].encl->isInInterior(where + coord.nb4[k]))
                                done[k] = 1;
                        }
                        auto tmp = countDotsTerrInEncl(*t[j].encl, 3 - who);
                        t[j].opp_dots = std::get<0>(tmp);
                        t[j].terr_points = std::get<1>(tmp);
                    }
                }
            }
        }
        // we add threats at the end, because it may invalidate 'thr' pointer
        for (int j = 0; j < 4; j++)
        {
            if (t[j].type)
            {
                t[j].hist_size = sg.getHistory().size();
                threats[who - 1].addThreat2moves(
                    where0, where, isSafeFor(where0, who),
                    isSafeFor(where, who), who, t[j]);
            }
        }
        // sg.worm[where] = 0;  by destructor of cleanup
    }
}

int Game::addThreat2moves(pti ind0, pti ind1, int who, Enclosure &&encl)
{
    Threat t;
    t.type = ThreatConsts::ENCL;
    t.zobrist_key = encl.zobristKey(who);
    auto tmp = countDotsTerrInEncl(encl, 3 - who);
    t.opp_dots = std::get<0>(tmp);
    t.terr_points = std::get<1>(tmp);
    t.encl = std::make_shared<Enclosure>(std::move(encl));
    t.hist_size = sg.getHistory().size();
    return threats[who - 1].addThreat2moves(ind0, ind1, isSafeFor(ind0, who),
                                            isSafeFor(ind1, who), who, t);
}

int debug_allt2m = 0, debug_sing_smallt2m = 0, debug_sing_larget2m = 0,
    debug_skippedt2m = 0;
int debug_n = 0, debug_N = 0;

void Game::checkThreats2moves_postDot(std::vector<pti> &newthr, pti ind,
                                      int who)
{
    if (not threats[who - 1].isActiveThreats2m()) return;
    // check our old threats
    for (auto &t2 : threats[who - 1].threats2m)
    {
        if (t2.where0 == ind)
        {
            // all threats should be removed, because we played at where0
            for (auto &t : t2.thr_list)
            {
                t.type |= ThreatConsts::TO_REMOVE;
            }
        }
        else
        {
            for (auto &t : t2.thr_list)
            {
                if (t.where == ind || t.isShortcut(ind))
                {  //   || t.encl->checkShortcut(t2.where0, ind) ||
                   //   t.encl->checkShortcut(t.where, ind)) {
                    /*
                    if (t.where != ind and (t.encl->checkShortcut(t2.where0,
                    ind) || t.encl->checkShortcut(t.where, ind)) !=
                    t.shortcuts.contains(ind)) { show(); std::cerr << "shortcut
                    " << coord.showPt(t2.where0) << "-" << coord.showPt(ind) <<
                    ": " << t.encl->checkShortcut(t2.where0, ind) << std::endl;
                      std::cerr << "shortcut " << coord.showPt(t.where) << "-"
                    << coord.showPt(ind) << ": " <<
                    t.encl->checkShortcut(t.where, ind) << std::endl; std::cerr
                    << "shortcuts " << t.shortcuts.show() << std::endl;
                    }
                    */
                    assert(t.where == ind ||
                           (t.encl->checkShortcut(t2.where0, ind) ||
                            t.encl->checkShortcut(t.where, ind)) ==
                               t.isShortcut(ind));
                    t.type |= ThreatConsts::TO_REMOVE;
                }
                else
                {
                    /*
                    if(t.where != ind and (t.encl->checkShortcut(t2.where0, ind)
                    || t.encl->checkShortcut(t.where, ind)) !=
                    t.shortcuts.contains(ind)) { show(); std::cerr << "shortcut
                    " << coord.showPt(t2.where0) << "-" << coord.showPt(ind) <<
                    ": " << t.encl->checkShortcut(t2.where0, ind) << std::endl;
                      std::cerr << "shortcut " << coord.showPt(t.where) << "-"
                    << coord.showPt(ind) << ": " <<
                    t.encl->checkShortcut(t.where, ind) << std::endl; std::cerr
                    << "shortcuts " << t.shortcuts.show() << std::endl;
                    }
                    */

                    if (!(t.where == ind ||
                          (t.encl->checkShortcut(t2.where0, ind) ||
                           t.encl->checkShortcut(t.where, ind)) ==
                              t.isShortcut(ind)))
                    {
#ifdef DEBUG_SGF
                        std::cerr.flush();
                        std::cerr << std::endl
                                  << "Sgf:" << std::endl
                                  << getSgf_debug() << std::endl
                                  << std::endl;
#endif

                        show();
                        std::cerr << "blad: " << coord.showPt(t.where) << " "
                                  << coord.showPt(ind) << " "
                                  << coord.showPt(t2.where0) << " "
                                  << t.encl->checkShortcut(t2.where0, ind)
                                  << "," << t.encl->checkShortcut(t.where, ind)
                                  << "," << t.isShortcut(ind) << std::endl;
                        std::cerr << "enclosure: " << t.encl->show()
                                  << std::endl;
                        std::cerr.flush();
                    }
                    assert(t.where == ind ||
                           (t.encl->checkShortcut(t2.where0, ind) ||
                            t.encl->checkShortcut(t.where, ind)) ==
                               t.isShortcut(ind));
                    if (t.encl->isInInterior(ind))
                    {
                        t.type |= ThreatConsts::TO_REMOVE;
                        // we removed the threat and now add it to check again
                        // TODO: in most cases it could be done more efficiently
                        // by checking whether the threat is still valid
                        if (t2.where0 < t.where and t.encl->interior.size() > 1)
                        {
                            assert(sg.worm[t2.where0] == 0 and
                                   sg.worm[t.where] == 0);
                            addClosableNeighbours(newthr, ind, t.where,
                                                  t2.where0, who);
                            newthr.push_back(t2.where0);
                            newthr.push_back(t.where);
                        }
                    }
                }
            }
        }
    }
    // process the threats marked as TO_CHECK -- there are currently none
    // (...)
    // remove opponents threats that need to put dot at [ind] or marked as to be
    // removed

    auto debug_time = std::chrono::high_resolution_clock::now();

    threats[2 - who].removeMarkedAndAtPoint2moves(ind);
    // remove our marked threats
    threats[who - 1].removeMarked2moves();

    debug_nanos2 += std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::high_resolution_clock::now() - debug_time)
                        .count();

    // check new
    while (!newthr.empty())
    {
        pti ind0 = newthr.back();
        newthr.pop_back();
        assert(!newthr.empty());
        pti ind1 = newthr.back();
        newthr.pop_back();
        assert(sg.worm[ind0] == 0 and sg.worm[ind1] == 0);
        {
            CleanupOneVar<pti> worm_where_cleanup0(
                &sg.worm[ind0], who);  //  sg.worm[ind0] = who;  with restored
                                       //  old value (0) by destructor
            CleanupOneVar<pti> worm_where_cleanup1(
                &sg.worm[ind1], who);  //  sg.worm[ind1] = who;  with restored
                                       //  old value (0) by destructor
            debug_allt2m++;
            assert(!newthr.empty());
            int count = newthr.back();
            newthr.pop_back();
            // we do not want threats2m such that one move is a usual threat,
            // and the other lies inside terr or encl
            if ((isInBorder(ind0, who) > 0 and
                 (isInEncl(ind1, who) > 0 || isInTerr(ind1, who) > 0)) ||
                (isInBorder(ind1, who) > 0 and
                 (isInEncl(ind0, who) > 0 || isInTerr(ind0, who) > 0)))
            {
                newthr.resize(newthr.size() - count);
                debug_skippedt2m++;
                continue;
            }

            // bool debug_probably_1_encl = (threats[who-1].is_in_terr[ind0]==0
            // and threats[who-1].is_in_terr[ind1]==0);
            if (isInTerr(ind0, who) == 0 and isInTerr(ind1, who) == 0)
            {
                // (threats[who-1].is_in_encl[ind0]==0 ||
                // threats[who-1].is_in_border[ind1]==0) and   // this is now
                // excluded above (threats[who-1].is_in_encl[ind1]==0 ||
                // threats[who-1].is_in_border[ind0]==0) and   //
                // coord.distBetweenPts_infty(ind0, ind1) == 1) {  // TODO:
                // check if this is important
                // Here usually there will be only one enclosure. Exception:
                // positions like the first one:
                //   . . . . . .              . . . . . .
                // .             .          .             .
                // .         L   .          .             .
                // .       .   0              .           0
                //   .       1                  .       L   1
                //     . . .                      . . . . .
                // L = last dot [ind], 0=[ind0], 1=[ind1].
                // In the first cases it is not very important to find both
                // threats. In the second, in fact there will be one enclosure
                // with L,0,1 on border. Another example with
                // coord.distBetweenPts_infty(ind0, ind1) > 1:
                //   . . . . .
                // .           .
                // .             .
                // .       L       .
                // .     0   1   .
                //   . .   .   .
                // Also here only one of 2 threats will be found, but it does
                // not matter much, since both are contained in another threat
                // (just 0 and 1)
                //   which should have been found before move L.
                // First pass: check for simple enclosures, maybe we can find
                // one.
                debug_n++;
                debug_N += count;
                // bool was_one = false;
                for (int i = count; i > 0; --i)
                {
                    pti pt = newthr[newthr.size() - i];
                    assert(coord.dist[pt] >= 1 and whoseDotMarginAt(pt) != who);
                    Enclosure encl = findSimpleEnclosure(pt, sg.MASK_DOT, who);
                    // if (!encl->isEmpty()) was_one = true;
                    if (!encl.isEmpty() and encl.isInBorder(ind) and
                        encl.isInBorder(ind0) and encl.isInBorder(ind1))
                    {
                        addThreat2moves(ind0, ind1, who, std::move(encl));
                        // was_one = true;
                        debug_sing_smallt2m++;
                        goto one_found;
                    }
                }
                // none was found, second pass: find non-simple enclosure
                for (int i = count; i > 0; --i)
                {
                    pti pt = newthr[newthr.size() - i];
                    Enclosure encl =
                        findNonSimpleEnclosure(pt, sg.MASK_DOT, who);
                    // if (!encl->isEmpty()) was_one = true;
                    if (!encl.isEmpty() and encl.isInBorder(ind) and
                        encl.isInBorder(ind0) and encl.isInBorder(ind1))
                    {
                        addThreat2moves(ind0, ind1, who, std::move(encl));
                        // was_one = true;
                        debug_sing_larget2m++;
                        goto one_found;
                    }
                }
            one_found:;
                newthr.resize(newthr.size() - count);
                /*
                if (!was_one) {
                  show();
                  std::cerr << "Zagrozenie2: " << who << " po ruchu " <<
                coord.showPt(ind) << " w " << coord.showPt(ind0) << " + " <<
                coord.showPt(ind1) << std::endl;
                }
                assert(was_one);
                */
            }
            else
            {
                // Here there might be more than 1 threat.
                // bool was_one = false;
                for (; count > 0; --count)
                {
                    assert(!newthr.empty());
                    pti pt = newthr.back();
                    newthr.pop_back();
                    assert(coord.dist[pt] >= 1 and whoseDotMarginAt(pt) != who);
                    Enclosure encl = findEnclosure(pt, sg.MASK_DOT, who);
                    // if (!encl->isEmpty()) was_one = true;
                    if (!encl.isEmpty() and encl.isInBorder(ind) and
                        encl.isInBorder(ind0) and encl.isInBorder(ind1))
                    {
                        addThreat2moves(ind0, ind1, who, std::move(encl));
                        // was_one = true;
                        /*
                        if (debug_count >=2 and debug_probably_1_encl) {
                          show();
                          std::cerr << "Zagrozenie2: " << who << " po ruchu " <<
                        coord.showPt(ind) << " w " << coord.showPt(ind0) << " +
                        " << coord.showPt(ind1) << std::endl; std::cin.ignore();
                        }
                        */
                    }
                }
                /*
                if (!was_one) {
                  show();
                  std::cerr << "Zagrozenie2: " << who << " po ruchu " <<
                coord.showPt(ind) << " w " << coord.showPt(ind0) << " + " <<
                coord.showPt(ind1) << std::endl;
                }
                assert(was_one);
                */
            }
        }
    }
}

/// Player who is now in danger at [ind]
///  -- function updates FLAG_SAFE bit of flag in threats2m and also saves
///  points to recalculate_list in order to update pattern3_value
///  -- updates also possible_moves and interesting_moves
void Game::pointNowInDanger2moves(pti ind, int who)
{
    for (auto &t2 : threats[who - 1].threats2m)
    {
        if (t2.where0 == ind)
        {
            // TODO: if (threats[who-1].is_in_border[ind]) { check if we may
            // cancel opp's threats by playing at ind }
            if (t2.flags & Threat2mconsts::FLAG_SAFE)
            {
                // it was safe but is not anymore
                threats[who - 1].changeFlagSafe(t2);
            }
        }
        else
        {
            // here we could check if the border of some threats in 2m contains
            // ind, i.e., is in danger
        }
    }
    for (int j = 0; j < 4; j++)
    {
        pti nb = ind + coord.nb4[j];
        if (whoseDotMarginAt(nb) == 0 and
            Pattern3::isPatternPossible(pattern3_at[nb]))
        {
            pattern3_at[nb] = PATTERN3_IMPOSSIBLE;
            recalculate_list.push_back(nb);
        }
    }
    if (whoseDotMarginAt(ind) == 0)
    {
        possible_moves.changeMove(ind, PossibleMovesConsts::TERRM);
        interesting_moves.changeMove(ind, InterestingMovesConsts::REMOVED);
    }
}

void Game::addThreat(Threat &&t, int who)
{
    krb::SmallVector<pti, 8>::allocator_type::arena_type arena_counted_worms;
    krb::SmallVector<pti, 8> counted_worms{
        arena_counted_worms};  // list of counted worms for singular_dots
    for (auto i : t.encl->interior)
    {
        if (t.type & ThreatConsts::TERR)
        {
            assert(i >= coord.first and i <= coord.last and
                   i < threats[who - 1].is_in_terr.size());
            ++isInTerr(i, who);
        }
        else
        {
            assert(t.type & ThreatConsts::ENCL);
            assert(i >= coord.first and i <= coord.last and
                   i < threats[who - 1].is_in_encl.size());
            ++isInEncl(i, who);
        }
        switch (isInTerr(i, who) + isInEncl(i, who))
        {
            case 1:
                pointNowInDanger2moves(i, 3 - who);
                if (whoseDotMarginAt(i) == 3 - who and
                    std::find(counted_worms.begin(), counted_worms.end(),
                              sg.descr.at(sg.worm[i]).leftmost) ==
                        counted_worms.end())
                {
                    t.singular_dots += sg.descr.at(sg.worm[i]).dots[2 - who];
                    counted_worms.push_back(sg.descr.at(sg.worm[i]).leftmost);
                }
                if (isInBorder(i, 3 - who) >= 1)
                {  // note: it may be an empty place, but still in_border (play
                   // here for ENCL)
                    for (auto &ot : threats[2 - who].threats)
                    {
                        if (ot.encl->isInBorder(i))
                        {
                            ot.border_dots_in_danger++;
                            addOppThreatZobrist(ot.opp_thr, t.zobrist_key);
                        }
                    }
                }
                break;
            case 2:
                if (whoseDotMarginAt(i) == 3 - who and
                    std::find(counted_worms.begin(), counted_worms.end(),
                              sg.descr.at(sg.worm[i]).leftmost) ==
                        counted_worms.end())
                {
                    threats[who - 1]
                        .findThreatWhichContains(i)
                        ->singular_dots -=
                        sg.descr.at(sg.worm[i]).dots[2 - who];
                    counted_worms.push_back(sg.descr.at(sg.worm[i]).leftmost);
                }
                [[fallthrough]];
            default:
                if (isInBorder(i, 3 - who) >= 1)
                {  // note: it may be an empty place, but still in_border (play
                   // here for ENCL)
                    for (auto &ot : threats[2 - who].threats)
                    {
                        if (ot.encl->isInBorder(i))
                        {
                            addOppThreatZobrist(ot.opp_thr, t.zobrist_key);
                        }
                    }
                }
        }
    }
    for (auto it = t.encl->border.begin() + 1; it != t.encl->border.end(); ++it)
    {
        pti i = *it;
        assert(i >= coord.first and i <= coord.last and
               i < threats[who - 1].is_in_border.size());
        ++isInBorder(i, who);
        if (isInEncl(i, 3 - who) || isInTerr(i, 3 - who))
        {
            t.border_dots_in_danger++;
            for (auto &ot : threats[2 - who].threats)
            {
                if (ot.encl->isInInterior(i))
                {
                    addOppThreatZobrist(t.opp_thr, ot.zobrist_key);
                }
            }
        }
    }
    if (t.where and Pattern3::isPatternPossible(pattern3_at[t.where]))
    {
        // add current move to recalculate list, because its neighbours could
        // become in danger now, and that means that curr move should change its
        // pattern3_at
        pattern3_at[t.where] = PATTERN3_IMPOSSIBLE;
        recalculate_list.push_back(t.where);
    }
    threats[who - 1].threats.push_back(std::move(t));
}

void Game::subtractThreat(const Threat &t, int who)
{
    krb::SmallVector<pti, 8>::allocator_type::arena_type arena_counted_worms;
    krb::SmallVector<pti, 8> counted_worms{
        arena_counted_worms};  // list of counted worms for singular_dots
    bool threatens = false;    // if t threatend some opp's threat
    for (auto i : t.encl->interior)
    {
        assert(i >= coord.first and i <= coord.last);
        if (t.type & ThreatConsts::TERR)
        {
            --isInTerr(i, who);
        }
        else
        {
            assert(t.type & ThreatConsts::ENCL);
            --isInEncl(i, who);
        }
        if (whoseDotMarginAt(i) == 3 - who and
            isInTerr(i, who) + isInEncl(i, who) == 1 and
            std::find(counted_worms.begin(), counted_worms.end(),
                      sg.descr.at(sg.worm[i]).leftmost) == counted_worms.end())
        {
            // Note: there are now 2 threats with i in interior, one being t,
            // i.e., the one that we are about to delete. We add to
            // singular_dots in both, because in t it does not matter anymore
            // (it will be deleted) and it is simpler to do so than to check
            // which of 2 threats is the right one.
            for (auto &t : threats[who - 1].threats)
            {
                if (t.encl->isInInterior(i))
                    t.singular_dots += sg.descr.at(sg.worm[i]).dots[2 - who];
            }
            counted_worms.push_back(sg.descr.at(sg.worm[i]).leftmost);
        }
        if (isInBorder(i, 3 - who) >= 1)
        {
            if (isInTerr(i, who) == 0 and isInEncl(i, who) == 0)
            {
                for (auto &thr : threats[2 - who].threats)
                {
                    if (thr.encl->isInBorder(i))
                    {
                        --thr.border_dots_in_danger;
                        assert(thr.border_dots_in_danger >= 0);
                    }
                }
            }
            threatens = true;
        }
        if (isInTerr(i, who) == 0 and isInEncl(i, who) == 0)
        {
            // point is now safe, recalculate pattern3 values and check moves
            // list
            for (int j = 0; j < 4; j++)
            {
                pti nb = i + coord.nb4[j];
                if (whoseDotMarginAt(nb) == 0 and
                    Pattern3::isPatternPossible(pattern3_at[nb]))
                {
                    pattern3_at[nb] = PATTERN3_IMPOSSIBLE;
                    recalculate_list.push_back(nb);
                }
            }
            if (isInTerr(i, 3 - who) == 0 and isInEncl(i, 3 - who) == 0 and
                whoseDotMarginAt(i) == 0)
            {
                // [i] is not anymore in danger and should be removed from TERRM
                // moves list
                possible_moves.changeMove(i, checkDame(i));
                if (Pattern3::isPatternPossible(pattern3_at[i]))
                {
                    interesting_moves.changeMove(i, checkInterestingMove(i));
                }
            }
        }
        if (isSafeFor(i, 3 - who)) pointNowSafe2moves(i, 3 - who);
    }
    for (auto it = t.encl->border.begin() + 1; it != t.encl->border.end(); ++it)
    {
        pti i = *it;
        assert(i >= coord.first and i <= coord.last);
        --isInBorder(i, who);
    }
    if (threatens)
    {
        for (auto &thr : threats[2 - who].threats)
        {
            removeOppThreatZobrist(thr.opp_thr, t.zobrist_key);
        }
    }
}

void Game::removeMarkedAndAtPoint(pti ind, int who)
{
    if (!threats[who - 1].threats.empty())
    {
        for (auto &t : threats[who - 1].threats)
            if (t.where == ind || (t.type & ThreatConsts::TO_REMOVE))
            {
                subtractThreat(t, who);
                t.type |= ThreatConsts::TO_REMOVE;
            }
        // removeMarkedThreats(threats[who-1].threats);

        threats[who - 1].threats.erase(
            std::remove_if(threats[who - 1].threats.begin(),
                           threats[who - 1].threats.end(),
                           [](Threat &t)
                           { return (t.type & ThreatConsts::TO_REMOVE); }),
            threats[who - 1].threats.end());
    }
}

/// Returns whether we removed a threat containing a dot of 3-who that touches
/// [ind]. This is important for calculating singular_dots.
/// TODO: in fact, this turned out to be irrelevant -> change the function to
/// void.
bool Game::removeAtPoint(pti ind, int who)
{
    bool removed = false;
    bool touch = false;
    if (isInBorder(ind, who) > 0)
    {
        for (auto &t : threats[who - 1].threats)
            if (t.where == ind)
            {
                subtractThreat(t, who);
                removed = true;
                if (!touch)
                {
                    for (int j = 0; j < 4; j++)
                    {
                        pti nb = ind + coord.nb4[j];
                        if (whoseDotMarginAt(nb) == 3 - who and
                            t.encl->isInInterior(nb))
                        {
                            touch = true;
                            break;
                        }
                    }
                }
                // t.type |= ThreatConsts::TO_REMOVE;
            }
        if (removed)
        {
            threats[who - 1].threats.erase(
                std::remove_if(threats[who - 1].threats.begin(),
                               threats[who - 1].threats.end(),
                               [ind](Threat &t) { return (t.where == ind); }),
                threats[who - 1].threats.end());
        }
    }
    return touch;
}

void Game::removeMarked(int who)
{
    // remove our marked threats
    if (!threats[who - 1].threats.empty())
    {
        for (auto &t : threats[who - 1].threats)
            if (t.type & ThreatConsts::TO_REMOVE)
            {
                subtractThreat(t, who);
            }
        // removeMarkedThreats(threats[who-1].threats);

        threats[who - 1].threats.erase(
            std::remove_if(threats[who - 1].threats.begin(),
                           threats[who - 1].threats.end(),
                           [](Threat &t)
                           { return (t.type & ThreatConsts::TO_REMOVE); }),
            threats[who - 1].threats.end());
    }
}

void Game::pointNowSafe2moves(pti ind, int who)
{
    for (auto &t2 : threats[who - 1].threats2m)
    {
        if (t2.where0 == ind)
        {
            if ((t2.flags & Threat2mconsts::FLAG_SAFE) == 0)
            {
                // it was not safe but is now safe
                threats[who - 1].changeFlagSafe(t2);
            }
        }
        else
        {
            // if we check borders in pointNowInDanger2moves, we should do it
            // also here
        }
    }
}

/// Checks whether it is safe for who to play at ind, to check if a
/// threat-in-2-moves starting with a move at ind is indeed a threat.
bool Game::isSafeFor(pti ind, int who) const
{
    return (isInTerr(ind, 3 - who) == 0 and isInEncl(ind, 3 - who) == 0);
}

pattern3_t Game::getPattern3_at(pti ind) const
{
    if (whoseDotMarginAt(ind) != 0) return 0;
    pattern3_t p = 0, atari = 0;
    const static pattern3_t atari_masks[8] = {0, 0x10000, 0, 0x20000,
                                              0, 0x40000, 0, 0x80000};
    for (int i = 7; i >= 0; i--)
    {
        pti nb = ind + coord.nb8[i];
        p <<= 2;
        auto dot = whoseDotMarginAt(nb);
        p |= dot;
        if (i & 1)
        {  // we are at N, W, S, or E neighbour
            switch (dot)
            {
                case 1:
                    if (threats[1].is_in_encl[nb] || threats[1].is_in_terr[nb])
                        atari |= atari_masks[i];
                    break;
                case 2:
                    if (threats[0].is_in_encl[nb] || threats[0].is_in_terr[nb])
                        atari |= atari_masks[i];
                    break;
            }
        }
    }
    p |= atari;
    return p;
}

/// Uses pattern3_at to determine whether 3 points: point
///   ind + coord.nb4[direcion]
/// and its 2 neighbours are empty.
bool Game::isEmptyInDirection(pti ind, int direction) const
{
    assert(coord.nb4[0] == coord.N);  // order of nb4 should be: N, E, S, W
    assert(coord.nb8[0] ==
           coord.NE);  // order of nb8 should be: NE, E, ... -- clockwise
    assert(coord.dist[ind] >= 1);

    pti nb = ind + 2 * coord.nb4[direction];
    const pattern3_t masks[4] = {0x3f0, 0x3f00, 0xf003, 0x3f};
    return (whoseDotMarginAt(nb) == 0) and
           (pattern3_at[nb] & masks[direction]) == 0;
}

bool Game::isEmptyInNeighbourhood(pti ind) const
{
    return (whoseDotMarginAt(ind) == 0) and (pattern3_at[ind] == 0);
}

// This function is slower, but useful for testing.
pattern3_val Game::getPattern3Value(pti ind, int who) const
{
    return global::patt3.getValue(getPattern3_at(ind), who);
}

void Game::showPattern3Values(int who) const
{
    std::vector<pti> val(coord.getSize(), 0);
    for (int i = coord.first; i <= coord.last; i++)
    {
        if (coord.dist[i] >= 0)
        {
            val[i] = getPattern3Value(i, who);
        }
    }
    std::cerr << "Patterns3 for player " << who << std::endl;
    std::cerr << coord.showBoard(val);
}

void Game::pattern3recalculatePoint(pti ind)
{
    pattern3_at[ind] = getPattern3_at(ind);
    auto oldv = pattern3_value[0][ind];
    pattern3_value[0][ind] = global::patt3.getValue(pattern3_at[ind], 1);
    if ((oldv < 0 and pattern3_value[0][ind] >= 0) ||
        (oldv >= 0 and pattern3_value[0][ind] < 0))
    {
        possible_moves.changeMove(ind, checkDame(ind));
    }
    pattern3_value[1][ind] = global::patt3.getValue(pattern3_at[ind], 2);
    interesting_moves.changeMove(ind, checkInterestingMove(ind));
}

void Game::recalculatePatt3Values()
{
    for (auto &p : recalculate_list)
    {
        if (whoseDotMarginAt(p) != 0)
        {
            pattern3_at[p] = 0;
            pattern3_value[0][p] = 0;
            pattern3_value[1][p] = 0;
        }
        else
        {
            pattern3recalculatePoint(p);
        }
    }
}

/* checkLadderStep:
 *  x    -- position of the last dot of escaping player
 *  x+v1 -- position of the next dot of escaping player
 *  x+v1+v2 -- position of the second next dot of escaping player
 *  escaping_group -- id of the worm that tries to escape
 *  escapes -- who escapes (1 or 2)
 *  ladder_ext -- true if attacker had a dot in previous step, and escaper has a
 * dot in this step iteration  -- 0 at the first ladder_breakers -- position of
 * places which matter for determining the outcome of this ladder (Warning:
 * ladder has to be recalculated also when a dot from the set of ladder_breakers
 *                      become connected.)
 * Returns -1 if escaper wins, +1 if attacker wins.
 *
 * Figure:
 *    . b
 *  . x a
 *    . .
 *  a=x+v1, b=x+v1+v2
 */
int Game::checkLadderStep(pti x, krb::PointsSet &ladder_breakers, pti v1,
                          pti v2, pti escaping_group, bool ladder_ext,
                          int escapes, int iteration) const
{
    const int ESC_WINS = -1, ATT_WINS = 1;
    pti nx = x + v1;
    ladder_breakers.insert(nx);
    int attacks = 3 - escapes;
    bool curr_ext = false;
    if (whoseDotMarginAt(nx) == escapes and !ladder_ext)
    {
        ladder_breakers.insert(nx + v1);
        if (whoseDotMarginAt(nx + v1) != attacks)
        {
            return ESC_WINS;
        }
        curr_ext = true;
    }
    auto getGroupId = [&](pti x) -> pti
    {
        if (sg.whoseDotAt(x) == 0) return 0;
        return sg.descr.at(sg.worm[x]).group_id;
    };
    if (whoseDotMarginAt(nx) == 0 || ladder_ext || curr_ext)
    {
        // if we're on the edge, then we escaped
        if (coord.dist[nx] == 0) return ESC_WINS;
        // if attacker is already connected, then he captured
        ladder_breakers.insert({(pti)(nx + v1), (pti)(nx + v2)});
        if (whoseDotMarginAt(nx + v1) == attacks and
            whoseDotMarginAt(nx + v2) == attacks)
            return ATT_WINS;
        // escaping player connects and encloses attacking group
        ladder_breakers.insert({(pti)(nx + v1 + v2), (pti)(nx + v1 - v2)});
        if (getGroupId(nx + v1) == escaping_group ||
            getGroupId(nx + v1 + v2) == escaping_group ||
            getGroupId(nx + v1 - v2) == escaping_group)
            return ESC_WINS;
        // attacking player has an atari thanks to a dot on the way of the
        // ladder
        if (((sg.worm[nx + v1] | sg.worm[nx + v2]) & sg.MASK_DOT) == attacks and
            !curr_ext)
        {
            return ATT_WINS;
        }
        // atari of the escaping player
        for (pti w : {(pti)(-v2), v2})
        {
            if ((sg.worm[nx + v1 - w] & sg.MASK_DOT) == 0 and
                coord.dist[nx + v1 - w] >= 0)
            {
                ladder_breakers.insert(
                    {(pti)(nx + 2 * v1), (pti)(nx + 2 * v1 - w),
                     (pti)(nx + 2 * v1 - 2 * w), (pti)(nx + v1 - 2 * w),
                     (pti)(nx - 2 * w)});
                if (getGroupId(nx + 2 * v1) == escaping_group ||
                    getGroupId(nx + 2 * v1 - w) == escaping_group ||
                    getGroupId(nx + 2 * v1 - 2 * w) == escaping_group ||
                    getGroupId(nx + v1 - 2 * w) == escaping_group ||
                    getGroupId(nx - 2 * w) == escaping_group)
                {
                    return ESC_WINS;
                }
            }
        }
        // atari of the escaping player thanks to his dot(s) on the way of the
        // ladder
        if (iteration >= 2)
        {
            ladder_breakers.insert(nx - 2 * v2);
            if (((sg.worm[nx + v1 - v2] | sg.worm[nx - 2 * v2]) &
                 sg.MASK_DOT) == escapes)
            {
                return ESC_WINS;
            }
        }
        // check what is on nx+v1
        if ((sg.worm[nx + v1] & sg.MASK_DOT) == escapes)
        {
            return ESC_WINS;
        }
        else
            ladder_ext = ((sg.worm[nx + v1] & sg.MASK_DOT) != 0);
        return checkLadderStep(nx, ladder_breakers, v2, v1, escaping_group,
                               ladder_ext, escapes, iteration + 1);
    }
    // should we ever get here?
    return ATT_WINS;
}

std::tuple<int, pti, pti> Game::checkLadder(int who_defends, pti where) const
// Defender about to play at 'where' or has just played at 'where'.
// Returns {status, next_attacker_dot, next_defender_dot}.
{
    const int who_attacks = 3 - who_defends;
    if (coord.dist[where] <= 0) return {0, 0, 0};
    if (isInTerr(where, 1) or isInTerr(where, 2))
        return {0, 0, 0};  // ignore ladders inside territory
    const bool defender_has_played_at_where =
        (whoseDotMarginAt(where) == who_defends);
    if (not defender_has_played_at_where and
        whoseDotMarginAt(where) != 0)  // check for correctness: 'where' should
                                       // be either empty or belong to defender
        return {0, 0, 0};
    if (not defender_has_played_at_where and
        isInBorder(where, who_attacks) == 0)
        return {0, 0, 0};  // even no atari
    if (defender_has_played_at_where and
        not sg.getHistory().isInOppEnclBorder(sg.getHistory().size() - 1))
        return {0, 0, 0};  // last dot of defender was not at opp's border
    pti attackers_neighb = 0;
    pti defenders_neighb = 0;
    for (int i = 0; i < 4; ++i)
    {
        pti nind = where + coord.nb4[i];
        if (whoseDotMarginAt(nind) == who_attacks)
        {
            if (attackers_neighb != 0)
                return {0, 0, 0};  // more than 2 attacker's dots
            attackers_neighb = nind;
        }
        if (whoseDotMarginAt(nind) == who_defends)
        {
            if (defenders_neighb != 0)
                return {0, 0, 0};  // more than 2 defender's dots
            defenders_neighb = nind;
        }
    }
    if (attackers_neighb == 0 or defenders_neighb == 0) return {0, 0, 0};
    if (attackers_neighb + defenders_neighb == where + where)
        return {0, 0, 0};  // attacker and defender have dots at opposite sides
                           // of where, it's not ladder
    const pti another_att = defenders_neighb + where - attackers_neighb;
    if (whoseDotMarginAt(another_att) != who_attacks) return {0, 0, 0};
    if (sg.descr.at(sg.worm[attackers_neighb]).group_id !=
        sg.descr.at(sg.worm[another_att]).group_id)
        return {0, 0, 0};  // attacker dots are where they should be, but they
                           // are not connected!
    if (defender_has_played_at_where)
    {
        const auto atari_code =
            sg.getHistory().getAtariCode(sg.getHistory().size() - 1);
        if (atari_code != 1 and atari_code != 2 and atari_code != 4 and
            atari_code != 8)
            return {0, 0, 0};  // in fact, ladder is possible with more than 1
                               // defender's dot in the neighbourhood,
        // but we don't handle such situations (we care most for further steps
        // in ladder)
        const int direction = where - defenders_neighb;
        const unsigned index_of_atari_code =
            (atari_code <= 2) ? (atari_code - 1) : (2 + (atari_code >> 3));
        if (coord.nb4[index_of_atari_code] != direction)
            return {0, 0, 0};  // the place that defender defended does not
                               // match the found defender's dot position
    }
    krb::PointsSet ladder_breakers;
    const pti x = defenders_neighb;
    const pti v1 = where - x;
    const pti v2 = where - attackers_neighb;
    const pti escaping_group = sg.descr.at(sg.worm[defenders_neighb]).group_id;
    const bool ladder_ext = false;
    const int escapes = who_defends;
    const int iteration = 0;
    const pti next_att_dot = where + v1;
    const pti next_def_dot = where + v2;
    return {checkLadderStep(x, ladder_breakers, v1, v2, escaping_group,
                            ladder_ext, escapes, iteration),
            next_att_dot, next_def_dot};
}

namespace
{

std::vector<int> findPathInGraph(
    std::map<Game::Edge, Game::EdgeInfo> &capAndFlow, int source, int sink)
{
    using Edge = Game::Edge;
    std::vector<int> path;
    int current_point = source;
    path.push_back(current_point);
    std::set<int> visited;
    visited.insert(current_point);
    using IteratorT = typename std::decay_t<decltype(capAndFlow)>::iterator;
    std::vector<IteratorT> stack;

    auto findEdgeToVisit = [&capAndFlow, &stack, &path](int current_point)
    {
        if (stack.size() != path.size())
        {
            stack.push_back(capAndFlow.lower_bound(Edge{current_point, 0}));
            return stack.back();
        }
        return std::next(stack.back());
    };

    auto tryToGoDeeper = [&capAndFlow, &stack, &path, &visited](
                             int current_point, IteratorT it) -> bool
    {
        auto isValid = [&capAndFlow, current_point](IteratorT it) -> bool
        {
            if (it == capAndFlow.end()) return false;
            return it->first.first == current_point;
        };

        auto mayVisit = [&visited](IteratorT it) -> bool
        {
            return (it->second.flow < it->second.capacity &&
                    not visited.contains(it->first.second));
        };

        auto visit =
            [&capAndFlow, &stack, &path, &visited, current_point](IteratorT it)
        {
            stack.back() = it;
            int next_point = it->first.second;
            ++(it->second.flow);
            --capAndFlow.at(Edge{next_point, current_point}).flow;
            visited.insert(next_point);
            path.push_back(next_point);
        };

        for (; isValid(it); ++it)
        {
            if (mayVisit(it))
            {
                visit(it);
                return true;
            }
        }
        return false;
    };

    auto unvisit = [&capAndFlow, &stack, &path, &visited](int current_point)
    {
        stack.pop_back();
        if (!stack.empty())
        {
            auto it = stack.back();
            int prev_point = it->first.first;
            --(it->second.flow);
            ++capAndFlow.at(Edge{current_point, prev_point}).flow;
        }
        visited.erase(current_point);
        path.pop_back();
    };

    try
    {
        for (;;)
        {
            current_point = path.back();
            if (current_point == sink) return path;
            const auto it = findEdgeToVisit(current_point);
            const bool went_deeper = tryToGoDeeper(current_point, it);
            if (went_deeper) continue;
            unvisit(current_point);
            if (path.empty()) return path;
        }
    }
    catch (...)
    {
        std::cout << "Wyjatek!\n";
        std::cout << "path:";
        for (auto el : path) std::cout << " " << el;
        std::cout << std::endl;
        std::cout << "stack:";
        for (auto el : stack)
        {
            auto [key, value] = *el;
            std::cout << " " << key.first << "->" << key.second << ":("
                      << value.capacity << "," << value.flow << ")";
        }
        std::cout << std::endl;

        std::cout << "full:";
        for (auto [key, value] : capAndFlow)
            std::cout << " " << key.first << "->" << key.second << ":("
                      << value.capacity << "," << value.flow << ")";
        std::cout << std::endl;
        throw;
    }
}

}  // anonymous namespace

std::map<Game::Edge, Game::EdgeInfo> Game::findCapAndFlow(pti source, int who,
                                                          pti sink,
                                                          pti outerMask,
                                                          int infty) const
{
    std::map<Edge, EdgeInfo> capAndFlow;
    for (auto ind = coord.first; ind <= coord.last; ind++)
    {
        if (whoseDotMarginAt(ind) == 0)
        {
            capAndFlow.emplace(Edge{ind, ind | outerMask}, EdgeInfo{1, 0});
            capAndFlow.emplace(Edge{ind | outerMask, ind}, EdgeInfo{0, 0});
            if (coord.dist[ind] == 0)
            {
                capAndFlow.emplace(Edge{ind | outerMask, sink}, EdgeInfo{1, 0});
                capAndFlow.emplace(Edge{sink, ind | outerMask}, EdgeInfo{0, 0});
                continue;
            }

            for (int d = 0; d < 4; d++)
            {
                const auto ind2 = ind + coord.nb4[d];
                if (whoseDotMarginAt(ind2) == 0)
                {
                    capAndFlow.emplace(Edge{ind | outerMask, ind2},
                                       EdgeInfo{1, 0});
                    capAndFlow.emplace(Edge{ind2, ind | outerMask},
                                       EdgeInfo{0, 0});
                }
            }
            continue;
        }

        if (whoseDotMarginAt(ind) == who &&
            sg.descr.at(sg.worm[ind]).leftmost == ind)
        {
            std::set<pti> neighbs;
            const auto leftmost = ind;
            auto addNeighbs = [&neighbs, this](int i)
            {
                for (int d = 0; d < 4; d++)
                {
                    const auto ind2 = i + coord.nb4[d];
                    if (whoseDotMarginAt(ind2) == 0) neighbs.insert(ind2);
                }
            };
            addNeighbs(ind);
            for (pti i = sg.nextDot[ind]; i != leftmost; i = sg.nextDot[i])
            {
                addNeighbs(i);
            }
            // we have all neighbours, connect them to sink if sg.worm is safe
            const bool isSourceInThisWorm =
                (whoseDotMarginAt(source) == who &&
                 sg.descr.at(sg.worm[source]).leftmost == ind);
            if (sg.descr.at(sg.worm[ind]).isSafe())
            {
                for (const auto el : neighbs)
                {
                    capAndFlow.emplace(Edge{el | outerMask, sink},
                                       EdgeInfo{1, 0});
                    capAndFlow.emplace(Edge{sink, el | outerMask},
                                       EdgeInfo{0, 0});
                }
                if (isSourceInThisWorm)
                {
                    capAndFlow.emplace(Edge{source | outerMask, sink},
                                       EdgeInfo{infty, 0});
                    capAndFlow.emplace(Edge{sink, source | outerMask},
                                       EdgeInfo{0, 0});
                }
                continue;
            }
            // not safe, so connect each point with a fixed point inside, and a
            // point inside with each point When source is inside, source can be
            // this fixed point and there is no need for connections from
            // neighbs to source.
            const auto fixedPoint = ind;
            if (not isSourceInThisWorm)
            {
                capAndFlow.emplace(Edge{fixedPoint, fixedPoint | outerMask},
                                   EdgeInfo{infty, 0});
                capAndFlow.emplace(Edge{fixedPoint | outerMask, fixedPoint},
                                   EdgeInfo{0, 0});
            }
            for (const auto el : neighbs)
            {
                if (isSourceInThisWorm)
                {
                    capAndFlow.emplace(Edge{source | outerMask, el},
                                       EdgeInfo{1, 0});
                    capAndFlow.emplace(Edge{el, source | outerMask},
                                       EdgeInfo{0, 0});
                }
                else
                {
                    capAndFlow.emplace(Edge{el | outerMask, fixedPoint},
                                       EdgeInfo{infty, 0});
                    capAndFlow.emplace(Edge{fixedPoint, el | outerMask},
                                       EdgeInfo{0, 0});
                    capAndFlow.emplace(Edge{fixedPoint | outerMask, el},
                                       EdgeInfo{1, 0});
                    capAndFlow.emplace(Edge{el, fixedPoint | outerMask},
                                       EdgeInfo{0, 0});
                }
            }
        }
    }
    return capAndFlow;
}

/// who tries to enclose point source, returns the minimum number of dots to
/// play returns infty if the number >= infty
int Game::findNumberOfDotsToEncloseBy(pti source, int who, int infty) const
{
    const pti sink = 0;
    const pti outerMask = 0x4000;
    std::map<Edge, EdgeInfo> capAndFlow =
        findCapAndFlow(source, who, sink, outerMask, infty);
    // now go from source to sink
    int current_point = source | outerMask;

    for (int count = 0; count < infty; ++count)
    {
        std::vector<int> path =
            findPathInGraph(capAndFlow, current_point, sink);
        if (path.empty()) return count;
    }
    return infty;
}

/// Finds simplifying enclosures (=those that have 0 territory).
bool Game::appendSimplifyingEncl(
    std::vector<std::shared_ptr<Enclosure>> &encl_moves, uint64_t &zobrists,
    int who)
{
    zobrists = 0;
    bool something_left = false;
    for (auto &t : threats[who - 1].threats)
    {
        if ((t.type & ThreatConsts::TERR) and (t.terr_points == 0))
        {
            assert(t.opp_dots);
            encl_moves.push_back(t.encl);
            zobrists ^= t.zobrist_key;
        }
        else if (t.opp_dots and !t.opp_thr.empty())
            something_left = true;
    }
    return something_left;
}

/// Finds simplifying enclosures (=those that have 0 territory) and priorities
/// to use in Game::getEnclMoves. Stores them in  Game::ml_encl_moves and
/// Game::ml_priorities
void Game::getSimplifyingEnclAndPriorities(int who)
{
    ml_encl_moves.clear();
    ml_encl_zobrists.clear();
    ml_encl_zobrists.push_back(0);
    bool something_left =
        appendSimplifyingEncl(ml_encl_moves, ml_encl_zobrists[0], who);
    ml_priorities.clear();
    if (!something_left) return;
    // this could be omitted, duplicates might slow down later, but checking for
    // them is also slow
    std::vector<uint64_t> ml_deleted_opp_thr;
    ml_deleted_opp_thr.reserve(threats[who - 1].threats.size());
    for (auto &t : threats[who - 1].threats)
    {
        if ((t.type & ThreatConsts::TERR) and (t.terr_points == 0))
        {
            for (auto z : t.opp_thr)
            {
                if (std::find(ml_deleted_opp_thr.begin(),
                              ml_deleted_opp_thr.end(),
                              z) == ml_deleted_opp_thr.end())
                {
                    ml_deleted_opp_thr.push_back(z);
                }
            }
        }
    }
    // now find priorities, i.e., those our threats that may cancel some opp's
    // threat not yet canceled by simplifying_encl
    for (auto &t : threats[who - 1].threats)
    {
        if (t.opp_dots and !t.opp_thr.empty())
        {
            ThrInfo ti;
            for (auto z : t.opp_thr)
            {
                if (std::find(ml_deleted_opp_thr.begin(),
                              ml_deleted_opp_thr.end(),
                              z) == ml_deleted_opp_thr.end())
                {
                    const Threat *othr =
                        threats[2 - who].findThreatZobrist_const(z);
                    assert(othr != nullptr);
                    if (othr->opp_dots)
                    {
                        // This:
                        //   ti.saved_dots += othr->opp_dots;
                        // is inaccurate, as it counts some dots many times.
                        for (auto p : othr->encl->interior)
                        {
                            if (whoseDotMarginAt(p) == who)
                            {
                                if (std::find(ti.saved_worms.begin(),
                                              ti.saved_worms.end(),
                                              sg.worm[p]) ==
                                    ti.saved_worms.end())
                                {
                                    // our worm not yet counted
                                    ti.saved_worms.push_back(sg.worm[p]);
                                    ti.saved_dots +=
                                        sg.descr.at(sg.worm[p]).dots[who - 1];
                                }
                            }
                        }
                    }
                    ti.opp_thr.push_back(z);
                }
            }
            if (!ti.opp_thr.empty())
            {
                ti.thr_pointer = &t;
                ti.lost_terr_points = t.terr_points;
                ti.type = t.type;
                ti.move = t.where;
                ti.won_dots = t.opp_dots;
                ti.priority_value = ti.calculatePriorityValue();
                ml_priorities.push_back(std::move(ti));
            }
        }
    }
}

/// @param[in] ind   Place on the edge to check
/// @return          1-4: makes sense to play here, 0: neutral, -1: dame
int Game::checkBorderMove(pti ind, int who) const
{
    assert(coord.dist[ind] == 0 and sg.worm[ind] == 0);
    if (threats[0].is_in_border[ind] > 0 || threats[1].is_in_border[ind] > 0)
        return 4;  // atari always makes sense
    int x = coord.x[ind];
    int y = coord.y[ind];
    pti viter[2], vnorm;  // vnorm: outer normal unit vector
    if (x == 0 || x == coord.wlkx - 1)
    {
        if (y == 0 || y == coord.wlky - 1)  // corner?
            return 0;
        viter[0] = coord.N;
        viter[1] = coord.S;
        vnorm = (x == 0) ? coord.W : coord.E;
    }
    else
    {
        viter[0] = coord.E;
        viter[1] = coord.W;
        vnorm = (y == 0) ? coord.N : coord.S;
    }
    int neighb_worm = sg.worm[ind - vnorm];
    int neighb_whose = (neighb_worm & sg.MASK_DOT);
    if (neighb_whose == 0)
    {
        // it's usually not very good to play on the edge, when 2nd line is
        // empty
        // TODO: check some situations
        return 0;
    }
    else if (neighb_whose == who)
    {
        // neighbour is our dot. Everything depends on its safety.
        if (sg.descr.at(neighb_worm).isSafe())
            return -1;  // dame
        else
        {
            // half-safe, check if we can escape
            auto safe0 =
                checkBorderOneSide(ind - vnorm, viter[0], vnorm, 3 - who);
            auto safe1 =
                checkBorderOneSide(ind - vnorm, viter[1], vnorm, 3 - who);
            return 2 + safe0 + safe1;  // 0 if we can escape in both directions,
                                       // 2: one, 4: none
        }
    }
    else
    {
        assert(neighb_whose == 3 - who);
        if (sg.descr.at(neighb_worm).isSafe())
        {
            return -1;  // dame
        }
        else
        {
            // half-safe, first check if it might be possible to reduce/secure
            // territory (v103+)
            for (int point : {ind - vnorm + viter[0], ind - vnorm + viter[1]})
            {
                if (threats[0].is_in_terr[point] > 0 ||
                    threats[0].is_in_encl[point] > 0 ||
                    threats[1].is_in_terr[point] > 0 ||
                    threats[1].is_in_encl[point] > 0)
                    return 1;
            }
            // then check if opp can escape
            auto safe0 = checkBorderOneSide(ind - vnorm, viter[0], vnorm, who);
            if (safe0 == -1) return -1;
            auto safe1 = checkBorderOneSide(ind - vnorm, viter[1], vnorm, who);
            return safe1 < 0 ? -1 : 2;  // -1 if opp can escape in at least one
                                        // direction, 2: if opp cannot escape
        }
    }
}

/// @param[in] ind    Place of a dot on a 2nd line to check.
/// @param[in] viter  Add this to ind to iterate over 2nd line (for example,
/// ==coord.E or .W for top line)
/// @param[in] vnorm  Add this to ind to get neighbour on the edge (for example,
/// ==coord.N for top line)
/// @param[in] who    Who is going to attack the dot on 2nd line.
/// @return           1: dot on the 2nd cannot escape in direction viter, -1:
/// can escape
int Game::checkBorderOneSide(pti ind, pti viter, pti vnorm, int who) const
{
    const short int restab[2][16] = {
        {0, 1, -1, 0, 1, 1, 0, 0, -1, 1, -1, 0, 0, 0, 0, 0},
        {0, -1, 1, 0, -1, -1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0}};
    ind += viter;
    while (coord.dist[ind] == 1)
    {
        int code = whoseDotMarginAt(ind) | (whoseDotMarginAt(ind + vnorm) << 2);
        auto r = restab[who - 1][code];
        if (r) return r;
        ind += viter;
    }
    // next margin
    return whoseDotMarginAt(ind) == who
               ? 1
               : -1;  // in fact, 1 is optimistic, opp could escape on the
                      // second edge
}

void Game::rollout(Treenode *node, int /*depth*/)
{
    // experiment: add loses to amaf inside opp enclosures; first remember empty
    // points
    // TODO: at this point we already do not know all empty points, there could
    // be enclosures in
    //       the generateMoves-playout phase. Solution: save empty points at
    //       findBestMove. BUT on the other hand, sometimes it's good to play in
    //       opp's terr to reduce it.
    std::vector<pti> amafboard(coord.getSize(), 0);
    const int amaf_empty = -5;
    for (auto i = coord.first; i <= coord.last; i++)
    {
        if (whoseDotMarginAt(i) == 0) amafboard[i] = amaf_empty;
    }
    // we are at leaf, playout...
    auto nmoves = sg.getHistory().size();
    real_t v = randomPlayout();
    auto lastWho = node->move.who;
    // auto endmoves = std::min(sg.getHistory().size(), nmoves + 50);
    auto endmoves = sg.getHistory().size();
    const int distance_rave = 3;
    const int distance_rave_TERR = 8;
    const int amaf_ENCL_BORDER = 16;
    const int distance_rave_SHIFT = 5;
    const int distance_rave_MASK = 7;
    sg.updateGoodReplies(lastWho, v);
    {
        int distance_rave_threshhold = (endmoves - nmoves + 2) / distance_rave;
        int distance_rave_current = distance_rave_threshhold / 2;
        int distance_rave_weight = distance_rave + 1;
        for (auto i = nmoves; i < endmoves; i++)
        {
            lastWho ^= 3;
            amafboard[sg.getHistory().get(i)] =
                lastWho | (distance_rave_weight << distance_rave_SHIFT) |
                (sg.getHistory().isInTerrWithAtari(i) ? distance_rave_TERR
                                                      : 0) |
                (sg.getHistory().isInEnclBorder(i) ? amaf_ENCL_BORDER : 0);
            if (--distance_rave_current == 0)
            {
                distance_rave_current = distance_rave_threshhold;
                if (distance_rave_weight > 1) --distance_rave_weight;
            }
            assert(coord.dist[sg.getHistory().get(i)] >= 1 ||
                   (whoseDotMarginAt(sg.getHistory().get(i)) == lastWho));
        }
        // experiment: add loses to amaf inside opp enclosures
        for (auto i = coord.first; i <= coord.last; i++)
        {
            if (amafboard[i] == amaf_empty)
            {
                int who = whoseDotMarginAt(i);
                if (who == 0)
                {
                    // here add inside territories, it's important when playouts
                    // stop playing before the end
                    if (threats[0].is_in_terr[i] > 0)
                        who = 1;
                    else if (threats[1].is_in_terr[i] > 0)
                        who = 2;
                    else
                        who = -1;  // dame!
                }
                if (who)
                {
                    amafboard[i] =
                        who - 3;  // == -(opponent of encl owner) or -4 for dame
                }
                else
                {
                    amafboard[i] = 0;
                }
            }
        }
    }
    // save playout outcome to t and amaf statistics
    for (;;)
    {
        auto move_ind = node->move.ind;
        auto move_who = node->move.who;
        auto adjusted_value = (move_who == 1) ? v : 1 - v;
        node->t.playouts +=
            1 -
            node->getVirtualLoss();  // add 1 new playout and undo virtual loss
        node->t.value_sum = node->t.value_sum.load() + adjusted_value;
        if (node == node->parent)
        {
            // we are at root
            // node->t.playouts +=
            //    node->getVirtualLoss();  // in root we do not add virtual
            //    loss, but we
            // 'undid' it, so we have to add it again -- now we just set it at 0
            break;
        }
        amafboard[move_ind] =
            move_who | ((distance_rave + 1)
                        << distance_rave_SHIFT);  // before 'for' loop, so that
                                                  // it counts also in amaf
        node = node->parent;
        Treenode *ch = node->children;
        for (;;)
        {
            if (amafboard[ch->move.ind] < 0)
            {
                if (ch->move.who == -amafboard[ch->move.ind])
                {
                    ch->amaf.playouts += 2;  // add 2 loses
                                             // ch->amaf.value_sum += 0;
                }
                else if (amafboard[ch->move.ind] == -4)
                {
                    // dame, add 1 playout (?)
                    ch->amaf.playouts += 1;
                    ch->amaf.value_sum =
                        ch->amaf.value_sum.load() + adjusted_value;
                }
            }
            else if ((amafboard[ch->move.ind] & distance_rave_MASK) ==
                     ch->move.who)
            {
                if (ch->move.enclosures.empty() or
                    (amafboard[ch->move.ind] & amaf_ENCL_BORDER))
                {  // we want to avoid situation when in ch there is enclosure,
                   // but in amaf not (anymore?)
                    int count = amafboard[ch->move.ind] >> distance_rave_SHIFT;
                    ch->amaf.playouts += count;
                    ch->amaf.value_sum =
                        ch->amaf.value_sum.load() + adjusted_value * count;
                }
            }
            else if (adjusted_value < 0.2 and
                     (amafboard[ch->move.ind] & distance_rave_MASK) ==
                         3 - ch->move.who)
            {  // inside encl, == 0 possible when there was enclosure in the
               // generateListOfMoves-playout phase
                // v114+: add also losses for playing inside opp's (reduced)
                // territory -- sometimes
                //  there is an opp terr which always may be reduced, then opp
                //  will mostly play there, but it does not usually make sense
                //  that we play
                if (amafboard[ch->move.ind] & distance_rave_TERR)
                {
                    int count = amafboard[ch->move.ind] >> distance_rave_SHIFT;
                    ch->amaf.playouts += count;
                    ch->amaf.value_sum =
                        ch->amaf.value_sum.load() + adjusted_value * count;
                }
            }
            if (ch->isLast()) break;
            ch++;
        }
        // amafboard[move_ind] = move_who;
    }
}

std::default_random_engine &Game::getRandomEngine() { return engine; }

/// Finds necessary and optional enclosures after move of player who.
/// move may be 0 to find enclosures after neutral moves (i.e., those that do
/// not cancel
///  any opp's threats).
/// ml_priorities should be set before calling.
/// @param[out] encl_moves  Here the obligatory enclosures are added.
/// @param[out] opt_moves   Here optional enclosures are saved (i.e., first
/// zeroed, then added).
/// @param[out] encl_zobrists  First is added the zobrist for all encl_moves.
/// Then zobrists for optional enclosures are added at the end.
void Game::getEnclMoves(std::vector<std::shared_ptr<Enclosure>> &encl_moves,
                        std::vector<std::shared_ptr<Enclosure>> &opt_encl_moves,
                        std::vector<uint64_t> &encl_zobrists, pti move, int who)
{
    // ml_encl_moves.clear();   this is already done in
    // Game::getSimplifyingEnclAndPriorities()
    opt_encl_moves.clear();
    encl_zobrists.push_back(0);
    if (ml_priorities.empty()) return;
    ml_priority_vect.clear();
    /*
    for (auto &t : ml_priorities) {
      if ((t.type & ThreatConsts::TERR) || (t.move == move and (t.type &
    ThreatConsts::ENCL))) { ml_priority_vect.push_back(t);   // TODO: use
    copy_if ?
      }
    }
    */
    std::copy_if(ml_priorities.begin(), ml_priorities.end(),
                 back_inserter(ml_priority_vect),
                 [move](ThrInfo &t)
                 {
                     return (t.type & ThreatConsts::TERR) ||
                            (t.move == move and (t.type & ThreatConsts::ENCL));
                 });
    if (move == 0)
    {
        ml_special_moves.clear();
        // here we are called by generateListOfMoves, so we will need
        // ml_special_moves list
        for (auto &t : threats[2 - who].threats)
        {
            if (t.where != 0)
            {
                assert(t.type & ThreatConsts::ENCL);
                ml_special_moves.push_back(t.where);
            }
        }
        for (auto &t : ml_priorities)
        {
            if (t.move != 0)
            {
                assert(t.type & ThreatConsts::ENCL);
                ml_special_moves.push_back(t.move);
            }
        }
    }
    if (ml_priority_vect.empty()) return;
    // delete those opp's threats that need to put a dot at 'move'
    if (move)
    {
        for (auto &t : threats[2 - who].threats)
        {
            if (t.where == move)
            {
                assert(t.type & ThreatConsts::ENCL);
                for (auto &ourt : ml_priority_vect)
                {
                    auto pos = std::find(ourt.opp_thr.begin(),
                                         ourt.opp_thr.end(), t.zobrist_key);
                    if (pos != ourt.opp_thr.end())
                    {
                        *pos = ourt.opp_thr.back();
                        ourt.opp_thr.pop_back();
                        auto opp_d = 0;
                        if (t.opp_dots)
                        {
                            // This:
                            //  ourt.saved_dots -= t.opp_dots;
                            // is inaccurate, so we calculate it properly
                            for (auto p : t.encl->interior)
                            {
                                if (whoseDotMarginAt(p) == who)
                                {
                                    auto pos = std::find(
                                        ourt.saved_worms.begin(),
                                        ourt.saved_worms.end(), sg.worm[p]);
                                    if (pos != ourt.saved_worms.end())
                                    {
                                        // our worm was counted, subtract
                                        *pos = ourt.saved_worms.back();
                                        ourt.saved_worms.pop_back();
                                        opp_d += sg.descr.at(sg.worm[p])
                                                     .dots[who - 1];
                                    }
                                }
                            }
                            ourt.saved_dots -= opp_d;
                        }
                        if (ourt.opp_thr.empty())
                        {
                            ourt.priority_value = ThrInfoConsts::MINF;
                        }
                        else
                        {
                            ourt.priority_value -=
                                ThrInfoConsts::VALUE_SAVED_DOT * opp_d;
                        }
                    }
                }
            }
        }
    }
    // sort enclosure according to priority_value
    std::sort(ml_priority_vect.begin(), ml_priority_vect.end());
    auto encl_zobrists_0 = encl_zobrists.back();
    for (auto it = ml_priority_vect.begin(); it != ml_priority_vect.end(); ++it)
    {
        if (it->priority_value > ThrInfoConsts::MINF)
        {
            /*
            Threat *thr = threats[who-1].findThreatZobrist(it->zobrist_key);
            assert(thr != nullptr);
            */
            if (it != ml_priority_vect.begin() and isInTerr(move, who))
            {
                // eido_u0O9GbZR problem, we played inside terr and now may have
                // overlapping threats, ignore those that contain prev threats
                // or is contained in prev threat
                bool is_contained = false;
                auto some_point_inside_it =
                    it->thr_pointer->encl->interior.at(0);
                for (auto otherIt = ml_priority_vect.begin(); otherIt != it;
                     ++otherIt)
                {
                    if (otherIt->thr_pointer->encl->isInInterior(
                            some_point_inside_it))
                    {
                        is_contained = true;
                        break;
                    }
                    auto some_point_inside_other =
                        otherIt->thr_pointer->encl->interior.at(0);
                    if (it->thr_pointer->encl->isInInterior(
                            some_point_inside_other))
                    {
                        is_contained = true;
                        break;
                    }
                }
                if (is_contained) continue;
            }
            if (it->priority_value > 0)
            {
                encl_moves.push_back(it->thr_pointer->encl);
                encl_zobrists_0 ^= it->thr_pointer->zobrist_key;
            }
            else
            {
                opt_encl_moves.push_back(it->thr_pointer->encl);
                encl_zobrists.push_back(it->thr_pointer->zobrist_key);
            }
        }
        else
            break;
        // remove opp threats that are already taken care of by t from our next
        // threats
        bool unsorted = false;
        for (uint64_t z : it->opp_thr)
        {
            const Threat *othr =
                nullptr;  // threats[2-who].findThreatZobrist(z);
                          // lazy evaluated later
            for (auto it2 = it + 1; it2 != ml_priority_vect.end(); ++it2)
            {
                auto pos =
                    std::find(it2->opp_thr.begin(), it2->opp_thr.end(), z);
                if (pos != it2->opp_thr.end())
                {
                    *pos = it2->opp_thr.back();
                    it2->opp_thr.pop_back();
                    unsorted = true;
                    if (it2->opp_thr.empty())
                    {
                        it2->priority_value = ThrInfoConsts::MINF;
                    }
                    else
                    {
                        if (othr == nullptr)
                            othr = threats[2 - who].findThreatZobrist_const(
                                z);  // lazy evaluated here
                        auto opp_d = 0;
                        if (othr->opp_dots)
                        {
                            // This:
                            //  it2->saved_dots -= othr->opp_dots;
                            // is inaccurate, so we calculate it properly
                            for (auto p : othr->encl->interior)
                            {
                                if (whoseDotMarginAt(p) == who)
                                {
                                    auto pos = std::find(
                                        it2->saved_worms.begin(),
                                        it2->saved_worms.end(), sg.worm[p]);
                                    if (pos != it2->saved_worms.end())
                                    {
                                        // our worm was counted, subtract
                                        *pos = it2->saved_worms.back();
                                        it2->saved_worms.pop_back();
                                        opp_d += sg.descr.at(sg.worm[p])
                                                     .dots[who - 1];
                                    }
                                }
                            }
                            it2->saved_dots -= opp_d;
                        }
                        it2->priority_value -=
                            ThrInfoConsts::VALUE_SAVED_DOT * opp_d;
                    }
                }
            }
        }
        // sort the rest again
        if (unsorted) std::sort(it + 1, ml_priority_vect.end());
    }
}

void Game::placeDot(int x, int y, int who)
// places a dot of who at (x,y),
// TODO: if rules:must-surround, then also makes necessary enclosures
{
    const pti ind = coord.ind(x, y);
    assert(sg.worm[ind] == 0);
    recalculate_list.clear();
    const uint32_t atari_neighb_code = threats[2 - who].getAtariNeighbCode(ind);
    const bool notInTerrOrEncl =
        (isInTerr(ind, who) == 0 and isInEncl(ind, who) == 0);
    const bool isInBorder_ind_who = isInBorder(ind, who) != 0;
    const bool isInBorder_ind_opp = isInBorder(ind, 3 - who) != 0;
#ifdef DEBUG_SGF
    sgf_tree.makePartialMove({(who == 1 ? "B" : "W"), {coord.indToSgf(ind)}});
#endif
    std::vector<pti> to_check = findThreats_preDot(ind, who);
    std::vector<pti> to_check2m = findThreats2moves_preDot(ind, who);
    // remove opp threats that need to put at ind
    // This is important to do before glueing worms, because otherwise counting
    // singular_dot becomes complicated.
    // TODO: we could probably just mark them as TO_REMOVE
    removeAtPoint(ind, 3 - who);
    if (isInEncl(ind, 3 - who) + isInTerr(ind, 3 - who) == 1)
    {
        // if the new dot is 'singular', we must add it here, because descr
        // still doesn't know about it at this stage
        threats[2 - who].findThreatWhichContains(ind)->singular_dots++;
    }
    const bool update_safety_dame =
        sg.placeDot(x, y, who, notInTerrOrEncl, atari_neighb_code,
                    isInBorder_ind_who, isInBorder_ind_opp, update_soft_safety);
    checkThreats_postDot(to_check, ind, who);
    checkThreats2moves_postDot(to_check2m, ind, who);

    // remove move [ind] from possible moves
    pattern3_value[0][ind] = 0;
    pattern3_value[1][ind] = 0;
    possible_moves.changeMove(ind, PossibleMovesConsts::REMOVED);
    interesting_moves.changeMove(ind, InterestingMovesConsts::REMOVED);
    if (update_safety_dame)
    {
        possibleMoves_updateSafetyDame();
    }
    else if (coord.dist[ind] == 1)
    {
        // update safety dame for neighbours of [ind]
        possibleMoves_updateSafety(ind);
    }
    for (int i = 0; i < 8; i++)
    {
        auto nb = ind + coord.nb8[i];
        if (whoseDotMarginAt(nb) == 0 and
            Pattern3::isPatternPossible(pattern3_at[nb]))
        {
            pattern3_at[nb] = PATTERN3_IMPOSSIBLE;
            recalculate_list.push_back(nb);
        }
    }
    zobrist ^= coord.zobrist_dots[who - 1][ind];
}

void Game::show() const
{
    //  std::cerr << coord.showBoard(sg.worm);  // sg.worm.data()); ?
    std::cerr << coord.showColouredBoardWithDots(sg.worm);
    std::cerr << "Score: " << sg.score[0].show() << "; " << sg.score[1].show()
              << std::endl;
}

void Game::show(const std::vector<pti> &moves) const
{
    auto col_getter = [&](int x, int y)
    {
        int value = (sg.worm[coord.ind(x, y)] == 0)
                        ? 0
                        : (sg.worm[coord.ind(x, y)] % 4);
        if (value == 0)
        {
            pti what = coord.ind(x, y);
            const bool found =
                (std::find(moves.begin(), moves.end(), what) != moves.end());
            return found ? -1 : 0;
        }
        return value;
    };
    auto str_getter = [&](int x, int y) -> std::string
    {
        pti what = coord.ind(x, y);
        const bool found =
            (std::find(moves.begin(), moves.end(), what) != moves.end());
        if (found) return "M ";
        return "  ";
    };
    std::cerr << coord.showColouredBoardWithDotsAndDebugMoves(col_getter,
                                                              str_getter);
}

void Game::showSvg(const std::string &filename,
                   const std::vector<pti> &tab) const
{
    Svg svg(coord.wlkx, coord.wlky);
    for (int i = coord.first; i <= coord.last; ++i)
        if (coord.dist[i] >= 0)
            svg.drawDot(coord.x[i], coord.y[i],
                        std::max<pti>(0, std::min<pti>(tab[i], 3)));

    std::fstream fs;
    fs.open(filename, std::fstream::out);
    fs << svg.to_str();
    fs.close();
}

void Game::showConnections()
{
    std::cerr << "Player 1" << std::endl;
    std::cerr << coord.showBoard(getConnects(0));
    std::cerr << "Player 2" << std::endl;
    std::cerr << coord.showBoard(getConnects(1));
}

void Game::showGroupsId()
{
    for (auto &d : sg.descr)
    {
        std::cerr << d.first << "->" << d.second.group_id << "  ";
    }
    std::cerr << std::endl;
}

void Game::showThreats2m()
{
    for (int g = 0; g < 2; ++g)
    {
        int inside = 0;
        std::cerr << "Threats for player " << g << ":" << std::endl;
        int count = 0;
        for (auto &t : threats[g].threats2m)
        {
            std::cerr << "[" << count++ << "] " << coord.showPt(t.where0)
                      << " -- moves: " << t.thr_list.size() << std::endl;
            if (threats[g].is_in_encl[t.where0] > 0 ||
                threats[g].is_in_terr[t.where0] > 0)
                inside++;
            for (auto &t2 : t.thr_list)
            {
                std::cerr << "   " << coord.showPt(t2.where);
            }
            std::cerr << std::endl;
        }
        std::cerr << "Stats: all/inside = " << count << " / " << inside
                  << std::endl;
    }
}

Enclosure Game::findSimpleEnclosure(pti point, pti mask, pti value)
{
    return findSimpleEnclosure(sg.worm, point, mask, value);
}

Enclosure Game::findSimpleEnclosure(std::vector<pti> &tab, pti point, pti mask,
                                    pti value) const
// tries to enclose 'point' using dots given by (tab[...] & mask) == value,
// check only size-1 or size-2 enclosures All points in 'tab' should have zero
// bits MASK_MARK and MASK_BORDER.
// TODO: check whether using an array is really efficient, maybe vector +
// std::move would be better
{
    {
        int count = 0;
        int direction{-1};  // initialisation not needed
        for (int i = 0; i < 4; i++)
        {
            pti nb = point + coord.nb4[i];
            if ((tab[nb] & mask) == value)
            {
                count++;
            }
            else
            {
                direction = i;
            }
        }
        if (count == 4)
        {
            std::vector<pti> border = {static_cast<pti>(point + coord.N),
                                       static_cast<pti>(point + coord.W),
                                       static_cast<pti>(point + coord.S),
                                       static_cast<pti>(point + coord.E),
                                       static_cast<pti>(point + coord.N)};
            return Enclosure({point}, std::move(border));
        }

        if (count == 3)
        {
            pti nb = point + coord.nb4[direction];
            if (coord.dist[nb] <= 0)
            {  // (added in v138)
                return empty_enclosure;
                ;  // nb is on the edge or outside; TODO: then in fact there'd
                   // be no enclosure at all, no need to check for the
                   // non-simple one
            }
            if ((tab[nb + coord.nb4[direction]] & mask) == value and
                (tab[nb + coord.nb4[(direction + 1) & 3]] & mask) == value and
                (tab[nb + coord.nb4[(direction + 3) & 3]] & mask) == value)
            {
                // 2-point enclosure!
                pti ptor = point,
                    nbor = nb;  // we save original order to make the program
                                // play in the same way
                if (direction == 0 || direction == 3)
                {  // N or W?
                    std::swap(nb, point);
                    direction ^= 2;  // take opposite direction
                }
                assert(point < nb);
                if (direction == 1)
                {  // E
                    std::vector<pti> border = {
                        static_cast<pti>(point + coord.N),
                        static_cast<pti>(point + coord.W),
                        static_cast<pti>(point + coord.S),
                        static_cast<pti>(nb + coord.S),
                        static_cast<pti>(nb + coord.E),
                        static_cast<pti>(nb + coord.N),
                        static_cast<pti>(point + coord.N)};
                    // return Enclosure( {point, nb}, border );
                    return Enclosure({ptor, nbor}, std::move(border));
                }
                else
                {  // S
                    std::vector<pti> border = {
                        static_cast<pti>(point + coord.N),
                        static_cast<pti>(point + coord.W),
                        static_cast<pti>(nb + coord.W),
                        static_cast<pti>(nb + coord.S),
                        static_cast<pti>(nb + coord.E),
                        static_cast<pti>(point + coord.E),
                        static_cast<pti>(point + coord.N)};
                    // return Enclosure( {point, nb}, border );
                    return Enclosure({ptor, nbor}, std::move(border));
                }
            }
        }
    }
    return empty_enclosure;
}

Enclosure Game::findNonSimpleEnclosure(pti point, pti mask, pti value)
{
    return findNonSimpleEnclosure(sg.worm, point, mask, value);
}

Enclosure Game::findNonSimpleEnclosure(std::vector<pti> &tab, pti point,
                                       pti mask, pti value) const
// tries to enclose 'point' using dots given by (tab[...] & mask) == value
// All points in 'tab' should have zero bits MASK_MARK and MASK_BORDER.
// TODO: check whether using an array is really efficient, maybe vector +
// std::move would be better
{
    std::array<pti, Coord::maxSize> stack;
    stack[0] = point;
    tab[point] |= sg.MASK_MARK;
    int stackSize = 1;
    // std::vector<uint8_t> tab(coord.getSize(), 0);
    pti leftmost = Coord::maxSize;
    mask |= sg.MASK_MARK;
    //  Cleanup<std::vector<pti>&, pti> cleanup(tab, ~sg.MASK_MARK);
    CleanupUsingList<std::vector<pti> &, pti> cleanup(
        tab, ~(sg.MASK_MARK | sg.MASK_BORDER));
    cleanup.push(point);
    //  int border_count = 0;
    do
    {
        pti ind = stack[--stackSize];
        for (int i = 0; i < 4; i++)
        {
            pti nb = ind + coord.nb4[i];
            if ((tab[nb] & mask) == value)
            {
                if ((tab[nb] & sg.MASK_BORDER) == 0)
                {  // a border dot not yet visited?
                    tab[nb] |= sg.MASK_BORDER;
                    cleanup.push(nb);
                    // border_count++;
                    if (nb < leftmost) leftmost = nb;
                }
            }
            else if ((tab[nb] & sg.MASK_MARK) == 0)
            {
                if (coord.dist[nb] > 0)
                {  // interior point on the board (and not on the edge)
                    tab[nb] |= sg.MASK_MARK;
                    stack[stackSize++] = nb;
                    cleanup.push(nb);
                }
                else
                {
                    // we got to the edge of the board!  note: cleaning done by
                    // 'cleanup' object
                    return empty_enclosure;
                }
            }
        }
    } while (stackSize);
    // traverse the border to find minimal-area enclosure
    stack[0] = leftmost + coord.NE;
    stack[1] = leftmost;
    int direction = coord.findDirectionNo(stack[1], stack[0]) + 1;
    stack[2] =
        stack[1] +
        coord.nb8[direction];  // stack[2] = coord.findNextOnRight(stack[1],
                               // stack[0]);
    tab[stack[0]] |= sg.MASK_MARK;
    tab[stack[1]] |= sg.MASK_MARK;
    int top = 2;
    do
    {
        // check if current point is on the border, if not, take next
        // (clockwise)
        while ((tab[stack[top]] & sg.MASK_BORDER) == 0)
        {
            direction++;
            assert(stack[top - 1] + coord.nb8[direction] ==
                   coord.findNextOnRight(stack[top - 1], stack[top]));
            stack[top] =
                stack[top - 1] +
                coord.nb8[direction];  // stack[top] =
                                       // coord.findNextOnRight(stack[top-1],
                                       // stack[top]);
        }
        if (stack[top] == stack[0])  // enclosure found
            break;
        if (tab[stack[top]] & sg.MASK_MARK)
        {
            // this point has been already visited, go back to the last visit of
            // that point and try next neighbour
            pti loop_pt = stack[top];
            top--;
            pti prev_pt = stack[top];
            while (stack[top] != loop_pt) tab[stack[top--]] &= ~sg.MASK_MARK;
            // now we are again at loop_pt which has been already unMARKed
            direction = coord.findDirectionNo(loop_pt, prev_pt) + 1;
            assert(loop_pt + coord.nb8[direction] ==
                   coord.findNextOnRight(loop_pt, prev_pt));
            stack[++top] =
                loop_pt +
                coord.nb8[direction];  // stack[++top] =
                                       // coord.findNextOnRight(loop_pt,
                                       // prev_pt);
        }
        else
        {
            // visit the point
            tab[stack[top]] |= sg.MASK_MARK;
            top++;
            direction =
                (direction + 5) &
                7;  // coord.findDirectionNo(stack[top-1], stack[top-2]) + 1;
            assert(stack[top - 1] + coord.nb8[direction] ==
                   coord.findNextOnRight(stack[top - 1], stack[top - 2]));
            stack[top] =
                stack[top - 1] +
                coord.nb8[direction];  // stack[top] =
                                       // coord.findNextOnRight(stack[top-1],
                                       // stack[top-2]);
        }
    } while (stack[top] != stack[0]);
    // now the enclosure is kept in stack[0..top], it has also (MARK | BORDER)
    // bits set, but these will be cleared by cleanup at exit
    // TODO: in most cases all interior points will be found, for example, if
    // number of MARKed points is small
    //  (in such cases it'd be also possible to reserve optimal amount of
    //  memory)
    std::vector<pti> interior;
    /*
    if (top == border_count) {
      int tt = cleanup.count - top;
      interior.reserve(cleanup.count - top);  // reserve exact memory for
    interior for (int i=0; i<cleanup.count; i++) if ((tab[cleanup.list[i]] &
    (sg.MASK_MARK | sg.MASK_BORDER)) != (sg.MASK_MARK | sg.MASK_BORDER)) {  //
    interior point interior.push_back(cleanup.list[i]);
        }
      assert(tt == interior.size());
      } else */
    {
        interior.reserve(
            cleanup.count +
            40);  // reserve memory for interior found so far + border + 40
                  // (arbitrary const), which should be enough in most cases
        for (int i = 0; i < cleanup.count; i++)
            if ((tab[cleanup.list[i]] & (sg.MASK_MARK | sg.MASK_BORDER)) !=
                (sg.MASK_MARK | sg.MASK_BORDER))
            {  // interior point
                interior.push_back(cleanup.list[i]);
                for (int j = 0; j < 4; j++)
                {
                    pti nb = cleanup.list[i] + coord.nb4[j];
                    if ((tab[nb] & (sg.MASK_MARK | sg.MASK_BORDER)) == 0)
                    {
                        tab[nb] |= sg.MASK_MARK;
                        cleanup.push(nb);
                    }
                }
            }
    }
    // std::sort(interior.begin(), interior.end());
    return Enclosure(std::move(interior),
                     std::move(std::vector<pti>(&stack[0], &stack[top + 1])));
}

Enclosure Game::findEnclosure(pti point, pti mask, pti value)
{
    return findEnclosure(sg.worm, point, mask, value);
}

Enclosure Game::findEnclosure(std::vector<pti> &tab, pti point, pti mask,
                              pti value) const
// tries to enclose 'point' using dots given by (tab[...] & mask) == value
// All points in 'tab' should have zero bits MASK_MARK and MASK_BORDER.
{
    auto encl = findSimpleEnclosure(tab, point, mask, value);
    if (encl.isEmpty())
        return findNonSimpleEnclosure(tab, point, mask, value);
    else
        return encl;
}

Enclosure Game::findEnclosure_notOptimised(pti point, pti mask, pti value)
{
    return findEnclosure_notOptimised(sg.worm, point, mask, value);
}

Enclosure Game::findEnclosure_notOptimised(std::vector<pti> &tab, pti point,
                                           pti mask, pti value) const
// tries to enclose 'point' using dots given by (tab[...] & mask) == value
// All points in 'tab' should have zero bits sg.MASK_MARK and sg.MASK_BORDER.
// TODO: check whether using an array is really efficient, maybe vector +
// std::move would be better
{
    // check simple enclosure
    {
        int count = 0;
        for (int i = 0; i < 4; i++)
        {
            pti nb = point + coord.nb4[i];
            if ((tab[nb] & mask) == value)
            {
                count++;
            }
            else
                break;
        }
        if (count == 4)
        {
            std::vector<pti> border = {static_cast<pti>(point + coord.N),
                                       static_cast<pti>(point + coord.W),
                                       static_cast<pti>(point + coord.S),
                                       static_cast<pti>(point + coord.E),
                                       static_cast<pti>(point + coord.N)};
            return Enclosure({point}, border);
        }
        // TODO: it'd be possible to optimise also for count==3 (then remove
        // 'break' in for loop above)
    }
    std::array<pti, Coord::maxSize> stack;
    stack[0] = point;
    tab[point] |= sg.MASK_MARK;
    int stackSize = 1;
    // std::vector<uint8_t> tab(coord.getSize(), 0);
    pti leftmost = Coord::maxSize;
    mask |= sg.MASK_MARK;
    //  Cleanup<std::vector<pti>&, pti> cleanup(tab, ~sg.MASK_MARK);
    CleanupUsingList<std::vector<pti> &, pti> cleanup(
        tab, ~(sg.MASK_MARK | sg.MASK_BORDER));
    cleanup.push(point);
    do
    {
        pti ind = stack[--stackSize];
        for (int i = 0; i < 4; i++)
        {
            pti nb = ind + coord.nb4[i];
            if ((tab[nb] & mask) == value)
            {
                if ((tab[nb] & sg.MASK_BORDER) == 0)
                {  // a border dot not yet visited?
                    tab[nb] |= sg.MASK_BORDER;
                    cleanup.push(nb);
                    if (nb < leftmost) leftmost = nb;
                }
            }
            else if ((tab[nb] & sg.MASK_MARK) == 0)
            {
                if (coord.dist[nb] > 0)
                {  // interior point on the board (and not on the edge)
                    tab[nb] |= sg.MASK_MARK;
                    stack[stackSize++] = nb;
                    cleanup.push(nb);
                }
                else
                {
                    // we got to the edge of the board!  note: cleaning done by
                    // 'cleanup' object
                    return empty_enclosure;
                }
            }
        }
    } while (stackSize);
    // traverse the border to find minimal-area enclosure
    stack[0] = leftmost + coord.NE;
    stack[1] = leftmost;
    stack[2] = coord.findNextOnRight(stack[1], stack[0]);
    tab[stack[0]] |= sg.MASK_MARK;
    tab[stack[1]] |= sg.MASK_MARK;
    int top = 2;
    do
    {
        // check if current point is on the border, if not, take next
        // (clockwise)
        while ((tab[stack[top]] & sg.MASK_BORDER) == 0)
        {
            stack[top] = coord.findNextOnRight(stack[top - 1], stack[top]);
        }
        if (stack[top] == stack[0])  // enclosure found
            break;
        if (tab[stack[top]] & sg.MASK_MARK)
        {
            // this point has been already visited, go back to the last visit of
            // that point and try next neighbour
            pti loop_pt = stack[top];
            top--;
            pti prev_pt = stack[top];
            while (stack[top] != loop_pt) tab[stack[top--]] &= ~sg.MASK_MARK;
            // now we are again at loop_pt which has been already unMARKed
            stack[++top] = coord.findNextOnRight(loop_pt, prev_pt);
        }
        else
        {
            // visit the point
            tab[stack[top]] |= sg.MASK_MARK;
            top++;
            stack[top] = coord.findNextOnRight(stack[top - 1], stack[top - 2]);
        }
    } while (stack[top] != stack[0]);
    // now the enclosure is kept in stack[0..top], it has also (MARK | BORDER)
    // bits set, but these will be cleared by cleanup at exit
    // TODO: in most cases all interior points will be found, for example, if
    // number of MARKed points is small
    //  (in such cases it'd be also possible to reserve optimal amount of
    //  memory)
    std::vector<pti> interior;
    interior.reserve(
        cleanup.count);  // reserve memory for interior found so far + border,
                         // which should be enough in most cases
    for (int i = 0; i < cleanup.count; i++)
        if ((tab[cleanup.list[i]] & (sg.MASK_MARK | sg.MASK_BORDER)) !=
            (sg.MASK_MARK | sg.MASK_BORDER))
        {  // interior point
            interior.push_back(cleanup.list[i]);
            for (int j = 0; j < 4; j++)
            {
                pti nb = cleanup.list[i] + coord.nb4[j];
                if ((tab[nb] & (sg.MASK_MARK | sg.MASK_BORDER)) == 0)
                {
                    tab[nb] |= sg.MASK_MARK;
                    cleanup.push(nb);
                }
            }
        }
    // std::sort(interior.begin(), interior.end());
    return Enclosure(std::move(interior),
                     std::vector<pti>(&stack[0], &stack[top + 1]));
}

int Game::floodFillExterior(std::vector<pti> &tab, pti mark_by,
                            pti stop_at) const
// Marks points in tab by mark_by, flooding from exterior and stopping if
// (tab[...] & stop_at). Returns number of marked points (inside the board).
{
    std::array<pti, Coord::maxSize> stack;
    int stackSize = 0;
    int count = 0;
    pti test_mask = (mark_by | stop_at);
    // left and right edge
    {
        int lind = coord.ind(0, 0);
        int rind = coord.ind(coord.wlkx - 1, 0);
        for (int j = 0; j < coord.wlky; j++)
        {
            if ((tab[lind] & test_mask) == 0)
            {
                tab[lind] |= mark_by;
                stack[stackSize++] = lind;
                count++;
            }
            if ((tab[rind] & test_mask) == 0)
            {
                tab[rind] |= mark_by;
                stack[stackSize++] = rind;
                count++;
            }
            lind += coord.S;
            rind += coord.S;
        }
    }
    // top and bottom edge
    {
        int tind = coord.ind(0, 0);
        int bind = coord.ind(0, coord.wlky - 1);
        for (int i = 0; i < coord.wlkx; i++)
        {
            if ((tab[tind] & test_mask) == 0)
            {
                tab[tind] |= mark_by;
                stack[stackSize++] = tind;
                count++;
            }
            if ((tab[bind] & test_mask) == 0)
            {
                tab[bind] |= mark_by;
                stack[stackSize++] = bind;
                count++;
            }
            tind += coord.E;
            bind += coord.E;
        }
    }
    // now mark exterior
    while (stackSize)
    {
        pti p = stack[--stackSize];
        for (int j = 0; j < 4; j++)
        {
            pti nb = p + coord.nb4[j];
            if (coord.dist[nb] >= 0 and (tab[nb] & test_mask) == 0)
            {
                tab[nb] |= mark_by;
                stack[stackSize++] = nb;
                count++;
            }
        }
    }
    return count;
}

Enclosure Game::findInterior(std::vector<pti> border) const
// in: list of border points (for example, from an sgf file -- therefore, the
// function needs not be fast) out: Enclosure class with border and interior
// v131: corrected, old version did not work for many types of borders (when
// there's a hole between enclosures).
{
    std::vector<pti> dad(coord.getSize(), 0);
    std::vector<pti> dad2(coord.getSize(), 0);
    // mark border id dad
    for (unsigned i = 0; i < border.size() - 1; ++i)
    {
        auto b = border[i];
        auto next = border[i + 1];
        dad[b] = next;
        dad2[next] = b;
    }
    std::vector<pti> interior;
    interior.reserve(border.size() < 12 ? 12
                                        : (coord.wlkx - 2) * (coord.wlky - 2));
    for (int i = 0; i < coord.wlkx; ++i)
    {
        int intersections = 0;
        int fromWhere = 0;
        pti ind = coord.ind(i, 0);
        for (int j = 0; j < coord.wlky; ++j, ind += coord.S)
        {
            if (dad[ind])
            {
                auto d = (coord.x[dad[ind]] - i) + (coord.x[dad2[ind]] - i);
                if (fromWhere == 0)
                {  // recently we were NOT on the border
                    if (d == 0)
                    {
                        intersections ^= 1;
                    }
                    else if (d == -1 || d == 1)
                    {
                        fromWhere = d;
                    }
                }
                else if (d)
                {  // recently we were on the border
                    if (d != fromWhere) intersections ^= 1;
                    fromWhere = 0;  // we get out from the border
                }
            }
            else if (intersections)
            {
                interior.push_back(ind);
            }
        }
    }
    return Enclosure(std::move(interior), std::move(border));
}

void Game::makeEnclosure(const Enclosure &encl, bool remove_it_from_threats)
// remove_it_from_threats == true: 'encl' has been already marked as TO_REMOVE
// in threats and will be removed
//   == false:  if 'encl' is in threats, it has to be found.
{
    const pti worm_no = sg.worm[encl.getBorderElement()];
    const int who = (worm_no & sg.MASK_DOT);
    bool is_inside_terr = true, is_inside_terr_or_encl = true,
         is_inside_some_encl = false, is_in_our_terr_or_encl = true;
    const uint64_t encl_zobr = encl.zobristKey(who);
#ifdef DEBUG_SGF
    sgf_tree.makePartialMove_addEncl(encl.toSgfString());
#endif
    bool update_safety_dame = false;
    std::set<std::pair<pti, pti>> singular_worms{};
    bool some_worms_were_not_singular = false;
    auto updateSingInfo = [&](pti leftmost)
    {
        if (isInEncl(leftmost, 3 - who) + isInTerr(leftmost, 3 - who) == 1)
        {
            // this worm was singular
            singular_worms.insert(
                {leftmost, sg.descr.at(sg.worm[leftmost]).dots[who - 1]});
        }
        else
        {
            some_worms_were_not_singular = true;
        }
    };
    updateSingInfo(sg.descr.at(worm_no).leftmost);
    for (auto &p : encl.border)
    {
        if (sg.worm[p] != worm_no)
        {
            // new worm on the border, merge it
            updateSingInfo(sg.descr.at(sg.worm[p]).leftmost);
            if ((sg.descr.at(sg.worm[p]).safety <= 1 ||
                 sg.descr.at(worm_no).safety <= 1) and
                (sg.descr.at(sg.worm[p]).safety + sg.descr.at(worm_no).safety >=
                 2))
            {
                update_safety_dame = true;
            }
            if ((sg.descr.at(sg.worm[p]).safety <= 1 ||
                 sg.descr.at(worm_no).safety <= 1) and
                (sg.descr.at(sg.worm[p]).safety + sg.descr.at(worm_no).safety >=
                 1))
            {
                update_soft_safety =
                    sg.safety_soft.getUpdateValueForAllMargins();
            }
            sg.wormMergeSame(worm_no, sg.worm[p]);
        }
        if (isInTerr(p, 3 - who) == 0)
        {
            is_inside_terr = false;
            if (isInEncl(p, 3 - who) == 0) is_inside_terr_or_encl = false;
        }
        if (isInEncl(p, 3 - who)) is_inside_some_encl = true;
        if (isInEncl(p, who) == 0 and isInTerr(p, who) == 0)
            is_in_our_terr_or_encl = false;
    }
    // if some worms were singular, and some not, it means that the enclosure
    // removed some of the opp's threats, to make this removal calculate
    // singular dots correctly, we need to remove current singular, see game
    // szkrab6492 (last red move)
    if (some_worms_were_not_singular and not singular_worms.empty())
    {
        for (auto [leftmost, dots] : singular_worms)
        {
            threats[2 - who].findThreatWhichContains(leftmost)->singular_dots -=
                dots;
        }
    }
    //
    bool check_encl = (is_inside_terr_or_encl and is_inside_some_encl);
    pti first = 0, last = 0;  // of the list of empty interior points
    int empty_count = 0;      // number of empty interior points
    int enemy_dots =
        0;  // number of enemy dots inside that have not been yet captured,
            // important only when (is_in_our_terr_or_encl == true)
    krb::SmallVector<pti, 32>::allocator_type::arena_type arena_gids_to_delete;
    krb::SmallVector<pti, 32> gids_to_delete{arena_gids_to_delete};
    std::vector<pti> stack;
    stack.reserve(coord.last + 1);
    for (auto &p : encl.interior)
    {
        if (sg.worm[p] == 0)
        {
            empty_count++;
            if (first)
            {
                sg.nextDot[last] = p;
                last = p;
            }
            else
            {
                first = last = p;
            }
            sg.worm[p] = worm_no;
        }
        else if (sg.worm[p] != worm_no)
        {
            if ((sg.worm[p] & sg.MASK_DOT) != who)
            {
                enemy_dots += sg.descr[sg.worm[p]].dots[2 - who];
                if (sg.descr[sg.worm[p]].neighb.size() > 1)
                {
                    // append if not there
                    if (std::find(gids_to_delete.begin(), gids_to_delete.end(),
                                  sg.descr[sg.worm[p]].group_id) ==
                        gids_to_delete.end())
                        gids_to_delete.push_back(sg.descr[sg.worm[p]].group_id);
                }
                sg.wormMergeOther(worm_no, sg.worm[p]);
            }
            else
                sg.wormMergeSame(worm_no, sg.worm[p]);
        }
    }
    // add the empty interior points, if any
    if (first)
    {
        pti next = sg.nextDot[encl.getBorderElement()];
        sg.nextDot[encl.getBorderElement()] = first;
        sg.nextDot[last] = next;
        if (is_inside_terr_or_encl)
        {
            assert(isInTerr(first, 3 - who) > 0 ||
                   isInEncl(first, 3 - who) > 0);
            for (auto &t : threats[2 - who].threats)
            {
                if (t.encl->isInInterior(first))
                {
                    t.terr_points -= empty_count;
                }
            }
        }
    }
    // recalculate opponents groups, if needed
    if (!gids_to_delete.empty())
    {
        for (auto &d : sg.descr)
        {
            if (std::find(gids_to_delete.begin(), gids_to_delete.end(),
                          d.second.group_id) != gids_to_delete.end())
                d.second.group_id = 0;
        }
        // go
        for (auto &d : sg.descr)
        {
            if (d.second.group_id == 0)
            {
                pti id = d.first;
                stack.push_back(d.first);
                d.second.group_id = id;
                while (!stack.empty())
                {
                    pti cmp = stack.back();
                    stack.pop_back();
                    for (auto n : sg.descr.at(cmp).neighb)
                        if (sg.descr.at(n).group_id == 0)
                        {
                            sg.descr.at(n).group_id = id;
                            stack.push_back(n);
                        }
                }
            }
        }
    }
    // mark interior of current enclosure
    stack.assign(coord.last + 1, 0);
    for (auto &p : encl.interior)
    {
        stack[p] = 1;
        pattern3_value[0][p] = 0;  // and zero pattern3 values
        pattern3_value[1][p] = 0;
        possible_moves.changeMove(p, PossibleMovesConsts::REMOVED);
        interesting_moves.changeMove(p, InterestingMovesConsts::REMOVED);
    }
    if (update_safety_dame)
    {
        possibleMoves_updateSafetyDame();
    }
    // remove our threats
    for (auto &t : threats[who - 1].threats)
    {
        if ((t.type & ThreatConsts::TO_REMOVE) == 0 and stack[t.where] == 0)
        {
            if (std::any_of(t.encl->border.begin(), t.encl->border.end(),
                            [&stack](auto p) { return stack[p]; }) or
                std::all_of(t.encl->interior.begin(), t.encl->interior.end(),
                            [&stack](auto p) { return stack[p]; }))
            {
                t.type |= ThreatConsts::TO_REMOVE;
            }
            if ((t.type & ThreatConsts::TO_REMOVE) == 0 and
                is_in_our_terr_or_encl)
            {
                // update t, if it happens to include made enclosure
                // first fast check -- if it does not contain one interior
                // point, it does not include enclosure encl, so we're done
                if (t.encl->isInInterior(encl.getInteriorElement()))
                {
                    // ok, fast check positive, it contains one interior point,
                    // maybe it's enough?
                    bool contains = true;
                    if (encl.interior.size() > 1)
                    {
                        // interior is bigger than 1 point, need to check
                        // carefully we want to do it faster than by m*n
                        // operations (m,n -- interior sizes of t.encl, encl)
                        unsigned count = 0;
                        for (auto &p : t.encl->interior) count += stack[p];
                        contains = (count == encl.interior.size());
                    }
                    if (contains)
                    {
                        t.terr_points -= empty_count;
                        t.opp_dots -= enemy_dots;  // captured enemy dots were
                                                   // not singular, so no need
                                                   // to update singular_dots
                    }
                }
            }
        }
        else
        {
            t.type |= ThreatConsts::TO_REMOVE;
        }
    }
    // remove our threats in 2 moves
    for (auto &t2 : threats[who - 1].threats2m)
    {
        for (auto &t : t2.thr_list)
        {
            if ((t.type & ThreatConsts::TO_REMOVE) == 0 and
                stack[t.where] == 0 and stack[t2.where0] == 0)
            {
                for (auto &p : t.encl->border)
                {
                    if (stack[p])
                    {
                        t.type |= ThreatConsts::TO_REMOVE;
                        break;
                    }
                }
            }
            else
            {
                t.type |= ThreatConsts::TO_REMOVE;
            }
        }
    }
    // remove current enclosure, if needed
    if (remove_it_from_threats)
    {
        auto zobr = encl_zobr;
        for (auto &t : threats[who - 1].threats)
        {
            if (t.zobrist_key == zobr)
            {
                t.type |= ThreatConsts::TO_REMOVE;
            }
        }
    }
    // remove
    removeMarked(who);
    threats[who - 1].removeMarked2moves();
    // remove opponent threats
    // std::cerr << "remove opp threats..." << std::endl;
    bool removed = false;
    for (unsigned tn = 0; tn < threats[2 - who].threats.size(); tn++)
    {
        Threat *thr = &threats[2 - who].threats[tn];
        if ((thr->type & ThreatConsts::TO_REMOVE) == 0)
        {
            if (stack[thr->where] == 1)
            {
                thr->type |= ThreatConsts::TO_REMOVE;
                removed = true;
                // std::cerr << "removing " << coord.showPt(thr->where) <<
                // std::endl;
            }
            else
            {
                std::shared_ptr<Enclosure> encl =
                    thr->encl;  // thr may be invalidated by adding new threats
                                // in checkThreat_terr
                for (auto &p : encl->border)
                {
                    if (stack[p])
                    {
                        thr->type |= ThreatConsts::TO_REMOVE;
                        removed = true;
                        // std::cerr << "removing2 " << coord.showPt(thr->where)
                        // << ", zobr=" << thr->zobrist_key << std::endl;
                        if (check_encl and (thr->type & ThreatConsts::ENCL))
                        {
                            checkThreat_encl(thr, 3 - who);
                            thr =
                                &threats[2 - who]
                                     .threats[tn];  // thr could be invalidated
                        }
                        break;
                    }
                }
                if ((thr->type & ThreatConsts::TO_REMOVE) == 0 and
                    thr->encl->isInInterior(encl->getBorderElement()))
                {
                    auto tmp = countDotsTerrInEncl(*thr->encl, who);
                    thr->opp_dots = std::get<0>(tmp);
                    thr->terr_points = std::get<1>(tmp);
                }
            }
        }
        else
            removed = true;
    }

    if (is_inside_terr)
    {
        checkThreat_terr(nullptr, encl.getBorderElement(), 3 - who);
    }
    if (removed)
    {
        removeMarked(3 - who);
    }
    // remove opponent threats in 2 moves
    bool removed2 = false;
    for (auto &thr2t : threats[2 - who].threats2m)
    {
        if (stack[thr2t.where0] == 1)
        {
            for (auto &t : thr2t.thr_list)
            {
                t.type |= ThreatConsts::TO_REMOVE;
            }
            removed2 = true;
        }
        else
        {
            for (unsigned tn = 0; tn < thr2t.thr_list.size(); tn++)
            {
                Threat *thr = &thr2t.thr_list[tn];
                if (stack[thr->where] == 1)
                {
                    thr->type |= ThreatConsts::TO_REMOVE;
                    removed2 = true;
                }
                else
                {
                    std::shared_ptr<Enclosure> encl =
                        thr->encl;  // thr may be invalidated by adding new
                                    // threats in checkThreat_terr
                    for (auto &p : encl->border)
                    {
                        if (stack[p])
                        {
                            removed2 = true;
                            // thr->type |= ThreatConsts::TO_REMOVE;  // in
                            // checkThreat2moves_encl if (check_encl2moves) {
                            // ...
                            checkThreat2moves_encl(thr, thr2t.where0, 3 - who);
                            // thr2 = &threats[2-who].threats2m[tn2];  // thr2
                            // and thr = &thr2->thr_list[tn];              //
                            // thr could be invalidated (when using vectors for
                            // threats2m[]
                            break;
                        }
                    }
                }
            }
        }
    }
    if (removed2)
    {
        threats[2 - who].removeMarked2moves();
    }
    // recalculate points outside the enclosure, and mark the border in stack
    for (auto &p : encl.border)
    {
        stack[p] = 1;  // mark additionally the border
        for (int i = 0; i < 4; i++)
        {
            auto nb = p + coord.nb4[i];
            assert(!(whoseDotMarginAt(nb) == 0 and stack[nb] != 0));
            if (whoseDotMarginAt(nb) ==
                    0  // and stack[nb] == 0  (always satisfied, when
                       // whoseDotMarginAt(nb) == 0)
                and Pattern3::isPatternPossible(pattern3_at[nb]))
            {
                pattern3_at[nb] = PATTERN3_IMPOSSIBLE;
                recalculate_list.push_back(nb);
            }
        }
    }
    // remove connections
    for (auto &p : encl.interior)
    {
        sg.connectionsReset(p, 1);
        sg.connectionsReset(p, 2);
        // check diagonal neighbours outside the enclosure
        for (int i = 0; i < 8; i += 2)
        {  // +=2, to visit only diagonal neighb.
            pti nb = p + coord.nb8[i];
            if (stack[nb] == 0)
            {
                sg.connectionsRecalculatePoint(nb, 3 - who);
            }
        }
    }
    if (!gids_to_delete.empty())
    {
        for (int ind = coord.first; ind <= coord.last; ind++)
        {
            for (int j = 0; j < 4; j++)
            {
                if (sg.getConnects(2 - who)[ind].groups_id[j] == 0) break;
                if (std::find(gids_to_delete.begin(), gids_to_delete.end(),
                              sg.getConnects(2 - who)[ind].groups_id[j]) !=
                    gids_to_delete.end())
                {
                    sg.connectionsRecalculateConnect(ind, 3 - who);
                    // sg.connectionsRecalculatePoint(ind, 3-who);
                    break;
                }
            }
        }
    }
    zobrist ^= encl_zobr;
}

std::pair<int, int> Game::countTerritory(int now_moves) const
{
    const int ct_B = 1;
    const int ct_W = 2;
    const int ct_NOT_TERR_B = 4;
    const int ct_NOT_TERR_W = 8;
    const int ct_TERR_B = 0x10;
    const int ct_TERR_W = 0x20;
    const int ct_COUNTED = 0x40;
    assert(((ct_B | ct_W | ct_NOT_TERR_B | ct_NOT_TERR_W | ct_TERR_B |
             ct_TERR_W | ct_COUNTED) &
            (sg.MASK_MARK | sg.MASK_BORDER)) ==
           0);  // sg.MASK_MARK and _BORDER should be different
    //
    std::vector<pti> marks(coord.getSize(), 0);
    for (int i = coord.first; i <= coord.last; i++) marks[i] = sg.whoseDotAt(i);
    floodFillExterior(marks, ct_NOT_TERR_B, ct_B);
    floodFillExterior(marks, ct_NOT_TERR_W, ct_W);
    // remove B dots inside W terr, and vice versa, they cannot be used as
    // border of pools
    /*  (not needed, we may use masks in findEnclosure())
    for (int i = coord.first; i<=coord.last; i++) {
      if ((marks[i] & (ct_B | ct_NOT_TERR_W)) == ct_B)
        marks[i] &= ~ct_B;
      else if ((marks[i] & (ct_W | ct_NOT_TERR_B)) == ct_W)
        marks[i] &= ~ct_W;
    }
    */
    // for each point masked as possibly inside a territory, try to enclose it
    // std::cerr << coord.showBoard(marks) << std::endl;

    std::vector<Enclosure> poolsB, poolsW;
    for (int i = coord.first; i <= coord.last; i++)
    {
        if ((marks[i] & (ct_B | ct_NOT_TERR_B)) == 0 and coord.dist[i] >= 0)
        {
            // for enclosures, do not use B dots which are inside W's terr
            auto encl = findEnclosure(marks, i, ct_B | ct_NOT_TERR_W,
                                      ct_B | ct_NOT_TERR_W);
            if (!encl.isEmpty())
            {
                poolsB.push_back(std::move(encl));
            }
        }
        if ((marks[i] & (ct_W | ct_NOT_TERR_W)) == 0 and coord.dist[i] >= 0)
        {
            // for enclosures, do not use W dots which are inside B's terr
            auto encl = findEnclosure(marks, i, ct_W | ct_NOT_TERR_B,
                                      ct_W | ct_NOT_TERR_B);
            if (!encl.isEmpty())
            {
                poolsW.push_back(std::move(encl));
            }
        }
    }
    // count points ignoring pools that are included in bigger pools
    int delta_score[4] = {0, 0, 0, 0};  // dots of 0,1, terr of 0,1
    for (auto &e : poolsB)
    {
        bool ok = true;
        for (pti ind : e.border)
            if (marks[ind] & ct_TERR_B)
            {
                ok = false;
                break;
            }
        if (ok)
        {
            for (pti ind : e.interior)
            {
                if ((sg.worm[ind] & sg.MASK_DOT) == 2 and
                    (marks[sg.descr.at(sg.worm[ind]).leftmost] & ct_COUNTED) ==
                        0)
                {
                    // take it
                    delta_score[0] += sg.descr.at(sg.worm[ind]).dots[1];
                    delta_score[1] -= sg.descr.at(sg.worm[ind]).dots[0];
                    marks[sg.descr.at(sg.worm[ind]).leftmost] |= ct_COUNTED;
                }
                else if ((sg.worm[ind] & sg.MASK_DOT) == 0 and
                         (marks[ind] & ct_TERR_B) == 0)
                {
                    delta_score[2]++;
                    marks[ind] |= ct_TERR_B;
                }
            }
            // TODO: if (must-surround)...
        }
    }
    // for W
    for (auto &e : poolsW)
    {
        bool ok = true;
        for (pti ind : e.border)
            if (marks[ind] & ct_TERR_W)
            {
                ok = false;
                break;
            }
        if (ok)
        {
            for (pti ind : e.interior)
            {
                if ((sg.worm[ind] & sg.MASK_DOT) == 1 and
                    (marks[sg.descr.at(sg.worm[ind]).leftmost] & ct_COUNTED) ==
                        0)
                {
                    // take it
                    delta_score[1] += sg.descr.at(sg.worm[ind]).dots[0];
                    delta_score[0] -= sg.descr.at(sg.worm[ind]).dots[1];
                    marks[sg.descr.at(sg.worm[ind]).leftmost] |= ct_COUNTED;
                }
                else if ((sg.worm[ind] & sg.MASK_DOT) == 0 and
                         (marks[ind] & ct_TERR_W) == 0)
                {
                    delta_score[3]++;
                    marks[ind] |= ct_TERR_W;
                }
            }
            // if (must-surround)...
        }
    }
    // calculate the score assuming last-dot-safe==false
    delta_score[3] += global::komi;
    int delta = (delta_score[0] - delta_score[1]);
    int small_score = 0;
    if ((delta_score[2] - delta_score[3]) % 2 == 0)
    {
        delta += (delta_score[2] - delta_score[3]) / 2;
    }
    else
    {
        int dame = 0;
        for (int i = coord.first; i <= coord.last; i++)
            if (coord.dist[i] >= 0)
            {
                if ((marks[i] & (ct_B | ct_W)) == 0 and
                    (marks[i] & (ct_NOT_TERR_B | ct_NOT_TERR_W)) ==
                        (ct_NOT_TERR_B | ct_NOT_TERR_W))
                    dame++;
            }
        int correction;
        if ((dame + now_moves) % 2)
        {
            correction = (delta_score[2] - delta_score[3] - 1) / 2;
        }
        else
        {
            correction = (delta_score[2] - delta_score[3] + 1) / 2;
        }
        delta += correction;
        small_score = delta_score[2] - delta_score[3] - 2 * correction;
    }
    //
    // std::cerr << "Terr-delta-score: " << delta << std::endl;
    return {(sg.score[0].dots - sg.score[1].dots) + delta, small_score};
}

/// Simple function for counting territory, assuming pools of other players do
/// not intersect.
std::pair<int, int> Game::countTerritory_simple(int now_moves) const
{
    // count points ignoring pools that are included in bigger pools
    std::set<pti> marks;
    int delta_score[4] = {0, 0, 0, 0};  // dots of 0,1, terr of 0,1
    int dame = 0;
    for (int ind = coord.first; ind <= coord.last; ++ind)
    {
        if (threats[0].is_in_terr[ind] > 0)
        {
            if (threats[1].is_in_terr[ind] > 0)
            {
                // one pool inside other, use the other function
                return countTerritory(now_moves);
            }
            if (sg.worm[ind])
            {
                if (whoseDotMarginAt(ind) == 2 and
                    marks.find(sg.worm[ind]) == marks.end())
                {
                    marks.insert(sg.worm[ind]);
                    delta_score[0] += sg.descr.at(sg.worm[ind]).dots[1];
                    delta_score[1] -= sg.descr.at(sg.worm[ind]).dots[0];
                }
            }
            else
            {
                delta_score[2]++;
            }
        }
        else if (threats[1].is_in_terr[ind] > 0)
        {
            if (sg.worm[ind])
            {
                if (whoseDotMarginAt(ind) == 1 and
                    marks.find(sg.worm[ind]) == marks.end())
                {
                    marks.insert(sg.worm[ind]);
                    delta_score[1] += sg.descr.at(sg.worm[ind]).dots[0];
                    delta_score[0] -= sg.descr.at(sg.worm[ind]).dots[1];
                }
            }
            else
            {
                delta_score[3]++;
            }
        }
        else if (whoseDotMarginAt(ind) == 0)
        {
            dame++;
        }
    }
    // calculate the score assuming last-dot-safe==false
    delta_score[3] += global::komi;
    int delta = (delta_score[0] - delta_score[1]);
    int small_score = 0;
    if ((delta_score[2] - delta_score[3]) % 2 == 0)
    {
        delta += (delta_score[2] - delta_score[3]) / 2;
    }
    else
    {
        int correction;
        if ((dame + now_moves) % 2)
        {
            correction = (delta_score[2] - delta_score[3] - 1) / 2;
        }
        else
        {
            correction = (delta_score[2] - delta_score[3] + 1) / 2;
        }
        delta += correction;
        small_score = delta_score[2] - delta_score[3] - 2 * correction;
    }
    //
    // std::cerr << "Terr-delta-score: " << delta << std::endl;
#ifndef NDEBUG
    auto ct_score = countTerritory(now_moves);
    if ((sg.score[0].dots - sg.score[1].dots) + delta != ct_score.first or
        small_score != ct_score.second)
    {
        show();
        std::cerr << "res = " << (sg.score[0].dots - sg.score[1].dots) + delta
                  << ", should be = " << ct_score.first << std::endl;
        std::cerr << "delta_score: " << delta_score[0] << ", " << delta_score[1]
                  << ", " << delta_score[2] << ", " << delta_score[3]
                  << ", komi=" << global::komi << std::endl;
        std::cerr << "small_score: " << small_score
                  << ", should be = " << ct_score.second << std::endl;
        //
        std::vector<pti> th(coord.getSize(), 0);
        for (int ind = coord.first; ind <= coord.last; ++ind)
        {
            if (threats[0].is_in_terr[ind] > 0)
            {
                th[ind] += 1;
            }
            if (threats[1].is_in_terr[ind] > 0)
            {
                th[ind] += 2;
            }
        }
        std::cerr << coord.showBoard(th);
        // assert(0);
    }
#endif
    return {(sg.score[0].dots - sg.score[1].dots) + delta, small_score};
}

/// Counts:
///   * dots of who inside worms belonging to who and contained in encl
///   * empty points inside encl
std::pair<int16_t, int16_t> Game::countDotsTerrInEncl(const Enclosure &encl,
                                                      int who,
                                                      bool optimise) const
{
    int16_t count = 0;
    int16_t terr = 0;
    if (optimise and encl.interior.size() <= 4)
    {  // if <=4, then there's no other enclosure inside encl
        for (pti p : encl.interior)
        {
            if (whoseDotMarginAt(p) == 0)
            {
                ++terr;
            }
            else if (whoseDotMarginAt(p) == who)
            {
                ++count;
            }
        }
        return std::make_pair(count, terr);
    }
    std::set<pti> counted;
    for (pti p : encl.interior)
    {
        if (whoseDotMarginAt(p) == 0)
            ++terr;
        else if (whoseDotMarginAt(p) == who and
                 counted.find(sg.worm[p]) == counted.end())
        {
            counted.insert(sg.worm[p]);
            count += sg.descr.at(sg.worm[p]).dots[who - 1];
        }
    }
    return std::make_pair(count, terr);
}

int sgf_move_no = 0;

std::pair<Move, std::vector<std::string>> Game::extractSgfMove(std::string m,
                                                               int who) const
{
    // TODO: in must-surround, this must be done simultanously
    Move move;
    move.ind = coord.sgfToPti(m);
    if (whoseDotMarginAt(move.ind))
    {
        show();
        std::cerr << "Trying to play at " << coord.showPt(move.ind)
                  << std::endl;
        throw std::runtime_error(
            "makeSgfMove error: trying to play at an occupied point");
    }
#ifndef NDEBUG
    std::cerr << "[" << ++sgf_move_no << "] Make move " << who << " : " << m
              << " = " << coord.showPt(coord.sgfToPti(m)) << std::endl;
#endif

    move.who = who;
    std::vector<pti> border;
    std::vector<std::string> points_to_enclose;
    unsigned pos = 2;
    char mode = '.';
    while (pos <= m.length())
    {
        if ((pos < m.length() and (m[pos] == '.' || m[pos] == '!')) ||
            pos == m.length())
        {
            // save last enclosure
            if (!border.empty())
            {
                if (mode == '.')
                {
                    move.enclosures.push_back(std::make_shared<Enclosure>(
                        findInterior(std::move(border))));
                }
                border.clear();
            }
            if (pos < m.length())
            {
                mode = m[pos];
            }
            ++pos;
        }
        else
        {
            if (mode == '.')
            {
                border.push_back(coord.ind(coord.sgfCoordToInt(m[pos]),
                                           coord.sgfCoordToInt(m.at(pos + 1))));
            }
            else
            {
                points_to_enclose.push_back(m.substr(pos, 2));
            }
            pos += 2;
        }
    }
    return {move, points_to_enclose};
}

void Game::makeSgfMove(const std::string &m, int who)
{
    auto [move, points_to_enclose] = extractSgfMove(m, who);
    if (points_to_enclose.empty())
    {
        makeMove(move);
    }
    else
    {
        makeMoveWithPointsToEnclose(move, points_to_enclose);
    }

    assert(checkThreatCorrectness());
    assert(checkThreatWithDfs());
    assert(checkThreat2movesCorrectness());
    assert(checkWormCorrectness());
    assert(checkConnectionsCorrectness());
    assert(checkPattern3valuesCorrectness());

    //#ifndef NDEBUG
    {
#ifdef DEBUG_SGF
        sgf_tree.finishPartialMove();
        if (1)
        {  // change to add marks for threats
            std::vector<pti> list;
            for (auto &t : threats[who - 1].threats2m)
            {
                if (std::find(list.begin(), list.end(), t.where0) == list.end())
                {
                    list.push_back(t.where0);
                }
            }
            SgfProperty prop;
            prop.first = "SQ";
            for (auto i : list) prop.second.push_back(coord.indToSgf(i));
            if (!prop.second.empty())
            {
                sgf_tree.addProperty(prop);
                prop.second.clear();
            }
            list.clear();

            SgfProperty miai;
            for (int i = coord.first; i <= coord.last; i++)
            {
                if (threats[who - 1].is_in_2m_encl[i])
                {
                    prop.second.push_back(coord.indToSgf(i));
                }
                if (threats[who - 1].is_in_2m_miai[i])
                {
                    miai.second.push_back(coord.indToSgf(i));
                }
            }
            if (!prop.second.empty())
            {
                prop.first = "TR";
                sgf_tree.addProperty(prop);
            }
            if (!miai.second.empty())
            {
                miai.first = "MA";  // = X
                sgf_tree.addProperty(miai);
            }
        }
#endif

        /*
        std::vector<pti> list_of_moves;
        for (int i=0; i<coord.maxSize; i++) {
          list_of_moves.push_back(worm[i] & sg.MASK_DOT);
          } */
        // generateListOfMoves(nullptr, who);
        // auto it = sg.getHistory().end();
        //--it;
        // Move m = choosePattern3Move((*it) & sg.getHistory()_move_MASK,
        // (*(it-1)) & sg.getHistory()_move_MASK, 3-who); Move m =
        // chooseAnyMove(3-who);
#ifndef NDEBUG
        /*
        if (m.ind) {
          std::cerr << "Chosen move " << coord.showPt(m.ind) << " with
        encl.size()==" << m.enclosures.size() << std::endl; if
        (m.enclosures.size()) { for (auto e : m.enclosures) { for (auto b :
        e->border) { std::cerr << coord.showPt(b) << "  ";
              }
              std::cerr << std::endl;
            }
          }
          show();
        }
        */
#endif
        /*
        Game g = *this;
        std::cerr << "Playout value = " << g.randomPlayout() << std::endl;
        */
        /*
          for (auto &t : res) {
          list_of_moves[t.move.ind] = 4;
          }*/
        // std::cerr << "List of moves: " << std::endl <<
        // coord.showColouredBoard(list_of_moves) << std::endl;
    }
    //#endif
}

void Game::makeMove(const Move &m)
{
    if (m.ind != 0)
    {
        placeDot(coord.x[m.ind], coord.y[m.ind], m.who);
        for (auto &encl : m.enclosures)
        {
            makeEnclosure(*encl, true);
        }
        if (not m.enclosures.empty()) sg.setEnclosureInLastMove();
        const uint32_t atari_neighb_code =
            threats[2 - m.who].getAtariNeighbCode(m.ind);
        sg.xorAtariNeigbhCodeInLastMove(atari_neighb_code);
        if (must_surround)
        {
            int corner_right_bottom = coord.ind(coord.wlkx - 2, coord.wlky - 2);
            int opponent = 3 - m.who;
            for (int i = coord.ind(1, 1); i <= corner_right_bottom; ++i)
            {
                if (isInTerr(i, m.who) and whoseDotMarginAt(i) == opponent)
                {
                    Enclosure encl = findEnclosure(i, sg.MASK_DOT, m.who);
                    if (!encl.isEmpty())
                    {
                        show();
                        std::cerr << "Enclosure:" << std::endl
                                  << encl.show() << std::endl;
                        std::cerr << "-----------------------------------------"
                                     "--------------------"
                                  << std::endl;
                        makeEnclosure(encl, true);
                    }
                }
            }
        }
        sg.safety_soft.updateAfterMove(&sg, update_soft_safety, m.ind);
        update_soft_safety = 0;
        assert(checkSoftSafetyCorrectness());
        recalculatePatt3Values();
        sg.nowMoves ^= 3;
    }
}

void Game::makeMoveWithPointsToEnclose(
    const Move &m, const std::vector<std::string> &to_enclose)
{
    if (m.ind != 0)
    {
        placeDot(coord.x[m.ind], coord.y[m.ind], m.who);
        for (auto &encl : m.enclosures)
        {
            makeEnclosure(*encl, true);
        }
        for (const auto &e : to_enclose)
        {
            Enclosure encl =
                findEnclosure(coord.sgfToPti(e), sg.MASK_DOT, sg.nowMoves);
            if (!encl.isEmpty())
            {
                makeEnclosure(encl, true);
            }
        }
        sg.safety_soft.updateAfterMove(&sg, update_soft_safety, m.ind);
        update_soft_safety = 0;
        assert(checkSoftSafetyCorrectness());
        recalculatePatt3Values();
        sg.nowMoves ^= 3;
    }
}

pti Game::isInTerr(pti ind, int who) const
{
    return threats[who - 1].is_in_terr[ind];
}

pti Game::isInEncl(pti ind, int who) const
{
    return threats[who - 1].is_in_encl[ind];
}

pti Game::isInBorder(pti ind, int who) const
{
    return threats[who - 1].is_in_border[ind];
}

pti &Game::isInTerr(pti ind, int who)
{
    return threats[who - 1].is_in_terr[ind];
}

pti &Game::isInEncl(pti ind, int who)
{
    return threats[who - 1].is_in_encl[ind];
}

pti &Game::isInBorder(pti ind, int who)
{
    return threats[who - 1].is_in_border[ind];
}

bool Game::isDameOnEdge(pti i, int who) const
{
    if (sg.safety_soft.isDameFor(who, i)) return true;
    const int x = coord.x[i];
    const int y = coord.y[i];
    const bool left = possible_moves.left;
    const bool right = possible_moves.right;
    const bool top = possible_moves.top;
    const bool bottom = possible_moves.bottom;

    if (x == 0)
    {
        if (left ||
            (sg.worm[i + coord.E] and
             sg.descr.at(sg.worm[i + coord.E]).isSafe()) ||
            (y == 0 || y == coord.wlky - 1))
            return true;
    }
    else if (x == coord.wlkx - 1)
    {
        if (right ||
            (sg.worm[i + coord.W] and
             sg.descr.at(sg.worm[i + coord.W]).isSafe()) ||
            (y == 0 || y == coord.wlky - 1))
            return true;
    }
    if (y == 0)
    {
        if (top || (sg.worm[i + coord.S] and
                    sg.descr.at(sg.worm[i + coord.S]).isSafe()))
            return true;
    }
    else if (y == coord.wlky - 1)
    {
        if (bottom || (sg.worm[i + coord.N] and
                       sg.descr.at(sg.worm[i + coord.N]).isSafe()))
            return true;
    }
    return false;
}

NonatomicMovestats Game::priorsAndDameForPattern3(bool &is_dame, bool is_root,
                                                  bool is_in_our_te,
                                                  bool is_in_opp_te, int i,
                                                  int who) const
{
    if (is_dame or is_in_our_te or is_in_opp_te) return {};

    auto v = pattern3_value[who - 1][i];
    if (is_root) out << "p3v=" << v << " ";
#ifndef NDEBUG
    if (v != getPattern3Value(i, who))
    {
        std::cerr << "recalculate_list: ";
        for (auto i : recalculate_list) std::cerr << coord.showPt(i) << " ";
        std::cerr << std::endl;
        show();
        std::cerr << "Wrong patt3 value for who=" << who << " at "
                  << coord.showPt(i) << ", is: " << v
                  << " should be: " << getPattern3Value(i, who) << std::endl;
    }
    assert(v == getPattern3Value(i, who));
#endif
    if (v < 0)
    {  // dame
        is_dame = true;
        return {};
    }

    int value = (v > 0) ? ((v + 15) >> 3) : 0;
    if (is_root) out << "p3p=" << value << " ";
    return wonSimulations(value);
}

NonatomicMovestats Game::priorsAndDameForEdgeMoves(bool &is_dame, bool is_root,
                                                   int i, int who) const
{
    // add prior values for edge moves
    if (is_dame or coord.dist[i] != 0) return {};

    int r = checkBorderMove(i, who);
    if (r < 0)
    {
        is_dame = true;
        return {};
    }
    else if (r > 0)
    {
        if (is_root) out << "edge=" << 3 * r << " ";
        return wonSimulations(3 * r);
    }
    return {};
}

NonatomicMovestats Game::priorsForInterestingMoves_cut_or_connect(bool is_root,
                                                                  int i) const
{
    int weight = 4 * interesting_moves.classOfMove(i);
    if (is_root) out << "intm=" << weight << " ";
    return wonSimulations(weight);
}

NonatomicMovestats Game::priorsForDistanceFromLastMoves(bool is_root,
                                                        int i) const
{
    int dist =
        std::min(coord.distBetweenPts_1(i, sg.getHistory().getLast()),
                 coord.distBetweenPts_1(i, sg.getHistory().getLastButOne()));
    if (dist <= 4)
    {
        const int n_won = (6 - dist);
        if (is_root) out << "dist=" << n_won << " ";
        return wonSimulations(n_won);
    }
    return {};
}

NonatomicMovestats Game::priorsForThreats(bool is_root, bool is_in_opp_te,
                                          int i, int who) const
{
    // add prior values because of threats2m (v118+)
    NonatomicMovestats value{};
    if (isInTerr(i, who) == 0 and !is_in_opp_te)
    {
        const int i_can_enclose =
            threats[who - 1].numberOfDotsToBeEnclosedIn2mAfterPlayingAt(i);
        if (i_can_enclose)
        {
            int num = 5 + std::min(i_can_enclose, 15);
            value += wonSimulations(num);
            if (is_root) out << "thr2m=" << num << " ";
        }

        const int opp_can_enclose =
            threats[2 - who].numberOfDotsToBeEnclosedIn2mAfterPlayingAt(i);
        if (opp_can_enclose)
        {  // and threats[2-who].is_in_terr[t.where0]==0 and
           // threats[2-who].is_in_encl[t.where0]==0 -- this is
           // above (!is_in_opp_te)
            int num = 5 + std::min(opp_can_enclose, 15);
            value += wonSimulations(num);
            if (is_root) out << "thr2mopp=" << num << " ";
        }

        // miai/encl2 (v127+)
        if ((threats[2 - who].is_in_2m_encl[i] > 0 or
             threats[2 - who].is_in_2m_miai[i] > 0) and
            isInBorder(i, who) == 0 and not threats[who - 1].isInBorder2m(i))
        {
            value += lostSimulations(15);
            if (is_root) out << "miai=-15 ";
        }
    }
    else
    {
        int min_terr_size = threats[is_in_opp_te ? 2 - who : who - 1]
                                .getMinAreaOfThreatEnclosingPoint(i);
        // in territory, maybe a good reduction possible
        const auto &whose_threats2m = is_in_opp_te ? threats[who - 1].threats2m
                                                   : threats[2 - who].threats2m;
        for (const auto &t : whose_threats2m)
        {
            if (t.where0 == i)
            {
                if (t.min_win2)
                {
                    int num = 1 + 2 * std::min(min_terr_size, 10);
                    value += wonSimulations(num);
                    if (is_root) out << "reduc=" << num << " ";
                }
                break;
            }
        }
    }
    return value;
}

NonatomicMovestats Game::priorsForLadderExtension(bool is_root, int i,
                                                  int who) const
{
    pti v_diag, v_keima;
    if (who == 1)
    {
        const std::map<pattern3_t, std::pair<pti, pti>> ladder_danger_att1{
            {0x9, {coord.NE, coord.SEE}},    {0x18, {coord.SE, coord.NEE}},
            {0x90, {coord.SE, coord.SSW}},   {0x180, {coord.SW, coord.SSE}},
            {0x900, {coord.SW, coord.NWW}},  {0x1800, {coord.NW, coord.SWW}},
            {0x9000, {coord.NW, coord.NNE}}, {0x8001, {coord.NE, coord.NNW}}};
        const auto iter = ladder_danger_att1.find(pattern3_at[i]);
        if (iter == ladder_danger_att1.end()) return {};
        std::tie(v_diag, v_keima) = iter->second;
    }
    else
    {
        const std::map<pattern3_t, std::pair<pti, pti>> ladder_danger_att2{
            {0x6, {coord.NE, coord.SEE}},    {0x24, {coord.SE, coord.NEE}},
            {0x60, {coord.SE, coord.SSW}},   {0x240, {coord.SW, coord.SSE}},
            {0x600, {coord.SW, coord.NWW}},  {0x2400, {coord.NW, coord.SWW}},
            {0x6000, {coord.NW, coord.NNE}}, {0x4002, {coord.NE, coord.NNW}}};
        const auto iter = ladder_danger_att2.find(pattern3_at[i]);
        if (iter == ladder_danger_att2.end()) return {};
        std::tie(v_diag, v_keima) = iter->second;
    }
    if (whoseDotMarginAt(i + v_keima) != who) return {};
    const auto group_id = sg.descr.at(sg.worm[i + v_diag]).group_id;
    if (sg.descr.at(sg.worm[i + v_keima]).group_id != group_id) return {};
    const pti v_defend = (v_diag + v_keima) / 3;
    const pti v_side = v_diag - v_defend;
    // here we have a shape of the type
    // ? ? x
    // x o .
    // ? i ?
    // where both x are of player who and from the same group, so a potential
    // ladder is possible v_defend is the direction from 'i' to 'o', so here it
    // would be coord.N v_side is the direction here to left (coord.W)
    krb::PointsSet ladder_breakers;
    const auto status = checkLadderStep(
        i + v_defend, ladder_breakers, -v_side, -v_defend,
        sg.descr.at(sg.worm[i + v_defend]).group_id, false, 3 - who, 0);
    const int attacker_wins = 1;
    if (status == attacker_wins)
    {
        if (is_root) out << "goodlad ";
        return wonSimulations(3);
    }
    int length_of_attacker_chain = 0;
    int opp_dots_along = 0;
    const pti outside = 3;
    for (pti point = i; length_of_attacker_chain < 3;
         ++length_of_attacker_chain)
    {
        if (whoseDotMarginAt(point + v_diag) != who) break;
        if (whoseDotMarginAt(point + v_side) == who or
            whoseDotMarginAt(point + v_side) == outside)
            break;
        if (whoseDotMarginAt(point + v_side) == 3 - who) ++opp_dots_along;
        if (isInEncl(point + v_side, who) > 0 or
            isInTerr(point + v_side, who) > 0)
            break;
        point += v_diag;
    }
    if (length_of_attacker_chain + opp_dots_along <= 1) return {};
    if (is_root)
        out << "badlad=" << length_of_attacker_chain + opp_dots_along << " ";
    if (length_of_attacker_chain + opp_dots_along == 2)
        return lostSimulations(5);
    if (length_of_attacker_chain + opp_dots_along == 3)
        return lostSimulations(40);
    return lostSimulations(80);
}

int encl_count, opt_encl_count, moves_count, priority_count;

DebugInfo Game::generateListOfMoves(TreenodeAllocator &alloc, Treenode *parent,
                                    int depth, int who)
{
    ++montec::generateMovesCount;
    assert(checkMarginsCorrectness());
    constexpr int32_t max_prior = 20;
    // get the list
    Treenode tn;
    tn.move.who = who;
    tn.parent = parent;
    tn.setDepth(depth);
    getSimplifyingEnclAndPriorities(who);
    std::vector<std::shared_ptr<Enclosure>> neutral_encl_moves,
        neutral_opt_encl_moves;
    std::vector<uint64_t> neutral_encl_zobrists;
    getEnclMoves(neutral_encl_moves, neutral_opt_encl_moves,
                 neutral_encl_zobrists, 0, who);
    tn.move.enclosures.reserve(ml_encl_moves.size() +
                               neutral_encl_moves.size() +
                               neutral_opt_encl_moves.size());
    // debug:
    encl_count += ml_encl_moves.size();
    opt_encl_count += ml_opt_encl_moves.size();
    priority_count += ml_priorities.size();
    moves_count++;
    //
    const bool is_root = (depth == 1);
    bool dame_already = false;  // to put only 1 dame move on the list
    bool is_nondame_not_in_terr_move =
        false;  // if there's no such move, then dame suddenly becomes a decent
                // move
    DebugInfo debug_info;
    for (int i = coord.first; i <= coord.last; i++)
    {
        // if (coord.dist[i] < 0) continue;
        if (sg.worm[i]) continue;
        if (is_root)
        {
            out.str("");
            out.clear();
            out << "   --> " << coord.showPt(i) << ": ";
        }
        bool is_dame = isDameOnEdge(i, who);
        // check threats -- not good, it may be good to play in opp's territory
        // to reduce it...
        // if ((threats[2-who].is_in_encl[i] > 0 || threats[2-who].is_in_terr[i]
        // > 0) and
        //  threats[who-1].is_in_border[i] == 0) continue;
        tn.move.ind = i;
        tn.flags = 0;
        tn.setDepth(depth);
        NonatomicMovestats priors = defaultInitialPriors();
        bool is_in_our_te =
            (isInEncl(i, who) > 0 ||
             isInTerr(i, who) > 0);  // te = territory or enclosure
        bool is_in_opp_te =
            (isInEncl(i, 3 - who) > 0 || isInTerr(i, 3 - who) > 0);

        priors += priorsAndDameForPattern3(is_dame, is_root, is_in_our_te,
                                           is_in_opp_te, i, who);
        priors += priorsAndDameForEdgeMoves(is_dame, is_root, i, who);

        // save only 1 dame move
#ifndef NDEBUG
        if (is_dame != isDame_directCheck(i, who))
        {
            std::cerr << "Move " << coord.showPt(i) << " for player=" << who
                      << " is_dame: " << is_dame << ", but direct check shows "
                      << !is_dame << std::endl;
            std::cerr << "Direct check for players: "
                      << isDame_directCheck(i, 1) << ", "
                      << isDame_directCheck(i, 2) << std::endl;
            assert(is_dame == isDame_directCheck(i, who));
        }
#endif
        if (is_dame)
        {
            if (dame_already)
            {
                continue;
            }
            dame_already = true;
            if (is_root) out << "dame=true,-5 ";
            priors += lostSimulations(5);
            tn.markAsDame();
        }
        priors += priorsForInterestingMoves_cut_or_connect(is_root, i);
        priors += priorsForDistanceFromLastMoves(is_root, i);
        priors += priorsForThreats(is_root, is_in_opp_te, i, who);
        priors += priorsForLadderExtension(is_root, i, who);

        // captures
        if (std::find(ml_special_moves.begin(), ml_special_moves.end(), i) ==
            ml_special_moves.end())
        {
            tn.move.zobrist_key = coord.zobrist_dots[who - 1][i] ^
                                  ml_encl_zobrists[0] ^
                                  neutral_encl_zobrists[0];
            tn.move.enclosures.clear();
            tn.move.enclosures.insert(tn.move.enclosures.end(),
                                      ml_encl_moves.begin(),
                                      ml_encl_moves.end());
            tn.move.enclosures.insert(tn.move.enclosures.end(),
                                      neutral_encl_moves.begin(),
                                      neutral_encl_moves.end());
            if (is_in_opp_te)
            {
                int min_terr_size =
                    threats[2 - who].getMinAreaOfThreatEnclosingPoint(i);
                if (sg.getConnects(who - 1)[i].groups_id[0] == 0)
                {
                    // dot does not touch any our dot
                    const int loss =
                        100 -
                        std::min(min_terr_size, 20);  // add lost simulations
                    if (is_root) out << "isol=" << -loss << " ";
                    priors += lostSimulations(loss);
                    tn.markAsInsideTerrNoAtari();
                }
                else
                {
                    // we touch our dot
                    const bool isAtari = (threats[who - 1].isInBorder2m(i) > 0);
                    if (not isAtari) tn.markAsInsideTerrNoAtari();
                    if (not isAtari or min_terr_size <= 2)
                    {
                        const int lost = (min_terr_size <= 2) ? 80 : 14;
                        priors += lostSimulations(lost);
                        if (is_root) out << "touch=-" << lost << " ";
                    }
                }
            }
            else
            {  // not in opp terr
                if (isInBorder(i, 3 - who) > 0)
                {
                    // check the value of opp's threat
                    int value = 0, terr_value = 0;
                    for (auto &t : threats[2 - who].threats)
                    {
                        if (t.where == i)
                        {
                            value += t.opp_dots;
                            terr_value += t.terr_points;
                        }
                    }
                    if (value)
                    {
                        int num = 5 + 2 * std::min(value, 15);
                        priors += wonSimulations(num);
                        if (is_root) out << "oppv2m=" << num << " ";
                    }
                    else if (terr_value >= 3)
                    {
                        priors += NonatomicMovestats{4, 4 * 0.9f};
                        if (is_root) out << "oppter=0.9*4 ";
                    }
                }
                else
                {  // not in the border of an opp's threat
                    if (is_in_our_te)
                    {
                        // inside our enclosure, add lost simulations
                        tn.markAsInsideTerrNoAtari();
                        if (isInBorder(i, who) == 0)
                        {
                            priors += lostSimulations(25);
                            if (is_root) out << "notborder=-25 ";

                            if (sg.getConnects(who - 1)[i].groups_id[0] == 0)
                            {  // does not touch our dots
                                priors += lostSimulations(30);
                                if (is_root) out << "andiso=-30 ";
                            }
                            if (sg.getConnects(2 - who)[i].groups_id[0] == 0)
                            {  // does not touch opp's dots
                                priors += lostSimulations(30);
                                if (is_root) out << " andiso2=-30 ";
                            }
                        }
                        else
                        {
                            for (auto &t : threats[who - 1].threats)
                            {
                                if (t.where == i and t.terr_points == 0)
                                {
                                    // simplifying enclosure: no territory
                                    // anyway
                                    tn.move.enclosures.push_back(t.encl);
                                    tn.move.zobrist_key ^= t.zobrist_key;
                                }
                            }
                        }
                        priors += lostSimulations(20);
                        if (is_root) out << "insidee=-20 ";
                    }
                    else if (isInBorder(i, who) > 0)
                    {
                        // check the value of our threat
                        int value = 0, terr_value = 0;
                        for (auto &t : threats[who - 1].threats)
                        {
                            if (t.where == i)
                            {
                                value += t.opp_dots;
                                terr_value += t.terr_points;
                                if (t.terr_points == 0)
                                {
                                    // simplifying enclosure: no territory
                                    // anyway
                                    tn.move.enclosures.push_back(t.encl);
                                    tn.move.zobrist_key ^= t.zobrist_key;
                                }
                            }
                        }
                        if (value)
                        {
                            int num = 5 + 2 * std::min(value, 15);
                            priors += wonSimulations(num);
                            if (is_root) out << "voft=" << num << " ";
                        }
                        else if (terr_value >= 2)
                        {
                            int num = 2 + std::min(terr_value, 15);
                            priors += wonSimulations(num);
                            if (is_root) out << "voftT=" << num << " ";
                        }
                    }
                }
            }

            // sims
            priors.normaliseTo(max_prior);
            tn.prior = priors;
            tn.t = priors;
            if (is_root)
            {
                out << " --> (" << priors.playouts << ", " << priors.value_sum
                    << ") ";
                debug_info.zobrist2priors_info[tn.move.zobrist_key] = out.str();
                out.str("");
                out.clear();
            }

            // ml_list.push_back(tn);
            if (not tn.isInsideTerrNoAtariOrDame())
                is_nondame_not_in_terr_move = true;
            *alloc.getNext() = tn;
            assert(neutral_opt_encl_moves.size() + 1 ==
                   neutral_encl_zobrists.size());
            for (unsigned opi = 0; opi < neutral_opt_encl_moves.size(); opi++)
            {
                tn.move.enclosures.push_back(neutral_opt_encl_moves[opi]);
                tn.move.zobrist_key ^= neutral_encl_zobrists[opi + 1];
                *alloc.getNext() = tn;
                // ml_list.push_back(tn);
            }
        }
        else
        {
            // special move, canceling opp's threat or allowing for our
            // enclosure
            int em = ml_encl_moves.size();
            int dots = 0, captured_dots = 0;
            if (isInBorder(i, who))
            {
                for (auto &t : threats[who - 1].threats)
                {
                    if (t.where == i) captured_dots += t.opp_dots;
                }
            }
            if (isInBorder(i, 3 - who))
            {
                for (auto &t : threats[2 - who].threats)
                {
                    if (t.where == i)
                    {
                        dots += t.opp_dots;
                        dots += (t.terr_points >= 2);
                    }
                }
            }
            ml_encl_zobrists.erase(ml_encl_zobrists.begin() + 1,
                                   ml_encl_zobrists.end());
            getEnclMoves(ml_encl_moves, ml_opt_encl_moves, ml_encl_zobrists, i,
                         who);
            tn.move.zobrist_key = coord.zobrist_dots[who - 1][i] ^
                                  ml_encl_zobrists[0] ^ ml_encl_zobrists[1];
            tn.move.enclosures.clear();
            tn.move.enclosures.insert(tn.move.enclosures.end(),
                                      ml_encl_moves.begin(),
                                      ml_encl_moves.end());
            // sims
            int saved_dots = 0;
            for (unsigned j = em; j < ml_encl_moves.size(); ++j)
            {
                if (is_root)
                    out << "Enclosure after move " << coord.showPt(i) << ":"
                        << std::endl
                        << ml_encl_moves[j]->show()
                        << "  priority info=" << ml_priority_vect[j - em].show()
                        << std::endl;
                if (ml_priority_vect[j - em].priority_value >
                    ThrInfoConsts::MINF)
                {
                    saved_dots += ml_priority_vect[j - em].saved_dots;
                }
            }
            int num =
                7 + 2 * std::min(dots, 15) + 3 * std::min(saved_dots, 15) +
                std::min(7 * saved_dots * (dots + captured_dots), 30) +
                (captured_dots == 0 ? 0
                                    : (4 * std::min(captured_dots, 15) - 1));
            NonatomicMovestats this_priors = priors;
            this_priors += wonSimulations(num);
            if (is_root)
            {
                out << "special=" << num << " --> (" << priors.playouts << ", "
                    << priors.value_sum << ") ";
                debug_info.zobrist2priors_info[tn.move.zobrist_key] = out.str();
                out.str("");
                out.clear();
            }

            this_priors.normaliseTo(max_prior);
            tn.prior = this_priors;
            tn.t = this_priors;
            if (not tn.isInsideTerrNoAtariOrDame())
                is_nondame_not_in_terr_move = true;
            *alloc.getNext() = tn;
            // ml_list.push_back(tn);
            assert(ml_opt_encl_moves.size() + 2 == ml_encl_zobrists.size());
            assert(ml_priority_vect.size() >=
                   ml_encl_moves.size() - em + ml_opt_encl_moves.size());
            for (unsigned opi = 0; opi < ml_opt_encl_moves.size(); opi++)
            {
                tn.move.enclosures.push_back(ml_opt_encl_moves[opi]);
                tn.move.zobrist_key ^= ml_encl_zobrists[opi + 2];
                NonatomicMovestats this_priors = priors;
                if (ml_priority_vect[ml_encl_moves.size() - em + opi]
                        .priority_value > ThrInfoConsts::MINF)
                {
                    if (is_root)
                    {
                        out << "Enclosure after move " << coord.showPt(i) << ":"
                            << std::endl
                            << ml_opt_encl_moves[opi]->show()
                            << "  priority info="
                            << ml_priority_vect[ml_encl_moves.size() - em + opi]
                                   .show()
                            << std::endl;
                    }
                    dots += ml_priority_vect[ml_encl_moves.size() - em + opi]
                                .saved_dots;
                    num = 5 + 2 * std::min(dots, 15);
                    this_priors += wonSimulations(num);
                    if (is_root)
                    {
                        out << "bonus=" << num << "  --> (" << priors.playouts
                            << ", " << priors.value_sum << ") ";
                    }
                }
                if (is_root)
                {
                    debug_info.zobrist2priors_info[tn.move.zobrist_key] =
                        out.str();
                    out.str("");
                    out.clear();
                }
                // ml_list.push_back(tn);
                this_priors.normaliseTo(max_prior);
                tn.prior = this_priors;
                tn.t = this_priors;
                *alloc.getNext() = tn;
            }
            ml_encl_moves.erase(ml_encl_moves.begin() + em,
                                ml_encl_moves.end());
        }
    }
    if (not is_nondame_not_in_terr_move and dame_already)
    {
        for (Treenode *ch = alloc.getLastBlockWithoutResetting(); ch != nullptr;
             ++ch)
        {
            if (ch->isDame())
            {
                ch->t += wonSimulations(max_prior);
                ch->prior += wonSimulations(max_prior);
                break;
            }
            if (ch->isLast()) break;
        }
    }
    return debug_info;
}

Move Game::getRandomEncl(Move &m)
{
    getSimplifyingEnclAndPriorities(m.who);
    getEnclMoves(ml_encl_moves, ml_opt_encl_moves, ml_encl_zobrists, m.ind,
                 m.who);
    if (ml_opt_encl_moves.size())
    {
        std::uniform_int_distribution<int> di(0, ml_opt_encl_moves.size());
        int number = di(engine);
        m.enclosures.clear();
        m.enclosures.reserve(ml_encl_moves.size() + number);
        m.enclosures.insert(m.enclosures.end(), ml_encl_moves.begin(),
                            ml_encl_moves.end());
        if (number)
        {
            m.enclosures.insert(m.enclosures.end(), ml_opt_encl_moves.begin(),
                                ml_opt_encl_moves.begin() + number);
        }
    }
    else
    {
        m.enclosures = ml_encl_moves;
    }
    return m;
}

/// This function selects enclosures using Game:::chooseRandomEncl().
Move Game::chooseAtariMove(int who, pti forbidden_place)
{
    krb::SmallVector<pti, 16>::allocator_type::arena_type arena_urgent;
    krb::SmallVector<pti, 16> urgent{arena_urgent};  //, non_urgent;
    for (auto &t : threats[who - 1].threats)
    {
        if (t.type & ThreatConsts::ENCL)
        {
            if ((t.border_dots_in_danger and t.opp_dots) || t.singular_dots)
            {
                urgent.push_back(t.where);
                int count =
                    t.border_dots_in_danger ? t.opp_dots : t.singular_dots;
                count += (t.terr_points >= 3);
                while (count >= 3)
                {
                    urgent.push_back(t.where);
                    count -= 3;
                }
            }
        }
    }
    for (auto &t : threats[2 - who].threats)
    {
        if (t.singular_dots and (t.type & ThreatConsts::ENCL) and
            t.where != forbidden_place)
        {
            if (isInEncl(t.where, 3 - who) == 0 and
                threats[2 - who].is_in_2m_miai[t.where] == 0 and
                threats[2 - who].is_in_2m_miai[t.where] == 0)
            {
                urgent.push_back(t.where);
                int count = t.singular_dots + (t.terr_points >= 3);
                while (count >= 3)
                {
                    urgent.push_back(t.where);
                    count -= 4;
                }
            }
        }
        // if (t.border_in_danger) {
        //   ... find our moves destroying threat t --> this is not necessary,
        // because such a move will be found above, unless the whole t lies in
        // some our threat, in which case no our reaction is needed
    }
    // add also some threats-in-2-moves (double atari, etc.)   (v80+)
    for (auto &t : threats[who - 1].threats2m)
    {
        if (t.min_win2 and t.isSafe() and t.where0 != forbidden_place)
        {
            urgent.push_back(t.where0);
            int count = t.min_win2;
            while (count >= 3)
            {
                urgent.push_back(t.where0);
                count -= 4;
            }
        }
    }
    for (auto &t : threats[2 - who].threats2m)
    {
        if (t.min_win2 and t.where0 != forbidden_place and t.isSafe() and
            isInTerr(t.where0, 3 - who) == 0 and
            isInEncl(t.where0, 3 - who) == 0)
        {
            urgent.push_back(t.where0);
            int count = t.min_win2;
            while (count >= 3)
            {
                urgent.push_back(t.where0);
                count -= 4;
            }
        }
    }
    // select randomly one of the moves
    int total =
        urgent.size();  //*COEFF_URGENT + non_urgent.size()*COEFF_NONURGENT;
    Move m;
    m.who = who;
    // for debug, save all choices
#ifdef DEBUG_SGF
    /*
    for (pti p : urgent) {
      m.ind = p;
      sgf_tree.addChild({getRandomEncl(m).toSgfString(), {"C",
    {"chooseAtariMove:urgent,total==" + std::to_string(total)}}});
    }
    */
#endif
    //
    if (total)
    {
        std::uniform_int_distribution<int> di(0, total - 1);
        int number = di(engine);
        m.ind = urgent[number];
        /*
        if (number < urgent.size()*COEFF_URGENT) {
          number /= COEFF_URGENT;
          m.ind = urgent[number];
        } else {
          number -= urgent.size()*COEFF_URGENT;
          number /= COEFF_NONURGENT;
          m.ind = non_urgent[number];
        }
        */
    }
    else
    {
        m.ind = 0;
        return m;
    }
    return getRandomEncl(m);
}

/// This function selects enclosures using Game:::chooseRandomEncl().
Move Game::chooseAtariResponse(pti lastMove, int who, pti forbidden_place)
{
    krb::SmallVector<pti, 16>::allocator_type::arena_type arena_urgent;
    krb::SmallVector<pti, 16> urgent{arena_urgent};
    for (auto &t : threats[2 - who].threats)
    {
        if (t.singular_dots and (t.type & ThreatConsts::ENCL) and
            t.where != forbidden_place and t.encl->isInBorder(lastMove))
        {
            if (isInEncl(t.where, 3 - who) == 0 and
                threats[2 - who].is_in_2m_miai[t.where] == 0 and
                threats[2 - who].is_in_2m_miai[t.where] == 0)
            {
                urgent.push_back(t.where);
                int count = t.singular_dots + (t.terr_points >= 3);
                while (count >= 3)
                {
                    urgent.push_back(t.where);
                    count -= 4;
                }
            }
            if (t.border_dots_in_danger)
            {
                for (auto &ourt : threats[who - 1].threats)
                {
                    if ((ourt.type & ThreatConsts::ENCL) and ourt.opp_dots and
                        (std::find(t.opp_thr.begin(), t.opp_thr.end(),
                                   ourt.zobrist_key) != t.opp_thr.end()))
                    {
                        urgent.push_back(ourt.where);
                        urgent.push_back(ourt.where);
                        int count = t.singular_dots + (t.terr_points >= 3);
                        while (count >= 2)
                        {
                            urgent.push_back(ourt.where);
                            count -= 3;
                        }
                    }
                }
            }
        }
    }
    // check opp threats in 2 moves
    for (auto &t : threats[2 - who].threats2m)
    {
        if (t.min_win2 and t.isSafe() and t.where0 != forbidden_place and
            isInTerr(t.where0, 3 - who) == 0 and
            isInEncl(t.where0, 3 - who) == 0 and
            isInTerr(t.where0, who) == 0 and isInEncl(t.where0, who) == 0 and
            (coord.distBetweenPts_infty(t.where0, lastMove) <= 1 or
             std::any_of(t.thr_list.begin(), t.thr_list.end(),
                         [lastMove](const Threat &thr)
                         { return thr.encl->isInBorder(lastMove); })))
        {
            urgent.push_back(t.where0);
            int count = t.min_win2;
            while (count >= 3)
            {
                urgent.push_back(t.where0);
                count -= 4;
            }
        }
    }
    // select randomly one of the moves
    int total = urgent.size();
    Move m;
    m.who = who;
    // for debug, save all choices
#ifdef DEBUG_SGF
    /*
    for (pti p : urgent) {
      m.ind = p;
      sgf_tree.addChild({getRandomEncl(m).toSgfString(), {"C",
    {"chooseAtariMove:urgent,total==" + std::to_string(total)}}});
    }
    */
#endif
    //
    if (total)
    {
        std::uniform_int_distribution<int> di(0, total - 1);
        int number = di(engine);
        m.ind = urgent[number];
    }
    else
    {
        m.ind = 0;
        return m;
    }
    return getRandomEncl(m);
}

/// This function selects enclosures using Game:::chooseRandomEncl().
Move Game::chooseSoftSafetyResponse(int who, pti forbidden_place)
{
    auto responses = sg.safety_soft.getCurrentlyAddedSugg();
    return selectMoveRandomlyFrom(responses[who - 1], who, forbidden_place);
}

/// This function selects enclosures using Game:::chooseRandomEncl().
Move Game::chooseSoftSafetyContinuation(int who, pti forbidden_place)
{
    auto responses = sg.safety_soft.getPreviouslyAddedSugg();
    return selectMoveRandomlyFrom(responses[who - 1], who, forbidden_place);
}

Move Game::selectMoveRandomlyFrom(const std::vector<pti> &moves, int who,
                                  pti forbidden_place)
{
    const int total = moves.size();
    Move m;
    m.who = who;
    if (total == 0)
    {
        m.ind = 0;
        return m;
    }
    std::vector<pti> moves_not_in_atari;
    moves_not_in_atari.reserve(total);
    for (auto move : moves)
    {
        if (move == forbidden_place) continue;
        if (sg.safety_soft.isDameFor(who, move)) continue;
        if ((isInEncl(move, 3 - who) == 0 and isInTerr(move, 3 - who) == 0) or
            isInBorder(move, who))
            moves_not_in_atari.push_back(move);
    }
    const int total_not_in_atari = moves_not_in_atari.size();
    if (total_not_in_atari == 0)
    {
        m.ind = 0;
        return m;
    }
    std::uniform_int_distribution<int> di(0, total_not_in_atari - 1);
    int number = di(engine);
    m.ind = moves_not_in_atari[number];
    return getRandomEncl(m);
}

/// Note: if move0 and move1 have common neighbours, then they have
///  the probability of being chosen doubled.
Move Game::choosePattern3Move(pti move0, pti move1, int who,
                              pti forbidden_place)
{
    Move move;
    move.who = who;
    typedef std::pair<pti, pattern3_val> MoveValue;
    krb::SmallVector<MoveValue, 24>::allocator_type::arena_type arena_stack;
    krb::SmallVector<MoveValue, 24> stack{arena_stack};
    int total = 0;
    for (pti m : {move0, move1})
    {
        if (m)
        {
            for (int i = 0; i < 8; i++)
            {
                pti nb = m + coord.nb8[i];
                auto v = pattern3_value[who - 1][nb];
                assert(v == getPattern3Value(nb, who));
                if (v > 0 and nb != forbidden_place and
                    ((isInEncl(nb, 3 - who) == 0 and
                      isInTerr(nb, 3 - who) == 0) ||
                     isInBorder(nb, who)))
                {
                    stack.push_back(std::make_pair(nb, v));
                    total += v;
                }
            }
        }
    }
    if (total <= 20)
    {
        for (pti m : {move0, move1})
        {
            if (m and coord.dist[m] >= 1)
            {
                for (int i = 0; i < 4; i++)
                {
                    if (isEmptyInDirection(m, i))
                    {  // in particular,  whoseDotMarginAt(m + 2*coord.nb4[i])
                       // == 0
                        pti nb = m + 2 * coord.nb4[i];
                        assert(coord.dist[nb] >= 0);
                        auto v = pattern3_value[who - 1][nb];
                        assert(v == getPattern3Value(nb, who));

                        if (v >= 0 and nb != forbidden_place and
                            // here non-dame (>=0) is enough
                            ((isInEncl(nb, 3 - who) == 0 and
                              isInTerr(nb, 3 - who) == 0) ||
                             isInBorder(nb, who)))
                        {
                            //	      v = std::min(80.0f, v+30);  // v will be
                            // in [50, 80]     // TODO! change type of
                            // pattern3_value to integer!
                            if (isEmptyInNeighbourhood(nb))
                            {
                                v = 20;
                            }
                            else
                            {
                                v = 10 + v / 4;
                            }
                            stack.push_back(std::make_pair(nb, v));
                            total += v;
                        }
                    }
                }
            }
        }
    }
    if (total)
    {
        // for debug, save all choices
#ifdef DEBUG_SGF
        /*
        for (auto p : stack) {
          move.ind = p.first;
          sgf_tree.addChild({getRandomEncl(move).toSgfString(), {"C", {"p3,t=="
        + std::to_string(int(100*total+0.01)) + ", v=" +
        std::to_string(int(100*p.second+0.01))}}});
        }
        */
#endif
        std::uniform_int_distribution<int> di(0, total - 1);
        int number = di(engine);
        for (auto p : stack)
        {
            number -= p.second;
            if (number < 0)
            {
                move.ind = p.first;
                return getRandomEncl(move);  // TODO: do we have to set zobrist?
            }
        }
        move.ind = stack[0].first;
        return getRandomEncl(move);  // TODO: do we have to set zobrist?
    }
    move.ind = 0;
    return move;
}

std::vector<pti> Game::getSafetyMoves(int /*who*/, pti forbidden_place)
{
    std::vector<pti> stack;
    std::set<pti> already_saved;
    using Tup = std::tuple<int, pti, pti>;
    const std::array<Tup, 4> p_vnorm_vside{
        Tup{coord.ind(1, 1), coord.N, coord.E},
        Tup{coord.ind(1, 1), coord.W, coord.S},
        Tup{coord.ind(coord.wlkx - 2, 1), coord.E, coord.S},
        Tup{coord.ind(1, coord.wlky - 2), coord.S, coord.E}};
    // top margin
    for (auto [p, vnorm, vside] : p_vnorm_vside)
    {
        for (; coord.dist[p] == 1; p += vside)
        {
            if (whoseDotMarginAt(p + vnorm) == 0 and
                whoseDotMarginAt(p - vnorm) + whoseDotMarginAt(p) == 3)
            {
                auto whose = whoseDotMarginAt(p);
                if (whoseDotMarginAt(p - vside) + whose == 3 and
                    whoseDotMarginAt(p + vside) == 0 and
                    sg.descr.at(sg.worm[p - vside]).safety <= 1 and
                    whoseDotMarginAt(p + vside + vnorm) != whose and
                    already_saved.find(p + vside) == already_saved.end())
                //    .
                //  . x O    with . of safety <= 1
                //    o ?    ? = .o but not 'x'    --> then mark O as
                //    interesting (anti-global)
                {
                    if (p + vside != forbidden_place)
                    {
                        stack.push_back(p + vside);
                        already_saved.insert(p + vside);
                    }
                }
                else if (whoseDotMarginAt(p + vside) + whose == 3 and
                         whoseDotMarginAt(p - vside) == 0 and
                         sg.descr.at(sg.worm[p + vside]).safety <= 1 and
                         whoseDotMarginAt(p - vside + vnorm) != whose and
                         already_saved.find(p - vside) == already_saved.end())
                //    .
                //  O x .    with . of safety <= 1
                //  ? o      ? = .o but not 'x'    --> then mark O as
                //  interesting (anti-global)
                {
                    if (p - vside != forbidden_place)
                    {
                        stack.push_back(p - vside);
                        already_saved.insert(p - vside);
                    }
                }
            }

            if (whoseDotMarginAt(p) != 0 and
                sg.descr.at(sg.worm[p]).safety == 1 and
                sg.safety_soft.getSafetyOf(p) <= 0.99f)
            {
                if (whoseDotMarginAt(p + vnorm) == 0)
                {
                    if (p + vnorm != forbidden_place)
                    {
                        stack.push_back(p + vnorm);
                        continue;
                    }
                }
                if (whoseDotMarginAt(p + vside) == 0 and
                    coord.dist[p + vside] == 1 and
                    whoseDotMarginAt(p + vside + vnorm) !=
                        whoseDotMarginAt(p) and
                    already_saved.find(p + vside + vnorm) ==
                        already_saved.end())
                {
                    // x ?
                    // o .   ? = .o but not 'x'
                    if (p + vside != forbidden_place)
                    {
                        stack.push_back(p + vside);
                        already_saved.insert(p + vside);
                    }
                }
                if (whoseDotMarginAt(p - vside) == 0 and
                    coord.dist[p - vside] == 1 and
                    whoseDotMarginAt(p - vside + vnorm) !=
                        whoseDotMarginAt(p) and
                    already_saved.find(p - vside + vnorm) ==
                        already_saved.end())
                {
                    // x ?
                    // o .   ? = .o but not 'x'
                    if (p - vside != forbidden_place)
                    {
                        stack.push_back(p - vside);
                        already_saved.insert(p - vside);
                    }
                }
            }
        }
    }
    return stack;
}

Move Game::chooseSafetyMove(int who, pti forbidden_place)
{
    Move move;
    move.who = who;
    const auto stack = getSafetyMoves(who, forbidden_place);
    if (!stack.empty())
    {
        std::uniform_int_distribution<int> di(0, stack.size() - 1);
        int number = di(engine);
        move.ind = stack[number];
        return getRandomEncl(move);  // TODO: do we have to set zobrist?
    }
    move.ind = 0;
    return move;
}

Move Game::chooseAnyMove(int who, pti forbidden_place)
{
    Move move;
    move.who = who;
    std::vector<pti> stack;
    std::vector<pti> bad_moves;
    for (pti i = coord.first; i <= coord.last; i++)
    {
        if (whoseDotMarginAt(i) == 0)
        {
            if (((sg.getConnects(who - 1)[i].groups_id[0] == 0) and
                 (isInTerr(i, who) > 0 || isInTerr(i, 3 - who) > 0)) or
                i == forbidden_place)
            {
                bad_moves.push_back(i);
            }
            else
            {
                stack.push_back(i);
            }
        }
    }
    if (!stack.empty())
    {
        std::uniform_int_distribution<int> di(0, stack.size() - 1);
        int number = di(engine);
        move.ind = stack[number];
        return getRandomEncl(move);  // TODO: do we have to set zobrist?
    }
    else if (!bad_moves.empty())
    {
        std::uniform_int_distribution<int> di(0, bad_moves.size() - 1);
        int number = di(engine);
        move.ind = bad_moves[number];
        return getRandomEncl(move);  // TODO: do we have to set zobrist?
    }
    move.ind = 0;
    return move;
}

/// Finds possibly good moves for who among TERRMoves.
std::vector<pti> Game::getGoodTerrMoves(int who) const
{
    std::vector<pti> good_moves;
    for (unsigned i = 0;
         i < possible_moves.lists[PossibleMovesConsts::LIST_TERRM].size(); ++i)
    {
        auto p = possible_moves.lists[PossibleMovesConsts::LIST_TERRM][i];
        if ((sg.getConnects(who - 1)[p].groups_id[0] == 0 and
             (isInTerr(p, 3 - who) > 0 || isInEncl(p, 3 - who) > 0)) ||
            (sg.getConnects(2 - who)[p].groups_id[0] == 0 and
             (isInTerr(p, who) > 0 || isInEncl(p, who) > 0)))
        {
            continue;
        }
        // check if it closes something
        if (isInBorder(p, who) || isInBorder(p, 3 - who))
        {
            good_moves.push_back(p);
            continue;
        }
        // check if it can be closed fast
        int count_opp = 0, count_our = 0;
        for (auto j : coord.nb4)
        {
            count_opp += (whoseDotMarginAt(p + j) == 3 - who);
            count_our += (whoseDotMarginAt(p + j) == who);
        }
        if (count_opp >= 3 and count_our == 0)
        {
            continue;
        }
        if (count_our >= 3 and count_opp == 0)
        {
            // check whether the point may be dangerous for us
            assert(coord.nb8[0] == coord.NE);  // could be any corner
            int count_danger = 0;
            for (int j = 0; j < 8; j += 2)
            {
                count_danger += (whoseDotMarginAt(p + coord.nb8[j]) != who);
            }
            if (count_danger <= 1)
            {
                continue;  // opp will not connect through p
            }
        }
        // here we could still check whether the move makes sense, for example,
        // if there's any chance of connecting something, but for now, we assume
        // p is a good move
        good_moves.push_back(p);
    }
    return good_moves;
}

/// Chooses any move using possible_moves.
Move Game::chooseAnyMove_pm(int who, pti /*forbidden_place*/)
{
    Move move;
    move.who = who;
    assert(checkPossibleMovesCorrectness());
    if (!possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].empty())
    {
        unsigned number;
        if (!possible_moves.lists[PossibleMovesConsts::LIST_TERRM].empty())
        {
            // if there are some TERRM moves, maybe we should consider one of
            // them; throw a dice to decide
            std::uniform_int_distribution<unsigned> di(
                0,
                possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].size() +
                    std::min(int(possible_moves
                                     .lists[PossibleMovesConsts::LIST_TERRM]
                                     .size()),
                             2) -
                    1);
            number = di(engine);
        }
        else
        {
            std::uniform_int_distribution<unsigned> di(
                0,
                possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].size() -
                    1);
            number = di(engine);
        }
        if (number <
            possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].size())
        {
            move.ind =
                possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL][number];
            dame_moves_so_far = 0;
            return getRandomEncl(move);  // TODO: do we have to set zobrist?
        }
    }

    // try early exit -- maybe one of neutral moves?
    if (possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].size())
    {
        const int guessed_g = 3;
        const int n_or_guessed_g =
            possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].size() +
            guessed_g;
        std::uniform_int_distribution<unsigned> di(0, n_or_guessed_g - 1);
        unsigned number = di(engine);
        if (number <
            possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].size())
        {
            move.ind =
                possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL][number];
            dame_moves_so_far = 0;
            return getRandomEncl(move);  // TODO: do we have to set zobrist?
        }
    }

    // check TERRM moves
    std::vector<pti> good_moves = getGoodTerrMoves(who);
    const auto n_good_moves = good_moves.size();
    if (n_good_moves > 0)
    {
        std::uniform_int_distribution<unsigned> di(0, n_good_moves - 1);
        unsigned number = di(engine);
        move.ind = good_moves[number];
        dame_moves_so_far = 0;
        return getRandomEncl(move);  // TODO: do we have to set zobrist?
    }
    // try dame
    if (possible_moves.lists[PossibleMovesConsts::LIST_DAME].size() > 0)
    {
        std::uniform_int_distribution<unsigned> di(
            0, possible_moves.lists[PossibleMovesConsts::LIST_DAME].size() - 1);
        unsigned number = di(engine);
        move.ind = possible_moves.lists[PossibleMovesConsts::LIST_DAME][number];
        dame_moves_so_far++;
        return getRandomEncl(move);  // TODO: do we have to set zobrist?
    }
    // maybe there's some bad move left
    if (possible_moves.lists[PossibleMovesConsts::LIST_TERRM].size() > 0)
    {  // note: here possible_moves.lists[LIST_TERRM] == list of bad_moves
        std::uniform_int_distribution<int> di(
            0,
            possible_moves.lists[PossibleMovesConsts::LIST_TERRM].size() - 1);
        int number = di(engine);
        move.ind =
            possible_moves.lists[PossibleMovesConsts::LIST_TERRM][number];
        dame_moves_so_far++;  // not really dame, but we just want to stop the
                              // simulation
        return getRandomEncl(move);  // TODO: do we have to set zobrist?
    }
    // no legal move left...
    move.ind = 0;
    return move;
}

/// Chooses interesting_move.
Move Game::chooseInterestingMove(int who, pti forbidden_place)
{
    Move move;
    move.who = who;
    int which_list = InterestingMovesConsts::LIST_0;
    assert(
        InterestingMovesConsts::LIST_0 + 1 == InterestingMovesConsts::LIST_1 and
        InterestingMovesConsts::LIST_1 + 1 == InterestingMovesConsts::LIST_2 and
        InterestingMovesConsts::LIST_2 + 1 ==
            InterestingMovesConsts::LIST_REMOVED);
    while (interesting_moves.lists[which_list].empty())
    {
        which_list++;
        if (which_list == InterestingMovesConsts::LIST_REMOVED)
        {
            move.ind = 0;
            return move;
        }
    }
    return selectMoveRandomlyFrom(interesting_moves.lists[which_list], who,
                                  forbidden_place);
}

Move Game::chooseLastGoodReply(int who, pti forbidden_place)
{
    Move move;
    move.who = who;
    move.ind = sg.getHistory().getLastGoodReplyFor(who);
    if (whoseDotMarginAt(move.ind) != 0 or move.ind == forbidden_place)
    {
        move.ind = 0;
        return move;
    }
    if (move.ind == 0) return move;
    return getRandomEncl(move);
}

Move Game::getLastMove() const
{
    // we do not get last enclosure, but this does not matter
    Move m;
    m.ind = sg.getHistory().getLast();
    m.who = (sg.nowMoves ^ 3);
    return m;
}

Move Game::getLastButOneMove() const
{
    // we do not get last enclosure, but this does not matter
    Move m;
    m.ind = sg.getHistory().getLastButOne();
    m.who = sg.nowMoves;
    return m;
}

std::pair<pti, pti> Game::checkLadderToFindBadOrGoodMoves() const
// Assumes that the last move played was atari.
// returns [forbidden_place, forced_move]
// if forced_move == 0, forbidden_place concerns current player (working
// ladder), otherwise, current player should play forced_move and then the other
// not at forbidden
{
    int opponent = 3 - sg.nowMoves;
    uint16_t last_move_no = sg.getHistory().size();
    const int min_value_threshold = 3;
    for (auto thr_it = threats[opponent - 1].threats.rbegin();
         thr_it != threats[opponent - 1].threats.rend(); ++thr_it)
    {
        if (thr_it->hist_size != last_move_no) return {0, 0};
        if (thr_it->type & ThreatConsts::TERR) continue;
        if (thr_it->where != 0 and
            coord.distBetweenPts_infty(thr_it->where,
                                       sg.getHistory().getLast()) == 1 and
            thr_it->opp_dots >= min_value_threshold)
        {
            const auto [status, next_att, next_def] =
                checkLadder(sg.nowMoves, thr_it->where);
            const int ESC_WINS = -1, ATT_WINS = 1;
            if (status == ESC_WINS)
            {
                return {next_att, thr_it->where};
            }
            if (status == ATT_WINS)
            {
                return {thr_it->where, 0};
            }
        }
    }
    return {0, 0};
}

real_t Game::randomPlayout()
{
    Move m;
    std::uniform_int_distribution<uint32_t> di(0, 0xffffff);
    constexpr int threats2m_threshold = 30;
    pti forbidden_place{};  // further attack in non-working ladder, or defense
                            // in a working ladder
    for (int move_number = 0;; ++move_number)
    {
        if (move_number > threats2m_threshold)
        {
            threats[0].turnOffThreats2m();
            threats[1].turnOffThreats2m();
        }
        if (forbidden_place == 0)
        {
            // check ladder to find new forbidden_place or forced_move
            const auto [forbidden, forced] = checkLadderToFindBadOrGoodMoves();
            forbidden_place = forbidden;
            if (forced and forbidden)
            {
                // prev player played non-working ladder
                Move forced_move;
                forced_move.ind = forced;
                forced_move.who = sg.nowMoves;
                dame_moves_so_far = 0;
                makeMove(getRandomEncl(forced_move));
                /*
                std::cout << "---- Non-working ladder played at " <<
                coord.showPt(sg.getHistory().getLast()) <<
                  ", forced move: " << coord.showPt(forced) << ", forbidden: "
                <<  coord.showPt(forbidden) << '\n'; show();
                */
                continue;
            }
            if (forbidden == 0)
            {
                // maybe prev player tried to defend working ladder?
                const auto [status, next_att, next_def] =
                    checkLadder(3 - sg.nowMoves, sg.getHistory().getLast());
                const int ESC_WINS = -1, ATT_WINS = 1;
                if (status == ATT_WINS)
                {
                    Move forced_move;
                    forced_move.ind = next_att;
                    forced_move.who = sg.nowMoves;
                    dame_moves_so_far = 0;
                    makeMove(getRandomEncl(forced_move));
                    if (not isInEncl(next_def, 3 - sg.nowMoves))
                        forbidden_place = next_def;
                    std::cout
                        << "-*-*-* Working ladder defended at "
                        << coord.showPt(sg.getHistory().getLast())
                        << ", forced move: " << coord.showPt(forced_move.ind)
                        << ", forbidden: " << coord.showPt(forbidden_place)
                        << '\n';
                    show();
                    continue;
                }
                if (status == ESC_WINS and not isInEncl(next_att, sg.nowMoves))
                {
                    forbidden_place = next_att;
                    std::cout
                        << "-*-*-*%^%^ Non-working ladder defended at "
                        << coord.showPt(sg.getHistory().getLast())
                        << ", forbidden: " << coord.showPt(forbidden_place)
                        << '\n';
                    show();
                }
            }
        }
        int number = di(engine);
        if ((number & 0x10000) != 0)  // probability 1/2
        {
            m = chooseLastGoodReply(sg.nowMoves, forbidden_place);
            if (m.ind != 0)
            {
                dame_moves_so_far = 0;
                makeMove(m);
                forbidden_place = 0;
                continue;
            }
        }
        if ((number & 0xc00) != 0)
        {
            m = chooseAtariResponse(sg.getHistory().getLast(), sg.nowMoves,
                                    forbidden_place);
            if (m.ind != 0)
            {
                dame_moves_so_far = 0;
                makeMove(m);
                forbidden_place = 0;
#ifdef DEBUG_SGF
                sgf_tree.addComment("ar");
#endif
                continue;
            }
        }
        if ((number & 0xc000) != 0)
        {
            m = chooseSoftSafetyResponse(sg.nowMoves, forbidden_place);
            if (m.ind != 0)
            {
                dame_moves_so_far = 0;
                makeMove(m);
                forbidden_place = 0;
#ifdef DEBUG_SGF
                sgf_tree.addComment("soft");
#endif
                continue;
            }
        }

        if ((number & 0x300) != 0)
        {
            m = choosePattern3Move(sg.getHistory().getLast(), 0, sg.nowMoves,
                                   forbidden_place);
            if (m.ind != 0)
            {
                dame_moves_so_far = 0;
#ifdef DEBUG_SGF
                auto v = pattern3_value[m.who - 1][m.ind];
#endif
                makeMove(m);
                forbidden_place = 0;
#ifdef DEBUG_SGF
                sgf_tree.addComment(std::string("pa:") + std::to_string(v));
#endif
                // std::cerr << "p";
                continue;
            }
        }

        if ((number & 0x2000) != 0)
        {
            m = chooseSoftSafetyContinuation(sg.nowMoves, forbidden_place);
            if (m.ind != 0)
            {
                dame_moves_so_far = 0;
                makeMove(m);
                forbidden_place = 0;
#ifdef DEBUG_SGF
                sgf_tree.addComment("sofc");
#endif
                continue;
            }
        }

        if ((number & 0x4) != 0)
        {
            m = choosePattern3Move(0, sg.getHistory().getLastButOne(),
                                   sg.nowMoves, forbidden_place);
            if (m.ind != 0)
            {
                dame_moves_so_far = 0;
#ifdef DEBUG_SGF
                auto v = pattern3_value[m.who - 1][m.ind];
#endif
                makeMove(m);
                forbidden_place = 0;
#ifdef DEBUG_SGF
                sgf_tree.addComment(std::string("pe:") + std::to_string(v));
#endif
                // std::cerr << "p";
                continue;
            }
        }
        if ((number & 0x2) != 0)
        {
            m = chooseAtariMove(sg.nowMoves, forbidden_place);
            if (m.ind != 0)
            {
                dame_moves_so_far = 0;
                makeMove(m);
                forbidden_place = 0;
#ifdef DEBUG_SGF
                sgf_tree.addComment("at");
#endif
                // std::cerr << "A";
                continue;
            }
        }
        if ((number & 0x80) != 0)
        {  // probability 1/2, could be 3/4 by changing 0x80 to 0xc0
            m = chooseInterestingMove(sg.nowMoves, forbidden_place);
            if (m.ind != 0)
            {
#ifdef DEBUG_SGF
                int which_list = InterestingMovesConsts::LIST_0;
                while (interesting_moves.lists[which_list].empty())
                {
                    which_list++;
                    if (which_list == InterestingMovesConsts::LIST_REMOVED)
                    {
                        break;
                    }
                }
                SgfProperty prop;
                if (which_list != InterestingMovesConsts::LIST_REMOVED)
                {
                    prop.first = "SQ";
                    for (auto m : interesting_moves.lists[which_list])
                        prop.second.push_back(coord.indToSgf(m));
                }
#endif
                dame_moves_so_far = 0;
                makeMove(m);
                forbidden_place = 0;
#ifdef DEBUG_SGF
                sgf_tree.addComment(std::string("cut"));
                if (!prop.second.empty())
                {
                    sgf_tree.addProperty(prop);
                }
#endif
                continue;
            }
        }
        if ((number & 0x1) != 0)
        {  // probability 1/2
            m = chooseSafetyMove(sg.nowMoves, forbidden_place);
            if (m.ind != 0)
            {
                dame_moves_so_far = 0;
                makeMove(m);
                forbidden_place = 0;
#ifdef DEBUG_SGF
                sgf_tree.addComment(std::string("saf"));
#endif
                continue;
            }
        }
        /*
        if ((number & 0xc) != 0) {
          m = chooseInfluenceMove(sg.nowMoves, forbidden_place);
          if (m.ind != 0) {
            dame_moves_so_far = 0;
            makeMove(m);
            forbidden_place = 0;
    #ifdef DEBUG_SGF
            sgf_tree.addComment(std::string("infl"));
    #endif
            continue;
          }
        }
        */
        m = chooseAnyMove_pm(sg.nowMoves, forbidden_place);
        if (m.ind != 0)
        {
            makeMove(m);
            forbidden_place = 0;
#ifdef DEBUG_SGF
            sgf_tree.addComment(std::string(":") +
                                std::to_string(dame_moves_so_far));
#endif
            // std::cerr << ".";
        }
        else
        {
            break;
        }
        if (dame_moves_so_far >= 2) break;
    }
    // std::cerr << std::endl;
    auto [res, res_small] = countTerritory_simple(sg.nowMoves);
    real_t win_value;
    if (res == 0)
    {
        const real_t scale = 0.04;
        win_value = 0.5 + scale * res_small;
    }
    else
    {
        int range = (coord.wlkx + coord.wlky) / 2;
        const real_t scale = 0.04;
        real_t scaled_score =
            scale * std::max(std::min(real_t(res + 0.5 * res_small) / range,
                                      real_t(1.0)),
                             real_t(-1.0));
        win_value = (res > 0) * (1 - 2 * scale) + scale + scaled_score;
    }
#ifdef DEBUG_SGF
    sgf_tree.addComment(std::string("res=") + std::to_string(res) +
                        std::string(" v=") + std::to_string(win_value));
    sgf_tree.finishPartialMove();
#endif
    return win_value;
}

/// This function checks if p is an interesting move, because of a change of its
/// pattern3_value or atari.
int Game::checkInterestingMove(pti p) const
{
    if (threats[0].is_in_encl[p] == 0 and threats[0].is_in_terr[p] == 0 and
        threats[1].is_in_encl[p] == 0 and threats[1].is_in_terr[p] == 0)
    {
        auto v = global::patt3_symm.getValue(pattern3_at[p], 1);
        assert(v >= 0 and v < 8);
        int lists[8] = {
            InterestingMovesConsts::REMOVED,
            InterestingMovesConsts::REMOVED,
            InterestingMovesConsts::MOVE_2,
            InterestingMovesConsts::MOVE_2,
            InterestingMovesConsts::MOVE_2,
            InterestingMovesConsts::MOVE_1,
            InterestingMovesConsts::MOVE_0,
            InterestingMovesConsts::MOVE_0};  // best moves goes to LIST_0
        return lists[v];
    }
    return InterestingMovesConsts::REMOVED;
}

/// This function checks if p is still dame or now dame, because of its
/// pattern3_value change.
int Game::checkDame(pti p) const
{
    if (threats[0].is_in_encl[p] == 0 and threats[0].is_in_terr[p] == 0 and
        threats[1].is_in_encl[p] == 0 and threats[1].is_in_terr[p] == 0)
    {
        if (pattern3_value[0][p] < 0) return PossibleMovesConsts::DAME;
        if (coord.dist[p] == 0)
        {
            int x = coord.x[p], y = coord.y[p];
            if ((x == 0 || x == coord.wlkx - 1) and
                (y == 0 || y == coord.wlky - 1))
                return PossibleMovesConsts::DAME;  // corner
            if ((x == 0 and possible_moves.left) ||
                (x == coord.wlkx - 1 and possible_moves.right) ||
                (y == 0 and possible_moves.top) ||
                (y == coord.wlky - 1 and possible_moves.bottom))
                return PossibleMovesConsts::DAME;
            for (int i = 0; i < 4; i++)
            {
                pti nb = p + coord.nb4[i];
                if (coord.dist[nb] == 1)
                {
                    if (sg.worm[nb] and sg.descr.at(sg.worm[nb]).isSafe())
                        return PossibleMovesConsts::DAME;
                    break;
                }
            }
        }
        return PossibleMovesConsts::NEUTRAL;
    }
    else
    {
        return PossibleMovesConsts::TERRM;
    }
}

/// This function checks if neighbour(s) of p is/are still dame or now dame,
/// because of the safety change of p.
void Game::possibleMoves_updateSafety(pti p)
{
    assert(coord.dist[p] == 1);
    if (sg.worm[p] != 0)
    {
        for (int i = 0; i < 4; i++)
        {
            pti nb = p + coord.nb4[i];
            if (coord.dist[nb] == 0 and whoseDotMarginAt(nb) == 0)
            {
                possible_moves.changeMove(nb,
                                          sg.descr.at(sg.worm[p]).isSafe()
                                              ? PossibleMovesConsts::DAME
                                              : PossibleMovesConsts::NEUTRAL);
            }
        }
    }
}

void Game::possibleMoves_updateSafetyDame()
{
    if (!possible_moves.left)
    {
        pti p = coord.ind(0, 1);
        for (int y = coord.wlky - 2; y > 0; --y)
        {
            if (sg.worm[p + coord.E] != 0 and whoseDotMarginAt(p) == 0)
            {
                possible_moves.changeMove(
                    p, sg.descr.at(sg.worm[p + coord.E]).isSafe()
                           ? PossibleMovesConsts::DAME
                           : PossibleMovesConsts::NEUTRAL);
            }
            p += coord.S;
        }
    }
    if (!possible_moves.right)
    {
        pti p = coord.ind(coord.wlkx - 1, 1);
        for (int y = coord.wlky - 2; y > 0; --y)
        {
            if (sg.worm[p + coord.W] != 0 and whoseDotMarginAt(p) == 0)
            {
                possible_moves.changeMove(
                    p, sg.descr.at(sg.worm[p + coord.W]).isSafe()
                           ? PossibleMovesConsts::DAME
                           : PossibleMovesConsts::NEUTRAL);
            }
            p += coord.S;
        }
    }
    if (!possible_moves.top)
    {
        pti p = coord.ind(1, 0);
        for (int x = coord.wlkx - 2; x > 0; --x)
        {
            if (sg.worm[p + coord.S] != 0 and whoseDotMarginAt(p) == 0)
            {
                possible_moves.changeMove(
                    p, sg.descr.at(sg.worm[p + coord.S]).isSafe()
                           ? PossibleMovesConsts::DAME
                           : PossibleMovesConsts::NEUTRAL);
            }
            p += coord.E;
        }
    }
    if (!possible_moves.bottom)
    {
        pti p = coord.ind(1, coord.wlky - 1);
        for (int x = coord.wlkx - 2; x > 0; --x)
        {
            if (sg.worm[p + coord.N] != 0 and whoseDotMarginAt(p) == 0)
            {
                possible_moves.changeMove(
                    p, sg.descr.at(sg.worm[p + coord.N]).isSafe()
                           ? PossibleMovesConsts::DAME
                           : PossibleMovesConsts::NEUTRAL);
            }
            p += coord.E;
        }
    }
}

/// This function is for testing.
bool Game::isDame_directCheck(pti p, int who) const
{
    if (coord.dist[p] == 0 and checkBorderMove(p, who) < 0) return true;
    if (sg.safety_soft.isDameFor(who, p)) return true;
    return isDame_directCheck_symm(p);
}

/// This function is for testing.
/// This is 'symmetric' version (i.e. good for both players), NOT using
/// checkBorderMove().
bool Game::isDame_directCheck_symm(pti p) const
{
    if (coord.dist[p] == 0)
    {
        int x = coord.x[p], y = coord.y[p];
        if ((x == 0 || x == coord.wlkx - 1) and (y == 0 || y == coord.wlky - 1))
            return true;  // corner
        if ((x == 0 and possible_moves.left) ||
            (x == coord.wlkx - 1 and possible_moves.right) ||
            (y == 0 and possible_moves.top) ||
            (y == coord.wlky - 1 and possible_moves.bottom))
            return true;
        for (int i = 0; i < 4; i++)
        {
            pti nb = p + coord.nb4[i];
            if (coord.dist[nb] == 1)
            {
                if (sg.worm[nb] and sg.descr.at(sg.worm[nb]).isSafe())
                    return true;
                break;
            }
        }
    }
    if (threats[0].is_in_encl[p] == 0 and threats[0].is_in_terr[p] == 0 and
        threats[1].is_in_encl[p] == 0 and threats[1].is_in_terr[p] == 0 and
        pattern3_value[0][p] < 0)
        return true;
    return false;
}

bool Game::checkRootListOfMovesCorrectness(Treenode *children) const
{
    int count = 0;
    int outside_terr = 0;
    int dame = 0;
    if (children != nullptr)
    {
        for (auto *ch = children; true; ++ch)
        {
            ++count;
            auto p = ch->move.ind;
            if (threats[0].is_in_encl[p] == 0 and
                threats[0].is_in_terr[p] == 0 and
                threats[1].is_in_encl[p] == 0 and threats[1].is_in_terr[p] == 0)
            {
                ++outside_terr;
            }
            if (isDame_directCheck_symm(p)) ++dame;
            if (ch->isLast()) break;
        }
    }
    if (debug_previous_count > 0 and debug_previous_count != count)
    {
        std::cerr << "Vanishing moves!!!" << std::endl;
        return false;
    }
    if (possible_moves.lists[PossibleMovesConsts::LIST_DAME].size() > 0 and
        possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].size() and
        dame == 0 and outside_terr == 0)
    {
        return false;  // no dame move among children, but there should be!
    }
    return true;
}

bool Game::checkMarginsCorrectness() const
{
    // check margins
    bool top = true, left = true, bottom = true,
         right = true;  // are margins empty?
    for (int i = 1; i < coord.wlkx - 1; i++)
    {
        int ind = coord.ind(i, 0);
        if (sg.worm[ind] != 0 || sg.worm[ind + coord.S] != 0) top = false;
        ind = coord.ind(i, coord.wlky - 1);
        if (sg.worm[ind] != 0 || sg.worm[ind + coord.N] != 0) bottom = false;
    }
    if (sg.worm[coord.ind(0, 1)] || sg.worm[coord.ind(coord.wlkx - 1, 1)])
        top = false;
    if (sg.worm[coord.ind(0, coord.wlky - 2)] ||
        sg.worm[coord.ind(coord.wlkx - 1, coord.wlky - 2)])
        bottom = false;
    for (int j = 1; j < coord.wlky - 1; j++)
    {
        int ind = coord.ind(0, j);
        if (sg.worm[ind] != 0 || sg.worm[ind + coord.E] != 0) left = false;
        ind = coord.ind(coord.wlkx - 1, j);
        if (sg.worm[ind] != 0 || sg.worm[ind + coord.W] != 0) right = false;
    }
    if (sg.worm[coord.ind(1, 0)] || sg.worm[coord.ind(1, coord.wlky - 1)])
        left = false;
    if (sg.worm[coord.ind(coord.wlkx - 2, 0)] ||
        sg.worm[coord.ind(coord.wlkx - 2, coord.wlky - 1)])
        right = false;
    return (left == possible_moves.left) and (right == possible_moves.right) and
           (top == possible_moves.top) and (bottom == possible_moves.bottom);
}

bool Game::checkWormCorrectness() const
{
    std::vector<pti> groups(coord.getSize(), 0);
    std::map<pti, WormDescr> test_descr;
    for (int ind = coord.first; ind <= coord.last; ind++)
        if (isDotAt(ind) and groups[ind] == 0)
        {
            // visit this group
            WormDescr dsc;
            test_descr.insert({sg.worm[ind], dsc});
            std::vector<pti> stack;
            stack.push_back(ind);
            groups[ind] = ind;
            int who = (sg.worm[ind] & sg.MASK_DOT);
            do
            {
                pti p = stack.back();
                stack.pop_back();
                for (int i = 0; i < 8; i++)
                {
                    pti nb = p + coord.nb8[i];
                    if ((sg.worm[nb] & sg.MASK_DOT) == who)
                    {
                        if (sg.worm[nb] != sg.worm[p])
                        {  // diagonal connection
                            if (test_descr.find(sg.worm[nb]) ==
                                test_descr.end())
                            {
                                test_descr.insert({sg.worm[nb], dsc});
                            }
                            if (std::find(
                                    test_descr[sg.worm[nb]].neighb.begin(),
                                    test_descr[sg.worm[nb]].neighb.end(),
                                    sg.worm[p]) ==
                                test_descr[sg.worm[nb]].neighb.end())
                            {
                                test_descr.at(sg.worm[nb])
                                    .neighb.push_back(sg.worm[p]);
                                test_descr.at(sg.worm[p])
                                    .neighb.push_back(sg.worm[nb]);
                            }
                        }
                        if (groups[nb] == 0)
                        {
                            groups[nb] = ind;
                            stack.push_back(nb);
                        }
                    }
                }
            } while (!stack.empty());
        }

    // std::cerr << coord.showBoard(groups) << std::endl;
    for (auto &d : test_descr)
    {
        if (sg.descr.find(d.first) == sg.descr.end())
        {
            std::cerr << "blad " << d.first << " nie wystepuje w descr"
                      << std::endl;
            return false;
        }
        d.second.safety = 0;
    }
    // add safety info
    for (int x = 0; x < coord.wlkx; x++)
    {
        if (sg.worm[coord.ind(x, 0)])
            test_descr.at(sg.worm[coord.ind(x, 0)]).safety =
                WormDescr::SAFE_VALUE;
        if (sg.worm[coord.ind(x, coord.wlky - 1)])
            test_descr.at(sg.worm[coord.ind(x, coord.wlky - 1)]).safety =
                WormDescr::SAFE_VALUE;
        if (x > 0 and x < coord.wlkx - 1)
        {
            if (sg.worm[coord.ind(x, 0)] == 0 and sg.worm[coord.ind(x, 1)])
                test_descr.at(sg.worm[coord.ind(x, 1)]).safety++;
            if (sg.worm[coord.ind(x, coord.wlky - 1)] == 0 and
                sg.worm[coord.ind(x, coord.wlky - 2)])
                test_descr.at(sg.worm[coord.ind(x, coord.wlky - 2)]).safety++;
        }
    }
    for (int y = 0; y < coord.wlky; y++)
    {
        if (sg.worm[coord.ind(0, y)])
            test_descr.at(sg.worm[coord.ind(0, y)]).safety =
                WormDescr::SAFE_VALUE;
        if (sg.worm[coord.ind(coord.wlkx - 1, y)])
            test_descr.at(sg.worm[coord.ind(coord.wlkx - 1, y)]).safety =
                WormDescr::SAFE_VALUE;
        if (y > 0 and y < coord.wlky - 1)
        {
            if (sg.worm[coord.ind(0, y)] == 0 and sg.worm[coord.ind(1, y)])
                test_descr.at(sg.worm[coord.ind(1, y)]).safety++;
            if (sg.worm[coord.ind(coord.wlkx - 1, y)] == 0 and
                sg.worm[coord.ind(coord.wlkx - 2, y)])
                test_descr.at(sg.worm[coord.ind(coord.wlkx - 2, y)]).safety++;
        }
    }

    for (auto &d1 : sg.descr)
    {
        // check keys
        if (test_descr.find(d1.first) == test_descr.end())
        {
            std::cerr << "blad, brak " << d1.first << std::endl;
            return false;
        }
        // check pairs
        for (auto &d2 : sg.descr)
        {
            bool not_in_neighb =
                (std::find(d1.second.neighb.begin(), d1.second.neighb.end(),
                           d2.first) == d1.second.neighb.end());
            bool not_in_test =
                (std::find(test_descr.at(d1.first).neighb.begin(),
                           test_descr.at(d1.first).neighb.end(),
                           d2.first) == test_descr.at(d1.first).neighb.end());
            if (not_in_neighb != not_in_test)
            {
                std::cerr << "blad" << std::endl;
                return false;
            }
            if ((d1.second.group_id == d2.second.group_id) !=
                (groups[d1.second.leftmost] == groups[d2.second.leftmost]))
            {
                std::cerr << "blad" << std::endl;
                return false;
            }
        }
        // safety
        if (d1.second.safety != test_descr.at(d1.first).safety and
            (d1.second.safety < WormDescr::SAFE_THRESHOLD ||
             test_descr.at(d1.first).safety < WormDescr::SAFE_THRESHOLD))
        {
            std::cerr << "blad safety " << d1.second.safety << " "
                      << test_descr.at(d1.first).safety << std::endl;
            return false;
        }
    }
    return true;
}

bool Game::checkSoftSafetyCorrectness()
{
    Safety s;
    s.init(&sg);
    bool shown = false;
    auto almost_eq = [](float x, float y)
    { return x >= y - 1e-4 and x <= y + 1e-4; };
    for (int ind = coord.first; ind <= coord.last; ind++)
    {
        if (not almost_eq(s.getSafetyOf(ind), sg.safety_soft.getSafetyOf(ind)))
        {
            if (not shown)
            {
                show();
                shown = true;
            }
            std::cerr << "bledne soft safety dla " << coord.showPt(ind)
                      << " jest: " << sg.safety_soft.getSafetyOf(ind)
                      << ", powinno byc: " << s.getSafetyOf(ind) << std::endl;
        }
        if (s.isDameFor(1, ind) != sg.safety_soft.isDameFor(1, ind) or
            s.isDameFor(2, ind) != sg.safety_soft.isDameFor(2, ind))
        {
            if (not shown)
            {
                show();
                shown = true;
            }
            std::cerr << "bledne is dame for dla " << coord.showPt(ind)
                      << " jest: " << sg.safety_soft.isDameFor(1, ind) << " "
                      << sg.safety_soft.isDameFor(2, ind)
                      << ", powinno byc: " << s.isDameFor(1, ind) << " "
                      << s.isDameFor(2, ind) << std::endl;
        }
    }
    return not shown;
}

bool Game::checkThreatCorrectness()
{
    std::map<uint64_t, Threat> checked[2];
    for (int pl = 0; pl < 2; ++pl)
    {
        uint16_t prev = 0;
        for (const Threat &thr : threats[pl].threats)
        {
            if (prev > thr.hist_size)
            {
                std::cerr << "No monotonicity of hist_size, for player "
                          << pl + 1 << " previous: " << prev
                          << ", current = " << thr.hist_size << std::endl;
                std::cerr << thr.show() << std::endl;
                return false;
            }
            if (thr.hist_size == 0)
            {
                std::cerr << "Unexpected hist_size == 0, for player " << pl + 1
                          << " previous: " << prev
                          << ", current = " << thr.hist_size << std::endl;
                std::cerr << thr.show() << std::endl;
                return false;
            }
            prev = thr.hist_size;
        }
    }

    for (int ind = coord.first + 1; ind < coord.last; ind++)
    {
        // if (coord.dist[ind]<0) continue;
        if (sg.worm[ind] == 0)
        {
            for (int who = 1; who <= 2; who++)
            {
                {
                    Threat t;
                    // first check territory enclosing [ind]
                    t.encl = std::make_shared<Enclosure>(
                        findEnclosure_notOptimised(ind, sg.MASK_DOT, who));
                    if (!t.encl->isEmpty())
                    {
                        t.type = ThreatConsts::TERR;
                        t.where = 0;
                        t.zobrist_key = t.encl->zobristKey(who);
                        // try to save it
                        if (checked[who - 1].find(t.zobrist_key) ==
                            checked[who - 1].end())
                        {
                            checked[who - 1].insert(
                                {t.zobrist_key, std::move(t)});
                        }
                    }
                }
                {
                    Threat t;
                    // now try to put a dot here and enclose
                    sg.worm[ind] = who;
                    for (int j = 0; j < 4; j++)
                    {
                        pti nb = ind + coord.nb4[j];
                        if (coord.dist[nb] >= 0 and
                            (sg.worm[nb] & sg.MASK_DOT) != who)
                        {
                            t.encl = std::make_shared<Enclosure>(
                                findEnclosure_notOptimised(nb, sg.MASK_DOT,
                                                           who));
                            if (!t.encl->isEmpty() and t.encl->isInBorder(ind))
                            {
                                t.type = ThreatConsts::ENCL;
                                t.where = ind;
                                t.zobrist_key = t.encl->zobristKey(who);
                                // try to save it, if not yet saved
                                if (checked[who - 1].find(t.zobrist_key) ==
                                    checked[who - 1].end())
                                {
                                    checked[who - 1].insert(
                                        {t.zobrist_key, std::move(t)});
                                }
                            }
                        }
                    }
                }
            }
            sg.worm[ind] = 0;
        }
        else if (coord.dist[ind] > 0)
        {
            // try to enclose it
            Threat t;
            int who = (sg.worm[ind] & sg.MASK_DOT) ^ sg.MASK_DOT;
            t.encl = std::make_shared<Enclosure>(
                findEnclosure_notOptimised(ind, sg.MASK_DOT, who));
            if (!t.encl->isEmpty())
            {
                t.type = ThreatConsts::TERR;
                t.where = 0;
                t.zobrist_key = t.encl->zobristKey(who);
                // try to save it
                if (checked[who - 1].find(t.zobrist_key) ==
                    checked[who - 1].end())
                {
                    checked[who - 1].insert({t.zobrist_key, t});
                }
            }
        }
    }
    // now check if they are the same
    bool shown = false;
    for (int pl = 0; pl < 2; pl++)
        for (Threat &thr : threats[pl].threats)
        {
            auto pos = checked[pl].find(thr.zobrist_key);
            if (pos == checked[pl].end())
            {
                if (!shown)
                {
                    shown = true;
                    show();
                }
                std::cerr << "Nieaktualne zagrozenie (gracz=" << pl + 1
                          << "):" << std::endl;
                std::cerr << thr.show() << std::endl;
            }
            else
            {
                if (pos->second.type & ThreatConsts::TO_REMOVE)
                {
                    if (!shown)
                    {
                        shown = true;
                        show();
                    }
                    std::cerr << "Powtórzone zagrożenie (gracz=" << pl + 1
                              << "):" << std::endl;
                }
                else
                {
                    pos->second.type |= ThreatConsts::TO_REMOVE;
                }
            }
            int16_t true_opp_dots, true_terr;
            auto tmp = countDotsTerrInEncl(*thr.encl, 2 - pl, false);
            true_opp_dots = std::get<0>(tmp);
            true_terr = std::get<1>(tmp);
            if (true_opp_dots != thr.opp_dots || true_terr != thr.terr_points)
            {
                if (!shown)
                {
                    shown = true;
                    show();
                }
                std::cerr << "Zla liczba kropek w zagrozeniu (gracz=" << pl + 1
                          << "): jest " << int(thr.opp_dots) << ", powinno byc "
                          << int(true_opp_dots) << std::endl;
                std::cerr << "lub liczba pktow terr: jest "
                          << int(thr.terr_points) << ", powinno byc "
                          << int(true_terr) << std::endl;
                std::cerr << thr.show() << std::endl;
            }
        }
    for (int pl = 0; pl < 2; pl++)
        for (auto &thr : checked[pl])
        {
            if ((thr.second.type & ThreatConsts::TO_REMOVE) == 0)
            {
                // maybe this is an uninteresting threat?
                if (thr.second.type & ThreatConsts::ENCL)
                {
                    auto zobr = thr.second.zobrist_key ^
                                coord.zobrist_encl[pl][thr.second.where];
                    Threat *tz = threats[pl].findThreatZobrist(zobr);
                    if (tz != nullptr and
                        (tz->type & (ThreatConsts::TO_CHECK |
                                     ThreatConsts::TO_REMOVE)) == 0)
                    {
                        continue;  // trivial enclosure! (we put dot inside our
                                   // territory and have an enclosure 1-dot
                                   // smaller than the original territory)
                    }
                    // maybe this is a still uninteresting threat, but more
                    // difficult to check? check if (thr.interior) is a subset
                    // of some territory's (threats.interior), which differs
                    // only by our dots
                    {
                        bool is_boring = false;
                        std::vector<int8_t> interior(coord.last + 1, 0);
                        for (auto i : thr.second.encl->interior)
                            interior[i] = 1;
                        for (auto &t : threats[pl].threats)
                            if (t.type & ThreatConsts::TERR)
                            {
                                bool subset = true;
                                // check if every point in old terr is a point
                                // inside new one or our dot
                                for (auto i : t.encl->interior)
                                {
                                    if (interior[i] == 0 and
                                        (sg.worm[i] & sg.MASK_DOT) !=
                                            (pl + 1) and
                                        i != thr.second.where)
                                    {
                                        subset = false;
                                        break;
                                    }
                                }
                                if (subset)
                                {
                                    // and vice versa, check if each point of a
                                    // new terr is a point in old one
                                    std::vector<int8_t> interior_old(
                                        coord.last + 1, 0);
                                    for (auto i : t.encl->interior)
                                        interior_old[i] = 1;
                                    for (auto i : thr.second.encl->interior)
                                    {
                                        if (interior_old[i] == 0)
                                        {
                                            subset = false;
                                            break;
                                        }
                                    }
                                }
                                if (subset)
                                {
                                    /*
                                    show();
                                    std::cerr << "Równoważne zagrożenia (oryg,
                                    nowe): " << std::endl; std::cerr << t.show()
                                    << std::endl; std::cerr << thr.second.show()
                                    << std::endl;
                                    */
                                    is_boring = true;
                                    break;
                                }
                            }
                        if (is_boring) continue;
                    }
                }
                if (!shown)
                {
                    shown = true;
                    show();
                }
                std::cerr << "Nieznalezione zagrozenie (gracz=" << pl + 1
                          << "):" << std::endl;
                std::cerr << thr.second.show() << std::endl;
            }
        }
    // check if is_in_terr, is_in_encl and is_in_border are correct
    // also check singular_dots and border_dots_in_danger
    for (int pl = 0; pl < 2; pl++)
    {
        std::vector<pti> te(coord.getSize(), 0);
        std::vector<pti> en(coord.getSize(), 0);
        std::vector<pti> bo(coord.getSize(), 0);
        for (Threat &thr : threats[pl].threats)
        {
            if (thr.type & ThreatConsts::TERR)
            {
                for (auto i : thr.encl->interior) ++te[i];
            }
            else if (thr.type & ThreatConsts::ENCL)
            {
                for (auto i : thr.encl->interior) ++en[i];
            }
            for (auto i : thr.encl->border) ++bo[i];
            bo[thr.encl->border[0]]--;  // this was counted twice
            if (thr.type & ThreatConsts::TO_REMOVE)
            {
                if (!shown)
                {
                    shown = true;
                    show();
                }
                std::cerr << "Nieusuniete zagrozenie (gracz=" << pl + 1
                          << "):" << std::endl
                          << thr.show() << std::endl;
            }
            // now check singular_dots
            int singular = 0;
            std::set<pti> counted;
            for (auto i : thr.encl->interior)
            {
                if (whoseDotMarginAt(i) == 2 - pl and
                    threats[pl].is_in_encl[i] + threats[pl].is_in_terr[i] ==
                        1 and
                    counted.find(sg.descr.at(sg.worm[i]).leftmost) ==
                        counted.end())
                {
                    singular += sg.descr.at(sg.worm[i]).dots[1 - pl];
                    counted.insert(sg.descr.at(sg.worm[i]).leftmost);
                }
            }
            if (singular != thr.singular_dots)
            {
                std::cerr << "Bledne singular_dots (gracz=" << pl + 1
                          << "), powinno byc: " << singular
                          << ", jest: " << thr.singular_dots << std::endl;
                show();
                std::cerr << thr.show() << std::endl;
                return false;
            }
            // check border_dots_in_danger and opp_thr
            std::vector<uint64_t> true_ot;
            int in_danger = 0;
            for (auto it = thr.encl->border.begin() + 1;
                 it != thr.encl->border.end(); ++it)
            {
                pti i = *it;
                if (threats[1 - pl].is_in_encl[i] > 0 ||
                    threats[1 - pl].is_in_terr[i] > 0)
                {
                    in_danger++;
                    for (auto &ot : threats[1 - pl].threats)
                    {
                        if (ot.encl->isInInterior(i))
                            addOppThreatZobrist(true_ot, ot.zobrist_key);
                    }
                }
            }
            if (in_danger != thr.border_dots_in_danger)
            {
                std::cerr << "Bledne border_dots_in_danger (gracz=" << pl + 1
                          << "), powinno byc: " << in_danger
                          << ", jest: " << thr.border_dots_in_danger
                          << std::endl;
                show();
                std::cerr << thr.show() << std::endl;
                return false;
            }
            std::vector<uint64_t> old_ot(thr.opp_thr);
            std::sort(true_ot.begin(), true_ot.end());
            std::sort(old_ot.begin(), old_ot.end());
            for (unsigned i = 0; i < std::min(true_ot.size(), old_ot.size());
                 i++)
            {
                if (true_ot[i] != old_ot[i])
                {
                    std::cerr << "Blad: jest " << old_ot[i] << ", powinno byc "
                              << true_ot[i] << std::endl;
                    show();
                    std::cerr << thr.show() << std::endl;
                    for (auto &ot : threats[1 - pl].threats)
                    {
                        if (ot.zobrist_key == old_ot[i])
                        {
                            std::cerr << ot.show() << std::endl;
                        }
                        if (ot.zobrist_key == true_ot[i])
                        {
                            std::cerr << ot.show() << std::endl;
                        }
                    }
                    return false;
                }
            }
            if (true_ot.size() != old_ot.size())
            {
                if (true_ot.size() < old_ot.size())
                {
                    for (unsigned i = true_ot.size(); i < old_ot.size(); i++)
                    {
                        std::cerr << "Nadmiarowe opp_thr " << old_ot[i]
                                  << std::endl;
                        for (auto &ot : threats[1 - pl].threats)
                        {
                            if (ot.zobrist_key == old_ot[i]) thr.show();
                        }
                    }
                }
                else
                {
                    for (unsigned i = old_ot.size(); i < true_ot.size(); i++)
                    {
                        std::cerr << "Nadmiarowe opp_thr " << true_ot[i]
                                  << std::endl;
                        for (auto &ot : threats[1 - pl].threats)
                        {
                            if (ot.zobrist_key == true_ot[i]) thr.show();
                        }
                    }
                }
                return false;
            }
        }
        //
        for (int i = coord.first; i <= coord.last; i++)
        {
            if (te[i] != threats[pl].is_in_terr[i])
            {
                if (!shown)
                {
                    shown = true;
                    show();
                }
                std::cerr << "Bledne is_in_terr (gracz=" << pl + 1
                          << "), powinno byc:" << std::endl
                          << coord.showFullBoard(te) << std::endl;
                std::cerr << "jest: " << std::endl
                          << coord.showFullBoard(threats[pl].is_in_terr)
                          << std::endl;
                std::cerr << "Zagrozenia:" << std::endl;
                for (Threat &thr : threats[pl].threats)
                    std::cerr << thr.show() << std::endl;
                break;
            }
            if (en[i] != threats[pl].is_in_encl[i])
            {
                if (!shown)
                {
                    shown = true;
                    show();
                }
                std::cerr << "Bledne is_in_encl (gracz=" << pl + 1
                          << "), powinno byc:" << std::endl
                          << coord.showFullBoard(en) << std::endl;
                std::cerr << "jest: " << std::endl
                          << coord.showFullBoard(threats[pl].is_in_encl)
                          << std::endl;
                break;
            }
            if (bo[i] != threats[pl].is_in_border[i])
            {
                if (!shown)
                {
                    shown = true;
                    show();
                }
                std::cerr << "Bledne is_in_border (gracz=" << pl + 1
                          << "), powinno byc:" << std::endl
                          << coord.showFullBoard(bo) << std::endl;
                std::cerr << "jest: " << std::endl
                          << coord.showFullBoard(threats[pl].is_in_border)
                          << std::endl;
                break;
            }
        }
    }

    return !shown;
}

bool Game::checkThreatWithDfs()
{
    for (int pl = 0; pl < 2; ++pl)
    {
        const int who = pl + 1;
        OnePlayerDfs dfs;
        dfs.player = who;
        dfs.AP(getSimpleGame(), coord.first, coord.last);
        auto allEncls = dfs.findAllEnclosures();
        std::set<std::pair<std::set<pti>, std::set<pti>>> dfsEncls;
        std::set<std::pair<std::set<pti>, std::set<pti>>> thrEncls;
        for (const auto &encl : allEncls)
            dfsEncls.emplace(
                std::set<pti>(encl.interior.begin(), encl.interior.end()),
                std::set<pti>(encl.border.begin(), encl.border.end()));
        for (const Threat &thr : threats[pl].threats)
        {
            // in DFS, we don't find territories (yet)
            if (thr.type & ThreatConsts::TERR) continue;
            // in DFS, we don't find enclorues inside territories	(yet)
            if (threats[pl].is_in_terr[thr.where]) continue;
            thrEncls.emplace(std::set<pti>(thr.encl->interior.begin(),
                                           thr.encl->interior.end()),
                             std::set<pti>(thr.encl->border.begin(),
                                           thr.encl->border.end()));
        }
        // check
        if (dfsEncls != thrEncls)
        {
            std::cerr << "*** DFS nie zgadza sie z threats dla gracza " << who
                      << " !!!!!!!!!!!!! ****" << std::endl
                      << coord.showFullBoard(threats[pl].is_in_border)
                      << std::endl;
            return false;
        }
    }
    return true;
}

bool Game::checkThreat2movesCorrectness()
{
    std::set<std::array<pti, 2>> pairs;
    std::vector<pti> is_in_2m_encl(coord.getSize(), 0);
    std::vector<pti> is_in_2m_miai(coord.getSize(), 0);
    for (int who = 1; who <= 2; who++)
    {
        for (auto &thr2 : threats[who - 1].threats2m)
        {
            pti where0 = thr2.where0;
            std::vector<pti> is_in_encl2(coord.getSize(), 0);
            int found_safe = ((thr2.flags & Threat2mconsts::FLAG_SAFE) != 0);
            int real_safe = (isInEncl(where0, 3 - who) == 0 and
                             isInTerr(where0, 3 - who) == 0);
            if (found_safe != real_safe)
            {
                std::cerr << "flag = " << thr2.flags << "; is_in_encl[where0]=="
                          << isInEncl(where0, 3 - who)
                          << ", is_in_terr[where0]=="
                          << isInTerr(where0, 3 - who) << std::endl;
                show();
                std::cerr << "where0 = " << coord.showPt(where0) << std::endl;
                std::cerr << "Threats: " << std::endl;
                for (auto t : thr2.thr_list) std::cerr << t.show();
                return false;
            }
            for (auto &thr : thr2.thr_list)
            {
                pti where = thr.where;
                pairs.insert({where0, where});
                bool correct = false;
                if (sg.worm[where] == 0 and sg.worm[where0] == 0)
                {
                    CleanupOneVar<pti> worm_where_cleanup0(
                        &sg.worm[where0],
                        who);  //  sg.worm[where0] = who;  with restored old
                               //  value (0) by destructor
                    CleanupOneVar<pti> worm_where_cleanup(
                        &sg.worm[where],
                        who);  //  sg.worm[where] = who;  with restored old
                               //  value (0) by destructor
                    int done[4] = {0, 0, 0, 0};
                    for (int j = 0; j < 4; j++)
                        if (!done[j])
                        {
                            pti nb = where + coord.nb4[j];
                            if ((sg.worm[nb] & sg.MASK_DOT) != who and
                                coord.dist[nb] >= 1 and
                                thr.encl->isInInterior(nb))
                            {
                                Threat t;
                                t.encl = std::make_shared<Enclosure>(
                                    findEnclosure_notOptimised(nb, sg.MASK_DOT,
                                                               who));
                                if (!t.encl->isEmpty() and
                                    t.encl->isInBorder(where) and
                                    t.encl->isInBorder(where0))
                                {
                                    t.zobrist_key = t.encl->zobristKey(who);
                                    if (t.zobrist_key == thr.zobrist_key)
                                    {  // our old threat is still valid
                                        correct = true;
                                        goto Thr2_correct;
                                    }
                                    // if some next neighbour has been enclosed,
                                    // mark it as done
                                    for (int k = j + 1; k < 4; k++)
                                    {
                                        if (t.encl->isInInterior(where +
                                                                 coord.nb4[k]))
                                            done[k] = 1;
                                    }
                                }
                            }
                        }
                }
            Thr2_correct:;
                if (!correct)
                {
                    show();
                    std::cerr << "where0 = " << coord.showPt(where0)
                              << ", where=" << coord.showPt(where) << std::endl;
                    thr.show();
                    return false;
                }
                // check stats
                for (pti p : thr.encl->interior)
                {
                    is_in_encl2[p] += Threat2mconsts::ENCL2_INSIDE_ADD;
                    if (thr2.win_move_count >= 2 ||
                        (thr2.win_move_count == 1 and thr.opp_dots == 0))
                        is_in_encl2[p] |= Threat2mconsts::ENCL2_MIAI;
                }
            }
            std::string err;
            bool correct = true;
            if (thr2.is_in_encl2.empty() and thr2.thr_list.size() > 1)
            {
                err = "empty encl2";
                correct = false;
            }
            else if (!thr2.is_in_encl2.empty())
            {
                for (int i = 0; i < coord.getSize(); i++)
                {
                    if (is_in_encl2[i] != thr2.is_in_encl2[i])
                    {
                        std::stringstream out;
                        out << " error in is_in_encl2 at " << coord.showPt(i)
                            << "  is: " << thr2.is_in_encl2[i]
                            << ", should be: " << is_in_encl2[i] << std::endl;
                        err = out.str();
                        correct = false;
                        break;
                    }
                    if (thr2.flags & Threat2mconsts::FLAG_SAFE)
                    {
                        is_in_2m_encl[i] +=
                            (is_in_encl2[i] >
                             Threat2mconsts::ENCL2_INSIDE_THRESHOLD);
                        is_in_2m_miai[i] +=
                            (is_in_encl2[i] & Threat2mconsts::ENCL2_MIAI) != 0;
                    }
                }
            }
            if (!correct)
            {
                show();
                std::cerr << "where0 = " << coord.showPt(where0)
                          << ", err = " << err << std::endl;
                std::cerr << "Threats: " << std::endl;
                for (auto t : thr2.thr_list) std::cerr << t.show();
                return false;
            }
        }
    }
    bool correct = true;
    for (auto p : pairs)
    {
        std::array<pti, 2> pair2{p[1], p[0]};
        if (pairs.find(pair2) == pairs.end())
        {
            if (correct) show();
            std::cerr << "Error: Threat2 " << coord.showPt(p[0]) << "--"
                      << coord.showPt(p[1])
                      << " found, but not the reversed one." << std::endl;
            correct = false;
        }
    }
    return correct;
}

bool Game::checkConnectionsCorrectness()
{
    std::vector<OneConnection> tmp[2]{sg.getConnects(0), sg.getConnects(1)};
    sg.findConnections();
    for (int ind = 0; ind < coord.getSize(); ind++)
    {
        if (sg.getConnects(0)[ind] != tmp[0][ind] ||
            sg.getConnects(1)[ind] != tmp[1][ind])
        {
            std::cerr << "Zla tablica connections, indeks " << ind << " = ("
                      << int(coord.x[ind]) << ", " << int(coord.y[ind]) << ")."
                      << std::endl;
            show();
            std::cerr << "Stara tab 1" << std::endl;
            std::cerr << coord.showBoard(tmp[0]);
            std::cerr << "Stara tab 2" << std::endl;
            std::cerr << coord.showBoard(tmp[1]);
            showConnections();
            //
            showGroupsId();
            for (int g = 0; g <= 1; g++)
            {
                for (int j = 0; j < 4; j++)
                    std::cerr << int(tmp[g][ind].groups_id[j]) << " ";
                if (g == 0)
                    std::cerr << ",";
                else
                    std::cerr << std::endl;
            }
            std::cerr << "Nowe conn: ";
            for (int g = 0; g <= 1; g++)
            {
                for (int j = 0; j < 4; j++)
                    std::cerr << int(sg.getConnects(g)[ind].groups_id[j])
                              << " ";
                if (g == 0)
                    std::cerr << ",";
                else
                    std::cerr << std::endl;
            }

            return false;
        }
    }
    return true;
}

bool Game::checkPattern3valuesCorrectness() const
{
    for (int ind = 0; ind < coord.getSize(); ind++)
    {
        real_t should_be[2];
        if (whoseDotMarginAt(ind) != 0)
        {
            should_be[0] = 0;
            should_be[1] = 0;
        }
        else
        {
            should_be[0] = getPattern3Value(ind, 1);
            should_be[1] = getPattern3Value(ind, 2);
        }
        for (int who = 1; who <= 2; ++who)
        {
            auto is = pattern3_value[who - 1][ind];
            if (is != should_be[who - 1])
            {
                std::cerr << "recalculate_list: ";
                for (auto i : recalculate_list)
                    std::cerr << coord.showPt(i) << " ";
                std::cerr << std::endl;
                show();
                std::cerr << "Wrong patt3 value for who=" << who << " at "
                          << coord.showPt(ind) << ", is: " << is
                          << " should be: " << should_be[who - 1] << std::endl;
                assert(is == should_be[who - 1]);
                return false;
            }
        }
    }
    // check list of moves
    std::vector<pti> listm[3];
    for (pti ind = coord.first; ind <= coord.last; ind++)
    {
        if (whoseDotMarginAt(ind) == 0)
        {
            if (threats[0].is_in_encl[ind] == 0 and
                threats[0].is_in_terr[ind] == 0 and
                threats[1].is_in_encl[ind] == 0 and
                threats[1].is_in_terr[ind] == 0)
            {
                if (isDame_directCheck_symm(ind))
                    listm[PossibleMovesConsts::LIST_DAME].push_back(ind);
                else
                    listm[PossibleMovesConsts::LIST_NEUTRAL].push_back(ind);
            }
            else
            {
                listm[PossibleMovesConsts::LIST_TERRM].push_back(ind);
            }
        }
    }
    std::string names[3] = {"neutral", "dame", "terrm"};
    bool status = true;
    for (int j = 0; j < 3; j++)
    {
        std::vector<pti> possm = possible_moves.lists[j];
        if (possm.size() != listm[j].size())
        {
            std::cerr << "Size of possible_moves (" << names[j]
                      << ") differ -- should be" << listm[j].size()
                      << ", is: " << possm.size() << std::endl;
            status = false;
        }
        std::sort(listm[j].begin(), listm[j].end());
        std::sort(possm.begin(), possm.end());
        std::vector<pti> diff;
        std::set_difference(listm[j].begin(), listm[j].end(), possm.begin(),
                            possm.end(), back_inserter(diff));
        if (!diff.empty())
        {
            std::cerr << "[" << names[j] << "] Moves that are missing: ";
            for (auto p : diff) std::cerr << coord.showPt(p) << " ";
            std::cerr << std::endl;
            status = false;
        }
        diff.clear();
        std::set_difference(possm.begin(), possm.end(), listm[j].begin(),
                            listm[j].end(), back_inserter(diff));
        if (!diff.empty())
        {
            std::cerr << "[" << names[j]
                      << "] Moves that are incorrecly contained: ";
            for (auto p : diff) std::cerr << coord.showPt(p) << " ";
            std::cerr << std::endl;
            status = false;
        }
    }
    if (!status) show();
    return status;
}

bool Game::checkPossibleMovesCorrectness() const
{
    int terr = 0, neutral_or_dame = 0;
    int ext = 0, exnd = 0;
    for (int i = coord.first; i <= coord.last; ++i)
    {
        if (whoseDotMarginAt(i) == 0)
        {
            if (threats[0].is_in_terr[i] || threats[0].is_in_encl[i] ||
                threats[1].is_in_terr[i] || threats[1].is_in_encl[i])
            {
                ++terr;
                ext = i;
            }
            else
            {
                ++neutral_or_dame;
                exnd = i;
            }
        }
    }
    if ((neutral_or_dame > 0) !=
        (!possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].empty() ||
         !possible_moves.lists[PossibleMovesConsts::LIST_DAME].empty()))
    {
        if (neutral_or_dame > 0)
        {
            std::cerr << "Possible neutral or dame moves is empty, but there "
                         "is such a move: "
                      << coord.showPt(exnd) << std::endl;
        }
        else
        {
            if (!possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL]
                     .empty())
            {
                std::cerr
                    << "Possible neutral moves has: "
                    << coord.showPt(
                           possible_moves
                               .lists[PossibleMovesConsts::LIST_NEUTRAL][0])
                    << std::endl;
            }
            if (!possible_moves.lists[PossibleMovesConsts::LIST_DAME].empty())
            {
                std::cerr << "Possible dame moves has: "
                          << coord.showPt(
                                 possible_moves
                                     .lists[PossibleMovesConsts::LIST_DAME][0])
                          << std::endl;
            }
        }
        show();
        return false;
    }
    if ((terr > 0) !=
        (!possible_moves.lists[PossibleMovesConsts::LIST_TERRM].empty()))
    {
        if (terr > 0)
        {
            std::cerr << "TERR moves is empty, but there is such a move: "
                      << coord.showPt(ext) << std::endl;
        }
        else
        {
            std::cerr
                << "Possible TERR move: "
                << coord.showPt(
                       possible_moves.lists[PossibleMovesConsts::LIST_TERRM][0])
                << std::endl;
        }
        show();
        return false;
    }
    return true;
}

bool Game::checkCorrectness(SgfSequence seq)
{
    if (seq[0].findProp("RE") == seq[0].props.end())
        return true;  // cannot check
    std::string re = seq[0].findProp("RE")->second[0];
    int res = 0;
    if (re == "W+R" || re == "W+F" || re == "W+T" || re == "B+R" ||
        re == "B+F" || re == "B+T" || re == "?")
        return true;  // cannot check
    if (re.substr(0, 2) == "W+")
    {
        res = -stoi(re.substr(2));
    }
    else if (re.substr(0, 2) == "B+")
    {
        res = stoi(re.substr(2));
    }
    else if (re == "0")
    {
        res = 0;
    }
    else
        return false;
    auto saved_komi = global::komi;
    global::komi = 0;
    auto [ct, ct_small_score] = countTerritory_simple(sg.nowMoves);
    if (ct == res)
    {
        global::komi = saved_komi;
        return true;
    }
    auto [ct2, ct2_small_score] = countTerritory(sg.nowMoves);
    global::komi = saved_komi;
    std::cerr << "Blad, ct=" << ct << ", res=" << res << ", zwykle ct=" << ct2
              << std::endl;
    return false;
}
