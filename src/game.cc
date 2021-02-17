/********************************************************************************************************
 kropla -- a program to play Kropki; file game.cc -- main file.
    Copyright (C) 2015,2016,2017,2018 Bartek Dyda,
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

#include <vector>
#include <array>
#include <map>
#include <set>
#include <list>  // ?
#include <queue>
#include <cassert>
#include <cstdlib>    // abs()
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <algorithm>
#include <memory>   // unique pointer
#include <cmath>
//#include <exception>
#include <stdexcept>
#include <cctype>  // iswhite()
#include <chrono>  // chrono::high_resolution_clock, only to measure elapsed time

#include <boost/container/small_vector.hpp>

#include "board.h"
#include "sgf.h"
#include "command.h"
#include "patterns.h"
#include "enclosure.h"
#include "threats.h"
#include "game.h"
#include "montecarlo.h"

Coord coord(15,15);

std::chrono::high_resolution_clock::time_point start_time = std::chrono::high_resolution_clock::now();   // to measure time, value only to use auto
long long debug_nanos = 0;
long long debug_nanos2 = 0;
long long debug_nanos3 = 0;
int debug_previous_count = 0;
thread_local std::default_random_engine Game::engine;

namespace global {
Pattern3 patt3;
Pattern3 patt3_symm;
Pattern3 patt3_cost;
Pattern3extra_array patt3_extra;
Pattern52 patt52_edge({});
Pattern52 patt52_inner({});
int komi;
int komi_ratchet;
}

/********************************************************************************************************
  Monte Carlo constants.
*********************************************************************************************************/
const constexpr real_t MC_SIMS_EQUIV_RECIPR = 1.0 / 400.0;  // originally 2500!
const constexpr real_t MC_SIMS_ENCL_EQUIV_RECIPR = 1.0 / 20.0;



/********************************************************************************************************
  Worm description class
*********************************************************************************************************/

std::string
WormDescr::show() const
{
  std::stringstream out;
  out << "dots={" << dots[0] << ", " << dots[1] << "}, leftmost=" << leftmost << ", group_id=" << group_id << ", safety=" << safety;
  return out.str();
}

/********************************************************************************************************
  Some helper functions
*********************************************************************************************************/

void addOppThreatZobrist(std::vector<uint64_t> &v, uint64_t z)
{
  if (v.empty()) {
    v.push_back(z);
  } else if (v.back() == z) {
    return;
  } else {
    auto pos = std::find(v.begin(), v.end(), z);
    if (pos == v.end()) {
      v.push_back(z);
    } else {
      std::swap(*pos, v.back());  // to save time on future function calls
    }
  }
}

void removeOppThreatZobrist(std::vector<uint64_t> &v, uint64_t z)
{
  auto pos = std::find(v.begin(), v.end(), z);
  if (pos != v.end()) {
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
  Connections class
*********************************************************************************************************/
bool
OneConnection::operator!=(const OneConnection& other) const
{
  for (int j=0; j<4; j++)
    if (other.groups_id[j]!=groups_id[j]) return true;
  return false;
}

/// Finds unique groups in the neighbourhood (i.e., ug[i] != ug[j] for i!=j and i,j<count_ug).
/// Does not change ug[i] for i>=count_ug.
/// @param[out] unique_groups  The array where the unique groups' numbers are stored.
/// @return                    The number of unique groups (count_ug).
int
OneConnection::getUniqueGroups(std::array<pti,4> &unique_groups) const
{
  if (groups_id[0]==0) return 0;
  unique_groups[0] = groups_id[0];
  if (groups_id[1]==0) return 1;
  int ug = 1;
  for (int g=1; g<4; g++) {
    if (groups_id[g]==0) break;
    for (int j=0; j<ug; j++)
      if (unique_groups[j] == groups_id[g]) goto was_saved;
    unique_groups[ug++] = groups_id[g];
  was_saved:;
  }
  return ug;
}

/********************************************************************************************************
  Movestats class for handling Monte Carlo tree.
*********************************************************************************************************/

const Movestats&
Movestats::operator+=(const Movestats& other)
{
  playouts += other.playouts;
  auto other_sum = other.value_sum.load();
  value_sum = value_sum.load() + other_sum;
  return *this;
}

Movestats& Movestats::operator=(const Movestats& other)
{
  playouts = other.playouts.load();
  value_sum = other.value_sum.load();
  return *this;
}


bool
Movestats::operator<(const Movestats& other) const
{
  if (other.playouts == 0) return false;
  if (playouts == 0) return true;
  return (value_sum / playouts  < other.value_sum / other.playouts);
}

std::string
Movestats::show() const
{
  std::stringstream out;
  out << " v=" << (playouts > 0 ? value_sum/playouts : 0.0) << " sim=" << playouts;
  return out.str();
}

/********************************************************************************************************
  Treenode class for handling Monte Carlo tree.
*********************************************************************************************************/

Treenode&
Treenode::operator=(const Treenode& other)
{
   parent = other.parent;
   children = other.children.load();
   t = other.t;
   amaf = other.amaf;
   prior = other.prior;
   move = other.move;
   flags = other.flags;
   return *this;
}

real_t
Treenode::getValue() const
{
  real_t value;
  real_t ucb_term = 0.0;
  if (parent != this) {  // not at root
    uint32_t N = parent->t.playouts;
    uint32_t n = t.playouts;
    constexpr real_t C = 0.1;
    ucb_term = C * std::sqrt(std::log(N+1) / (n + 0.1));
  }
  if (t.playouts > 0 && amaf.playouts>0) {
    const real_t mc_sims_equiv = move.enclosures.empty() ? MC_SIMS_EQUIV_RECIPR : MC_SIMS_ENCL_EQUIV_RECIPR;
    real_t beta = amaf.playouts / (amaf.playouts + t.playouts + t.playouts* mc_sims_equiv * amaf.playouts);
    value = beta * amaf.value_sum / amaf.playouts + (1-beta) * t.value_sum / t.playouts;
  } else {
    if (t.playouts > 0) {
      value = t.value_sum / t.playouts;
    } else {
      value = amaf.value_sum / amaf.playouts;
    }
  }
  return value + ucb_term;
}


bool
Treenode::operator<(const Treenode& other) const
{
  //return t < other.t;
  return getValue() < other.getValue();
}

const Treenode*
Treenode::getBestChild() const
{
  if (children == nullptr) return nullptr;
  int max = 0;
  Treenode *best = nullptr;
  for (Treenode *ch = children; true ;++ch) {
    if (ch->t.playouts - ch->prior.playouts > max) {
      max = ch->t.playouts - ch->prior.playouts;
      best = ch;
    }
    if (ch->isLast()) break;
  }
  return best;
}

std::string
Treenode::show() const
{
  return move.show() + " " + t.show() + ", prior: " + prior.show() +  ", amaf: " + amaf.show();
}

std::string
Treenode::getMoveSgf() const
{
  SgfProperty pr = move.toSgfString();
  return std::string(";") + pr.first + "[" + pr.second[0] + "]";
}

/********************************************************************************************************
  TreenodeAllocator class for thread-safe memory allocation in Monte Carlo.
*********************************************************************************************************/
TreenodeAllocator::TreenodeAllocator()
{
  pools.push_back( new Treenode[pool_size] );
  min_block_size = 3*coord.wlkx*coord.wlky;
  last_block_start = 0;
  cursor = 0;
}


TreenodeAllocator::~TreenodeAllocator()
{
  std::cerr << "Memory use (Treenode) " << pools.size() << " * " << pool_size << " * " << sizeof(Treenode)
	    << ";  in last pool: " << cursor << std::endl;
  for (auto &el : pools) {
    delete[] el;
  }
  pools.clear();
}


/// Returns pointer to the next (free) element.
Treenode*
TreenodeAllocator::getNext()
{
  if (cursor == pool_size) {
    assert(last_block_start > 0);  // otherwise our pools are too small
    // reallocate
    Treenode *newt = new Treenode[pool_size];
    std::copy(&pools.back()[last_block_start], &pools.back()[last_block_start] + cursor-last_block_start, newt);
    pools.push_back( newt );
    cursor = pool_size - last_block_start;
    last_block_start = 0;
  }
  //  pools.back()[cursor] = Treenode();
  return &pools.back()[cursor++];
}

/// Returns pointer to the last block, and resets the last block.
Treenode*
TreenodeAllocator::getLastBlock()
{
  if (cursor == last_block_start)
    return nullptr;
  else {
    Treenode *res = &pools.back()[last_block_start];
    pools.back()[cursor-1].markAsLast();
    if (last_block_start + min_block_size < pool_size) {
      last_block_start = cursor;
    } else {
      pools.push_back( new Treenode[pool_size] );
      last_block_start = 0;
      cursor = 0;
    }
    return res;
  }
}

void
TreenodeAllocator::copyPrevious()
{
  assert(cursor >= last_block_start+2);
  pools.back()[cursor-1] = pools.back()[cursor-2];
}

int
TreenodeAllocator::getSize(Treenode *ch)
{
  int n = 0;
  if (ch != nullptr) {
    for (;;) {
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
void
PossibleMoves::removeFromList(pti p)
{
  int curr_list = mtype[p] >> PossibleMovesConsts::MASK_SHIFT;
  if (curr_list < 3) {
    unsigned int ind = (mtype[p] & PossibleMovesConsts::INDEX_MASK);
    if (ind+1 < lists[curr_list].size()) {
      // p is not the last element on the curr_list, so replace it by the last one
      pti last = lists[curr_list].back();
      lists[curr_list][ind] = last;
      mtype[last] = (curr_list << PossibleMovesConsts::MASK_SHIFT) | ind;
    }
    lists[curr_list].pop_back();
  }
}

void
PossibleMoves::newDotOnEdge(pti p, EdgeType edge)
{
  int ind, iter, count = 0;
  switch (edge) {
  case EdgeType::LEFT:
    if (left) {
      ind = coord.ind(0,1);
      iter = coord.S;
      count = coord.wlky-2;
      left = false;
    }
    break;
  case EdgeType::RIGHT:
    if (right) {
      ind = coord.ind(coord.wlkx-1, 1);
      iter = coord.S;
      count = coord.wlky-2;
      right = false;
    }
    break;
  case EdgeType::TOP:
    if (top) {
      ind = coord.ind(1, 0);
      iter = coord.E;
      count = coord.wlkx-2;
      top = false;
    }
    break;
  case EdgeType::BOTTOM:
    if (bottom) {
      ind = coord.ind(1, coord.wlky-1);
      iter = coord.E;
      count = coord.wlkx-2;
      bottom = false;
    }
    break;
  }
  // change dame moves on the edge to neutral
  for (; count!=0; --count) {
    if (ind != p) changeMove(ind, PossibleMovesConsts::NEUTRAL);
    ind += iter;
  }
}

void
PossibleMoves::generate()
{
  lists[0].reserve(coord.last);
  lists[1].reserve(coord.last);
  lists[2].reserve(coord.last);
  mtype = std::vector<pti>(coord.getSize(), PossibleMovesConsts::REMOVED);
  for (int i=coord.first; i<=coord.last; ++i) {
    if (coord.dist[i] > 0) {
      mtype[i] = PossibleMovesConsts::NEUTRAL | lists[PossibleMovesConsts::LIST_NEUTRAL].size();
      lists[PossibleMovesConsts::LIST_NEUTRAL].push_back(i);
    }
    else if (coord.dist[i] == 0) {
      mtype[i] = PossibleMovesConsts::DAME | lists[PossibleMovesConsts::LIST_DAME].size();
      lists[PossibleMovesConsts::LIST_DAME].push_back(i);
    }
  }
  left = true;  top = true;  right = true;  bottom = true;
}

/// new_type should be NEUTRAL, DAME, TERRM or REMOVED
void
PossibleMoves::changeMove(pti p, int new_type)
{
  if (new_type != PossibleMovesConsts::REMOVED) {
    auto curr_type = mtype[p] & PossibleMovesConsts::TYPE_MASK;
    if (curr_type == new_type) return;
    removeFromList(p);
    mtype[p] = lists[new_type >> PossibleMovesConsts::MASK_SHIFT].size() | new_type;
    lists[new_type >> PossibleMovesConsts::MASK_SHIFT].push_back(p);
  } else {
    removeFromList(p);
    mtype[p] = PossibleMovesConsts::REMOVED;
    // check where is the new dot
    int x = coord.x[p], y = coord.y[p];
    if (x == 1 || (x == 0 && y !=0 && y != coord.wlky-1)) {
      newDotOnEdge(p, EdgeType::LEFT);
    } else if (x == coord.wlkx-2 || (x == coord.wlkx-1 && y !=0 && y != coord.wlky-1)) {
      newDotOnEdge(p, EdgeType::RIGHT);
    }
    if (y == 1 || (y == 0 && x !=0 && x != coord.wlkx-1)) {
      newDotOnEdge(p, EdgeType::TOP);
    } else if (y == coord.wlky-2 || (y == coord.wlky-1 && x !=0 && x != coord.wlkx-1)) {
      newDotOnEdge(p, EdgeType::BOTTOM);
    }
  }
}


/********************************************************************************************************
  InterestingMoves -- class for keeping track of 3 lists of interesting moves, very similar to PossibleMoves
*********************************************************************************************************/
/// This function removes move p from 'lists', but does not update mtype[p].
void
InterestingMoves::removeFromList(pti p)
{
  int curr_list = mtype[p] >> InterestingMovesConsts::MASK_SHIFT;
  if (curr_list < 3) {
    unsigned int ind = (mtype[p] & InterestingMovesConsts::INDEX_MASK);
    if (ind+1 < lists[curr_list].size()) {
      // p is not the last element on the curr_list, so replace it by the last one
      pti last = lists[curr_list].back();
      lists[curr_list][ind] = last;
      mtype[last] = (curr_list << InterestingMovesConsts::MASK_SHIFT) | ind;
    }
    lists[curr_list].pop_back();
  }
}

void
InterestingMoves::generate()
{
  lists[0].reserve(coord.last);
  lists[1].reserve(coord.last);
  lists[2].reserve(coord.last);
  mtype = std::vector<pti>(coord.getSize(), InterestingMovesConsts::REMOVED);
}

/// new_type should be MOVE_0, MOVE_1, MOVE_2 or REMOVED
void
InterestingMoves::changeMove(pti p, int new_type)
{
  assert(new_type == InterestingMovesConsts::MOVE_0 || new_type == InterestingMovesConsts::MOVE_1
	 || new_type == InterestingMovesConsts::MOVE_2 || new_type == InterestingMovesConsts::REMOVED);
  assert(p>=coord.first && p<=coord.last);
  if (new_type != InterestingMovesConsts::REMOVED) {
    auto curr_type = mtype[p] & InterestingMovesConsts::TYPE_MASK;
    if (curr_type == new_type) return;
    removeFromList(p);
    mtype[p] = lists[new_type >> InterestingMovesConsts::MASK_SHIFT].size() | new_type;
    lists[new_type >> InterestingMovesConsts::MASK_SHIFT].push_back(p);
  } else {
    removeFromList(p);
    mtype[p] = InterestingMovesConsts::REMOVED;
  }
}

int
InterestingMoves::classOfMove(pti p) const
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

void
Game::initWorm()
{
  // set outside points to 3 in worm[]
  for (int i=0; i<coord.getSize(); i++) {
    if (coord.dist[i] < 0) worm[i] = 3;
  }
}

Game::Game(SgfSequence seq, int max_moves)
{
  assert(Pattern3extra::MASK_DOT == MASK_DOT);
  global::komi = 0;
  global::komi_ratchet = 10000;
  auto sz_pos = seq[0].findProp("SZ");
  std::string sz = (sz_pos != seq[0].props.end()) ? sz_pos->second[0] : "";
  if (sz.find(':') == std::string::npos) {
    if (sz != "") {
      int x = stoi(sz);
      coord.changeSize(x, x);
    }
  } else {
    std::string::size_type i = sz.find(':');
    int x = stoi(sz.substr(0, i));
    int y = stoi(sz.substr(i+1));
    coord.changeSize(x, y);
  }
#ifdef DEBUG_SGF
  sgf_tree.changeBoardSize(coord.wlkx, coord.wlky);
#endif
  // reserve memory
  worm = std::vector<pti>(coord.getSize(), 0);
  nextDot = std::vector<pti>(coord.getSize(), 0);
  recalculate_list.reserve(coord.wlkx * coord.wlky);
  pattern3_value[0] = std::vector<pattern3_val>(coord.getSize(), 0);
  pattern3_value[1] = std::vector<pattern3_val>(coord.getSize(), 0);
  pattern3_at = std::vector<pattern3_t>(coord.getSize(), 0);
  descr = std::map<pti, WormDescr>();
  connects[0] = std::vector<OneConnection>(coord.getSize(), OneConnection());
  connects[1] = std::vector<OneConnection>(coord.getSize(), OneConnection());
  threats[0] = AllThreats();
  threats[1] = AllThreats();
  lastWormNo[0]=1; lastWormNo[1]=2;
  initWorm();
  history.push_back(0);
  history.push_back(0);
  possible_moves.generate();
  interesting_moves.generate();

  // prepare patterns, taken from Pachi (playout/moggy.c)
  // (these may be pre-calculated)
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
        "x.?",  "32",  // TODO: empty triange for O possible, when upper ? == O
	// hane pattern - thin hane 		// 0.22
	"XOO"
	".H."
	"?.?" "X",   "22",
	// generic pattern - katatsuke or diagonal attachment; similar to magari 	// 0.37
	".Q."
	"YH."
	"...",  "37",   // TODO: is it good? seems like Q should be in corner for the diagonal attachment
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
  // global::patt3.showCode();  <-- to precalculate


  global::patt3_symm.generate({
      // hane pattern - enclosing hane
      "XOX"
        ".H."
        "?o?",  "6",   // note: ?O? is contained in other part
        // keima cut
	"?XO"
	"OHX"
	"???", "5",
	// 'keima' cut with two dots O on E and W
	"?XO"
	"OHO"
	"...", "6",
	//
	"?XO"
	"OHO"
	"O..", "4",
	//
	"?XO"
	"OHO"
	"..O", "4",
	//
	"?XO"
	"OHO"
	".O.", "4",
	//
	"?X?"
	"OHO"
	"?X?", "7",  // more general then just keima cut
	//
	"?X?"
	"OHO"
	"X??", "7",  // more general then just keima cut; contains "??X" by symmetry
	// keima/one space jump on the edge
	"?XO"
	"OHx"
	"###", "6",
	//
	"?X?"
	"OHO"
	"###", "6",
	// keima cut with empty at E
	"?XO"
	"OH."
	"...", "5",
	//
	"?XO"
	"OH."
	"X.?", "7",
	//
	"?XO"
	"OH."
	"?XX", "5",
	//
	"?XO"
	"OH."
	"?XO", "7",
	//
	"?XO"
	"OH."
	"?X.", "6",
	//
	"?XO"
	"OH."
	"XO?", "6",
	//
	"?XO"
	"OH."
	".OX", "2",   // ???
	// bambus moves ... -- TODO
	//
	// cut between diagonal jump
	"?.O"
	".H."
	"O.?", "5",
	//
	"?XO"
	".H."
	"O.?", "6",  // maybe too much?
	//
	"?XO"
	"XH."
	"O.O", "4",
	// two keimas
	"X.O"
	"OHX"
	"???", "2",
	// cut -- warning: contains some previous shapes
	"oOo"
	".H."
	"oOo", "4",
	//
	"?Oo"
	"XH."
	"?O?", "6",   // 'o' in the corner to rule out bambus
	//
	"?O?"
	"XHX"
	"?O?", "7",
	//
	"?Oo"
	"XHO"
	"?Oo", "5",
	// diagonal cut (contains some previous shapes)
	"XO?"
	"OH?"
	"???", "3"
	});


  global::patt3_cost.generate({
      // our bambus
      "X.X"
        "XHX"
        "???" "X", "0",
        //
	"X.."
	"XHX"
	"?.?" "X", "2",
	//
	"Ox?"
	"xH?"
	"???" "X", "40",
	//
	"O??"
	"xH?"
	"?x?" "X", "40",
	//
	"O??"
	"?Hx"
	"?x?" "X", "40",
	//
	"?Q?"
	"xH?"
	"???" "X", "100",
	// attacker's bambus
	"O.O"
	"OHO"
	"???" "X", "10000",
	//
	"O.."
	"OHO"
	"?.?" "X", "2000",
	// keima
	"O.?"
	"?HQ"
	"???" "X", "500",
	//
	"OX?"
	"xHQ"
	"?x?" "X", "270",
	// one-point jump
	"?.?"
	"QHQ"
	"?.?" "X", "600",
	//
	"?.?"
	"QHQ"
	"?X?" "X", "400",
	//
	"?.?"
	"QHQ"
	"?Q?" "X", "700",  // may seem unnecessary, X cannot go anywhere from the centre, but it helps to count treesize correctly
	// diagonal
	"O.."
	".H."
	"..O" "X", "250",
	//
	"O.."
	"XH."
	"??O" "X", "200",
	//
	"OX?"
	"XHx"
	"??O" "X", "10",
	//
	"Oxx"
	"XHX"
	"??O" "X", "10",
	//
	},
    Pattern3::TYPE_MAX);


  global::patt52_inner.generate({
        // locally bad moves (WARNING: they may be actually good, if there are X above)
        "?OH.O"
	"?x..x" "X", "-0.1",
	//
        "?OHO?"
	"?x.x?" "X", "-0.1",
	//
        "O.H.O"
	"x...x" "X", "-0.1",
        // locally good moves (usually reductions)
        "O.H.."
	"x....", "0.02",   // risky
	//
        "O.H.X"
	"x...o", "0.1",
	//
        "?OH.X"
	"?x..o" "X", "0.1",
	// connecting moves (usually anti-reductions)
	"o.H.o"
	"X.O.X", "0.5",
	//
	"?oH.o"
	"?XO.X", "0.5",
	//
	"?oHo?"
	"?XOX?", "0.5",
         //
	"O.H.o"
	"X.O.X", "0.1",
	//
	"?oH.O"
	"?XO.X", "0.1",
	//
	"?OHo?"
	"?XOX?", "0.02",
	// another connecting moves
	"X.H.?"
	"O..x?", "0.1" ,
	//
	"X.H.?"
	"o..x?" "X", "0.3",
	//
	"?XHX?"
	"??O??", "0.3",
	//
	"?XH.X"
	"??O??", "0.3",
	//
	"X.H.X"
	"?.O.?", "0.4",
	//
	"X.H.X"
	"?XO??", "0.4",
	//
	"?OHXO"
	"??..?" "X", "0.7"
	});

  //  global::patt52_inner.showCode(); // <-- to precalculate
  global::patt52_edge.generate({
        // locally good moves (usually reductions)
        "?X.X?"
	"?xHx?", "0.2",
	//
        "?X..X"
	"??H.?", "0.2",
	//
        "X...X"
	"?.H.?", "0.2",
	// connecting moves
	"X.O.X"
	"?.H.?", "0.6",
	//
	"?XO.X"
	"??H.?" "X", "0.6",
	//
	"?XOX?"
	"??H??", "0.6"
	//
	});
  //  global::patt52_edge.showCode(); // <-- to precalculate

  global::patt3_extra.generate({
      // bamboo -- first move
      "Y.."
	"Y.."
	"***" "NN* EH", "2",
	// bamboo -- second move
	".Y."
	"..."
	"?.x" "EEY NEEY NN* H", "2",
	//
	".Y."
	"..."
	"?O?" "EEY NEEY NN* H", "2",
	//
	"OX."
	"X.."
	"?x?" "EEY NEEY NN* H", "2",
	//
	".Y."
	"O.."
	"?.?" "EEY NEEY NN* H", "2",
	//
	"OY."
	"O.."
	"?.?" "EEY NEEY NN* H", "1",
	//
	"OY."
	"..."
	"?.?" "EEY NEEY NN* H", "2",
	// longer bamboo
	"Y.."
	"Y.."
	"Y.." "EE. EEH", "2",
	// net
	".x?"    // ".?? ..x"  not needed by symmetry
	"..?"
	"Q.." "WWY SSY H", "3",
	// anti-net
	".o?"
	"..?"
	"Y.." "WWQ SSQ H", "3",
	// longer net without support at SW
	"..?"
	"..."
	"xQ." "WWY SEE. SSEY EH", "2",
	// ...and with support at SW
	"..?"
	"..."
	"YQ." "WWY SEE. SSEY EH", "3",
	// anti-nets
	"..?"
	"..."
	"oY." "WWQ SEE. SSEQ EH", "2",
	// ...and anti-net with support at SW
	"..?"
	"..."
	"QY." "WWQ SEE. SSEQ EH", "3",
	// net with keima
	"Y.?"
	"?.."
	"?Q." "SSEY SEE. EH", "3",
	// anti-net with keima
	"Q.?"
	"?.."
	"?Y." "SSEQ SEE. EH", "3",
	// net with double keima
	"Y.x"
	"?.."
	"?Q." "EEx SEE. SSEEY EH", "3",
	// anti-net with double keima
	"Q.o"
	"?.."
	"?Y." "EEo SEE. SSEEQ EH", "3",
	// anti-net thanks to atari
	"?oo"
	"o.Q"
	"oQY" "SSEY EEY SSW. SS. H", "3",
	// the same with different configuration at N/W
	"?Qo"
	"Y.Q"
	"oQY" "SSEY EEY SSW. SS. H", "3",
	// the same with yet another configuration at N/W
	"?Yo"
	"Q.Q"
	"oQY" "SSEY EEY SSW. SS. H", "3",
	// keima
	"?.."
	"Y.."
	"?O?" "EE* NEH", "2",
	// 2-point extension
	"..."
	"..?"
	"###" "NWWY EE* NEH", "1",
	// 1-point jump
	"..."
	"Y.."
	"x.." "EH", "1",
	// 2-point jump
	"..."
	"Y.."
	"??." "SWWY WWo EE. EEH", "1",  // WWx -> WWo in v136d
	// diagonal
	"..."
	"..."
	"X.." "SSEx NWWx H", "1",
	// 2LM: second line moves -- moved here from patt52_inner
	// (2LM) connecting moves (usually anti-reductions)
	"???"
	"..."
	".Q." "SS# WWo SWWY EEo SEEY H", "2",  // maybe it's not worth the effort...
	//
	"???"
	"o.."
	"YQ." "SS# EEo SEEY H", "2",
	//
	// "???" "o.o"	"YQY" "SS# H", "2",  -- this seems redundant, it's usual cut
	// (2LM) another connecting moves
	"?x?"
	"Y.."
	"?Q?" "EEY H", "2",  // could be SS#, but in fact it seems OK also apart from the edge
	// now 5 similar with little different first line from above
	"Yx?"
	"..."
	".Q." "SS# WWY EEY H", "2",
	//
	"Q.o"
	"..."
	".Q." "SS# WWY EEY H", "2",
	//
	"..."
	"..."
	".Q." "SS# WWY EEY H", "2",
	//
	".Q."
	"..."
	".Q." "SS# WWY EEY H", "1",
	//
	"QYQ"
	"..."
	".Q." "SS# WWY EEY H", "1",
	// another one
	// "???"   // should be more precise here
	// "..."
	// "YQ." "SS# WWY EEY H", "2",
	//
	"Q.Y"
	"?.."
	"###" "NEEQ NH", "4",  // defend Y at NE
	//
	"Y.Q"
	"?.."
	"###" "NEEY EH", "4",  // attack Q at NE
	// border moves:
        // locally good moves (usually reductions)
	"Y.Y"
	"x.x"
	"###" "H", "2",
	//
	"Y.."
	"?.."
	"###" "NEEY H", "2",
	//
	"..."
	"..."
	"###" "NWWY NEEY H", "2",
	// connecting edge moves
	".Q."
	"..."
	"###" "NWWY NEEY H", "4",
	//
	"YQ."
	"?.."
	"###"  "NEEY H", "4",
	//
	"YQY"
	"?.?"
	"###"  "H", "4"  // this could be a usual 3x3 pattern!
	});

  for (int i=coord.first; i<=coord.last; ++i) {
    if (coord.dist[i] == 0) {
      pattern3recalculatePoint(i);
    }
  }
  // process RU, AB, AW properties
  // (...)
  // make moves
  start_time = std::chrono::high_resolution_clock::now();
  nowMoves = 1;
  replaySgfSequence(seq, max_moves);
}


void
Game::replaySgfSequence(SgfSequence seq, int max_moves)
{
  for (auto &node : seq) {
    if (node.findProp("B") != node.props.end()) {
      if (--max_moves < 0) break;
      for (auto &value : node.findProp("B")->second) {
	if (value.empty()) continue;   // Program kropki gives empty moves sometimes.
	makeSgfMove(value, 1);
      }
      nowMoves = 2;
    }
    if (node.findProp("W") != node.props.end()) {
      if (--max_moves < 0) break;
      for (auto &value : node.findProp("W")->second) {
	if (value.empty()) continue;   // Program kropki gives empty moves sometimes.
	makeSgfMove(value, 2);
      }
      nowMoves = 1;
    }
    if (node.findProp("AB") != node.props.end()) {
      for (auto &value : node.findProp("AB")->second) {
	makeSgfMove(value, 1);
      }
    }
    if (node.findProp("AW") != node.props.end()) {
      for (auto &value : node.findProp("AW")->second) {
	makeSgfMove(value, 2);
      }
    }
  }
}

std::vector<pti>
Game::findThreats_preDot(pti ind, int who)
// find possible new threats because of the (future) dot of who at [ind]
// Each possible threat is a pair of pti's, first denote a point where to play,
// second gives a code which neighbours may go inside.
{
  std::vector<pti> possible_threats;
  Threat *smallest_terr = nullptr;
  unsigned int smallest_size = coord.maxSize;
  // find smallest territory that [ind] is (if any), return immediately if we are inside an 1-point-territory pool
  // (so we cannot check for ind==thr.where here, because of this side-effect!)
  if (threats[who-1].is_in_terr[ind]) {
    int nthr = threats[who-1].is_in_terr[ind];
    for (auto &thr : threats[who-1].threats) {
      if (thr.type & ThreatConsts::TERR) {
	if (thr.encl->isInInterior(ind)) {
	  if (thr.encl->interior.size() == 1) {
	    thr.type |= ThreatConsts::TO_REMOVE;
	    return possible_threats;
	  }
	  if (thr.encl->interior.size() < smallest_size) {
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
  std::array<pti, 4> groups = connects[who-1][ind].groups_id;
  int top = connects[who-1][ind].count();
  if (top == 0) {  // isolated dot cannot pose any threats
    return possible_threats;
  }
  // if inside TERR, then we want to know whether we make a new connection between the border, so that the TERR splits into 2 or more smaller
  if (smallest_terr) {
    auto border_group = descr.at(worm[smallest_terr->encl->getBorderElement()]).group_id;
    int count = 0;
    for (int g=0; g<top; g++)
      count += (groups[g] == border_group);
    if (count >= 2) {
      int encl_count = 0;
      for (auto &thr : threats[who-1].threats) {
	if (thr.where == ind) encl_count++;
      }
      if (encl_count == count) {
	smallest_terr->type |= ThreatConsts::TO_REMOVE;  // territory will split into 2+ new ones
      } else {
	// in this case the territory does not split
	// example situation, where O = last dot, the large territory of 'o' is not divided into two, although there is a new smaller one inside
	//   o o o   o o o
	// o       o       o
	// o x     o       o
	// o     o x o     o
	// o       O       o
	//   o           o
	//     o o o o o
	// Hence we leave the threat unchanged.
	/* debug:
	   std::cerr << "count = " << count << ", encl_count = " << encl_count << std::endl;
	   std::cerr << "last move [" << nowMoves << "] at " << coord.showPt(history.back() & ~HISTORY_TERR) << std::endl;
	   show();
	   assert(encl_count == count);
	*/
      }
    }
    else if (count==1) { // && dots>1)  we could check if there's more than 1 our dot in the neighbourhood, as only then we need to check
      // maybe it's just one dot, then interior will be the same
      int gdots=0;
      for (int j=0; j<8; j++) {
	pti nb = ind + coord.nb8[j];
	if (isDotAt(nb) && descr.at(worm[nb]).group_id == border_group) gdots++;
      }
      if (gdots>1) smallest_terr->type |= ThreatConsts::TO_CHECK; // territory may (will?) have now smaller area
    }
  }
  // check our enclosures containing [ind]
  if (threats[who-1].is_in_encl[ind] || threats[who-1].is_in_border[ind]) {
    std::vector<Threat*> this_encl;
    for (auto &thr : threats[who-1].threats) {
      if (thr.type & ThreatConsts::ENCL) {
	if (thr.where == ind) {
	  thr.type ^= (ThreatConsts::ENCL | ThreatConsts::TERR);   // switch ENCL bit to TERR
	  thr.where = 0;
	  threats[who-1].changeEnclToTerr(thr);
	  this_encl.push_back(&thr);
	  // thr.zobrist_key does not change
	} else if (thr.encl->isInInterior(ind)) {
	  if (thr.encl->interior.size() == 1) {
	    thr.type |= ThreatConsts::TO_REMOVE;
	  } else {
	    thr.type |= ThreatConsts::TO_CHECK;
	  }
	}
      }
    }
    // check enclosures inside our new territory
    if (!this_encl.empty()) {
      std::vector<int8_t> this_interior(coord.last+1, 0);
      for (auto tt : this_encl)
	for (auto i : tt->encl->interior) this_interior[i] = 1;
      for (auto &thr : threats[who-1].threats) {
	if ((thr.type & ThreatConsts::ENCL) && this_interior[thr.where]) {
	  thr.type |= ThreatConsts::TO_CHECK;
	  // TODO: we could remove thr if it is almost the same (one of) current enclosure(s)  [almost: thr.interior + [ind] == curr_encl.interior ]
	}
      }
    }
  }
  // find other points which touch points from found groups, but do not touch the new dot
  if (top >=2) {

    auto debug_time = std::chrono::high_resolution_clock::now();

    std::array<pti, 4> unique_groups = {0,0,0,0};
    int ug = connects[who-1][ind].getUniqueGroups(unique_groups);
    if (ug >= 2) {
      for (int i=coord.first; i<=coord.last; i++) {
	if (connects[who-1][i].groups_id[1] != 0          // this point connects at least 2 groups
	    && !coord.isInNeighbourhood(i, ind))    // and does not touch [ind]
	  {
	    // check groups
	    int count = 0;
	    for (int k=0; k<ug; k++) {
	      for (int j=0; j<4; j++)
		if (connects[who-1][i].groups_id[j] == unique_groups[k]) {
		  count++;
		  break;
		}
	    }
	    if (count >= 2) {
	      possible_threats.push_back(i);
	      int w = 0;  // find the code for the neighbourhood
	      for (int j=7; j>=0; j--) {
		pti nb2 = i + coord.nb8[j];
		if (whoseDotMarginAt(nb2) == who &&
		    std::find(unique_groups.begin(), unique_groups.end(), descr.at(worm[nb2]).group_id) != unique_groups.end()) {
		  w |= 1;
		} else if (whoseDotMarginAt(nb2) == 3) {
		  w |= 3;   // outside the board
		}
		w <<= 2;
	      }
	      w >>= 2;
	      possible_threats.push_back(coord.connections_tab[w]);
	    }
	  }
      }
    }

    debug_nanos += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - debug_time).count();

  }
  // check neighbours of [ind]
  for (int i=0; i<8; i++) {
    pti nb = ind + coord.nb8[i];
    if (connects[who-1][nb].groups_id[0] != 0) {   // there is some dot of who, and we're inside the board
      int w = 0;  // find the code for the neighbourhood
      for (int j=7; j>=0; j--) {
	pti nb2 = nb + coord.nb8[j];
	if ((nb2==ind) || (whoseDotMarginAt(nb2) == who && std::find(groups.begin(), groups.end(), descr.at(worm[nb2]).group_id) != groups.end())) {
	  w |= 1;
	} else if (whoseDotMarginAt(nb2) == 3) {
	  w |= 3;  // outside the board
	}
	w <<= 2;
      }
      w >>= 2;
      if (coord.connections_tab[w]) {
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
/// @return    A pair [left, right) -- we should iterate over j there and consider nb+coord.nb8[j&7]
std::array<int,2>
Game::findThreats2moves_preDot__getRange(pti ind, pti nb, int i, int who) const {
  assert(coord.nb8[0] == coord.NE && coord.nb8[7] == coord.N);
  int shortcutL = (whoseDotMarginAt(ind+coord.nb8[i+7]) == who);   // (i+7)&7
  int shortcutR = (whoseDotMarginAt(ind+coord.nb8[i+1]) == who);   // (i+1)&7
  std::array<int,2> range;
  if ((i&1) == 0) {
    // diagonal connection between [ind] and [nb]
    if (shortcutL) {
      // check also longer (2-dot) shortcut
      range[0] = (whoseDotMarginAt(nb + coord.nb8[i+7]) == who) ? (i+10) : (i+8);
    } else {
      range[0] = i+6;
    }
    if (shortcutR) {
      // check also longer (2-dot) shortcut
      range[1] = (whoseDotMarginAt(nb + coord.nb8[i+1]) == who) ? (i+7) : (i+9);
    } else {
      range[1] = i+11;
    }
  } else {
    // horizontal or vertical connection between [ind] and [nb]
    if ((shortcutL || shortcutR) && (whoseDotMarginAt(nb + coord.nb8[i]) == who)) {
      // there is a 2-dot shortcut, take empty range
      range[0] = 0;
      range[1] = 0;
    } else {
      range[0] = shortcutL ? (i+9) : (i+7);
      range[1] = shortcutR ? (i+8) : (i+10);
    }
  }
  return range;
}

/// Finds up to 4 direct neighbours of [ind] which are closable, i.e.,
/// empty or with opponent dot, and not on the edge of the board
/// @param[in] forbidden1, forbidden2 Typically points at which who will play, hence we do not want to put it into
///                                   the array of closable neighbours. It may be e.g. 0 if there are no such points.
/// @return  Array with [0]=neighbours count, [1],... -- neighbours
std::array<int,5>
Game::findClosableNeighbours(pti ind, pti forbidden1, pti forbidden2, int who) const
{
  std::array<int,5> res;
  res[0] = 0;
  bool prev_ok = false;
  bool first_ok = false;
  for (int i=0; i<4; i++) {
    int nb = ind + coord.nb4[i];
    if (whoseDotMarginAt(nb) != who &&
	nb != forbidden1 && nb != forbidden2) {
      if (coord.dist[nb] >= 1) {
	// check if nb is connected with previous nb
	if (i>0) {
	  int connecting = nb + coord.nb4[i-1];
	  if (prev_ok &&
	      whoseDotMarginAt(connecting) != who &&
	      connecting != forbidden1 && connecting != forbidden2) {
	    continue;
	  }
	  if (i==3) {
	    int connecting = nb + coord.nb4[0];
	    if (first_ok &&
		whoseDotMarginAt(connecting) != who &&
		connecting != forbidden1 && connecting != forbidden2) {
	      continue;
	    }
	  }
	}
	res[++res[0]] = nb;
      }
      if (i==0) first_ok = true;
      prev_ok = true;
    } else prev_ok = false;
  }
  return res;
}

/// 3 points: p0, p1, p2 should be on the boundary of an enclosure, so check them and
/// take the optimal one.
/// TODO: use connections_tab, like in usual Threats
void
Game::addClosableNeighbours(std::vector<pti> &tab, pti p0, pti p1, pti p2, int who) const
{
  auto cn0 = findClosableNeighbours(p0, p1, p2, who);
  auto cn1 = findClosableNeighbours(p1, p0, p2, who);
  auto cn2 = findClosableNeighbours(p2, p0, p1, who);
  if (cn0[0] <= cn1[0] && cn0[0] <= cn2[0]) {
    for (int i=0; i<cn0[0]; i++) tab.push_back(cn0[i+1]);
    tab.push_back(cn0[0]);
  } else if (cn1[0] <= cn2[0]) {
    for (int i=0; i<cn1[0]; i++) tab.push_back(cn1[i+1]);
    tab.push_back(cn1[0]);
  } else {
    for (int i=0; i<cn2[0]; i++) tab.push_back(cn2[i+1]);
    tab.push_back(cn2[0]);
  }
}


/// Checks if empty points p1, p2 are connected, i.e., if they both have the same group of who as a neighbour.
bool
Game::haveConnection(pti p1, pti p2, int who) const
{
  std::array<pti, 4> unique_groups1 = {0,0,0,0};
  int ug1 = connects[who-1][p1].getUniqueGroups(unique_groups1);
  if (ug1 == 0) return false;
  std::array<pti, 4> unique_groups2 = {0,0,0,0};
  int ug2 = connects[who-1][p2].getUniqueGroups(unique_groups2);
  if (ug2 == 0) return false;
  for (int i=0; i<ug1; i++) {
    for (int j=0; j<ug2; j++)
      if (unique_groups1[i] == unique_groups2[j]) return true;
  }
  return false;
}

std::vector<pti>
Game::findThreats2moves_preDot(pti ind, int who)
// find possible new threats in 2 moves because of the (future) dot of who at [ind]
// Each possible threat consists of:
//  points (possibly) to enclose,
//  number of these points to enclose,
//  a pair of pti's, denoting the points to play.
// (The order is so that we may then pop_back two points, count and points to enclose).
// At the end we put the pairs:
//   (empty neighbour, group_id that this neigbhour touches)
// and at the end the number of such pairs.
// However, if this number is 0 and there are no threats, we do not put this to keep possible_threats
// empty.
{
  std::vector<pti> possible_threats;
  // find groups in the neighbourhood
  std::array<pti, 4> groups = connects[who-1][ind].groups_id;
  int top = connects[who-1][ind].count();
  if (top == 4)
    return possible_threats;
  SmallMultiset<pti, 4> connected_groups;
  if (top>=1) {
    for (int i=0; i<4; i++) {
      pti g = groups[i];
      if (g==0) break;
      connected_groups.insert(g);
    }
  }
  possible_threats.reserve(64);
  if (top<=2) {
    // here it's possible to put 2 dots in the neighbourhood of [ind] to make an enclosure
    for (int i=0; i<=4; i++) {
      pti nb = ind + coord.nb8[i];
      if (whoseDotMarginAt(nb) == 0) {
	pti other_groups[4];
	int count = 0;
	for (int c=0; c<4; c++) {
	  auto g = connects[who-1][nb].groups_id[c];
	  if (g==0) break;
	  if (!connected_groups.contains(g)) {
	    other_groups[count++] = g;
	  }
	}
	if (count) {
	  int start_ind, stop_ind;
	  if ((i & 1)==0) {  // diagonal connection
	    start_ind = i+2;
	    stop_ind = std::min(i+6, 7);
	  } else {
	    start_ind = i+3;
	    stop_ind = std::min(i+5, 7);
	  }
	  for (int j=start_ind; j<=stop_ind; ++j) {
	    pti nb2 = ind + coord.nb8[j];
	    if (whoseDotMarginAt(nb2) == 0) {
	      for (int c=0; c<4; c++) {
		auto g = connects[who-1][nb2].groups_id[c];
		if (g==0) break;
		// we found place nb2, which has group g as a neighbour, check if g is in other_groups
		for (int n=0; n<count; ++n)
		  if (other_groups[n] == g) {
		    addClosableNeighbours(possible_threats, ind, nb, nb2, who);
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
  if (top>=1 && top<=3) {

    auto debug_time = std::chrono::high_resolution_clock::now();

    //std::cerr << "ind = " << coord.showPt(ind) << std::endl;
    for (int i=0; i<8; i++) {
      pti nb = ind + coord.nb8[i];
      if (whoseDotMarginAt(nb) != 0) continue;  // dot or outside
      auto p2 = findThreats2moves_preDot__getRange(ind, nb, i, who);
      // check if nb and ind have a common neighbour, for later use (could be lazy evaluated, but does not seem that slow to do it in this way...)
      bool are_ind_nb_connected = false;
      auto connected_only = connected_groups;
      if ((i&1) == 0) {  // diagonal connection between ind and nb
	auto tnb = ind + coord.nb8[i+1];   // (i+1)&7
	if (whoseDotMarginAt(tnb) == who) {
	  connected_only.remove_one( descr.at(worm[tnb]).group_id );
	} else {
	  tnb = ind + coord.nb8[i+7];   // (i+7)&7
	  if (whoseDotMarginAt(tnb) == who) {
	    connected_only.remove_one( descr.at(worm[tnb]).group_id );
	  }
	}
      } else { // horizontal or vertical connection between ind and nb
	// first pair of neighbours
	auto tnb = ind + coord.nb8[i+1];   // (i+1)&7
	if (whoseDotMarginAt(tnb) == who) {
	  connected_only.remove_one( descr.at(worm[tnb]).group_id );
	} else {
	  tnb = ind + coord.nb8[i+2];   // (i+2)&7
	  if (whoseDotMarginAt(tnb) == who) {
	    connected_only.remove_one( descr.at(worm[tnb]).group_id );
	  }
	}
	// second pair of neighbours
	tnb = ind + coord.nb8[i+7];   // (i+7)&7
	if (whoseDotMarginAt(tnb) == who) {
	  connected_only.remove_one( descr.at(worm[tnb]).group_id );
	} else {
	  tnb = ind + coord.nb8[i+6];   // (i+6)&7
	  if (whoseDotMarginAt(tnb) == who) {
	    connected_only.remove_one( descr.at(worm[tnb]).group_id );
	  }
	}
      }
      if ( connected_only.empty() ) {
	continue;   // there will be no new threats, because [ind] would be redundant for a threat with [nb]
      }
      for (int gn=0; gn<4; gn++) {
	pti g = connects[who-1][nb].groups_id[gn];
	if (g==0) break;
	if (connected_groups.contains(g)) {
	  are_ind_nb_connected = true;
	  break;
	}
      }
      for (int j=p2[0]; j<p2[1]; j++) {
	pti nb2 = nb + coord.nb8[j];  // [j & 7];
	if (whoseDotMarginAt(nb2) != 0) continue;  // dot or outside
	auto p3 = findThreats2moves_preDot__getRange(nb, nb2, j&7, who);
	//std::cerr << "  nb2 = " << coord.showPt(nb2) << ", range = [" << p3[0] << ", " << p3[1] << "]" << std::endl;
	for (int k=p3[0]; k<p3[1]; k++) {
	  pti nb3 = nb2 + coord.nb8[k];  // [k & 7];
	  //std::cerr << "    nb3 = " << coord.showPt(nb3) << std::endl;
	  if (whoseDotMarginAt(nb3)==who && connected_groups.contains( descr.at(worm[nb3]).group_id )) {
	    /*
	    // dla pokazania ustaw kropki na ind, nb, nb2
	    CleanupOneVar<pti> worm_where_cleanup0(&worm[ind], who+4);
	    CleanupOneVar<pti> worm_where_cleanup(&worm[nb], who);
	    CleanupOneVar<pti> worm_where_cleanup2(&worm[nb2], who);
	    show();
	    std::cerr << "dodaje (ind=" << coord.showPt(ind) << "): " << coord.showPt(nb) << ", " << coord.showPt(nb2) << " --> " << coord.showPt(nb3) << std::endl;
	    std::cin.ignore();
	    */
	    // now add closable neighbours, as few as possible
	    if (!are_ind_nb_connected) {   // if (!haveConnection(ind, nb, who)) {
	      int count = 0;
	      if (coord.dist[ind + coord.nb8[i+7]] >= 1) {
		possible_threats.push_back(ind + coord.nb8[i+7]);
		count++;
	      }
	      if (coord.dist[ind + coord.nb8[i+1]] >= 1) {
		possible_threats.push_back(ind + coord.nb8[i+1]);
		count++;
	      }
	      possible_threats.push_back(count);
	    } else if (!haveConnection(nb, nb2, who)) {
	      int count = 0;
	      if (coord.dist[nb + coord.nb8[j+7]] >= 1) {
		possible_threats.push_back(nb + coord.nb8[j+7]);
		count++;
	      }
	      if (coord.dist[nb + coord.nb8[j+1]] >= 1) {
		possible_threats.push_back(nb + coord.nb8[j+1]);
		count++;
	      }
	      possible_threats.push_back(count);
	    } else {
	      addClosableNeighbours(possible_threats, ind, nb, nb2, who);
	    }
	    possible_threats.push_back(nb);
	    possible_threats.push_back(nb2);
	    /*
	    if (no_thr_expexcted) {
	      // dla pokazania ustaw kropki na ind, nb, nb2
	      CleanupOneVar<pti> worm_where_cleanup0(&worm[ind], who+4);
	      CleanupOneVar<pti> worm_where_cleanup(&worm[nb], who);
	      CleanupOneVar<pti> worm_where_cleanup2(&worm[nb2], who);
	      show();
	      std::cerr << "Nieoczekiwanie dodaje (ind=" << coord.showPt(ind) << "): " << coord.showPt(nb) << ", " << coord.showPt(nb2) << " --> " << coord.showPt(nb3) << std::endl;
	      std::cin.ignore();
	    }
	    */
	    break;
	  }
	}
      }
    }

    debug_nanos3 += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - debug_time).count();

  }
  /*
  // (new part, v137)
  if (top >= 2) {
    // flood fill from exterior...
    std::array<pti, Coord::maxSize> queue;
    int queue_top = 0;
    int queue_tail = 0;
    // left and right edge
    {
      int lind = coord.ind(0, 0);
      int rind = coord.ind(coord.wlkx-1, 0);
      for (int j=0; j<coord.wlky; j++) {
	if (whoseDotMarginAt(lind) != who) {
	  queue[queue_tail++] = lind;
	}
	if (whoseDotMarginAt(rind) != who) {
	  queue[queue_tail++] = rind;
	}
	lind += coord.S;
	rind += coord.S;
      }
    }
    // top and bottom edge
    {
      int tind = coord.ind(0, 0);
      int bind = coord.ind(0, coord.wlky-1);
      for (int i=0; i<coord.wlkx; i++) {
	if (whoseDotMarginAt(tind) != who) {
	  queue[queue_tail++] = tind;
	}
	if (whoseDotMarginAt(bind) != who) {
	  queue[queue_tail++] = bind;
	}
	tind += coord.E;
	bind += coord.E;
      }
    }
    // now mark exterior
    while (queue_top < queue_tail) {
      pti p = queue[queue_top++];
      // visit the point p
      if (whoseDotMarginAt(p) == 0) {
	if (connects[who-1][p].groups_id[0] != 0){
	  // check if it's connected to one of 'connected_groups'
	  int connected = 0;
	  for (int g=0; g<4; ++g) {
	    auto this_g = connects[who-1][p].groups_id[g];
	    if (this_g == 0) break;
	    if (this_g != connected &&
		connected_groups.contains(this_g)) {
	      if (connected != 0) goto one_move_encl;
	      connected = this_g;
	    }
	  }
	}
      }

      // visit neighbours
      for (int j=0; j<4; j++) {
	pti nb = p + coord.nb4[j];
	if (coord.dist[nb] >= 0 && (tab[nb] & test_mask) == 0) {
	  tab[nb] |= mark_by;
	  stack[stackSize++] = nb;
	  count++;
	}
      }
    }

  }  // end of new part (v137)
  */

  // find empty points in the neighbourhood which touch some other group
  {
    int count = 0;   // number of pairs (group_id, neighbour)
    if (top > 0 && top < 4) {
      for (int i=0; i<8; i++) {
	pti nb = ind + coord.nb8[i];
	if (whoseDotMarginAt(nb) != 0 || connects[who-1][nb].groups_id[0] == 0) continue;
	std::array<pti, 4> unique_groups = {0,0,0,0};
	int ug = connects[who-1][nb].getUniqueGroups(unique_groups);
	// check if [nb] touches one of our groups
	for (int j=0; j<ug; j++) {
	  if (connected_groups.contains(unique_groups[j])) {
	    goto touches_group;
	  }
	}
	// we touch no group, so all should be saved
	for (int j=0; j<ug; j++) {
	  possible_threats.push_back(nb);
	  possible_threats.push_back(unique_groups[j]);
	  ++count;
	}
      touches_group:;
      }
    }
    if (!possible_threats.empty() || count) {
      possible_threats.push_back(count);
    }
  }
  return possible_threats;
}



void
Game::checkThreat_encl(Threat* thr, int who)
/// The function may add a threat, thus invalidating threat list and perhaps also 'thr' pointer.
/// @param[in] who  Whose threat it is (i.e., who makes this enclosure).
{
  thr->type |= ThreatConsts::TO_REMOVE;
  int where = thr->where;
#ifndef NDEBUG
  //std::cerr << "Zagrozenie do usuniecia: " << who << " w " << coord.showPt(where) << std::endl;
  //std::cerr << thr->show() << std::endl;
#endif
  if (worm[where] == 0) {
    CleanupOneVar<pti> worm_where_cleanup(&worm[where], who);   //  worm[where] = who;  with restored old value (0) by destructor
    int done[4] = {0,0,0,0};
    for (int j=0; j<4; j++)
      if (!done[j]) {
	pti nb = where + coord.nb4[j];
	if (whoseDotMarginAt(nb) != who && coord.dist[nb] >= 1) {
	  Threat t;
	  t.encl = std::make_shared<Enclosure>(findEnclosure(nb, MASK_DOT, who));
	  if (!t.encl->isEmpty() && !t.encl->isInInterior(where)) {
	    t.type = ThreatConsts::ENCL;
	    t.where = where;
	    auto zobr = t.zobrist_key = t.encl->zobristKey(who);
	    // if some next neighbour has been enclosed, mark it as done
	    for (int k=j+1; k<4; k++) {
	      if (t.encl->isInInterior(where + coord.nb4[k]))
		done[k]=1;
	    }
#ifndef NDEBUG
	    //std::cerr << "Zagrozenie mozliwe do dodania: " << who << " w " << coord.showPt(where) << std::endl;
#endif
	    Threat *thrfz = threats[who-1].findThreatZobrist(zobr);
	    if (thrfz == nullptr) {
	      auto tmp = countDotsTerrInEncl(*t.encl, 3-who);
	      t.opp_dots = std::get<0>(tmp);
	      t.terr_points = std::get<1>(tmp);
	      //std::tie<t.opp_dots, t.terr_points>
	      addThreat(std::move(t), who);
#ifndef NDEBUG
	      //std::cerr << "Zagrozenie dodane" << std::endl;
#endif
	    } else {
	      thrfz->type &= ~(ThreatConsts::TO_CHECK | ThreatConsts::TO_REMOVE);
#ifndef NDEBUG
	      //std::cerr << "Zagrozenie NIEDODANE" << std::endl;
	      //if (where == coord.ind(24, 23)) std::cerr << t.show() << std::endl;
#endif
	    }
	  }
	  else { // debug
#ifndef NDEBUG
	    /*
	      if (!t.encl->isEmpty()) {
	      std::cerr << "Zagrozenie ODRZUCONE w " << coord.showPt(where) << std::endl;
	      //if (where == coord.ind(24, 23)) std::cerr << t.show() << std::endl;
	      }*/
#endif
	  }
	}
      }
    //worm[where] = 0;  by destructor of cleanup
  }
}

std::shared_ptr<Enclosure>
Game::checkThreat_terr(Threat* thr, pti p, int who)
/// Try to enclose point [p] by who.
/// @param[in] thr  Threat with a territory to check, it may happen that the territory found will be the same
///                 and we will just change thr.type (reset REMOVE flag) instead of saving new one.
{
  Threat t;
  std::shared_ptr<Enclosure> en = t.encl = std::make_shared<Enclosure>(findEnclosure(p, MASK_DOT, who));
  if (!t.encl->isEmpty()) {
    t.type = ThreatConsts::TERR;
    t.where = 0;
    auto zobr = t.zobrist_key = t.encl->zobristKey(who);
    if (thr && t.zobrist_key == thr->zobrist_key) {
      thr->type &= ~(ThreatConsts::TO_CHECK | ThreatConsts::TO_REMOVE);  //  thr->type = ThreatConsts::TERR;
    } else {
      Threat *tz = threats[who-1].findThreatZobrist(zobr);
      if (tz != nullptr) {
	tz->type  &= ~(ThreatConsts::TO_CHECK | ThreatConsts::TO_REMOVE);  // may be redundant, but it's as fast to do as to check
      } else {
	auto tmp = countDotsTerrInEncl(*t.encl, 3-who);
	t.opp_dots = std::get<0>(tmp);
	t.terr_points = std::get<1>(tmp);
	addThreat(std::move(t), who);
      }
    }
  }
  return en;
}

void
Game::checkThreats_postDot(std::vector<pti> &newthr, pti ind, int who)
{
  assert(who==1 || who==2);
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
  //for (int who=1; who<=2; who++)   // <-- not needed, because there are no opponent threats markes as TO_CHECK
  for (unsigned int tn=0; tn<threats[who-1].threats.size(); tn++) {
    Threat *thr = &threats[who-1].threats[tn];
    if (thr->type & ThreatConsts::TO_CHECK) {
      if (thr->type & ThreatConsts::ENCL) {
	checkThreat_encl(thr, who);
      } else if (thr->type & ThreatConsts::TERR) {
	thr->type |= ThreatConsts::TO_REMOVE;
	std::vector<int8_t> done(coord.last+1, 0);
	std::shared_ptr<Enclosure> encl = thr->encl;     // thr may be invalidated by adding new threats in checkThreat_terr
	for (auto p : encl->interior)
	  if (!done[p] && whoseDotMarginAt(p) != who) {
	    std::shared_ptr<Enclosure> en = checkThreat_terr(&threats[who-1].threats[tn], p, who);   // try to enclose p; cannot use *thr because it can be invalidated
	    if (!en->isEmpty()) {
	      for (auto i : en->interior) done[i] = 1;
	    }
	  }
      }
    }
  }
  // remove opponents threats that ((old: that need to put dot at [ind] -- now in placeDot, or )) are marked as to be removed
  removeMarked(3-who);
  // recalculate number of dots in opponent threats, if needed
  if (threats[2-who].is_in_encl[ind] || threats[2-who].is_in_terr[ind]) {
    for (auto &t : threats[2-who].threats) {
      if (t.encl->isInInterior(ind)) {
	t.opp_dots++;
	t.terr_points--;
	// singular_dots are already updated in subtractThreat (in removeAtPoint in placeDot)
	// if (threats[2-who].is_in_encl[ind] + threats[2-who].is_in_terr[ind] == 1)
	//  t.singular_dots++;
      }
    }
  }
  // recalculate terr points in our threats, if needed
  if (threats[who-1].is_in_encl[ind] || threats[who-1].is_in_terr[ind]) {
    for (auto &t : threats[who-1].threats) {
      if (t.encl->isInInterior(ind)) {
	t.terr_points--;
      }
    }
  }
  // remove our marked threats
  removeMarked(who);
  // check new
  while (!newthr.empty()) {
    pti interior = newthr.back();  newthr.pop_back();
    pti where = newthr.back();     newthr.pop_back();
    if (worm[where] == 0) {
      CleanupOneVar<pti> worm_where_cleanup(&worm[where], who);   //  worm[where] = who;  with restored old value (0) by destructor
      int last = (interior & 0x1000) ? 1 : 4;
      pti last_pt = 0;
      for (int j=0; j<last; j++) {
	pti pt = where + coord.nb8[(interior+1) & 7];
	if (pt == last_pt) break;
	last_pt = pt;
	interior >>= 3;
	Threat t;
	t.encl = std::make_shared<Enclosure>(findEnclosure(pt, MASK_DOT, who));
	if (!t.encl->isEmpty() && !t.encl->isInInterior(where)) {
	  t.type = ThreatConsts::ENCL;
	  t.where = where;
	  auto zobr = t.zobrist_key = t.encl->zobristKey(who);
	  if (threats[who-1].findThreatZobrist(zobr) == nullptr) {
	    auto tmp = countDotsTerrInEncl(*t.encl, 3-who);
	    t.opp_dots = std::get<0>(tmp);
	    t.terr_points = std::get<1>(tmp);
	    addThreat(std::move(t), who);
	  }
	  /* for debugging */
	  /*
	  show();
	  std::cerr << "Zagrozenie: " << who << " w " << coord.showPt(where) << std::endl;
	  */
	}
      }
      // worm[where] = 0;   this does the destructor of worm_where_cleanup
    }
  }
}


void
Game::checkThreat2moves_encl(Threat* thr, pti where0, int who)
/// @param[in] where0  The first dot of the threat.
/// @param[in] who  Whose threat it is (i.e., who makes this enclosure).
{
  thr->type |= ThreatConsts::TO_REMOVE;
  int where = thr->where;
  if (worm[where] == 0 && worm[where0] == 0) {
    CleanupOneVar<pti> worm_where_cleanup0(&worm[where0], who);   //  worm[where0] = who;  with restored old value (0) by destructor
    CleanupOneVar<pti> worm_where_cleanup(&worm[where], who);   //  worm[where] = who;  with restored old value (0) by destructor
    int done[4] = {0,0,0,0};
    Threat t[4];
    for (int j=0; j<4; j++) {
      t[j].type = 0;
      if (!done[j]) {
	pti nb = where + coord.nb4[j];
	if (whoseDotMarginAt(nb) != who && coord.dist[nb] >= 1 && thr->encl->isInInterior(nb)) {

	  t[j].encl = std::make_shared<Enclosure>(findEnclosure(nb, MASK_DOT, who));
	  if (!t[j].encl->isEmpty() && t[j].encl->isInBorder(where) && t[j].encl->isInBorder(where0)
	      && !t[j].encl->checkIfRedundant(where) && !t[j].encl->checkIfRedundant(where0)) {   // added in v139
	    t[j].zobrist_key = t[j].encl->zobristKey(who);
	    if (t[j].zobrist_key == thr->zobrist_key) {   // our old threat is still valid --> this is in addThreat2moves
	      thr->type &= ~ThreatConsts::TO_REMOVE;
	      break;
	    }
	    t[j].type = ThreatConsts::ENCL;
	    t[j].where = where;
	    // if some next neighbour has been enclosed, mark it as done
	    for (int k=j+1; k<4; k++) {
	      if (t[j].encl->isInInterior(where + coord.nb4[k]))
		done[k]=1;
	    }
	    auto tmp = countDotsTerrInEncl(*t[j].encl, 3-who);
	    t[j].opp_dots = std::get<0>(tmp);
	    t[j].terr_points = std::get<1>(tmp);
	  }
	}
      }
    }
    // we add threats at the end, because it may invalidate 'thr' pointer
    for (int j=0; j<4; j++) {
      if (t[j].type) {
	threats[who-1].addThreat2moves(where0, where, isSafeFor(where0, who), isSafeFor(where, who), who, t[j]);
      }
    }
    //worm[where] = 0;  by destructor of cleanup
  }
}

int
Game::addThreat2moves(pti ind0, pti ind1, int who, std::shared_ptr<Enclosure> &encl)
{
  Threat t;
  t.type = ThreatConsts::ENCL;
  t.zobrist_key = encl->zobristKey(who);
  t.encl = encl;
  auto tmp = countDotsTerrInEncl(*encl, 3-who);
  t.opp_dots = std::get<0>(tmp);
  t.terr_points = std::get<1>(tmp);
  return threats[who-1].addThreat2moves(ind0, ind1, isSafeFor(ind0, who), isSafeFor(ind1, who), who, t);
}

int debug_allt2m = 0, debug_sing_smallt2m = 0, debug_sing_larget2m = 0, debug_skippedt2m = 0;
int debug_n = 0, debug_N = 0;

void
Game::checkThreats2moves_postDot(std::vector<pti> &newthr, pti ind, int who)
{
  if (!newthr.empty()) {
    // get the number of pairs in neighbourhood
    int pairs_count = newthr.back();
    newthr.pop_back();
    if (pairs_count) {
      // here we will find threats in 2 moves such that one move is close to [ind], but the other is neither close to [ind], nor to the first move
      const int TAB_SIZE = 7;   // 1 for group_id and 5 for moves (it seems 5 is max), 1 redundant?
      const int N_SIZE = 5;     // max count for neighbours group, it seems that 5 is max (but if not, we would just lose some groups)
      std::array<pti, N_SIZE*TAB_SIZE> groups{};  // groups[0,...,N_SIZE-1]: group_id's,  groups[j+N_SIZE*k], k=1,2,... -- neighbours for group j
      int group_count = 0;
      for (int i=0; i<pairs_count; i++) {
	auto g = newthr.back();  newthr.pop_back();
	auto neighb = newthr.back();  newthr.pop_back();
	for (int j=0; j<N_SIZE; ++j) {
	  if (groups[j] == 0) {  // group g occurs the first time, save it as new
	    groups[j] = g;
	    groups[j+N_SIZE] = neighb;
	    ++group_count;
	    break;
	  }
	  if (groups[j] == g) {  // there was already a neighbour for group g, find the first empty place to save the neighbour
	    for (int p=1; p<TAB_SIZE; ++p) {
	      if (groups[j + N_SIZE*p] == 0) {
		groups[j + N_SIZE*p] = neighb;
		break;
	      }
	    }
	  }
	}
      }
      // info about groups is collected, now find the other places to play
      auto this_group = descr.at(worm[ind]).group_id;
      for (int i=coord.first; i<=coord.last; ++i) {
	if (whoseDotMarginAt(i) == 0 && connects[who-1][i].groups_id[1] != 0 &&
	    !coord.isInNeighbourhood(i, ind)) {
	  std::array<pti, 4> unique_groups = {0,0,0,0};
	  int ug = connects[who-1][i].getUniqueGroups(unique_groups);
	  if (ug < 2) continue;
	  // check if [i] touches this_group
	  for (int j=0; j<ug; ++j) {
	    if (unique_groups[j] == this_group) {
	      // ok, it touches, so check if it touches some of the groups in the neighbourhood of [ind]
	      for (int k=0; k<ug; ++k) {
		if (k==j) continue;
		for (int g=0; g<group_count; ++g) {
		  if (groups[g] == unique_groups[k]) {
		    for (int p=1; p<TAB_SIZE; ++p) {
		      auto nei = groups[g + N_SIZE*p];
		      if (nei == 0) break;
		      if (!coord.isInNeighbourhood(i, nei)) {
			// we have found a threat! save it
			addClosableNeighbours(newthr, ind, i, nei, who);
			newthr.push_back(i);
			newthr.push_back(nei);
			/*
			CleanupOneVar<pti> worm_where_cleanup0(&worm[i], who);   //  worm[ind0] = who;  with restored old value (0) by destructor
			CleanupOneVar<pti> worm_where_cleanup1(&worm[nei], who);   //  worm[ind1] = who;  with restored old value (0) by destructor
			show();
			std::cerr << "Zagrozenie2: " << who << " w " << coord.showPt(nei) << " + " << coord.showPt(i) << std::endl;
			std::cin.ignore();
			*/
		      }
		    }
		  }
		}
	      }
	    }
	  }
	}
      }
    }
  }
  // check our old threats
  for (auto &t2 : threats[who-1].threats2m) {
    if (t2.where0 == ind) {
      // all threats should be removed, because we played at where0
      for (auto &t : t2.thr_list) {
	t.type |= ThreatConsts::TO_REMOVE;
      }
    } else {
      for (auto &t : t2.thr_list) {
	if (t.where == ind || t.isShortcut(ind)) {  //   || t.encl->checkShortcut(t2.where0, ind) || t.encl->checkShortcut(t.where, ind)) {
	  /*
	  if (t.where != ind && (t.encl->checkShortcut(t2.where0, ind) || t.encl->checkShortcut(t.where, ind)) != t.shortcuts.contains(ind)) {
	    show();
	    std::cerr << "shortcut " << coord.showPt(t2.where0) << "-" << coord.showPt(ind) << ": " << t.encl->checkShortcut(t2.where0, ind) << std::endl;
	    std::cerr << "shortcut " << coord.showPt(t.where) << "-" << coord.showPt(ind) << ": " << t.encl->checkShortcut(t.where, ind) << std::endl;
	    std::cerr << "shortcuts " << t.shortcuts.show() << std::endl;
	  }
	  */
	  assert(t.where == ind || (t.encl->checkShortcut(t2.where0, ind) || t.encl->checkShortcut(t.where, ind)) == t.isShortcut(ind));
	  t.type |= ThreatConsts::TO_REMOVE;
	} else {
	  /*
	  if(t.where != ind && (t.encl->checkShortcut(t2.where0, ind) || t.encl->checkShortcut(t.where, ind)) != t.shortcuts.contains(ind)) {
	    show();
	    std::cerr << "shortcut " << coord.showPt(t2.where0) << "-" << coord.showPt(ind) << ": " << t.encl->checkShortcut(t2.where0, ind) << std::endl;
	    std::cerr << "shortcut " << coord.showPt(t.where) << "-" << coord.showPt(ind) << ": " << t.encl->checkShortcut(t.where, ind) << std::endl;
	    std::cerr << "shortcuts " << t.shortcuts.show() << std::endl;
	  }
	  */

	  if ( !(t.where == ind || (t.encl->checkShortcut(t2.where0, ind) || t.encl->checkShortcut(t.where, ind)) == t.isShortcut(ind)) ) {
#ifdef DEBUG_SGF
	    std::cerr.flush();
	    std::cerr << std::endl << "Sgf:" << std::endl << getSgf_debug() << std::endl << std::endl;
#endif

	    show();
	    std::cerr << "blad: " << coord.showPt(t.where) << " " <<  coord.showPt(ind) << " " << coord.showPt(t2.where0) << " " <<
	      t.encl->checkShortcut(t2.where0, ind) << ","<<
	      t.encl->checkShortcut(t.where, ind) << "," << t.isShortcut(ind) << std::endl;
	    std::cerr << "enclosure: " << t.encl->show() << std::endl;
	    std::cerr.flush();
	  }
	  assert(t.where == ind || (t.encl->checkShortcut(t2.where0, ind) || t.encl->checkShortcut(t.where, ind)) == t.isShortcut(ind));
	  if (t.encl->isInInterior(ind)) {
	    t.type |= ThreatConsts::TO_REMOVE;
	    // we removed the threat and now add it to check again
	    // TODO: in most cases it could be done more efficiently by checking whether the threat is still valid
	    if (t2.where0 < t.where && t.encl->interior.size() > 1) {
	      assert(worm[t2.where0] == 0 && worm[t.where] == 0);
	      addClosableNeighbours(newthr, ind, t.where, t2.where0, who);
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
  // remove opponents threats that need to put dot at [ind] or marked as to be removed

	auto debug_time = std::chrono::high_resolution_clock::now();

  threats[2-who].removeMarkedAndAtPoint2moves(ind);
  // remove our marked threats
  threats[who-1].removeMarked2moves();

	debug_nanos2 += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - debug_time).count();

  // check new
  while (!newthr.empty()) {
    pti ind0 = newthr.back();  newthr.pop_back();
    assert(!newthr.empty());
    pti ind1 = newthr.back();  newthr.pop_back();
    assert(worm[ind0] == 0 && worm[ind1] == 0);
    {
      CleanupOneVar<pti> worm_where_cleanup0(&worm[ind0], who);   //  worm[ind0] = who;  with restored old value (0) by destructor
      CleanupOneVar<pti> worm_where_cleanup1(&worm[ind1], who);   //  worm[ind1] = who;  with restored old value (0) by destructor
      debug_allt2m++;
      assert(!newthr.empty());
      int count = newthr.back();  newthr.pop_back();
      // we do not want threats2m such that one move is a usual threat, and the other lies inside terr or encl
      if ((threats[who-1].is_in_border[ind0] > 0 && (threats[who-1].is_in_encl[ind1] > 0 || threats[who-1].is_in_terr[ind1] > 0)) ||
	  (threats[who-1].is_in_border[ind1] > 0 && (threats[who-1].is_in_encl[ind0] > 0 || threats[who-1].is_in_terr[ind0] > 0))) {
	newthr.resize( newthr.size() - count);
	debug_skippedt2m++;
	continue;
      }

      //bool debug_probably_1_encl = (threats[who-1].is_in_terr[ind0]==0 && threats[who-1].is_in_terr[ind1]==0);
      if (threats[who-1].is_in_terr[ind0]==0 && threats[who-1].is_in_terr[ind1]==0) {
	  // (threats[who-1].is_in_encl[ind0]==0 || threats[who-1].is_in_border[ind1]==0) &&   // this is now excluded above
	  // (threats[who-1].is_in_encl[ind1]==0 || threats[who-1].is_in_border[ind0]==0) &&   //
	  //coord.distBetweenPts_infty(ind0, ind1) == 1) {  // TODO: check if this is important
	// Here usually there will be only one enclosure. Exception: positions like the first one:
	//   . . . . . .              . . . . . .
	// .             .          .             .
	// .         L   .          .             .
	// .       .   0              .           0
	//   .       1                  .       L   1
	//     . . .                      . . . . .
	// L = last dot [ind], 0=[ind0], 1=[ind1].
	// In the first cases it is not very important to find both threats. In the second, in fact there will be one enclosure with L,0,1 on border.
	// Another example with coord.distBetweenPts_infty(ind0, ind1) > 1:
	//   . . . . .
	// .           .
	// .             .
	// .       L       .
	// .     0   1   .
	//   . .   .   .
	// Also here only one of 2 threats will be found, but it does not matter much, since both are contained in another threat (just 0 and 1)
	//   which should have been found before move L.
	// First pass: check for simple enclosures, maybe we can find one.
	debug_n++;  debug_N+=count;
	//bool was_one = false;
	for (int i=count; i>0; --i) {
	  pti pt = newthr[newthr.size() - i];
	  assert(coord.dist[pt] >= 1 && whoseDotMarginAt(pt) != who);
	  std::shared_ptr<Enclosure> encl = std::make_shared<Enclosure>(findSimpleEnclosure(pt, MASK_DOT, who));
	  //if (!encl->isEmpty()) was_one = true;
	  if (!encl->isEmpty() && encl->isInBorder(ind) && encl->isInBorder(ind0) && encl->isInBorder(ind1)) {
	    addThreat2moves(ind0, ind1, who, encl);
	    //was_one = true;
	    debug_sing_smallt2m++;
	    goto one_found;
	  }
	}
	// none was found, second pass: find non-simple enclosure
	for (int i=count; i>0; --i) {
	  pti pt = newthr[newthr.size() - i];
	  std::shared_ptr<Enclosure> encl = std::make_shared<Enclosure>(findNonSimpleEnclosure(pt, MASK_DOT, who));
									//findEnclosure(pt, MASK_DOT, who));  // findNonSimpleEnclosure(pt, MASK_DOT, who));
	  //if (!encl->isEmpty()) was_one = true;
	  if (!encl->isEmpty() && encl->isInBorder(ind) && encl->isInBorder(ind0) && encl->isInBorder(ind1)) {
	    addThreat2moves(ind0, ind1, who, encl);
	    //was_one = true;
	    debug_sing_larget2m++;
	    goto one_found;
	  }
	}
      one_found:;
	newthr.resize( newthr.size() - count);
	/*
	if (!was_one) {
	  show();
	  std::cerr << "Zagrozenie2: " << who << " po ruchu " << coord.showPt(ind) << " w " << coord.showPt(ind0) << " + " << coord.showPt(ind1) << std::endl;
	}
	assert(was_one);
	*/
      } else {
	// Here there might be more than 1 threat.
	//bool was_one = false;
	for (; count>0; --count) {
	  assert(!newthr.empty());
	  pti pt = newthr.back();  newthr.pop_back();
	  assert(coord.dist[pt] >= 1 && whoseDotMarginAt(pt) != who);
	  std::shared_ptr<Enclosure> encl = std::make_shared<Enclosure>(findEnclosure(pt, MASK_DOT, who));
	  //if (!encl->isEmpty()) was_one = true;
	  if (!encl->isEmpty() && encl->isInBorder(ind) && encl->isInBorder(ind0) && encl->isInBorder(ind1)) {
	    addThreat2moves(ind0, ind1, who, encl);
	    //was_one = true;
	    /*
	    if (debug_count >=2 && debug_probably_1_encl) {
	      show();
	      std::cerr << "Zagrozenie2: " << who << " po ruchu " << coord.showPt(ind) << " w " << coord.showPt(ind0) << " + " << coord.showPt(ind1) << std::endl;
	      std::cin.ignore();
	    }
	    */
	  }
	}
	/*
	if (!was_one) {
	  show();
	  std::cerr << "Zagrozenie2: " << who << " po ruchu " << coord.showPt(ind) << " w " << coord.showPt(ind0) << " + " << coord.showPt(ind1) << std::endl;
	}
	assert(was_one);
	*/
      }
    }
  }
}


/// Player who is now in danger at [ind]
///  -- function updates FLAG_SAFE bit of flag in threats2m and also saves points to recalculate_list in order to update pattern3_value
///  -- updates also possible_moves and interesting_moves
void
Game::pointNowInDanger2moves(pti ind, int who)
{
  for (auto &t2 : threats[who-1].threats2m) {
    if (t2.where0 == ind) {
      // TODO: if (threats[who-1].is_in_border[ind]) { check if we may cancel opp's threats by playing at ind }
      if (t2.flags & Threat2mconsts::FLAG_SAFE) {
	// it was safe but is not anymore
	threats[who-1].changeFlagSafe(t2);
      }
    }
    else {
      // here we could check if the border of some threats in 2m contains ind, i.e., is in danger
    }
  }
  for (int j=0; j<4; j++) {
    pti nb = ind + coord.nb4[j];
    if (whoseDotMarginAt(nb) == 0 && Pattern3::isPatternPossible(pattern3_at[nb])) {
      pattern3_at[nb] = PATTERN3_IMPOSSIBLE;
      recalculate_list.push_back(nb);
    }
  }
  if (whoseDotMarginAt(ind)==0) {
    possible_moves.changeMove(ind, PossibleMovesConsts::TERRM);
    interesting_moves.changeMove(ind, InterestingMovesConsts::REMOVED);
  }
}


void
Game::addThreat(Threat&& t, int who)
{
  boost::container::small_vector<pti,8> counted_worms;   // list of counted worms for singular_dots
  for (auto i : t.encl->interior) {
    if (t.type & ThreatConsts::TERR) {
      assert(i>=coord.first && i<=coord.last && i<threats[who-1].is_in_terr.size());
      ++threats[who-1].is_in_terr[i];
    } else {
      assert(t.type & ThreatConsts::ENCL);
      assert(i>=coord.first && i<=coord.last && i<threats[who-1].is_in_encl.size());
      ++threats[who-1].is_in_encl[i];
    }
    if (whoseDotMarginAt(i)==3-who) {
      addOppThreatZobrist(descr.at(worm[i]).opp_threats, t.zobrist_key);
    }
    switch (threats[who-1].is_in_terr[i] + threats[who-1].is_in_encl[i]) {
    case 1:
      pointNowInDanger2moves(i, 3-who);
      if (whoseDotMarginAt(i)==3-who &&
	  std::find(counted_worms.begin(), counted_worms.end(), descr.at(worm[i]).leftmost) == counted_worms.end()) {
	t.singular_dots += descr.at(worm[i]).dots[2-who];
	counted_worms.push_back(descr.at(worm[i]).leftmost);
      }
      if (threats[2-who].is_in_border[i] >= 1) {   // note: it may be an empty place, but still in_border (play here for ENCL)
	for (auto &ot : threats[2-who].threats) {
	  if (ot.encl->isInBorder(i)) {
	    ot.border_dots_in_danger++;
	    addOppThreatZobrist(ot.opp_thr, t.zobrist_key);
	  }
	}
      }
      break;
    case 2:
      if (whoseDotMarginAt(i)==3-who &&
	  std::find(counted_worms.begin(), counted_worms.end(), descr.at(worm[i]).leftmost) == counted_worms.end()) {
	threats[who-1].findThreatWhichContains(i)->singular_dots -= descr.at(worm[i]).dots[2-who];
	counted_worms.push_back(descr.at(worm[i]).leftmost);
      }
      // no break!
    default:
      if (threats[2-who].is_in_border[i] >= 1) {   // note: it may be an empty place, but still in_border (play here for ENCL)
	for (auto &ot : threats[2-who].threats) {
	  if (ot.encl->isInBorder(i)) {
	    addOppThreatZobrist(ot.opp_thr, t.zobrist_key);
	  }
	}
      }
    }
  }
  for (auto it = t.encl->border.begin()+1; it!=t.encl->border.end(); ++it) {
    pti i = *it;
    assert(i>=coord.first && i<=coord.last && i<threats[who-1].is_in_border.size());
    ++threats[who-1].is_in_border[i];
    if (threats[2-who].is_in_encl[i] || threats[2-who].is_in_terr[i]) {
      t.border_dots_in_danger++;
      for (auto &ot : threats[2-who].threats) {
	if (ot.encl->isInInterior(i)) {
	  addOppThreatZobrist(t.opp_thr, ot.zobrist_key);
	}
      }
    }
  }
  threats[who-1].threats.push_back(std::move(t));
}


void
Game::subtractThreat(const Threat& t, int who)
{
  boost::container::small_vector<pti,8> counted_worms;   // list of counted worms for singular_dots
  bool threatens = false;   // if t threatend some opp's threat
  for (auto i : t.encl->interior) {
    assert(i>=coord.first && i<=coord.last);
    if (t.type & ThreatConsts::TERR) {
      --threats[who-1].is_in_terr[i];
    } else {
      assert(t.type & ThreatConsts::ENCL);
      --threats[who-1].is_in_encl[i];
    }
    if (whoseDotMarginAt(i)==3-who) {
      removeOppThreatZobrist(descr.at(worm[i]).opp_threats, t.zobrist_key);
    }
    if (whoseDotMarginAt(i)==3-who &&
	threats[who-1].is_in_terr[i] + threats[who-1].is_in_encl[i] == 1 &&
	std::find(counted_worms.begin(), counted_worms.end(), descr.at(worm[i]).leftmost) == counted_worms.end()) {
      // Note: there are now 2 threats with i in interior, one being t, i.e., the one that we are about to delete.
      // We add to singular_dots in both, because in t it does not matter anymore (it will be deleted)
      // and it is simpler to do so than to check which of 2 threats is the right one.
      for (auto &t : threats[who-1].threats) {
	if (t.encl->isInInterior(i)) t.singular_dots += descr.at(worm[i]).dots[2-who];
      }
      counted_worms.push_back(descr.at(worm[i]).leftmost);
    }
    if (threats[2-who].is_in_border[i] >= 1) {
      if (threats[who-1].is_in_terr[i] == 0 && threats[who-1].is_in_encl[i] == 0) {
	for (auto &thr : threats[2-who].threats) {
	  if (thr.encl->isInBorder(i)) {
	    --thr.border_dots_in_danger;
	    assert(thr.border_dots_in_danger >= 0);
	  }
	}
      }
      threatens = true;
    }
    if (threats[who-1].is_in_terr[i] == 0 && threats[who-1].is_in_encl[i] == 0) {
      // point is now safe, recalculate pattern3 values and check moves list
      for (int j=0; j<4; j++) {
	pti nb = i + coord.nb4[j];
	if (whoseDotMarginAt(nb) == 0 && Pattern3::isPatternPossible(pattern3_at[nb])) {
	  pattern3_at[nb] = PATTERN3_IMPOSSIBLE;
	  recalculate_list.push_back(nb);
	}
      }
      if (threats[2-who].is_in_terr[i] == 0 && threats[2-who].is_in_encl[i] == 0 && whoseDotMarginAt(i)==0) {
	// [i] is not anymore in danger and should be removed from TERRM moves list
	possible_moves.changeMove(i, checkDame(i));
	if (Pattern3::isPatternPossible(pattern3_at[i])) {
	  interesting_moves.changeMove(i, checkInterestingMove(i));
	}
      }
    }
    if (isSafeFor(i, 3-who)) pointNowSafe2moves(i, 3-who);
  }
  for (auto it = t.encl->border.begin()+1; it!=t.encl->border.end(); ++it) {
    pti i = *it;
    assert(i>=coord.first && i<=coord.last);
    --threats[who-1].is_in_border[i];
  }
  if (threatens) {
    for (auto &thr : threats[2-who].threats) {
      removeOppThreatZobrist(thr.opp_thr, t.zobrist_key);
    }
  }
}

void
Game::removeMarkedAndAtPoint(pti ind, int who)
{
  if (!threats[who-1].threats.empty()) {
    for (auto &t : threats[who-1].threats)
      if (t.where == ind || (t.type & ThreatConsts::TO_REMOVE)) {
	subtractThreat(t, who);
	t.type |= ThreatConsts::TO_REMOVE;
      }
    //removeMarkedThreats(threats[who-1].threats);

    threats[who-1].threats.erase( std::remove_if( threats[who-1].threats.begin(), threats[who-1].threats.end(),
						  [](Threat &t) { return (t.type & ThreatConsts::TO_REMOVE); } ),
				  threats[who-1].threats.end() );

  }
}

/// Returns whether we removed a threat containing a dot of 3-who that touches [ind].
/// This is important for calculating singular_dots.
/// TODO: in fact, this turned out to be irrelevant -> change the function to void.
bool
Game::removeAtPoint(pti ind, int who)
{
  bool removed = false;
  bool touch = false;
  if (threats[who-1].is_in_border[ind] > 0) {
    for (auto &t : threats[who-1].threats)
      if (t.where == ind) {
	subtractThreat(t, who);
	removed = true;
	if (!touch) {
	  for (int j=0; j<4; j++) {
	    pti nb = ind + coord.nb4[j];
	    if (whoseDotMarginAt(nb) == 3-who && t.encl->isInInterior(nb)) {
	      touch = true;
	      break;
	    }
	  }
	}
	//t.type |= ThreatConsts::TO_REMOVE;
      }
    if (removed) {
      threats[who-1].threats.erase( std::remove_if( threats[who-1].threats.begin(), threats[who-1].threats.end(),
						    [ind](Threat &t) { return (t.where == ind); } ),
				    threats[who-1].threats.end() );
    }
  }
  return touch;
}

void
Game::removeMarked(int who)
{
  // remove our marked threats
  if (!threats[who-1].threats.empty()) {
    for (auto &t : threats[who-1].threats)
      if (t.type & ThreatConsts::TO_REMOVE) {
	subtractThreat(t, who);
      }
    //removeMarkedThreats(threats[who-1].threats);

    threats[who-1].threats.erase( std::remove_if( threats[who-1].threats.begin(), threats[who-1].threats.end(),
						  [](Threat &t) { return (t.type & ThreatConsts::TO_REMOVE); } ),
				  threats[who-1].threats.end() );

  }
}


void
Game::pointNowSafe2moves(pti ind, int who)
{
  for (auto &t2 : threats[who-1].threats2m) {
    if (t2.where0 == ind) {
      if ((t2.flags & Threat2mconsts::FLAG_SAFE)==0) {
	// it was not safe but is now safe
	threats[who-1].changeFlagSafe(t2);
      }
    } else {
      // if we check borders in pointNowInDanger2moves, we should do it also here
    }
  }
}

/// Checks whether it is safe for who to play at ind, to check if a threat-in-2-moves
/// starting with a move at ind is indeed a threat.
bool
Game::isSafeFor(pti ind, int who) const
{
  return (threats[2-who].is_in_terr[ind] == 0 && threats[2-who].is_in_encl[ind]==0);
}

void
Game::connectionsRenameGroup(pti dst, pti src)
{
  int g = (dst & MASK_DOT) - 1;
  assert(g==0 || g==1);
  for (auto &p : connects[g]) {
    for (int j=0; j<4; j++) if (p.groups_id[j]==src) p.groups_id[j]=dst;
  }
}


void
Game::connectionsRecalculateCode(pti ind, int who)
{
  // TODO: we could use pattern3 and delete codes completely
  if (!isDotAt(ind)) {
    int code = 0;    // the codes for the neighbourhood for player who
    for (int i=7; i>=0; i--) {
      pti nb = ind + coord.nb8[i];
      if (whoseDotMarginAt(nb)==who) {
	code |= 1;
      }
      code <<= 1;
    }
    code >>= 1;
    connects[who-1][ind].code = code;
  } else {
    connects[who-1][ind].code = 0;
  }
}

void
Game::connectionsRecalculateConnect(pti ind, int who)
{
  int g = who-1;
  auto new_connections = coord.connections_tab_simple[connects[g][ind].code];
  for (int j=0; j<4; j++) {
    if (new_connections[j] >= 0) {
      pti pt = ind + coord.nb8[new_connections[j]];
      connects[g][ind].groups_id[j] = descr.at(worm[pt]).group_id;
    } else {
      connects[g][ind].groups_id[j] = 0;
    }
  }
}

void
Game::connectionsRecalculatePoint(pti ind, int who)
{
  connectionsRecalculateCode(ind, who);
  connectionsRecalculateConnect(ind, who);
}


// Used after placing dot of who at [ind].
void
Game::connectionsRecalculateNeighb(pti ind, int who)
{
  for (int i=0; i<8; i++) {
    pti nb = ind + coord.nb8[i];
    if (whoseDotMarginAt(nb) == 0) {
      connects[who-1][nb].code |= (1 << (i ^ 4));
      //connectionsRecalculatePoint(nb, who);
      connectionsRecalculateConnect(nb, who);
    }
  }
  connectionsRecalculatePoint(ind, 1);
  connectionsRecalculatePoint(ind, 2);
}

pattern3_t
Game::getPattern3_at(pti ind) const
{
  if (whoseDotMarginAt(ind) != 0) return 0;
  pattern3_t p = 0, atari = 0;
  const static pattern3_t atari_masks[8] = { 0, 0x10000, 0, 0x20000, 0, 0x40000, 0, 0x80000 };
  for (int i=7; i>=0; i--) {
    pti nb = ind + coord.nb8[i];
    p <<= 2;
    auto dot = whoseDotMarginAt(nb);
    p |= dot;
    if (i & 1) {  // we are at N, W, S, or E neighbour
      switch(dot) {
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
bool
Game::isEmptyInDirection(pti ind, int direction) const
{
  assert(coord.nb4[0] == coord.N);  // order of nb4 should be: N, E, S, W
  assert(coord.nb8[0] == coord.NE); // order of nb8 should be: NE, E, ... -- clockwise
  assert(coord.dist[ind] >= 1);

  pti nb = ind + 2*coord.nb4[direction];
  const pattern3_t masks[4] = { 0x3f0, 0x3f00, 0xf003, 0x3f };
  return (whoseDotMarginAt(nb) == 0) && (pattern3_at[nb] & masks[direction]) == 0;
}

bool
Game::isEmptyInNeighbourhood(pti ind) const
{
  return (whoseDotMarginAt(ind) == 0) && (pattern3_at[ind] == 0);
}

// This function is slower, but useful for testing.
pattern3_val
Game::getPattern3Value(pti ind, int who) const
{
  return global::patt3.getValue(getPattern3_at(ind), who);
}

void
Game::showPattern3Values(int who) const
{
  std::vector<pti> val(coord.getSize(), 0);
  for (int i=coord.first; i<=coord.last; i++) {
    if (coord.dist[i]>=0) {
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
  if ((oldv < 0 && pattern3_value[0][ind] >= 0) || (oldv >= 0 && pattern3_value[0][ind] < 0)) {
    possible_moves.changeMove(ind, checkDame(ind));
  }
  pattern3_value[1][ind] = global::patt3.getValue(pattern3_at[ind], 2);
  interesting_moves.changeMove(ind, checkInterestingMove(ind));
}

void Game::recalculatePatt3Values()
{
  for (auto &p : recalculate_list) {
    if (whoseDotMarginAt(p)!=0) {
      pattern3_at[p] = 0;
      pattern3_value[0][p] = 0;
      pattern3_value[1][p] = 0;
    } else {
      pattern3recalculatePoint(p);
    }
  }
}

real_t
Game::getPattern52Value(pti ind, int who) const
{
  if (whoseDotMarginAt(ind) != 0) return 0.0;
  if (coord.dist[ind] > 1) return 0.0;
  real_t value = 0;
  for (int k=0; k<=1; k++) {
    pattern3_t p = 0;
    std::array<pti,9> deltas;
    enum { NONE, EDGE, INNER } type = NONE;
    if (k==0) {
      if (coord.y[ind] > 0 && coord.y[ind] < coord.wlky-1) {
	if (coord.x[ind] == 0 || coord.x[ind] == coord.wlkx-2) {
	  std::array<pti, 9> ds = { coord.NNE, coord.NN, coord.NE, coord.N, coord.E,
				    coord.SSE, coord.SS, coord.SE, coord.S };
	  deltas = ds;
	  type = (coord.x[ind] == 0) ? EDGE : INNER;
	} else if (coord.x[ind] == 1 || coord.x[ind] == coord.wlkx-1) {
	  std::array<pti, 9> ds = { coord.NN, coord.NNW, coord.N, coord.NW, coord.W,
				    coord.SS, coord.SSW, coord.S, coord.SW };
	  deltas = ds;
	  type = (coord.x[ind] == 1) ? INNER : EDGE;
	}
      }
    } else {  // k==1
      if (coord.x[ind] > 0 && coord.x[ind] < coord.wlkx-1) {
	if (coord.y[ind] == 0 || coord.y[ind] == coord.wlky-2) {
	  std::array<pti, 9> ds = { coord.SWW, coord.WW, coord.SW, coord.W, coord.S,
				    coord.SEE, coord.EE, coord.SE, coord.E };
	  deltas = ds;
	  type = (coord.y[ind] == 0) ? EDGE : INNER;
	} else if (coord.y[ind] == 1 || coord.y[ind] == coord.wlky-1) {
	  std::array<pti, 9> ds = { coord.EE, coord.NEE, coord.E, coord.NE, coord.N,
				    coord.WW, coord.NWW, coord.W, coord.NW };
	  deltas = ds;
	  type = (coord.y[ind] == 1) ? INNER : EDGE;
	}
      }
    }
    if (type != NONE) {
      for (int i=8; i>=0; i--) {
	pti nb = ind + deltas[i];
	p <<= 2;
	auto dot = whoseDotMarginAt(nb);
	p |= dot;
      }
      value += (type == EDGE) ? global::patt52_edge.getValue(p, who) : global::patt52_inner.getValue(p, who);
    }
  }
  return value;
}

void
Game::showPattern52Values(int who) const
{
  std::vector<pti> val(coord.getSize(), 0);
  for (int i=coord.first; i<=coord.last; i++) {
    if (coord.dist[i]>=0 && coord.dist[i]<=1) {
      val[i] = int( 100*getPattern52Value(i, who) + 0.5);
    }
  }
  std::cerr << "Patterns52 for player " << who << std::endl;
  std::cerr << coord.showBoard(val);
}

/* checkLadderStep:
 *  x    -- position of the last dot of escaping player
 *  x+v1 -- position of the next dot of escaping player
 *  x+v1+v2 -- position of the second next dot of escaping player
 *  escaping_group -- id of the worm that tries to escape
 *  escapes -- who escapes (1 or 2)
 *  ladder_ext -- true if attacker had a dot in previous step, and escaper has a dot in this step
 *  iteration  -- 0 at the first
 *  ladder_breakers -- position of places which matter for determining the outcome of this ladder
 *                     (Warning: ladder has to be recalculated also when a dot from the set of ladder_breakers
 *                      become connected.)
 * Returns -1 if escaper wins, +1 if attacker wins.
 *
 * Figure:
 *    . b
 *  . x a
 *    . .
 *  a=x+v1, b=x+v1+v2
 */
int
Game::checkLadderStep(pti x, PointsSet &ladder_breakers, pti v1, pti v2, pti escaping_group, bool ladder_ext, int escapes, int iteration)
{
  const static int ESC_WINS = -1, ATT_WINS = 1;
  pti nx = x+v1;
  ladder_breakers.insert(nx);
  int attacks = 3-escapes;
  bool curr_ext = false;
  if ((worm[nx] & MASK_DOT) == escapes && !ladder_ext) {
    ladder_breakers.insert(nx+v1);
    if ((worm[nx+v1] & MASK_DOT) != attacks) {
      return ESC_WINS;
    }
    curr_ext = true;
  }
  if ((worm[nx] & MASK_DOT) == 0 || ladder_ext || curr_ext) {
    // if we're on the edge, then we escaped
    if (coord.dist[nx] == 0) return ESC_WINS;
    // if attacker is already connected, then he captured
    ladder_breakers.insert({(pti)(nx+v1), (pti)(nx+v2)});
    if ((worm[nx+v1] & MASK_DOT) == attacks && (worm[nx+v2] & MASK_DOT) == attacks)
      return ATT_WINS;
    // escaping player connects and encloses attacking group
    ladder_breakers.insert({(pti)(nx+v1+v2), (pti)(nx+v1-v2)});
    if (descr.at(worm[nx+v1]).group_id == escaping_group ||
	descr.at(worm[nx+v1+v2]).group_id == escaping_group || descr.at(worm[nx+v1-v2]).group_id == escaping_group)
      return ESC_WINS;
    // attacking player has an atari thanks to a dot on the way of the ladder
    if (((worm[nx+v1] | worm[nx+v2]) & MASK_DOT) == attacks && !curr_ext) {
      return ATT_WINS;
    }
    // atari of the escaping player
    for (pti w : {(pti)(-v2), v2}) {
      if ((worm[nx+v1-w] & MASK_DOT)==0 && coord.dist[nx+v1-w] >= 0) {
	ladder_breakers.insert({(pti)(nx+2*v1), (pti)(nx+2*v1-w), (pti)(nx+2*v1-2*w), (pti)(nx+v1-2*w), (pti)(nx-2*w)});
	if (descr.at(worm[nx+2*v1]).group_id == escaping_group || descr.at(worm[nx+2*v1-w]).group_id == escaping_group ||
	    descr.at(worm[nx+2*v1-2*w]).group_id == escaping_group || descr.at(worm[nx+v1-2*w]).group_id == escaping_group ||
	    descr.at(worm[nx-2*w]).group_id == escaping_group) {
	  return ESC_WINS;
	}
      }
    }
    // atari of the escaping player thanks to his dot(s) on the way of the ladder
    if (iteration >= 2) {
      ladder_breakers.insert(nx-2*v2);
      if (((worm[nx+v1-v2] | worm[nx-2*v2]) & MASK_DOT) == escapes) {
	return ESC_WINS;
      }
    }
    // check what is on nx+v1
    if ((worm[nx+v1] & MASK_DOT) == escapes) {
      return ESC_WINS;
    } else ladder_ext = ((worm[nx+v1] & MASK_DOT) != 0);
    return checkLadderStep(nx, ladder_breakers, v2, v1, escaping_group, ladder_ext, escapes, iteration+1);
  }
  // should we ever get here?
  return ATT_WINS;
}



/// Finds simplifying enclosures (=those that have 0 territory).
bool
Game::appendSimplifyingEncl(std::vector<std::shared_ptr<Enclosure>> &encl_moves, uint64_t &zobrists, int who)
{
  zobrists = 0;
  bool something_left = false;
  for (auto &t : threats[who-1].threats) {
    if ((t.type & ThreatConsts::TERR) && (t.terr_points == 0)) {
      assert(t.opp_dots);
      encl_moves.push_back(t.encl);
      zobrists ^= t.zobrist_key;
    } else if (t.opp_dots && !t.opp_thr.empty())
      something_left = true;
  }
  return something_left;
}

/// Finds simplifying enclosures (=those that have 0 territory) and priorities to use in Game::getEnclMoves.
/// Stores them in  Game::ml_encl_moves and Game::ml_priorities
void
Game::getSimplifyingEnclAndPriorities(int who)
{
  ml_encl_moves.clear();
  ml_encl_zobrists.clear();  ml_encl_zobrists.push_back(0);
  bool something_left = appendSimplifyingEncl(ml_encl_moves, ml_encl_zobrists[0], who);
  ml_priorities.clear();
  if (!something_left) return;
  // this could be omitted, duplicates might slow down later, but checking for them is also slow
  ml_deleted_opp_thr.clear();
  for (auto &t : threats[who-1].threats) {
    if ((t.type & ThreatConsts::TERR) && (t.terr_points == 0)) {
      for (auto z : t.opp_thr) {
	if (std::find(ml_deleted_opp_thr.begin(), ml_deleted_opp_thr.end(), z) == ml_deleted_opp_thr.end()) {
	  ml_deleted_opp_thr.push_back(z);
	}
      }
    }
  }
  // now find priorities, i.e., those our threats that may cancel some opp's threat not yet canceled by simplifying_encl
  for (auto &t : threats[who-1].threats) {
    if (t.opp_dots && !t.opp_thr.empty()) {
      ThrInfo ti;
      for (auto z : t.opp_thr) {
	if (std::find(ml_deleted_opp_thr.begin(), ml_deleted_opp_thr.end(), z) == ml_deleted_opp_thr.end()) {
	  const Threat* othr = threats[2-who].findThreatZobrist_const(z);
	  assert(othr != nullptr);
	  if (othr->opp_dots) {
	    // This:
	    //   ti.saved_dots += othr->opp_dots;
	    // is inaccurate, as it counts some dots many times.
	    for (auto p : othr->encl->interior) {
	      if (whoseDotMarginAt(p) == who) {
		if (std::find(ti.saved_worms.begin(), ti.saved_worms.end(), worm[p]) == ti.saved_worms.end()) {
		  // our worm not yet counted
		  ti.saved_worms.push_back(worm[p]);
		  ti.saved_dots += descr.at(worm[p]).dots[who-1];
		}
	      }
	    }
	  }
	  ti.opp_thr.push_back(z);
	}
      }
      if (!ti.opp_thr.empty()) {
	ti.thr_pointer = &t;
	ti.lost_terr_points = t.terr_points;
	ti.type = t.type;
	ti.move = t.where;
	ti.won_dots = t.opp_dots;
	ti.priority_value = ti.calculatePriorityValue();
	ml_priorities.push_back(ti);
      }
    }
  }
}

/// @param[in] ind   Place on the edge to check
/// @return          1-4: makes sense to play here, 0: neutral, -1: dame
int
Game::checkBorderMove(pti ind, int who) const
{
  assert(coord.dist[ind]==0 && worm[ind]==0);
  if (threats[0].is_in_border[ind] > 0 || threats[1].is_in_border[ind] > 0) return 4;    // atari always makes sense
  int x = coord.x[ind];
  int y = coord.y[ind];
  pti viter[2], vnorm;   // vnorm: outer normal unit vector
  if (x==0 || x==coord.wlkx-1) {
    if (y==0 || y==coord.wlky-1)  // corner?
      return 0;
    viter[0] = coord.N;    viter[1] = coord.S;
    vnorm = (x==0) ? coord.W : coord.E;
  } else {
    viter[0] = coord.E;    viter[1] = coord.W;
    vnorm = (y==0) ? coord.N : coord.S;
  }
  int neighb_worm = worm[ind - vnorm];
  int neighb_whose = (neighb_worm & MASK_DOT);
  if (neighb_whose == 0) {
    // it's usually not very good to play on the edge, when 2nd line is empty
    // TODO: check some situations
    return 0;
  } else if (neighb_whose == who) {
    // neighbour is our dot. Everything depends on its safety.
    if (descr.at(neighb_worm).isSafe())
      return -1;  // dame
    else {
      // half-safe, check if we can escape
      auto safe0 = checkBorderOneSide(ind-vnorm, viter[0], vnorm, 3-who);
      auto safe1 = checkBorderOneSide(ind-vnorm, viter[1], vnorm, 3-who);
      return 2 + safe0 + safe1;   // 0 if we can escape in both directions, 2: one, 4: none
    }
  } else {
    assert(neighb_whose == 3-who);
    if (descr.at(neighb_worm).isSafe()) {
      return -1;  // dame
    }
    else {
      // half-safe, first check if it might be possible to reduce/secure territory (v103+)
      for (int point : {ind-vnorm+viter[0], ind-vnorm+viter[1]}) {
	if (threats[0].is_in_terr[point] > 0 || threats[0].is_in_encl[point] > 0 ||
	    threats[1].is_in_terr[point] > 0 || threats[1].is_in_encl[point] > 0)
	  return 1;
      }
      // then check if opp can escape
      auto safe0 = checkBorderOneSide(ind-vnorm, viter[0], vnorm, who);
      if (safe0 == -1) return -1;
      auto safe1 = checkBorderOneSide(ind-vnorm, viter[1], vnorm, who);
      return safe1 < 0 ? -1 : 2;   // -1 if opp can escape in at least one direction, 2: if opp cannot escape
    }
  }
}


/// @param[in] ind    Place of a dot on a 2nd line to check.
/// @param[in] viter  Add this to ind to iterate over 2nd line (for example, ==coord.E or .W for top line)
/// @param[in] vnorm  Add this to ind to get neighbour on the edge (for example, ==coord.N for top line)
/// @param[in] who    Who is going to attack the dot on 2nd line.
/// @return           1: dot on the 2nd cannot escape in direction viter, -1: can escape
int
Game::checkBorderOneSide(pti ind, pti viter, pti vnorm, int who) const
{
  const short int restab[2][16] = { {0,1,-1,0, 1,1,0,0, -1,1,-1,0, 0,0,0,0}, {0,-1,1,0, -1,-1,1,0, 1,0,1,0, 0,0,0,0} };
  ind += viter;
  while (coord.dist[ind] == 1) {
    int code = whoseDotMarginAt(ind) | (whoseDotMarginAt(ind+vnorm) << 2);
    auto r = restab[who-1][code];
    if (r) return r;
    ind += viter;
  }
  // next margin
  return whoseDotMarginAt(ind) == who ? 1:-1;  // in fact, 1 is optimistic, opp could escape on the second edge
}

void
Game::descend(TreenodeAllocator &alloc, Treenode *node, int depth, bool expand)
{
  if (node->children == nullptr && (expand || (node->t.playouts - node->prior.playouts) >= MC_EXPAND_THRESHOLD)) {
    //std::cerr << " expand: node->move = " << coord.showPt(node->move.ind) << std::endl;
    generateListOfMoves(alloc, node, depth, node->move.who ^ 3);
    if (node->children == nullptr) {
      node->children = alloc.getLastBlock();
    }
    else {
      alloc.getLastBlock();
    }
#ifndef NDEBUG
    if (node == node->parent) {
      assert(checkRootListOfMovesCorrectness(node->children));
    }
#endif
  }
  if (node->children != nullptr) {
    // select one of the children
    Treenode *best = nullptr;
    real_t bestv = -1e5;
    Treenode *ch = node->children;
    for(;;) {
      assert(ch->amaf.playouts + ch->t.playouts > 0);
      real_t value = ch->getValue();
      //if (ch->move.who == 2) value = -value;
      if (value > bestv) {
	bestv = value;
	best = ch;
      }
      if (ch->isLast()) break;
      ch++;
    }
    if (best != nullptr) {
      makeMove(best->move);
      best->t.playouts += VIRTUAL_LOSS;
      descend(alloc, best, depth+1, false);
      return;
    }
  }

  {
    // experiment: add loses to amaf inside opp enclosures; first remember empty points
    // TODO: at this point we already do not know all empty points, there could be enclosures in
    //       the generateMoves-playout phase. Solution: save empty points at findBestMove.
    //       BUT on the other hand, sometimes it's good to play in opp's terr to reduce it.
    std::vector<pti> amafboard(coord.getSize(), 0);
    const int amaf_empty = -5;
    for (auto i=coord.first; i<=coord.last; i++) {
      if (whoseDotMarginAt(i) == 0) amafboard[i] = amaf_empty;
    }
    // we are at leaf, playout...
    auto nmoves = history.size();
    real_t v = randomPlayout();
    auto lastWho = node->move.who;
    //auto endmoves = std::min(history.size(), nmoves + 50);
    auto endmoves = history.size();
    const int distance_rave = 3;
    const int distance_rave_TERR =  8;
    const int amaf_ENCL_BORDER = 16;
    const int distance_rave_SHIFT = 5;
    const int distance_rave_MASK  = 7;
    {
      int distance_rave_threshhold = (endmoves - nmoves + 2) / distance_rave;
      int distance_rave_current = distance_rave_threshhold / 2;
      int distance_rave_weight = distance_rave + 1;
      for (auto i=nmoves; i<endmoves; i++) {
	lastWho ^= 3;
	amafboard[history[i] & history_move_MASK] = lastWho | (distance_rave_weight << distance_rave_SHIFT) |
	  ( (history[i] & HISTORY_TERR) ? distance_rave_TERR : 0 ) |
	  ( (history[i] & HISTORY_ENCL_BORDER) ? amaf_ENCL_BORDER : 0);
	if (--distance_rave_current == 0) {
	  distance_rave_current = distance_rave_threshhold;
	  if (distance_rave_weight > 1) --distance_rave_weight;
	}
	assert(coord.dist[history[i] & history_move_MASK] >= 1 || (whoseDotMarginAt(history[i] & history_move_MASK) == lastWho));
      }
      // experiment: add loses to amaf inside opp enclosures
      for (auto i=coord.first; i<=coord.last; i++) {
	if (amafboard[i] == amaf_empty) {
	  int who = whoseDotMarginAt(i);
	  if (who == 0) {
	    // here add inside territories, it's important when playouts stop playing before the end
	    if (threats[0].is_in_terr[i] > 0) who = 1;
	    else if (threats[1].is_in_terr[i] > 0) who = 2;
	    else who = -1;  // dame!
	  }
	  if (who) {
	    amafboard[i] = who-3;  // == -(opponent of encl owner) or -4 for dame
	  } else {
	    amafboard[i] = 0;
	  }
	}
      }
    }
    // save playout outcome to t and amaf statistics
    for(;;) {
      auto move_ind = node->move.ind;
      auto move_who = node->move.who;
      auto adjusted_value = (move_who == 1) ? v : 1-v;
      node->t.playouts += 1 - VIRTUAL_LOSS;  // add 1 new playout and undo virtual loss
      node->t.value_sum = node->t.value_sum.load() + adjusted_value;
      if (node == node->parent) {
	// we are at root
	node->t.playouts += VIRTUAL_LOSS;  // in root we do not add virtual loss, but we 'undid' it, so we have to add it again
	break;
      }
      amafboard[move_ind] = move_who | ((distance_rave+1) << distance_rave_SHIFT);  // before 'for' loop, so that it counts also in amaf
      node = node->parent;
      Treenode *ch = node->children;
      for (;;) {
	if (amafboard[ch->move.ind] < 0) {
	  if (ch->move.who == -amafboard[ch->move.ind]) {
	    ch->amaf.playouts += 2;  // add 2 loses
	    //ch->amaf.value_sum += 0;
	  } else if (amafboard[ch->move.ind] == -4) {
	    // dame, add 1 playout (?)
	    ch->amaf.playouts += 1;
	    ch->amaf.value_sum = ch->amaf.value_sum.load() + adjusted_value;
	  }
	} else if ((amafboard[ch->move.ind] & distance_rave_MASK) == ch->move.who) {
	  if (ch->move.enclosures.empty() or (amafboard[ch->move.ind] & amaf_ENCL_BORDER)) {  // we want to avoid situation when in ch there is enclosure, but in amaf not (anymore?)
	    int count = amafboard[ch->move.ind] >> distance_rave_SHIFT;
	    ch->amaf.playouts += count;
	    ch->amaf.value_sum = ch->amaf.value_sum.load() + adjusted_value*count;
	  }
	} else if (adjusted_value < 0.2 &&
		   (amafboard[ch->move.ind] & distance_rave_MASK) == 3 - ch->move.who) {  // inside encl, == 0 possible when there was enclosure in the generateListOfMoves-playout phase
	  // v114+: add also losses for playing inside opp's (reduced) territory -- sometimes
	  //  there is an opp terr which always may be reduced, then opp will mostly play there, but it does not usually make sense
	  //  that we play
	  if (amafboard[ch->move.ind] & distance_rave_TERR) {
	    int count = amafboard[ch->move.ind] >> distance_rave_SHIFT;
	    ch->amaf.playouts += count;
	    ch->amaf.value_sum = ch->amaf.value_sum.load() + adjusted_value*count;
	  }
	}
	if (ch->isLast()) break;
	ch++;
      }
      //amafboard[move_ind] = move_who;
    } // while (node != node->parent);
  }
}


/// Finds necessary and optional enclosures after move of player who.
/// move may be 0 to find enclosures after neutral moves (i.e., those that do not cancel
///  any opp's threats).
/// ml_priorities should be set before calling.
/// @param[out] encl_moves  Here the obligatory enclosures are added.
/// @param[out] opt_moves   Here optional enclosures are saved (i.e., first zeroed, then added).
/// @param[out] encl_zobrists  First is added the zobrist for all encl_moves. Then zobrists for optional enclosures are added at the end.
void
Game::getEnclMoves(std::vector<std::shared_ptr<Enclosure> > &encl_moves, std::vector<std::shared_ptr<Enclosure> > &opt_encl_moves,
		   std::vector<uint64_t> &encl_zobrists,
		   pti move, int who)
{
  // ml_encl_moves.clear();   this is already done in Game::getSimplifyingEnclAndPriorities()
  opt_encl_moves.clear();
  encl_zobrists.push_back(0);
  if (ml_priorities.empty()) return;
  ml_priority_vect.clear();
  /*
  for (auto &t : ml_priorities) {
    if ((t.type & ThreatConsts::TERR) || (t.move == move && (t.type & ThreatConsts::ENCL))) {
      ml_priority_vect.push_back(t);   // TODO: use copy_if ?
    }
  }
  */
  std::copy_if(ml_priorities.begin(), ml_priorities.end(), back_inserter(ml_priority_vect),
	       [move](ThrInfo &t) { return (t.type & ThreatConsts::TERR) || (t.move == move && (t.type & ThreatConsts::ENCL)); });
  if (move==0) {
    ml_special_moves.clear();
    // here we are called by generateListOfMoves, so we will need  ml_special_moves list
    for (auto &t: threats[2-who].threats) {
      if (t.where != 0) {
	assert(t.type & ThreatConsts::ENCL);
	ml_special_moves.push_back(t.where);
      }
    }
    for (auto &t : ml_priorities) {
      if (t.move !=0) {
	assert(t.type & ThreatConsts::ENCL);
	ml_special_moves.push_back(t.move);
      }
    }
  }
  if (ml_priority_vect.empty()) return;
  // delete those opp's threats that need to put a dot at 'move'
  if (move) {
    for (auto &t: threats[2-who].threats) {
      if (t.where == move) {
	assert(t.type & ThreatConsts::ENCL);
	for (auto &ourt : ml_priority_vect) {
	  auto pos = std::find(ourt.opp_thr.begin(), ourt.opp_thr.end(), t.zobrist_key);
	  if (pos != ourt.opp_thr.end()) {
	    *pos = ourt.opp_thr.back();
	    ourt.opp_thr.pop_back();
	    auto opp_d = 0;
	    if (t.opp_dots) {
	      // This:
	      //  ourt.saved_dots -= t.opp_dots;
	      // is inaccurate, so we calculate it properly
	      for (auto p : t.encl->interior) {
		if (whoseDotMarginAt(p) == who) {
		  auto pos = std::find(ourt.saved_worms.begin(), ourt.saved_worms.end(), worm[p]);
		  if (pos != ourt.saved_worms.end()) {
		    // our worm was counted, subtract
		    *pos = ourt.saved_worms.back();
		    ourt.saved_worms.pop_back();
		    opp_d += descr.at(worm[p]).dots[who-1];
		  }
		}
	      }
	      ourt.saved_dots -= opp_d;
	    }
	    if (ourt.opp_thr.empty()) {
	      ourt.priority_value = ThrInfoConsts::MINF;
	    } else {
	      ourt.priority_value -= ThrInfoConsts::VALUE_SAVED_DOT * opp_d;
	    }
	  }
	}
      }
    }
  }
  // sort enclosure according to priority_value
  std::sort(ml_priority_vect.begin(), ml_priority_vect.end());
  auto encl_zobrists_0 = encl_zobrists.back();
  for (auto it = ml_priority_vect.begin(); it!=ml_priority_vect.end(); ++it) {
    if (it->priority_value > ThrInfoConsts::MINF) {
      /*
      Threat *thr = threats[who-1].findThreatZobrist(it->zobrist_key);
      assert(thr != nullptr);
      */
      if (it->priority_value > 0) {
	encl_moves.push_back(it->thr_pointer->encl);
	encl_zobrists_0 ^= it->thr_pointer->zobrist_key;
      }
      else {
	opt_encl_moves.push_back(it->thr_pointer->encl);
	encl_zobrists.push_back(it->thr_pointer->zobrist_key);
      }
    }
    else break;
    // remove opp threats that are already taken care of by t from our next threats
    bool unsorted = false;
    for (uint64_t z : it->opp_thr) {
      const Threat *othr = nullptr;  // threats[2-who].findThreatZobrist(z);   lazy evaluated later
      for (auto it2 = it+1; it2!=ml_priority_vect.end(); ++it2) {
	auto pos = std::find(it2->opp_thr.begin(), it2->opp_thr.end(), z);
	if (pos != it2->opp_thr.end()) {
	    *pos = it2->opp_thr.back();
	    it2->opp_thr.pop_back();
	    unsorted = true;
	    if (it2->opp_thr.empty()) {
	      it2->priority_value = ThrInfoConsts::MINF;
	    } else {
	      if (othr == nullptr) othr = threats[2-who].findThreatZobrist_const(z);   // lazy evaluated here
	      auto opp_d = 0;
	      if (othr->opp_dots) {
		// This:
		//  it2->saved_dots -= othr->opp_dots;
		// is inaccurate, so we calculate it properly
		for (auto p : othr->encl->interior) {
		  if (whoseDotMarginAt(p) == who) {
		    auto pos = std::find(it2->saved_worms.begin(), it2->saved_worms.end(), worm[p]);
		    if (pos != it2->saved_worms.end()) {
		      // our worm was counted, subtract
		      *pos = it2->saved_worms.back();
		      it2->saved_worms.pop_back();
		      opp_d += descr.at(worm[p]).dots[who-1];
		    }
		  }
		}
		it2->saved_dots -= opp_d;
	      }
	      it2->priority_value -= ThrInfoConsts::VALUE_SAVED_DOT * opp_d;
	    }
	}
      }
    }
    // sort the rest again
    if (unsorted) std::sort(it+1, ml_priority_vect.end());
  }
}

void
Game::placeDot(int x, int y, int who)
// places a dot of who at (x,y),
// TODO: if rules:must-surround, then also makes necessary enclosures
{
  pti ind = coord.ind(x,y);
  assert(worm[ind] == 0);
  recalculate_list.clear();
  if (threats[who-1].is_in_terr[ind] == 0 && threats[who-1].is_in_encl[ind] == 0) {
    history.push_back(threats[who-1].is_in_border[ind] ? (ind | HISTORY_ENCL_BORDER) : ind);
  } else {
    // check for an anti-reduction move, which would be bad for the opponent
    // Move would be bad, if all neighbours are 'who' dots, or all but one empty point
    assert(coord.dist[ind] >= 1);   // because we're inside a threat
    int count = 0;
    for (int i=0; i<4; i++) {
      if (whoseDotMarginAt(ind + coord.nb4[i]) == 0) {
	count++;
      } else if (whoseDotMarginAt(ind + coord.nb4[i]) == (who^3)) {
	count = -1; break;
      }
    }
    history.push_back((count == 0 || count == 1) ? (ind | HISTORY_TERR) : ind);
  }
#ifdef DEBUG_SGF
  sgf_tree.makePartialMove({ (who==1 ? "B":"W"), { coord.indToSgf(ind) } });
#endif
  // if (mustSurround) { ... place the dot, do the necessary surrounds }
  pti numb[4];
  int count = 0;
  for (int i=0; i<4; i++) {
    pti nb = ind + coord.nb4[i];
    if (whoseDotMarginAt(nb) == who) {
      // check if it was already saved
      for (int j=0; j<count; j++)
	if (numb[j] == worm[nb]) goto AlreadyThere;
      numb[count++] = worm[nb];
    AlreadyThere:;
    }
  }
  std::vector<pti> to_check = findThreats_preDot(ind, who);
  std::vector<pti> to_check2m = findThreats2moves_preDot(ind, who);
  // remove opp threats that need to put at ind
  // This is important to do before glueing worms, because otherwise counting singular_dot becomes complicated.
  // TODO: we could probably just mark them as TO_REMOVE
  removeAtPoint(ind, 3-who);
  if (threats[2-who].is_in_encl[ind] + threats[2-who].is_in_terr[ind] == 1) {
    // if the new dot is 'singular', we must add it here, because descr still doesn't know about it at this stage
    threats[2-who].findThreatWhichContains(ind)->singular_dots++;
  }
  bool update_safety_dame = false;   // if some worms start/stop being safe, we need to recalculate dame moves on the first line
  bool nonisolated = (count > 0);
  if (count) {
    while (count >= 2) {
      // glue the smaller to the larger worm
      if ((descr.at(numb[count-1]).safety <= 1 || descr.at(numb[count-2]).safety <= 1) && (descr.at(numb[count-1]).safety + descr.at(numb[count-2]).safety >= 2)) {
	update_safety_dame = true;
      }
      if (descr.at(numb[count-1]).dots[0] + descr.at(numb[count-1]).dots[1] > descr.at(numb[count-2]).dots[0] + descr.at(numb[count-2]).dots[1]) {
	wormMergeSame(numb[count-1], numb[count-2]);
	numb[count-2] = numb[count-1];
      } else {
	wormMergeSame(numb[count-2], numb[count-1]);
      }
      count--;
    }
    // add our dot
    pti next = nextDot[ descr.at(numb[0]).leftmost ];
    nextDot[ descr.at(numb[0]).leftmost ] = ind;
    nextDot[ ind ] = next;
    worm[ind] = numb[0];
    descr.at(numb[0]).leftmost = std::min(descr.at(numb[0]).leftmost, ind);
    descr.at(numb[0]).dots[who-1]++;
  } else {
    // 'isolated' dot (note: new enclosure is possible also here due to diagonal connections)
    assert(who == 1 || who == 2);
    pti c = (lastWormNo[who-1] += CONST_WORM_INCR);
    assert(descr.find(c) == descr.end());
    worm[ind] = c;
    nextDot[ind] = ind;
    WormDescr dsc;
    dsc.dots[0] = (who == 1);
    dsc.dots[1] = (who == 2);
    dsc.leftmost = ind;
    dsc.group_id = c;
    dsc.safety = 0;
    if (threats[2-who].is_in_encl[ind] || threats[2-who].is_in_terr[ind]) {
      for (auto &t : threats[2-who].threats) {
	if (t.encl->isInInterior(ind))
	  dsc.opp_threats.push_back(t.zobrist_key);
      }
    }
    descr.insert({c, dsc});
  }
  // update safety info
  {
    int dist = coord.dist[ind];
    if (dist==0) {
      descr.at(worm[ind]).safety = WormDescr::SAFE_VALUE;
      for (int i=0; i<4; i++) {
	pti nb = ind + coord.nb4[i];
	if (coord.dist[nb]==1 && worm[nb]) {
	  descr.at(worm[nb]).safety--;   // it may happen that worm[nb]==worm[ind], but we may safely decrease SAFE_VALUE by one
	  if (descr.at(worm[nb]).safety == 1) {
	    update_safety_dame = true;  // worm at nb stopped being safe
	  }
	  break;
	}
      }
    } else if (dist==1) {
      bool was_unsafe_and_nonisolated = nonisolated && !descr.at(worm[ind]).isSafe();
      for (int i=0; i<4; i++) {
	pti nb = ind + coord.nb4[i];
	if (coord.dist[nb]==0 && worm[nb]==0) {
	  descr.at(worm[ind]).safety++;  // without break, because of the 4 corner places (1,1), etc. which are close to the edge from 2 sides
	}
      }
      if (was_unsafe_and_nonisolated && descr.at(worm[ind]).isSafe()) update_safety_dame = true;
    }
  }
  // check diag neighbours
  pti cm[4];  int top=0;
  pti our_group_id = descr.at(worm[ind]).group_id;
  for (int i=0; i<8; i+=2) {
    pti nb = ind + coord.nb8[i];
    if (whoseDotMarginAt(nb) == who && worm[nb] != worm[ind]) {
      // connection!
      if (descr.at(worm[nb]).group_id != our_group_id) {
	for (int j=0; j<top; j++) {
	  if (cm[j] == descr.at(worm[nb]).group_id) goto check_ok;
	}
	cm[top++] = descr.at(worm[nb]).group_id;
      check_ok:;
      }
      // add to neighbours if needed
      if (std::find( descr.at(worm[ind]).neighb.begin(), descr.at(worm[ind]).neighb.end(), worm[nb]) == descr.at(worm[ind]).neighb.end()) {
	descr.at(worm[ind]).neighb.push_back( worm[nb] );
	descr.at(worm[nb]).neighb.push_back( worm[ind] );
      }
    }
  }
  while (top >= 1) {
    for (auto &d : descr) {
      if (d.second.group_id == cm[top-1])
	d.second.group_id = our_group_id;
    }
    connectionsRenameGroup(our_group_id, cm[top-1]);
    top--;
  }
  connectionsRecalculateNeighb(ind, who);
  checkThreats_postDot(to_check, ind, who);
  checkThreats2moves_postDot(to_check2m, ind, who);
  // remove move [ind] from possible moves
  pattern3_value[0][ind] = 0;    pattern3_value[1][ind] = 0;
  possible_moves.changeMove(ind, PossibleMovesConsts::REMOVED);
  interesting_moves.changeMove(ind, InterestingMovesConsts::REMOVED);
  if (update_safety_dame) {
    possibleMoves_updateSafetyDame();
  } else if (coord.dist[ind]==1) {
    // update safety dame for neighbours of [ind]
    possibleMoves_updateSafety(ind);
  }
  for (int i=0; i<8; i++) {
    auto nb = ind + coord.nb8[i];
    if (whoseDotMarginAt(nb) == 0 && Pattern3::isPatternPossible(pattern3_at[nb])) {
      pattern3_at[nb] = PATTERN3_IMPOSSIBLE;
      recalculate_list.push_back(nb);
    }
  }
}

void
Game::Test()
{
  placeDot(2,2, 1);
  placeDot(3,1, 1);
  placeDot(4,2, 1);
  placeDot(3,3, 1);
  placeDot(3,2, 2);
}

void
Game::show() const
{
  //  std::cerr << coord.showBoard(worm);  // worm.data()); ?
  std::cerr << coord.showColouredBoardWithDots(worm);
  std::cerr << "Score: " << score[0].show() << "; " << score[1].show() << std::endl;
}

void
Game::showSvg()
{
}


void
Game::showConnections()
{
  std::cerr << "Player 1" << std::endl;
  std::cerr << coord.showBoard(connects[0]);
  std::cerr << "Player 2" << std::endl;
  std::cerr << coord.showBoard(connects[1]);
}

void
Game::showGroupsId()
{
  for (auto &d : descr) {
    std::cerr << d.first << "->" << d.second.group_id << "  ";
  }
  std::cerr << std::endl;
}

void
Game::showThreats2m()
{
  for (int g=0; g<2; ++g) {
    int inside = 0;
    std::cerr << "Threats for player " << g << ":" << std::endl;
    int count = 0;
    for (auto &t : threats[g].threats2m) {
      std::cerr << "[" << count++ << "] " << coord.showPt(t.where0) << " -- moves: " << t.thr_list.size() << std::endl;
      if (threats[g].is_in_encl[t.where0] > 0 || threats[g].is_in_terr[t.where0] > 0) inside++;
      for (auto &t2 : t.thr_list) {
	std::cerr << "   " << coord.showPt(t2.where);
      }
      std::cerr << std::endl;
    }
    std::cerr << "Stats: all/inside = " << count << " / " << inside << std::endl;
  }
}

std::vector<pti>
Game::getPatt3extraValues() const
{
  std::vector<pti> values(coord.getSize(), 0);
  for (int i=coord.first; i<=coord.last; ++i) {
    if (whoseDotMarginAt(i) == 0) {
      global::patt3_extra.setValues(values, worm, pattern3_at[i], i, nowMoves);
      //auto symm_value = global::patt3_symm.getValue(pattern3_at[i], 1);
      //if (values[i] < symm_value) values[i] = symm_value;
    }
  }
  return values;
}

void
Game::showPattern3extra()
{
  std::vector<pti> values = getPatt3extraValues();
#ifdef DEBUG_SGF
  {
    SgfProperty prop;
    prop.first = "LB";
    for (int i=coord.first; i<=coord.last; ++i) {
      if (values[i] > 0) {
	prop.second.push_back(coord.indToSgf(i) + ":" + std::to_string(values[i]));
      }
    }
    sgf_tree.addProperty(prop);
  }
#endif
}

Enclosure
Game::findSimpleEnclosure(pti point, pti mask, pti value)
{
  return findSimpleEnclosure(worm, point, mask, value);
}

Enclosure
Game::findSimpleEnclosure(std::vector<pti> &tab, pti point, pti mask, pti value) const
// tries to enclose 'point' using dots given by (tab[...] & mask) == value, check only size-1 or size-2 enclosures
// All points in 'tab' should have zero bits MASK_MARK and MASK_BORDER.
// TODO: check whether using an array is really efficient, maybe vector + std::move would be better
{
  {
    int count = 0;
    int direction;
    for (int i=0; i<4; i++) {
      pti nb = point + coord.nb4[i];
      if ((tab[nb] & mask) == value) {
	count++;
      } else {
	direction = i;
      }
    }
    if (count == 4) {
      std::vector<pti> border = { static_cast<pti>(point + coord.N), static_cast<pti>(point + coord.W),
				  static_cast<pti>(point + coord.S), static_cast<pti>(point + coord.E),
				  static_cast<pti>(point + coord.N) };
      return Enclosure( {point}, std::move(border) );
    }

    if (count==3) {
      pti nb = point + coord.nb4[direction];
      if (coord.dist[nb] <= 0) {  // (added in v138)
	return empty_enclosure;;   // nb is on the edge or outside; TODO: then in fact there'd be no enclosure at all, no need to check for the non-simple one
      }
      if ((tab[nb+coord.nb4[direction]] & mask) == value &&
	  (tab[nb+coord.nb4[(direction+1)&3]] & mask) == value &&
	  (tab[nb+coord.nb4[(direction+3)&3]] & mask) == value) {
	// 2-point enclosure!
	pti ptor = point, nbor = nb;   // we save original order to make the program play in the same way
	if (direction == 0 || direction == 3) {  // N or W?
	  std::swap(nb, point);
	  direction ^= 2;  // take opposite direction
	}
	assert(point < nb);
	if (direction == 1) {  // E
	  std::vector<pti> border = { static_cast<pti>(point + coord.N), static_cast<pti>(point + coord.W),
				      static_cast<pti>(point + coord.S), static_cast<pti>(nb + coord.S),
				      static_cast<pti>(nb + coord.E), static_cast<pti>(nb + coord.N),
				      static_cast<pti>(point + coord.N) };
	  //return Enclosure( {point, nb}, border );
	  return Enclosure( {ptor, nbor}, std::move(border) );
	} else {  // S
	  std::vector<pti> border = { static_cast<pti>(point + coord.N), static_cast<pti>(point + coord.W),
				      static_cast<pti>(nb + coord.W), static_cast<pti>(nb + coord.S),
				      static_cast<pti>(nb + coord.E), static_cast<pti>(point + coord.E),
				      static_cast<pti>(point + coord.N) };
	  //return Enclosure( {point, nb}, border );
	  return Enclosure( {ptor, nbor}, std::move(border) );
	}
      }
    }
  }
  return empty_enclosure;
}


Enclosure
Game::findNonSimpleEnclosure(pti point, pti mask, pti value)
{
  return findNonSimpleEnclosure(worm, point, mask, value);
}

Enclosure
Game::findNonSimpleEnclosure(std::vector<pti> &tab, pti point, pti mask, pti value) const
// tries to enclose 'point' using dots given by (tab[...] & mask) == value
// All points in 'tab' should have zero bits MASK_MARK and MASK_BORDER.
// TODO: check whether using an array is really efficient, maybe vector + std::move would be better
{
  std::array<pti, Coord::maxSize> stack;
  stack[0] = point;
  tab[point] |= MASK_MARK;
  int stackSize = 1;
  //std::vector<uint8_t> tab(coord.getSize(), 0);
  pti leftmost = Coord::maxSize;
  mask |= MASK_MARK;
  //  Cleanup<std::vector<pti>&, pti> cleanup(tab, ~MASK_MARK);
  CleanupUsingList<std::vector<pti>&, pti> cleanup(tab, ~(MASK_MARK | MASK_BORDER));
  cleanup.push(point);
  //  int border_count = 0;
  do {
    pti ind = stack[--stackSize];
    for (int i=0; i<4; i++) {
      pti nb = ind + coord.nb4[i];
      if ((tab[nb] & mask) == value) {
	if ((tab[nb] & MASK_BORDER) == 0) {  // a border dot not yet visited?
	  tab[nb] |= MASK_BORDER;
	  cleanup.push(nb);
	  //border_count++;
	  if (nb < leftmost) leftmost = nb;
	}
      } else if ((tab[nb] & MASK_MARK) == 0) {
	if (coord.dist[nb] > 0) {  // interior point on the board (and not on the edge)
	  tab[nb] |= MASK_MARK;
	  stack[stackSize++] = nb;
	  cleanup.push(nb);
	} else {
	  // we got to the edge of the board!  note: cleaning done by 'cleanup' object
	  return empty_enclosure;
	}
      }
    }
  } while (stackSize);
  // traverse the border to find minimal-area enclosure
  stack[0] = leftmost + coord.NE;
  stack[1] = leftmost;
  int direction = coord.findDirectionNo(stack[1], stack[0]) + 1;
  stack[2] = stack[1] + coord.nb8[direction];   // stack[2] = coord.findNextOnRight(stack[1], stack[0]);
  tab[stack[0]] |= MASK_MARK;
  tab[stack[1]] |= MASK_MARK;
  int top = 2;
  do {
    // check if current point is on the border, if not, take next (clockwise)
    while ((tab[stack[top]] & MASK_BORDER) == 0) {
      direction++;
      assert( stack[top-1] + coord.nb8[direction] == coord.findNextOnRight(stack[top-1], stack[top]) );
      stack[top] = stack[top-1] + coord.nb8[direction];   // stack[top] = coord.findNextOnRight(stack[top-1], stack[top]);
    }
    if (stack[top] == stack[0])  // enclosure found
      break;
    if (tab[stack[top]] & MASK_MARK) {
      // this point has been already visited, go back to the last visit of that point and try next neighbour
      pti loop_pt = stack[top];
      top--;
      pti prev_pt = stack[top];
      while (stack[top] != loop_pt) tab[stack[top--]] &= ~MASK_MARK;
      // now we are again at loop_pt which has been already unMARKed
      direction = coord.findDirectionNo(loop_pt, prev_pt) + 1;
      assert( loop_pt + coord.nb8[direction] == coord.findNextOnRight(loop_pt, prev_pt) );
      stack[++top] = loop_pt + coord.nb8[direction];    // stack[++top] = coord.findNextOnRight(loop_pt, prev_pt);
    } else {
      // visit the point
      tab[stack[top]] |= MASK_MARK;
      top++;
      direction = (direction + 5) & 7;  //coord.findDirectionNo(stack[top-1], stack[top-2]) + 1;
      assert( stack[top-1] + coord.nb8[direction] == coord.findNextOnRight(stack[top-1], stack[top-2]) );
      stack[top] = stack[top-1] + coord.nb8[direction];   // stack[top] = coord.findNextOnRight(stack[top-1], stack[top-2]);
    }
  } while (stack[top] != stack[0]);
  // now the enclosure is kept in stack[0..top], it has also (MARK | BORDER) bits set, but these will be cleared
  // by cleanup at exit
  // TODO: in most cases all interior points will be found, for example, if number of MARKed points is small
  //  (in such cases it'd be also possible to reserve optimal amount of memory)
  std::vector<pti> interior;
  /*
  if (top == border_count) {
    int tt = cleanup.count - top;
    interior.reserve(cleanup.count - top);  // reserve exact memory for interior
    for (int i=0; i<cleanup.count; i++)
      if ((tab[cleanup.list[i]] & (MASK_MARK | MASK_BORDER)) != (MASK_MARK | MASK_BORDER)) {  // interior point
	interior.push_back(cleanup.list[i]);
      }
    assert(tt == interior.size());
    } else */
  {
    interior.reserve(cleanup.count + 40);  // reserve memory for interior found so far + border + 40 (arbitrary const), which should be enough in most cases
    for (int i=0; i<cleanup.count; i++)
      if ((tab[cleanup.list[i]] & (MASK_MARK | MASK_BORDER)) != (MASK_MARK | MASK_BORDER)) {  // interior point
	interior.push_back(cleanup.list[i]);
	for (int j=0; j<4; j++) {
	  pti nb = cleanup.list[i] + coord.nb4[j];
	  if ((tab[nb] & (MASK_MARK | MASK_BORDER)) == 0) {
	    tab[nb] |= MASK_MARK;
	    cleanup.push(nb);
	  }
	}
      }

  }
  //std::sort(interior.begin(), interior.end());
  return Enclosure(std::move(interior), std::move(std::vector<pti>(&stack[0], &stack[top+1])));
}


Enclosure
Game::findEnclosure(pti point, pti mask, pti value)
{
  return findEnclosure(worm, point, mask, value);
}

Enclosure
Game::findEnclosure(std::vector<pti> &tab, pti point, pti mask, pti value) const
// tries to enclose 'point' using dots given by (tab[...] & mask) == value
// All points in 'tab' should have zero bits MASK_MARK and MASK_BORDER.
{
  auto encl = findSimpleEnclosure(tab, point, mask, value);
  if (encl.isEmpty())
    return findNonSimpleEnclosure(tab, point, mask, value);
  else
    return encl;
}

Enclosure
Game::findEnclosure_notOptimised(pti point, pti mask, pti value)
{
  return findEnclosure_notOptimised(worm, point, mask, value);
}

Enclosure
Game::findEnclosure_notOptimised(std::vector<pti> &tab, pti point, pti mask, pti value) const
// tries to enclose 'point' using dots given by (tab[...] & mask) == value
// All points in 'tab' should have zero bits MASK_MARK and MASK_BORDER.
// TODO: check whether using an array is really efficient, maybe vector + std::move would be better
{
  // check simple enclosure
  {
    int count = 0;
    for (int i=0; i<4; i++) {
      pti nb = point + coord.nb4[i];
      if ((tab[nb] & mask) == value) {
	count++;
      } else
	break;
    }
    if (count == 4) {
      std::vector<pti> border = { static_cast<pti>(point + coord.N), static_cast<pti>(point + coord.W),
				  static_cast<pti>(point + coord.S), static_cast<pti>(point + coord.E),
				  static_cast<pti>(point + coord.N) };
      return Enclosure( {point}, border );
    }
    // TODO: it'd be possible to optimise also for count==3 (then remove 'break' in for loop above)
  }
  std::array<pti, Coord::maxSize> stack;
  stack[0] = point;
  tab[point] |= MASK_MARK;
  int stackSize = 1;
  //std::vector<uint8_t> tab(coord.getSize(), 0);
  pti leftmost = Coord::maxSize;
  mask |= MASK_MARK;
  //  Cleanup<std::vector<pti>&, pti> cleanup(tab, ~MASK_MARK);
  CleanupUsingList<std::vector<pti>&, pti> cleanup(tab, ~(MASK_MARK | MASK_BORDER));
  cleanup.push(point);
  do {
    pti ind = stack[--stackSize];
    for (int i=0; i<4; i++) {
      pti nb = ind + coord.nb4[i];
      if ((tab[nb] & mask) == value) {
	if ((tab[nb] & MASK_BORDER) == 0) {  // a border dot not yet visited?
	  tab[nb] |= MASK_BORDER;
	  cleanup.push(nb);
	  if (nb < leftmost) leftmost = nb;
	}
      } else if ((tab[nb] & MASK_MARK) == 0) {
	if (coord.dist[nb] > 0) {  // interior point on the board (and not on the edge)
	  tab[nb] |= MASK_MARK;
	  stack[stackSize++] = nb;
	  cleanup.push(nb);
	} else {
	  // we got to the edge of the board!  note: cleaning done by 'cleanup' object
	  return empty_enclosure;
	}
      }
    }
  } while (stackSize);
  // traverse the border to find minimal-area enclosure
  stack[0] = leftmost + coord.NE;
  stack[1] = leftmost;
  stack[2] = coord.findNextOnRight(stack[1], stack[0]);
  tab[stack[0]] |= MASK_MARK;
  tab[stack[1]] |= MASK_MARK;
  int top = 2;
  do {
    // check if current point is on the border, if not, take next (clockwise)
    while ((tab[stack[top]] & MASK_BORDER) == 0) {
      stack[top] = coord.findNextOnRight(stack[top-1], stack[top]);
    }
    if (stack[top] == stack[0])  // enclosure found
      break;
    if (tab[stack[top]] & MASK_MARK) {
      // this point has been already visited, go back to the last visit of that point and try next neighbour
      pti loop_pt = stack[top];
      top--;
      pti prev_pt = stack[top];
      while (stack[top] != loop_pt) tab[stack[top--]] &= ~MASK_MARK;
      // now we are again at loop_pt which has been already unMARKed
      stack[++top] = coord.findNextOnRight(loop_pt, prev_pt);
    } else {
      // visit the point
      tab[stack[top]] |= MASK_MARK;
      top++;
      stack[top] = coord.findNextOnRight(stack[top-1], stack[top-2]);
    }
  } while (stack[top] != stack[0]);
  // now the enclosure is kept in stack[0..top], it has also (MARK | BORDER) bits set, but these will be cleared
  // by cleanup at exit
  // TODO: in most cases all interior points will be found, for example, if number of MARKed points is small
  //  (in such cases it'd be also possible to reserve optimal amount of memory)
  std::vector<pti> interior;
  interior.reserve(cleanup.count);  // reserve memory for interior found so far + border, which should be enough in most cases
  for (int i=0; i<cleanup.count; i++)
    if ((tab[cleanup.list[i]] & (MASK_MARK | MASK_BORDER)) != (MASK_MARK | MASK_BORDER)) {  // interior point
      interior.push_back(cleanup.list[i]);
      for (int j=0; j<4; j++) {
	pti nb = cleanup.list[i] + coord.nb4[j];
	if ((tab[nb] & (MASK_MARK | MASK_BORDER)) == 0) {
	  tab[nb] |= MASK_MARK;
	  cleanup.push(nb);
	}
      }
    }
  //std::sort(interior.begin(), interior.end());
  return Enclosure(std::move(interior), std::vector<pti>(&stack[0], &stack[top+1]));
}

float
Game::costOfPoint(pti p, int who) const
{
  assert(p>=coord.first && p<=coord.last);
  assert(coord.dist[p]>=0);
  if (threats[who-1].is_in_terr[p] > 0) return COST_INFTY;
  if (threats[2-who].is_in_terr[p] > 0) return 0.00001;
  if (threats[who-1].is_in_encl[p] > 0) return COST_INFTY;
  if (threats[2-who].is_in_encl[p] > 0) return 0.0001;
  if (whoseDotMarginAt(p) == who) return COST_INFTY;
  if (whoseDotMarginAt(p) == 3-who) return 0.00001;
  // here it should depend on the pattern...
  return 0.0001 + global::patt3_cost.getValue(pattern3_at[p], 3-who) * 0.001;
}

std::vector<pti>
Game::findImportantMoves(pti who)
{
  struct PointValue {
    pti point;
    float value;
    PointValue(pti p, float v) { point=p; value=v; };
    bool operator<(const PointValue& other) const { return value > other.value; }  // > to sort descending
  };
  std::vector<PointValue> moves;
  moves.reserve(coord.wlkx * coord.wlky - history.size() + 2);
  for (int p=coord.first+1; p<coord.last; p++) {
    if (whoseDotMarginAt(p) == 0 &&
	threats[0].is_in_terr[p]==0 && threats[0].is_in_encl[p]==0 &&
	threats[1].is_in_terr[p]==0 && threats[1].is_in_encl[p]==0) {
      float opp_value, our_value, opp_value2;
      CleanupOneVar<pti> worm_where_cleanup0(&worm[p], who);   //  worm[where0] = who;  with restored old value (0) by destructor
      CleanupUsingListOfValues<decltype(pattern3_at)&, pattern3_t> pattern3_cleanup(pattern3_at);
      {
	// change patterns in the neighbourhood of ind
	pattern3_cleanup.push(p, pattern3_at[p]);
	pattern3_at[p] = getPattern3_at(p);
	for (int i=0; i<8; i++) {
	  pti nb = p + coord.nb8[i];
	  pattern3_cleanup.push(nb, pattern3_at[nb]);
	  pattern3_at[nb] = getPattern3_at(nb);
	}
	our_value = floodFillCost(who);
	opp_value = floodFillCost(3-who);
      }
      {
	worm[p] = 3-who;
	// change patterns in the neighbourhood of ind
	pattern3_at[p] = getPattern3_at(p);
	for (int i=0; i<8; i++) {
	  pti nb = p + coord.nb8[i];
	  pattern3_at[nb] = getPattern3_at(nb);
	}
	opp_value2 = floodFillCost(3-who);
      }
      //std::cerr << coord.showPt(p) << ": " << our_value << ", opp: " << opp_value << ", opp2: " << opp_value2 <<std::endl;
      moves.push_back(PointValue(p, our_value-opp_value+opp_value2));
    }
  }
  std::vector<pti> list(coord.getSize(), 0);
  if (moves.size() <= 1) return list;
  std::sort(moves.begin(), moves.end());
  int ten = (moves.size() / 10);
  if (ten < 3) ten = moves.size() / 3;
  if (ten == 0) ten = 1;
  float median = (moves.size() & 1) ? moves[moves.size() / 2].value : (moves[moves.size() / 2 - 1].value + moves[moves.size() / 2].value)*0.5;
  float maxi = moves[0].value;
  const float shift = 0.1;
  float dist_rec_points = 7.01 / (maxi - median + shift);
  for (unsigned i=0; i<moves.size(); i++) {
    int no = int( (moves[i].value - median + shift)  * dist_rec_points );
    if (no >= 1) {
      list[moves[i].point] = no;
    } else break;
  }
  for (int i=moves.size()-1; i>=0; i--) {
    int no = int( (median - moves[i].value + shift)  / dist_rec_points );
    if (no >= 1) {
      list[moves[i].point] = -1;
    } else break;
  }

  /*
  std::cerr << "Plansza: " << std::endl;
  show();
  std::cerr << "Ciekawe ruchy dla gracza " << who << ": " << ten*2 << std::endl;
  std::cerr << coord.showBoard(list);
  */
  return list;
}


/// who is the attacker, 3-who defends
/// @return  Value of the territory of 'who' (bigger = better for 'who').
float
Game::floodFillCost(int who) const
{
  struct PointCost {
    pti point;
    float cost;
    PointCost(pti p, float c) { point=p; cost=c; };
    bool operator<(const PointCost& other) const { return cost > other.cost; }  // warning: > to make 'cheaper' points of higher priority!
  };
  struct PointCostHigh : PointCost {
    PointCostHigh(pti p, float c) : PointCost(p,c) {}; // { point=p; cost=c; };
    bool operator<(const PointCost& other) const { return cost < other.cost; }
  };
  std::priority_queue<PointCost> queue;
  std::priority_queue<PointCostHigh> all_points;
  std::vector<float> costs(coord.getSize(), 0.0);
  std::vector<pti> dad(coord.getSize(), 0);
  // left and right edge
  {
    int lind = coord.ind(0, 0);
    int rind = coord.ind(coord.wlkx-1, 0);
    for (int j=0; j<coord.wlky; j++) {
      auto c = costOfPoint(lind, who);
      if (c < COST_INFTY) {
	queue.push(PointCost(lind, c));
	all_points.push(PointCostHigh(lind, c));
	costs[lind] = c;
	dad[lind] = lind;
      }
      c = costOfPoint(rind, who);
      if (c < COST_INFTY) {
	queue.push(PointCost(rind, c));
	all_points.push(PointCostHigh(rind, c));
	costs[rind] = c;
	dad[rind] = rind;
      }
      lind += coord.S;
      rind += coord.S;
    }
  }
  // top and bottom edge
  {
    int tind = coord.ind(1, 0);
    int bind = coord.ind(1, coord.wlky-1);
    for (int i=2; i<coord.wlkx; i++) {  // 2,...,coord.wlkx-1, to omit the corners
      auto c = costOfPoint(tind, who);
      if (c < COST_INFTY) {
	queue.push(PointCost(tind, c));
	all_points.push(PointCostHigh(tind, c));
	costs[tind] = c;
	dad[tind] = tind;
      }
      c = costOfPoint(bind, who);
      if (c < COST_INFTY) {
	queue.push(PointCost(bind, c));
	all_points.push(PointCostHigh(bind, c));
	costs[bind] = c;
	dad[bind] = bind;
      }
      tind += coord.E;
      bind += coord.E;
    }
  }
  // now flood fill
  while (!queue.empty()) {
    auto pc = queue.top();    queue.pop();
    for (int i=0; i<4; i++) {
      pti nb = pc.point + coord.nb4[i];
      if (dad[nb]==0 && coord.dist[nb] >= 1) {  // edge has been already visited, therefore >=1, not 0
	auto c = costOfPoint(nb, who);
	if (c < COST_INFTY) {
	  c += pc.cost;
	  queue.push(PointCost(nb, c));
	  all_points.push(PointCostHigh(nb, c));
	  costs[nb] = c;
	  dad[nb] = pc.point;
	}
      }
    }
  }

  // check value of points
  /*
  std::vector<pti> tree_size(coord.getSize(), 0);
  std::vector<float> tree_cost(coord.getSize(), 0.0);
  //  std::vector<pti> interesting_moves;
  //  std::vector<pti> debug_board(coord.getSize(), 0);    // show interesting moves
  //  std::vector<pti> debug_2(coord.getSize(), 0);
  //  std::vector<pti> debug_tc(coord.getSize(), 0);
  while (!all_points.empty()) {
    auto pc = all_points.top();    all_points.pop();
    tree_size[pc.point]++;
    tree_cost[pc.point] *= 0.6;
    tree_cost[pc.point] += costs[pc.point];
    if (dad[pc.point] != pc.point) {
      tree_cost[dad[pc.point]] += tree_cost[pc.point];
      tree_size[dad[pc.point]] += tree_size[pc.point];
    }

    //    debug_tc[pc.point] = std::min(int(100*tree_cost[pc.point]), 200);
    //
    //    if (whoseDotMarginAt(pc.point) == 0 &&
    //	tree_size[pc.point] >= 4 &&
    //	(costs[pc.point] <= 0.05 || costs[dad[pc.point]] <= 0.05) &&
    //	tree_cost[pc.point] >= 0.4) {
    //      interesting_moves.push_back(pc.point);
    //      debug_board[pc.point] = who;
    //    }
  }
  */
  float value = 0.0;
  for (int p=coord.first; p<=coord.last; p++) {
    if ((whoseDotMarginAt(p) == 0 || whoseDotMarginAt(p) == (who^3)) && coord.dist[p]>=1) {
      if (threats[who-1].is_in_terr[p] > 0 || threats[who-1].is_in_encl[p] > 0) {
	if (whoseDotMarginAt(p) == 0)
	  value += 0.5;
	else             // dot of 3-who
	  value += 1.0;  // this is simplified, it may be an empty point inside who's enclosure, which is not worth 1 point
      } else if (costs[p] >= 0.1) {
	for (int i=0; i<4; i++) {
	  int nb = p + coord.nb4[i];
	  if (whoseDotMarginAt(nb) == who || costs[nb] >= 0.01 || threats[who-1].is_in_terr[nb] > 0 || threats[who-1].is_in_encl[nb] > 0) {
	    // ok
	  } else {
	    goto cost_outside;
	  }
	}
	{
	  float weight = std::min(costs[p], 1.0f);
	  value += (whoseDotMarginAt(p) == 0) ? 0.5*weight : weight;
	}
      cost_outside:;
      }
    }
  }
  return value;
  /*
  std::cerr << "Plansza: " << std::endl;
  show();
  std::cerr << "Ciekawe ruchy dla gracza " << who << ": " << interesting_moves.size() << std::endl;
  std::cerr << coord.showBoard(debug_board);
  for (int i=coord.first; i<=coord.last; i++) {
    debug_2[i] = std::min(int(100*costs[i]), 200);
    if (dad[i]!=0) {
      if (dad[i] == i) dad[i] = 1;
      else
	for (int j=0; j<4; j++)
	  if (dad[i]-i == coord.nb4[j]) dad[i] = (j+1)*10;
    }
  }
  std::cerr << "costs: " << std::endl << coord.showBoard(debug_2) << std::endl;
  std::cerr << "tree costs: " << std::endl << coord.showBoard(debug_tc) << std::endl;
  std::cerr << "dad: " << std::endl << coord.showBoard(dad) << std::endl;
  */
}

int
Game::floodFillExterior(std::vector<pti> &tab, pti mark_by, pti stop_at) const
// Marks points in tab by mark_by, flooding from exterior and stopping if (tab[...] & stop_at).
// Returns number of marked points (inside the board).
{
  std::array<pti, Coord::maxSize> stack;
  int stackSize = 0;
  int count = 0;
  pti test_mask = (mark_by | stop_at);
  // left and right edge
  {
    int lind = coord.ind(0, 0);
    int rind = coord.ind(coord.wlkx-1, 0);
    for (int j=0; j<coord.wlky; j++) {
      if ((tab[lind] & test_mask) == 0) {
	tab[lind] |= mark_by;
	stack[stackSize++] = lind;
	count++;
      }
      if ((tab[rind] & test_mask) == 0) {
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
    int bind = coord.ind(0, coord.wlky-1);
    for (int i=0; i<coord.wlkx; i++) {
      if ((tab[tind] & test_mask) == 0) {
	tab[tind] |= mark_by;
	stack[stackSize++] = tind;
	count++;
      }
      if ((tab[bind] & test_mask) == 0) {
	tab[bind] |= mark_by;
	stack[stackSize++] = bind;
	count++;
      }
      tind += coord.E;
      bind += coord.E;
    }
  }
  // now mark exterior
  while (stackSize) {
    pti p = stack[--stackSize];
    for (int j=0; j<4; j++) {
      pti nb = p + coord.nb4[j];
      if (coord.dist[nb] >= 0 && (tab[nb] & test_mask) == 0) {
	tab[nb] |= mark_by;
	stack[stackSize++] = nb;
	count++;
      }
    }
  }
  return count;
}


Enclosure
Game::findInterior(std::vector<pti> border) const
// in: list of border points (for example, from an sgf file -- therefore, the function needs not be fast)
// out: Enclosure class with border and interior
// v131: corrected, old version did not work for many types of borders (when there's a hole between enclosures).
{
  std::vector<pti> dad(coord.getSize(), 0);
  std::vector<pti> dad2(coord.getSize(), 0);
  // mark border id dad
  for (unsigned i=0; i<border.size()-1; ++i) {
    auto b = border[i];
    auto next = border[i+1];
    dad[b] = next;
    dad2[next] = b;
  }
  std::vector<pti> interior;
  interior.reserve(border.size() < 12 ? 12 : (coord.wlkx-2) * (coord.wlky-2) );
  for (int i=0; i<coord.wlkx; ++i) {
    int intersections = 0;
    int fromWhere = 0;
    pti ind = coord.ind(i, 0);
    for (int j=0; j<coord.wlky; ++j, ind += coord.S) {
      if (dad[ind]) {
	auto d = (coord.x[dad[ind]] - i) + (coord.x[dad2[ind]] - i);
	if (fromWhere == 0) { // recently we were NOT on the border
	  if (d==0) {
	    intersections ^= 1;
	  } else if (d == -1 || d == 1) {
	    fromWhere = d;
	  }
	} else if (d) {  // recently we were on the border
	  if (d != fromWhere) intersections ^= 1;
	  fromWhere = 0;  // we get out from the border
	}
      } else if (intersections) {
	interior.push_back(ind);
      }
    }
  }
  return Enclosure(std::move(interior), std::move(border));
}

void
Game::wormMergeAny(pti dst, pti src)
// merge worm number 'src' to 'dst',
{
  if ((dst & MASK_DOT) == (src & MASK_DOT))
    wormMergeSame(dst, src);
  else
    wormMergeOther(dst, src);
}

void
Game::wormMergeOther(pti dst, pti src)
// note: removing (src) may disconnect some other worms,
//  but this is NOT recalculated. So after removing all enemy worms
//  one must check again which are connected. However, neighbour's list is updated.
{
  if ((dst & MASK_DOT) == 1) {
    score[0].dots += descr.at(src).dots[1];
    score[1].dots -= descr.at(src).dots[0];
  } else {
    score[0].dots -= descr.at(src).dots[1];
    score[1].dots += descr.at(src).dots[0];
  }
  // remove (src) from all its former neighbours
  for (auto n : descr.at(src).neighb) {
    descr.at(n).neighb.erase( std::find( descr.at(n).neighb.begin(), descr.at(n).neighb.end(), src) );
  }
  wormMerge_common(dst, src);
}

void
Game::wormMergeSame(pti dst, pti src)
{
  WormDescr &descr_src = descr.at(src);
  WormDescr &descr_dst = descr.at(dst);
  for (auto n : descr_src.neighb) {
    if (n == dst ||       // remove (src) from (n)==(dst)'s neighbours, note: src is not always a neighbour of dst
	std::find( descr_dst.neighb.begin(), descr_dst.neighb.end(), n) != descr_dst.neighb.end()) {  	// n was already a neighbour of (dst), so just remove (src) as n's neighbour
      descr.at(n).neighb.erase( std::find( descr.at(n).neighb.begin(), descr.at(n).neighb.end(), src) );
    } else {
      // n!=dst was not a neighbour of (dst), so replace (src) to (dst) as n's neighbour and add (n) as a new dst's neighbour
      std::replace( descr.at(n).neighb.begin(), descr.at(n).neighb.end(), src, dst );
      descr_dst.neighb.push_back(n);
    }
  }
  // if (dst) and (src) were in different groups, merge them
  if (descr_src.group_id != descr_dst.group_id) {
    pti old_gid = descr_src.group_id;
    pti new_gid = descr_dst.group_id;
    for (auto &d : descr) {
      if (d.second.group_id == old_gid) d.second.group_id = new_gid;
    }
    connectionsRenameGroup(new_gid, old_gid);
  }
  // merge opp_threats:  we do not need to do anything, because the threats
  // that will remain are in both lists. Those in only one list will be removed anyway.
  // ...
  // common part
  wormMerge_common(dst, src);
}

void
Game::wormMerge_common(pti dst, pti src)
// common part of MergeOther and MergeSame
{
  WormDescr &descr_src = descr.at(src);
  WormDescr &descr_dst = descr.at(dst);
  pti leftmost = descr_src.leftmost;
  pti x = leftmost;
  do {
    worm[x] = dst;
    x=nextDot[x];
  } while (x!=leftmost);
  descr_dst.dots[0] += descr_src.dots[0];
  descr_dst.dots[1] += descr_src.dots[1];
  pti n = nextDot[ descr_dst.leftmost ];
  nextDot[ descr_dst.leftmost ] = nextDot[ descr_src.leftmost ];
  nextDot[ descr_src.leftmost ] = n;
  descr_dst.leftmost = std::min(descr_dst.leftmost, leftmost );
  descr_dst.safety += descr_src.safety;   // note: this is possible only when safety has at least 32 bits, otherwise we should check for overflow
  descr.erase(src);
}

void
Game::makeEnclosure(const Enclosure& encl, bool remove_it_from_threats)
// remove_it_from_threats == true: 'encl' has been already marked as TO_REMOVE in threats and will be removed
//   == false:  if 'encl' is in threats, it has to be found.
{
  pti worm_no = worm[encl.getBorderElement()];
  int who = (worm_no & MASK_DOT);
  bool is_inside_terr = true, is_inside_terr_or_encl = true, is_inside_some_encl = false, is_in_our_terr_or_encl = true;
#ifdef DEBUG_SGF
  sgf_tree.makePartialMove_addEncl(encl.toSgfString());
#endif
  bool update_safety_dame = false;
  std::set<std::pair<pti,pti>> singular_worms{};
  bool some_worms_were_not_singular = false;
  auto updateSingInfo = [&](pti leftmost) {
			  if (threats[2-who].is_in_encl[leftmost] + threats[2-who].is_in_terr[leftmost] == 1) {
			    // this worm was singular
			    singular_worms.insert({leftmost, descr.at(worm[leftmost]).dots[who-1]});
			  } else {
			    some_worms_were_not_singular = true;
			  }
			};
  updateSingInfo(descr.at(worm_no).leftmost);
  for (auto &p : encl.border) {
    if (worm[p] != worm_no) {
      // new worm on the border, merge it
      updateSingInfo(descr.at(worm[p]).leftmost);
      if ((descr.at(worm[p]).safety <= 1 || descr.at(worm_no).safety <= 1) && (descr.at(worm[p]).safety + descr.at(worm_no).safety >= 2)) {
	update_safety_dame = true;
      }
      wormMergeSame(worm_no, worm[p]);
    }
    if (threats[2-who].is_in_terr[p]==0) {
      is_inside_terr = false;
      if (threats[2-who].is_in_encl[p]==0) is_inside_terr_or_encl = false;
    }
    if (threats[2-who].is_in_encl[p]) is_inside_some_encl = true;
    if (threats[who-1].is_in_encl[p]==0 && threats[who-1].is_in_terr[p]==0) is_in_our_terr_or_encl = false;
  }
  // if some worms were singular, and some not, it means that the enclosure removed some of the opp's threats,
  // to make this removal calculate singular dots correctly, we need to remove current singular, see game szkrab6492 (last red move)
  if (some_worms_were_not_singular and not singular_worms.empty()) {
    for (auto [leftmost, dots] : singular_worms) {
      threats[2-who].findThreatWhichContains(leftmost)->singular_dots -= dots;
    }
  }
  //
  bool check_encl = (is_inside_terr_or_encl && is_inside_some_encl);
  pti first=0, last=0;  // of the list of empty interior points
  int empty_count = 0;  // number of empty interior points
  int enemy_dots = 0;   // number of enemy dots inside that have not been yet captured, important only when (is_in_our_terr_or_encl == true)
  std::vector<pti> gids_to_delete, stack;
  for (auto &p : encl.interior) {
    if (worm[p] == 0) {
      empty_count++;
      if (first) {
	nextDot[last] = p;
	last = p;
      } else {
	first = last = p;
      }
      worm[p] = worm_no;
    } else if (worm[p] != worm_no) {
      if ((worm[p] & MASK_DOT) != who) {
	enemy_dots += descr[worm[p]].dots[2-who];
	if (descr[worm[p]].neighb.size() > 1) {
	  // append if not there
	  if (std::find(gids_to_delete.begin(), gids_to_delete.end(), descr[worm[p]].group_id) == gids_to_delete.end())
	    gids_to_delete.push_back( descr[worm[p]].group_id );
	}
	wormMergeOther(worm_no, worm[p]);
      } else
	wormMergeSame(worm_no, worm[p]);
    }
  }
  // add the empty interior points, if any
  if (first) {
    pti next = nextDot[ encl.getBorderElement() ];
    nextDot[ encl.getBorderElement() ] = first;
    nextDot[ last ] = next;
    if (is_inside_terr_or_encl) {
      assert(threats[2-who].is_in_terr[first] > 0 || threats[2-who].is_in_encl[first] > 0);
      for (auto &t : threats[2-who].threats) {
	if (t.encl->isInInterior(first)) {
	  t.terr_points -= empty_count;
	}
      }
    }
  }
  // recalculate opponents groups, if needed
  if (!gids_to_delete.empty()) {
    for (auto &d : descr) {
      if (std::find(gids_to_delete.begin(), gids_to_delete.end(), d.second.group_id) != gids_to_delete.end())
	d.second.group_id = 0;
    }
    // go
    for (auto &d : descr) {
      if (d.second.group_id == 0) {
	pti id = d.first;
	stack.push_back( d.first );
	d.second.group_id = id;
	while (!stack.empty()) {
	  pti cmp = stack.back();  stack.pop_back();
	  for (auto n : descr.at(cmp).neighb)
	    if (descr.at(n).group_id == 0) {
	      descr.at(n).group_id = id;
	      stack.push_back(n);
	    }
	}
      }
    }
  }
  // mark interior of current enclosure
  stack.assign(coord.last+1, 0);
  for (auto &p : encl.interior) {
    stack[p] = 1;
    pattern3_value[0][p] = 0;  // and zero pattern3 values
    pattern3_value[1][p] = 0;
    possible_moves.changeMove(p, PossibleMovesConsts::REMOVED);
    interesting_moves.changeMove(p, InterestingMovesConsts::REMOVED);
  }
  if (update_safety_dame) {
    possibleMoves_updateSafetyDame();
  }
  // remove our threats
  for (auto &t: threats[who-1].threats) {
    if ((t.type & ThreatConsts::TO_REMOVE)==0 && stack[t.where] == 0) {
      if (std::any_of(t.encl->border.begin(), t.encl->border.end(),
		      [&stack](auto p) { return stack[p]; }) or
	  std::all_of(t.encl->interior.begin(), t.encl->interior.end(),
		      [&stack](auto p) { return stack[p]; })) {
	t.type |= ThreatConsts::TO_REMOVE;
      }
      if ((t.type & ThreatConsts::TO_REMOVE) == 0 && is_in_our_terr_or_encl) {
	// update t, if it happens to include made enclosure
	// first fast check -- if it does not contain one interior point, it does not include enclosure encl, so we're done
	if (t.encl->isInInterior(encl.getInteriorElement())) {
	  // ok, fast check positive, it contains one interior point, maybe it's enough?
	  bool contains = true;
	  if (encl.interior.size() > 1) {
	    // interior is bigger than 1 point, need to check carefully
	    // we want to do it faster than by m*n operations (m,n -- interior sizes of t.encl, encl)
	    unsigned count = 0;
	    for (auto &p : t.encl->interior) count += stack[p];
	    contains = (count == encl.interior.size());
	  }
	  if (contains) {
	    t.terr_points -= empty_count;
	    t.opp_dots    -= enemy_dots;	  // captured enemy dots were not singular, so no need to update singular_dots
	  }
	}
      }
    } else {
      t.type |= ThreatConsts::TO_REMOVE;
    }
  }
  // remove our threats in 2 moves
  for (auto &t2: threats[who-1].threats2m) {
    for (auto &t : t2.thr_list) {
      if ((t.type & ThreatConsts::TO_REMOVE)==0 && stack[t.where] == 0 && stack[t2.where0] == 0) {
	for (auto &p : t.encl->border) {
	  if (stack[p]) {
	    t.type |= ThreatConsts::TO_REMOVE;
	    break;
	  }
	}
      } else {
	t.type |= ThreatConsts::TO_REMOVE;
      }
    }
  }
  // remove current enclosure, if needed
  if (remove_it_from_threats) {
    auto zobr = encl.zobristKey(who);
    for (auto &t: threats[who-1].threats) {
      if (t.zobrist_key == zobr) {
	t.type |= ThreatConsts::TO_REMOVE;
      }
    }
  }
  // remove
  removeMarked(who);
  threats[who-1].removeMarked2moves();
  // remove opponent threats
  //std::cerr << "remove opp threats..." << std::endl;
  bool removed = false;
  for (unsigned tn=0; tn<threats[2-who].threats.size(); tn++) {
    Threat *thr = &threats[2-who].threats[tn];
    if ((thr->type & ThreatConsts::TO_REMOVE)==0) {
      if (stack[thr->where] == 1) {
	thr->type |= ThreatConsts::TO_REMOVE;   removed = true;
	//std::cerr << "removing " << coord.showPt(thr->where) << std::endl;
      } else {
	std::shared_ptr<Enclosure> encl = thr->encl;     // thr may be invalidated by adding new threats in checkThreat_terr
	for (auto &p : encl->border) {
	  if (stack[p]) {
	    thr->type |= ThreatConsts::TO_REMOVE;   removed = true;
	    //std::cerr << "removing2 " << coord.showPt(thr->where) << ", zobr=" << thr->zobrist_key << std::endl;
	    if (check_encl && (thr->type & ThreatConsts::ENCL)) {
	      checkThreat_encl(thr, 3-who);
	      thr = &threats[2-who].threats[tn];  // thr could be invalidated
	    }
	    break;
	  }
	}
	if ((thr->type & ThreatConsts::TO_REMOVE) == 0 && thr->encl->isInInterior(encl->getBorderElement())) {
	  auto tmp = countDotsTerrInEncl(*thr->encl, who);
	  thr->opp_dots = std::get<0>(tmp);
	  thr->terr_points = std::get<1>(tmp);
	}
      }
    } else removed = true;
  }

  if (is_inside_terr) {
    checkThreat_terr(nullptr, encl.getBorderElement(), 3-who);
  }
  if (removed) {
    removeMarked(3-who);
  }
  // remove opponent threats in 2 moves
  bool removed2 = false;
  for (auto &thr2t : threats[2-who].threats2m) {
    if (stack[thr2t.where0] == 1) {
      for (auto &t : thr2t.thr_list) {
	t.type |= ThreatConsts::TO_REMOVE;
      }
      removed2 = true;
    } else {
      for (unsigned tn=0; tn < thr2t.thr_list.size(); tn++) {
	Threat *thr = &thr2t.thr_list[tn];
	if (stack[thr->where] == 1) {
	  thr->type |= ThreatConsts::TO_REMOVE;   removed2 = true;
	} else {
	  std::shared_ptr<Enclosure> encl = thr->encl;     // thr may be invalidated by adding new threats in checkThreat_terr
	  for (auto &p : encl->border) {
	    if (stack[p]) {
	      removed2 = true;
	      // thr->type |= ThreatConsts::TO_REMOVE;  // in checkThreat2moves_encl
	      // if (check_encl2moves) { ...
	      checkThreat2moves_encl(thr, thr2t.where0, 3-who);
	      //thr2 = &threats[2-who].threats2m[tn2];  // thr2 and
	      //thr = &thr2->thr_list[tn];              // thr could be invalidated (when using vectors for threats2m[]
	      break;
	    }
	  }
	}
      }
    }
  }
  if (removed2) {
    threats[2-who].removeMarked2moves();
  }
  // recalculate points outside the enclosure, and mark the border in stack
  for (auto &p : encl.border) {
    stack[p] = 1;  // mark additionally the border
    for (int i=0; i<4; i++) {
      auto nb = p + coord.nb4[i];
      assert( !(whoseDotMarginAt(nb) == 0 && stack[nb] != 0) );
      if (whoseDotMarginAt(nb) == 0 // && stack[nb] == 0  (always satisfied, when whoseDotMarginAt(nb) == 0)
	  && Pattern3::isPatternPossible(pattern3_at[nb])) {
	pattern3_at[nb] = PATTERN3_IMPOSSIBLE;
	recalculate_list.push_back(nb);
      }
    }
  }
  // remove connections
  for (auto &p : encl.interior) {
    connects[0][p] = OneConnection();
    connects[1][p] = OneConnection();
    // check diagonal neighbours outside the enclosure
    for (int i=0; i<8; i+=2) {  // +=2, to visit only diagonal neighb.
      pti nb = p + coord.nb8[i];
      if (stack[nb]==0)	{
	connectionsRecalculatePoint(nb, 3-who);
      }
    }
  }
  if (!gids_to_delete.empty()) {
    for (int ind=coord.first; ind<=coord.last; ind++) {
      for (int j=0; j<4; j++) {
	if (connects[2-who][ind].groups_id[j] == 0) break;
	if (std::find(gids_to_delete.begin(), gids_to_delete.end(), connects[2-who][ind].groups_id[j]) != gids_to_delete.end()) {
	  connectionsRecalculateConnect(ind, 3-who);
	  //connectionsRecalculatePoint(ind, 3-who);
	  break;
	}
      }
    }
  }
}



std::pair<int, int>
Game::countTerritory(int now_moves) const
{
  const int ct_B = 1;
  const int ct_W = 2;
  const int ct_NOT_TERR_B = 4;
  const int ct_NOT_TERR_W = 8;
  const int ct_TERR_B = 0x10;
  const int ct_TERR_W = 0x20;
  const int ct_COUNTED = 0x40;
  assert(((ct_B | ct_W | ct_NOT_TERR_B | ct_NOT_TERR_W | ct_TERR_B | ct_TERR_W | ct_COUNTED) & (MASK_MARK | MASK_BORDER)) == 0);  // MASK_MARK and _BORDER should be different
  //
  std::vector<pti> marks(coord.getSize(), 0);
  for (int i = coord.first; i<=coord.last; i++)
    marks[i] = whoseDotAt(i);
  floodFillExterior(marks, ct_NOT_TERR_B, ct_B);
  floodFillExterior(marks, ct_NOT_TERR_W, ct_W);
  // remove B dots inside W terr, and vice versa, they cannot be used as border of pools
  /*  (not needed, we may use masks in findEnclosure())
  for (int i = coord.first; i<=coord.last; i++) {
    if ((marks[i] & (ct_B | ct_NOT_TERR_W)) == ct_B)
      marks[i] &= ~ct_B;
    else if ((marks[i] & (ct_W | ct_NOT_TERR_B)) == ct_W)
      marks[i] &= ~ct_W;
  }
  */
  // for each point masked as possibly inside a territory, try to enclose it
  //std::cerr << coord.showBoard(marks) << std::endl;

  std::vector<Enclosure> poolsB, poolsW;
  for (int i = coord.first; i<=coord.last; i++) {
    if ((marks[i] & (ct_B | ct_NOT_TERR_B)) == 0 && coord.dist[i]>=0) {
      // for enclosures, do not use B dots which are inside W's terr
      auto encl = findEnclosure(marks, i, ct_B | ct_NOT_TERR_W, ct_B | ct_NOT_TERR_W);
      if (!encl.isEmpty()) {
	poolsB.push_back(encl);
      }
    }
    if ((marks[i] & (ct_W | ct_NOT_TERR_W)) == 0 && coord.dist[i]>=0) {
      // for enclosures, do not use W dots which are inside B's terr
      auto encl = findEnclosure(marks, i, ct_W | ct_NOT_TERR_B, ct_W | ct_NOT_TERR_B);
      if (!encl.isEmpty()) {
	poolsW.push_back(encl);
      }
    }
  }
  // count points ignoring pools that are included in bigger pools
  int delta_score[4] = {0,0,0,0};   // dots of 0,1, terr of 0,1
  for (auto &e : poolsB) {
    bool ok = true;
    for (pti ind : e.border)
      if (marks[ind] & ct_TERR_B) {
	ok = false;  break;
      }
    if (ok) {
      for (pti ind : e.interior) {
	if ((worm[ind] & MASK_DOT) == 2 && (marks[ descr.at(worm[ind]).leftmost ] & ct_COUNTED) == 0) {
	  // take it
	  delta_score[0] += descr.at(worm[ind]).dots[1];
	  delta_score[1] -= descr.at(worm[ind]).dots[0];
	  marks[ descr.at(worm[ind]).leftmost ] |= ct_COUNTED;
	} else if ((worm[ind] & MASK_DOT) == 0 && (marks[ind] & ct_TERR_B) == 0) {
	  delta_score[2]++;
	  marks[ind] |= ct_TERR_B;
	}
      }
      // TODO: if (must-surround)...
    }
  }
  // for W
  for (auto &e : poolsW) {
    bool ok = true;
    for (pti ind : e.border)
      if (marks[ind] & ct_TERR_W) {
	ok = false;  break;
      }
    if (ok) {
      for (pti ind : e.interior) {
	if ((worm[ind] & MASK_DOT) == 1 && (marks[ descr.at(worm[ind]).leftmost ] & ct_COUNTED) == 0) {
	  // take it
	  delta_score[1] += descr.at(worm[ind]).dots[0];
	  delta_score[0] -= descr.at(worm[ind]).dots[1];
	  marks[ descr.at(worm[ind]).leftmost ] |= ct_COUNTED;
	} else if ((worm[ind] & MASK_DOT) == 0 && (marks[ind] & ct_TERR_W) == 0) {
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
  if ((delta_score[2]-delta_score[3]) % 2 == 0) {
    delta += (delta_score[2] - delta_score[3]) / 2;
  } else {
    int dame = 0;
    for (int i = coord.first; i<=coord.last; i++)
      if (coord.dist[i]>=0) {
	if ((marks[i] & (ct_B | ct_W)) == 0 && (marks[i] & (ct_NOT_TERR_B | ct_NOT_TERR_W)) == (ct_NOT_TERR_B | ct_NOT_TERR_W) ) dame++;
      }
    int correction;
    if ((dame + now_moves) % 2) {
      correction = (delta_score[2] - delta_score[3] - 1) / 2;
    }
    else {
      correction = (delta_score[2] - delta_score[3] + 1) / 2;
    }
    delta += correction;
    small_score = delta_score[2] - delta_score[3] - 2*correction;
  }
  //
  //std::cerr << "Terr-delta-score: " << delta << std::endl;
  return {(score[0].dots - score[1].dots) + delta, small_score};
}



/// Simple function for counting territory, assuming pools of other players do not intersect.
std::pair<int, int>
Game::countTerritory_simple(int now_moves) const
{
  // count points ignoring pools that are included in bigger pools
  std::set<pti> marks;
  int delta_score[4] = {0,0,0,0};  // dots of 0,1, terr of 0,1
  int dame = 0;
  for (int ind=coord.first; ind<=coord.last; ++ind) {
    if (threats[0].is_in_terr[ind] > 0) {
      if (threats[1].is_in_terr[ind] > 0) {
	// one pool inside other, use the other function
	return countTerritory(now_moves);
      }
      if (worm[ind]) {
	if (whoseDotMarginAt(ind) == 2 && marks.find(worm[ind]) == marks.end()) {
	  marks.insert(worm[ind]);
	  delta_score[0] += descr.at(worm[ind]).dots[1];
	  delta_score[1] -= descr.at(worm[ind]).dots[0];
	}
      } else {
	delta_score[2]++;
      }
    } else if (threats[1].is_in_terr[ind] > 0) {
      if (worm[ind]) {
	if (whoseDotMarginAt(ind) == 1 && marks.find(worm[ind]) == marks.end()) {
	  marks.insert(worm[ind]);
	  delta_score[1] += descr.at(worm[ind]).dots[0];
	  delta_score[0] -= descr.at(worm[ind]).dots[1];
	}
      } else {
	delta_score[3]++;
      }
    } else if (whoseDotMarginAt(ind)==0) {
      dame++;
    }
  }
  // calculate the score assuming last-dot-safe==false
  delta_score[3] += global::komi;
  int delta = (delta_score[0] - delta_score[1]);
  int small_score = 0;
  if ((delta_score[2]-delta_score[3]) % 2 == 0) {
    delta += (delta_score[2] - delta_score[3]) / 2;
  } else {
    int correction;
    if ((dame + now_moves) % 2) {
      correction = (delta_score[2] - delta_score[3] - 1) / 2;
    }
    else {
      correction = (delta_score[2] - delta_score[3] + 1) / 2;
    }
    delta += correction;
    small_score = delta_score[2] - delta_score[3] - 2*correction;
  }
  //
  //std::cerr << "Terr-delta-score: " << delta << std::endl;
#ifndef NDEBUG
  auto ct_score = countTerritory(now_moves);
  if ((score[0].dots - score[1].dots) + delta != ct_score.first or small_score != ct_score.second) {
    show();
    std::cerr << "res = " << (score[0].dots - score[1].dots) + delta << ", should be = " << ct_score.first << std::endl;
    std::cerr << "delta_score: " << delta_score[0] << ", " << delta_score[1] << ", " << delta_score[2] << ", " << delta_score[3] << ", komi=" << global::komi << std::endl;
    std::cerr << "small_score: " << small_score << ", should be = " << ct_score.second << std::endl;
    //
    std::vector<pti> th(coord.getSize(), 0);
    for (int ind=coord.first; ind<=coord.last; ++ind) {
      if (threats[0].is_in_terr[ind] > 0) {
	th[ind] += 1;
      }
      if (threats[1].is_in_terr[ind] > 0) {
	th[ind] += 2;
      }
    }
    std::cerr << coord.showBoard(th);
    //assert(0);
  }
#endif
  return {(score[0].dots - score[1].dots) + delta, small_score};
}



/// Counts:
///   * dots of who inside worms belonging to who and contained in encl
///   * empty points inside encl
std::pair<int16_t, int16_t>
Game::countDotsTerrInEncl(const Enclosure& encl, int who, bool optimise) const
{
  int16_t count = 0;
  int16_t terr = 0;
  if (optimise && encl.interior.size() <= 4) {  // if <=4, then there's no other enclosure inside encl
    for (pti p : encl.interior) {
      if (whoseDotMarginAt(p) == 0) {
	++terr;
      } else if (whoseDotMarginAt(p) == who) {
	++count;
      }
    }
    return std::make_pair(count, terr);
  }
  std::set<pti> counted;
  for (pti p : encl.interior) {
    if (whoseDotMarginAt(p) == 0)
      ++terr;
    else
      if (whoseDotMarginAt(p) == who && counted.find(worm[p]) == counted.end()) {
	counted.insert(worm[p]);
	count += descr.at(worm[p]).dots[who-1];
      }
  }
  return std::make_pair(count, terr);
}

int sgf_move_no = 0;

std::pair<Move, std::vector<std::string>>
Game::extractSgfMove(std::string m, int who) const
{
  // TODO: in must-surround, this must be done simultanously
  Move move;
  move.ind = coord.sgfToPti(m);
  if (whoseDotMarginAt(move.ind)) {
    throw std::runtime_error("makeSgfMove error: trying to play at an occupied point");
  }
#ifndef NDEBUG
  std::cerr << "[" << ++sgf_move_no << "] Make move " << who << " : " << m << " = " << coord.showPt(coord.sgfToPti(m)) << std::endl;
#endif

  move.who = who;
  std::vector<pti> border;
  std::vector<std::string> points_to_enclose;
  unsigned pos = 2;
  char mode = '.';
  while (pos<=m.length()) {
    if ((pos<m.length() && (m[pos] == '.' || m[pos] == '!')) || pos == m.length()) {
      // save last enclosure
      if (!border.empty()) {
	if (mode == '.') {
	  move.enclosures.push_back(std::make_shared<Enclosure>(findInterior(border)));
	}
	border.clear();
      }
      if (pos<m.length()) { mode = m[pos]; }
      ++pos;
    } else {
      if (mode == '.') {
	border.push_back( coord.ind( coord.sgfCoordToInt(m[pos]), coord.sgfCoordToInt(m.at(pos+1)) ));
      } else {
	points_to_enclose.push_back(m.substr(pos, 2));
      }
      pos+=2;
    }
  }
  return {move, points_to_enclose};
}

void
Game::makeSgfMove(std::string m, int who)
{
  auto [move, points_to_enclose] = extractSgfMove(m, who);
  if (points_to_enclose.empty()) {
    makeMove(move);
  } else {
    makeMoveWithPointsToEnclose(move, points_to_enclose);
  }


  //floodFillCost(nowMoves);
  //findImportantMoves(nowMoves);
  //std::this_thread::sleep_for(std::chrono::milliseconds(200));

  /*
  showSvg();
  std::this_thread::sleep_for(std::chrono::milliseconds(1200));
  */

  assert(checkThreatCorrectness());
  assert(checkThreat2movesCorrectness());
  assert(checkWormCorrectness());
  assert(checkConnectionsCorrectness());
  assert(checkPattern3valuesCorrectness());

  //#ifndef NDEBUG
  {
#ifdef DEBUG_SGF
    sgf_tree.finishPartialMove();
    if (1) {     // change to add marks for threats
      std::vector<pti> list;
      for (auto &t : threats[who-1].threats2m) {
	if (std::find(list.begin(), list.end(), t.where0) == list.end()) {
	  list.push_back(t.where0);
	}
      }
      SgfProperty prop;
      prop.first = "SQ";
      for (auto i : list) prop.second.push_back(coord.indToSgf(i));
      if (!prop.second.empty()) {
	sgf_tree.addProperty(prop);
	prop.second.clear();
      }
      list.clear();

      SgfProperty miai;
      for (int i=coord.first; i<=coord.last; i++) {
	if (threats[who-1].is_in_2m_encl[i]) {
	  prop.second.push_back(coord.indToSgf(i));
	}
	if (threats[who-1].is_in_2m_miai[i]) {
	  miai.second.push_back(coord.indToSgf(i));
	}
      }
      if (!prop.second.empty()) {
	prop.first = "TR";
	sgf_tree.addProperty(prop);
      }
      if (!miai.second.empty()) {
	miai.first = "MA";  // = X
	sgf_tree.addProperty(miai);
      }
    }
#endif
    showPattern3extra();

    /*
    std::vector<pti> list_of_moves;
    for (int i=0; i<coord.maxSize; i++) {
      list_of_moves.push_back(worm[i] & MASK_DOT);
      } */
    //generateListOfMoves(nullptr, who);
    //auto it = history.end();
    //--it;
    //Move m = choosePattern3Move((*it) & history_move_MASK, (*(it-1)) & history_move_MASK, 3-who);
    //Move m = chooseAnyMove(3-who);
#ifndef NDEBUG
    /*
    if (m.ind) {
      std::cerr << "Chosen move " << coord.showPt(m.ind) << " with encl.size()==" << m.enclosures.size() << std::endl;
      if (m.enclosures.size()) {
	for (auto e : m.enclosures) {
	  for (auto b : e->border) {
	    std::cerr << coord.showPt(b) << "  ";
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
    //std::cerr << "List of moves: " << std::endl << coord.showColouredBoard(list_of_moves) << std::endl;
  }
  //#endif


}


void
Game::makeMove(Move &m)
{
  if (m.ind != 0) {
    placeDot(coord.x[m.ind], coord.y[m.ind], m.who);
    for (auto &encl : m.enclosures) {
      makeEnclosure(*encl, true);
    }
    recalculatePatt3Values();
    nowMoves ^= 3;
  }
}

void
Game::makeMoveWithPointsToEnclose(Move &m, std::vector<std::string> to_enclose)
{
  if (m.ind != 0) {
    placeDot(coord.x[m.ind], coord.y[m.ind], m.who);
    for (auto &encl : m.enclosures) {
      makeEnclosure(*encl, true);
    }
    for (auto &e : to_enclose) {
      Enclosure encl = findEnclosure(coord.sgfToPti(e), MASK_DOT, nowMoves);
      if (!encl.isEmpty()) {
	makeEnclosure(encl, true);
      }
    }
    recalculatePatt3Values();
    nowMoves ^= 3;
  }
}


int encl_count, opt_encl_count, moves_count, priority_count;

void
Game::generateListOfMoves(TreenodeAllocator &alloc, Treenode *parent, int depth, int who)
{
#ifndef NDEBUG
  // check margins
  bool top=true, left=true, bottom=true, right=true;  // are margins empty?
  for (int i=1; i<coord.wlkx-1; i++) {
    int ind = coord.ind(i, 0);
    if (worm[ind] != 0 || worm[ind+coord.S] != 0) top = false;
    ind = coord.ind(i, coord.wlky-1);
    if (worm[ind] != 0 || worm[ind+coord.N] != 0) bottom = false;
  }
  if (worm[coord.ind(0,1)] || worm[coord.ind(coord.wlkx-1,1)]) top = false;
  if (worm[coord.ind(0,coord.wlky-2)] || worm[coord.ind(coord.wlkx-1,coord.wlky-2)]) bottom = false;
  for (int j=1; j<coord.wlky-1; j++) {
    int ind = coord.ind(0, j);
    if (worm[ind] != 0 || worm[ind+coord.E] != 0) left = false;
    ind = coord.ind(coord.wlkx-1, j);
    if (worm[ind] != 0 || worm[ind+coord.W] != 0) right = false;
  }
  if (worm[coord.ind(1,0)] || worm[coord.ind(1,coord.wlky-1)]) left = false;
  if (worm[coord.ind(coord.wlkx-2,0)] || worm[coord.ind(coord.wlkx-2,coord.wlky-1)]) right = false;
  assert(left == possible_moves.left);
  assert(right == possible_moves.right);
  assert(top == possible_moves.top);
  assert(bottom == possible_moves.bottom);
#else
  bool left = possible_moves.left;
  bool right = possible_moves.right;
  bool top = possible_moves.top;
  bool bottom = possible_moves.bottom;
#endif
  // get the list
  Treenode tn;
  tn.move.who = who;
  tn.parent = parent;
  getSimplifyingEnclAndPriorities(who);
  std::vector<std::shared_ptr<Enclosure> > neutral_encl_moves, neutral_opt_encl_moves;
  std::vector<uint64_t> neutral_encl_zobrists;
  getEnclMoves(neutral_encl_moves, neutral_opt_encl_moves, neutral_encl_zobrists, 0, who);
  tn.move.enclosures.reserve(ml_encl_moves.size() + neutral_encl_moves.size() + neutral_opt_encl_moves.size());
  // get list of good moves found by costs
  /*
  std::vector<pti> cost_moves;
  if (depth <= 2) {
    //std::cerr << "Depth: "  <<  depth << ", finding cost..." << std::endl;
    cost_moves = findImportantMoves(who);
  }
  */
  // get patt3extra boni
  std::vector<pti> patt3extrav = getPatt3extraValues();
  // debug:
  encl_count += ml_encl_moves.size();  opt_encl_count += ml_opt_encl_moves.size();  priority_count += ml_priorities.size();  moves_count++;
  //
  bool dame_already = false;   // to put only 1 dame move on the list
  for (int i=coord.first; i<=coord.last; i++) {
    //if (coord.dist[i] < 0) continue;
    if (worm[i]) continue;
#ifndef NDEBUG
    std::stringstream out;
    out << "Move " << coord.showPt(i) << ": ";
#endif
    bool is_dame = false;
    int x = coord.x[i], y = coord.y[i];
    if (x==0) {
      if (left || (worm[i+coord.E] && descr.at(worm[i+coord.E]).isSafe()) || (y==0 || y==coord.wlky-1)) is_dame = true;
    } else if (x==coord.wlkx-1) {
      if (right || (worm[i+coord.W] && descr.at(worm[i+coord.W]).isSafe()) || (y==0 || y==coord.wlky-1)) is_dame = true;
    }
    if (y==0) {
      if (top || (worm[i+coord.S] && descr.at(worm[i+coord.S]).isSafe())) is_dame = true;
    } else if (y==coord.wlky-1) {
      if (bottom || (worm[i+coord.N] && descr.at(worm[i+coord.N]).isSafe())) is_dame = true;
    }
    // check threats -- not good, it may be good to play in opp's territory to reduce it...
    //if ((threats[2-who].is_in_encl[i] > 0 || threats[2-who].is_in_terr[i] > 0) &&
    //  threats[who-1].is_in_border[i] == 0) continue;
    tn.move.ind = i;
    tn.t.playouts = 30;  tn.t.value_sum = 15;
    // add prior values according to Pattern3
    bool is_in_our_te = (threats[who-1].is_in_encl[i]>0 || threats[who-1].is_in_terr[i]>0);  // te = territory or enclosure
    bool is_in_opp_te = (threats[2-who].is_in_encl[i]>0 || threats[2-who].is_in_terr[i]>0);
    if (!is_dame && !is_in_our_te && !is_in_opp_te) {
      auto v = pattern3_value[who-1][i];
#ifndef NDEBUG
      out << "p3v=" << v << " ";
      if (v != getPattern3Value(i, who)) {
	std::cerr << "recalculate_list: ";
	for (auto i : recalculate_list) std::cerr << coord.showPt(i) << " ";
	std::cerr << std::endl;
	show();
	std::cerr << "Wrong patt3 value for who=" << who << " at " << coord.showPt(i) << ", is: " << v << " should be: " <<  getPattern3Value(i, who) << std::endl;
      }
      assert(v == getPattern3Value(i, who));
#endif
      if (v < 0) {            // dame
	is_dame = true;
      } else {
	int value = patt3extrav[i] * 3;  	// add prior values according to patt3extrav
	if (v > 0) {
	  value += (v + 7) >> 3;
	}
#ifndef NDEBUG
	out << "p3p=" << value << " ";
#endif
	tn.t.playouts += value;  tn.t.value_sum = tn.t.value_sum.load() + value;  // add won simulations
      }
    }
    // add prior values for edge moves
    if (!is_dame && coord.dist[i] == 0) {
      int r = checkBorderMove(i, who);
      if (r < 0) {
	is_dame = true;
      }
      else if (r > 0) {
#ifndef NDEBUG
	out << "edge=" << 3*r << " ";
#endif
	tn.t.playouts += 3*r;  tn.t.value_sum = tn.t.value_sum.load()+ 3*r;  // add won simulations (3--12)
      }
    }
    // save only 1 dame move
#ifndef NDEBUG
    if (is_dame != isDame_directCheck(i, who)) {
      std::cerr << "Move " << coord.showPt(i) << " for player=" << who << " is_dame: " << is_dame << ", but direct check shows " << !is_dame << std::endl;
      std::cerr << "Direct check for players: " << isDame_directCheck(i,1) << ", " << isDame_directCheck(i,2) << std::endl;
      assert(is_dame == isDame_directCheck(i, who));
    }
#endif
    if (is_dame) {
      if (dame_already) {
#ifndef NDEBUG
	if (parent->parent == parent) std::cerr << out.str() << " --dame already!" << std::endl;	
#endif
	continue;
      }
      dame_already = true;
#ifndef NDEBUG
      out << "dame=true,-5 ";
#endif
      tn.t.playouts += 5;    // add lost simulations
    }
    // add prior values according to Pattern52
    /* // turned off in v136
    if (coord.dist[i] == 0) {  // v136: inner turned off; v119: edge turned on  [before: on the edge Pattern52 are not that good, esp. that this situation is checked elsewhere]
      auto v = getPattern52Value(i, who);
      if (v < 0) {
	tn.t.playouts += 5;    // add lost simulations
      } else if (v > 0) {
	int count = 1 + (v*12);
	tn.t.playouts += count;  tn.t.value_sum += count;  // add won simulations
      }
    }
    */
    // add prior values according to influence value
    /*
    if (isInfluential(i, true)) {
      tn.t.playouts += 2;  tn.t.value_sum += 2;  // add won simulations
    }
    */
    // add prior values according to costs
    /*
    if (depth <= 2) {
      if (cost_moves[i] > 0) {
	tn.t.playouts += cost_moves[i];   tn.t.value_sum += cost_moves[i];  // add won simulations
      } else {
	tn.t.playouts -= cost_moves[i];  // add lost simulations
      }
    }
    */
    // add prior values according to InterestingMoves (cut/connect)
    {
      int w = interesting_moves.classOfMove(i);
      tn.t.playouts += 4*w;  tn.t.value_sum = tn.t.value_sum.load() + 4*w;  // add w=(0,1,2,3 times 3) won simulations
#ifndef NDEBUG
      out << "intm=" << 4*w << " ";
#endif

    }
    // add prior values according to distance from last moves
    {
      int dist = std::min(coord.distBetweenPts_1(i, history.back() & history_move_MASK), coord.distBetweenPts_1(i, (*(history.end()-2))  & history_move_MASK));
      if (dist <= 4) {
	tn.t.playouts += (6-dist);  tn.t.value_sum = tn.t.value_sum.load() + (6-dist);  // add won simulations
#ifndef NDEBUG
	out << "dist=" << 6-dist << " ";
#endif

      }
    }
    // add prior values because of threats2m (v118+)
    if (threats[who-1].is_in_terr[i] == 0 && !is_in_opp_te) {
      for (auto &t : threats[who-1].threats2m) {
	if (t.where0 == i) {
	  if (t.min_win2 && t.isSafe()) {
	    int num = 5 + std::min(int(t.min_win2), 15);
	    tn.t.playouts += num;  tn.t.value_sum = tn.t.value_sum.load() + num;  // add won simulations
#ifndef NDEBUG
	    out << "thr2m=" << num << " ";
#endif

	  }
	  break;
	}
      }
      for (auto &t : threats[2-who].threats2m) {
	if (t.where0 == i) {
	  if (t.min_win2 && t.isSafe()) { // && threats[2-who].is_in_terr[t.where0]==0 && threats[2-who].is_in_encl[t.where0]==0 -- this is above (!is_in_opp_te)
	    int num = 5 + std::min(int(t.min_win2), 15);
	    //tn.t.playouts += num;   // add lost simulations ???
	    tn.t.playouts += num;  tn.t.value_sum = tn.t.value_sum.load() + num;  // add won simulations
#ifndef NDEBUG
	    out << "thr2mopp=" << num << " ";
#endif

	  }
	  break;
	}
      }
      // miai/encl2 (v127+)
      if ((threats[2-who].is_in_2m_encl[i] > 0 || threats[2-who].is_in_2m_miai[i] > 0) && threats[who-1].is_in_border[i] == 0) {
	tn.t.playouts += 15;   // add lost simulations
#ifndef NDEBUG
	out << "miai=-15 ";
#endif

      }
    }
    // captures
    if (std::find(ml_special_moves.begin(), ml_special_moves.end(), i) == ml_special_moves.end()) {
      tn.move.zobrist_key = coord.zobrist_dots[who-1][i] ^ ml_encl_zobrists[0] ^ neutral_encl_zobrists[0];
      tn.move.enclosures.clear();
      tn.move.enclosures.insert(tn.move.enclosures.end(), ml_encl_moves.begin(), ml_encl_moves.end());
      tn.move.enclosures.insert(tn.move.enclosures.end(), neutral_encl_moves.begin(), neutral_encl_moves.end());
      if (is_in_opp_te) {
	// note:  (threats[who-1].is_in_border[i] > 0) possible only for territory threats (i.e., we build a new territory)
	int min_terr_size = std::numeric_limits<int>::max();
	for (auto &t : threats[2-who].threats) {
	  if (t.type & ThreatConsts::TERR) {
	    if (t.terr_points < min_terr_size && t.encl->isInInterior(i)) {
	      min_terr_size = t.terr_points;
	      if (min_terr_size <= 2) break;
	    }
	  } else {  // ENCL
	    if (t.terr_points+1 < min_terr_size && t.encl->isInInterior(i)) {
	      min_terr_size = t.terr_points+1;
	      if (min_terr_size <= 2) break;
	    }
	  }
	}
	if (connects[who-1][i].groups_id[0] == 0) {
	  // dot does not touch any our dot
	  tn.t.playouts += 100 - std::min(min_terr_size, 20);    // add lost simulations
#ifndef NDEBUG
	  out << "isol=" << -(100 - std::min(min_terr_size, 20)) << " ";
#endif

	} else {
	  // we touch our dot
	  if (min_terr_size <= 2) {
	    tn.t.playouts += 80;    // add lost simulations
#ifndef NDEBUG
	    out << "touch=-80 ";
#endif
	  } else {
	    tn.t.playouts += 7;    // add lost simulation
#ifndef NDEBUG
	    out << "touch=-7 ";
#endif
	  }
	}
      } else {   // not in opp terr
	if (threats[2-who].is_in_border[i] > 0) {
	  // check the value of opp's threat
	  int value = 0, terr_value = 0;
	  for (auto &t : threats[2-who].threats) {
	    if (t.where == i) {
	      value += t.opp_dots;   terr_value += t.terr_points;
	    }
	  }
	  if (value) {
	    int num = 5 + 2*std::min(value, 15);
	    tn.t.playouts += num;  tn.t.value_sum = tn.t.value_sum.load() + num;  // add won simulations
#ifndef NDEBUG
	    out << "oppv2m=" << num << " ";
#endif

	  } else if (terr_value >= 3) {
	    tn.t.playouts += 4;  tn.t.value_sum = tn.t.value_sum.load() + 4*0.9;  // add 0.9 won simulations
#ifndef NDEBUG
	    out << "oppter=0.9*4 ";
#endif

	  }
	} else {  // not in the border of an opp's threat
	  if (is_in_our_te) {
	    // inside our enclosure, add lost simulations
	    if (threats[who-1].is_in_border[i] == 0) {
	      tn.t.playouts += 25;
#ifndef NDEBUG
	      out << "notborder=-25 ";
#endif

	      if (connects[who-1][i].groups_id[0] == 0) {  // does not touch our dots
		tn.t.playouts +=30;
#ifndef NDEBUG
		out << "andiso=-30 ";
#endif
	      }
	      if (connects[2-who][i].groups_id[0] == 0) {  // does not touch opp's dots
		tn.t.playouts +=30;
#ifndef NDEBUG
		out << " andiso2=-30 ";
#endif
	      }
	    } else {
	      for (auto &t : threats[who-1].threats) {
		if (t.where == i and t.terr_points == 0) {
		    // simplifying enclosure: no territory anyway
		    tn.move.enclosures.push_back(t.encl);
		    tn.move.zobrist_key ^= t.zobrist_key;
		}
	      }
	    }
	    tn.t.playouts += 20;
#ifndef NDEBUG
	    out << "insidee=-20 ";
#endif

	  } else if (threats[who-1].is_in_border[i] > 0) {
	    // check the value of our threat
	  int value = 0, terr_value = 0;
	  for (auto &t : threats[who-1].threats) {
	    if (t.where == i) {
	      value += t.opp_dots;   terr_value += t.terr_points;
	      if (t.terr_points == 0) {
		// simplifying enclosure: no territory anyway
		tn.move.enclosures.push_back(t.encl);
		tn.move.zobrist_key ^= t.zobrist_key;
	      }
	    }
	  }
	  if (value) {
	    int num = 5 + 2*std::min(value, 15);
	    tn.t.playouts += num;  tn.t.value_sum = tn.t.value_sum.load() + num;  // add won simulations
#ifndef NDEBUG
	    out << "voft=" << num << " ";
#endif

	  } else if (terr_value >= 2) {
	    int num = 2 + std::min(terr_value, 15);
	    tn.t.playouts += num;  tn.t.value_sum = tn.t.value_sum.load() + num;  // add won simulations
#ifndef NDEBUG
	    out << "voftT=" << num << " ";
#endif
	  }
	  }
	}
      }

      //tn.t.playouts *= 3;   tn.t.value_sum *= 3;   // take more virtual sims
      tn.prior = tn.t;
#ifndef NDEBUG
      out << " --> (" << tn.t.playouts << ", " << tn.t.value_sum << ") ";
      if (parent->parent == parent) std::cerr << out.str() << std::endl;
      out.str("");  out.clear();
#endif

      //ml_list.push_back(tn);
      *alloc.getNext() = tn;
      assert(neutral_opt_encl_moves.size()+1 == neutral_encl_zobrists.size());
      for (unsigned opi=0; opi<neutral_opt_encl_moves.size(); opi++) {
	tn.move.enclosures.push_back(neutral_opt_encl_moves[opi]);
	tn.move.zobrist_key ^= neutral_encl_zobrists[opi+1];
	*alloc.getNext() = tn;
	//ml_list.push_back(tn);
      }
    } else {
      // special move, canceling opp's threat or allowing for our enclosure
      int em = ml_encl_moves.size();
      auto tplayouts = tn.t.playouts.load();
      auto tvalue_sum = tn.t.value_sum.load();
      int dots = 0, captured_dots = 0;
      if (threats[who-1].is_in_border[i]) {
	for (auto &t : threats[who-1].threats) {
	  if (t.where == i)
	    captured_dots += t.opp_dots;
	}
      }
      if (threats[2-who].is_in_border[i]) {
	for (auto &t : threats[2-who].threats) {
	  if (t.where == i) {
	    dots += t.opp_dots;
	    dots += (t.terr_points >= 2);
	  }
	}
      }
      ml_encl_zobrists.erase(ml_encl_zobrists.begin()+1, ml_encl_zobrists.end());
      getEnclMoves(ml_encl_moves, ml_opt_encl_moves, ml_encl_zobrists, i, who);
      tn.move.zobrist_key = coord.zobrist_dots[who-1][i] ^ ml_encl_zobrists[0] ^ ml_encl_zobrists[1];
      tn.move.enclosures.clear();
      tn.move.enclosures.insert(tn.move.enclosures.end(), ml_encl_moves.begin(), ml_encl_moves.end());
      //tn.t.playouts *= 3;   tn.t.value_sum *= 3;   // take more virtual sims
      int saved_dots = 0;
      for (unsigned j=em; j<ml_encl_moves.size(); ++j) {
#ifndef NDEBUG
	if (parent->parent == parent)
	  std::cerr << "Enclosure after move " << coord.showPt(i) << ":" << std::endl << ml_encl_moves[j]->show() << "  priority info=" << ml_priority_vect[j-em].show() << std::endl;
#endif
	if (ml_priority_vect[j-em].priority_value > ThrInfoConsts::MINF) {
	  saved_dots += ml_priority_vect[j-em].saved_dots;
	}
      }
      int num = 7 + 2*std::min(dots, 15) + 3*std::min(saved_dots, 15) + std::min(7*saved_dots*(dots + captured_dots), 30) + (captured_dots == 0 ? 0 : (4*std::min(captured_dots, 15) - 1));
      tn.t.playouts += num;  tn.t.value_sum = tn.t.value_sum.load() + num;  // add won simulations
#ifndef NDEBUG
      out << "special=" << num << " --> (" << tn.t.playouts << ", " << tn.t.value_sum << ") ";
      if (parent->parent == parent) std::cerr << out.str() << std::endl;
      out.str("");  out.clear();
#endif

      tn.prior = tn.t;
      *alloc.getNext() = tn;
      //ml_list.push_back(tn);
      assert(ml_opt_encl_moves.size()+2 == ml_encl_zobrists.size());
      assert(ml_priority_vect.size() >= ml_encl_moves.size() - em + ml_opt_encl_moves.size());
      for (unsigned opi=0; opi<ml_opt_encl_moves.size(); opi++) {
	tn.move.enclosures.push_back(ml_opt_encl_moves[opi]);
	tn.move.zobrist_key ^= ml_encl_zobrists[opi+2];
	if (ml_priority_vect[ml_encl_moves.size() - em + opi].priority_value > ThrInfoConsts::MINF) {
#ifndef NDEBUG
	  if (parent->parent == parent)
	    std::cerr << "Enclosure after move " << coord.showPt(i) << ":" << std::endl << ml_opt_encl_moves[opi]->show() << "  priority info=" << ml_priority_vect[ml_encl_moves.size() - em + opi].show() << std::endl;
#endif
	  dots += ml_priority_vect[ml_encl_moves.size() - em + opi].saved_dots;
	  num = 5 + 2*std::min(dots, 15);
	  tn.t.playouts = tplayouts + num;  tn.t.value_sum = tvalue_sum + num;  // add won simulations
#ifndef NDEBUG
	  out << "bonus=" << num << "  --> (" << tn.t.playouts << ", " << tn.t.value_sum << ") ";
#endif

	}
#ifndef NDEBUG
	if (parent->parent == parent) std::cerr << out.str() << std::endl;
	out.str("");  out.clear();
#endif
	//ml_list.push_back(tn);
	tn.prior = tn.t;
	*alloc.getNext() = tn;
      }
      ml_encl_moves.erase(ml_encl_moves.begin()+em, ml_encl_moves.end());
    }
  }
}

Move
Game::getRandomEncl(Move &m)
{
  getSimplifyingEnclAndPriorities(m.who);
  getEnclMoves(ml_encl_moves, ml_opt_encl_moves, ml_encl_zobrists, m.ind, m.who);
  if (ml_opt_encl_moves.size()) {
    std::uniform_int_distribution<int> di(0, ml_opt_encl_moves.size());
    int number = di(engine);
    m.enclosures.clear();
    m.enclosures.reserve(ml_encl_moves.size() + number);
    m.enclosures.insert(m.enclosures.end(), ml_encl_moves.begin(), ml_encl_moves.end());
    if (number) {
      m.enclosures.insert(m.enclosures.end(), ml_opt_encl_moves.begin(), ml_opt_encl_moves.begin()+number);
    }
  } else {
    m.enclosures = ml_encl_moves;
  }
  return m;
}


/// This function selects enclosures using Game:::chooseRandomEncl().
Move
Game::chooseAtariMove(int who)
{
  boost::container::small_vector<pti, 16> urgent; //, non_urgent;
  for (auto &t : threats[who-1].threats) {
    if (t.type & ThreatConsts::ENCL) {
      if ((t.border_dots_in_danger && t.opp_dots) || t.singular_dots) {
	urgent.push_back(t.where);
	int count = t.border_dots_in_danger ? t.opp_dots : t.singular_dots;
	count += (t.terr_points >= 3);
	while (count >= 3) {
	  urgent.push_back(t.where);
	  count -= 3;
	}
      }
    }
  }
  for (auto &t : threats[2-who].threats) {
    if (t.singular_dots && (t.type & ThreatConsts::ENCL)) {
      // TODO: check also the ladder
      if (threats[2-who].is_in_encl[t.where]==0 && threats[2-who].is_in_2m_miai[t.where]==0 && threats[2-who].is_in_2m_miai[t.where]==0) {
	urgent.push_back(t.where);
	int count = t.singular_dots +  (t.terr_points >= 3);
	while (count >= 3) {
	  urgent.push_back(t.where);
	  count -= 4;
	}
      }
    }
    // if (t.border_in_danger) {
    //   ... find our moves destroying threat t --> this is not necessary,
    // because such a move will be found above, unless the whole t lies in some our threat,
    // in which case no our reaction is needed
  }
  // add also some threats-in-2-moves (double atari, etc.)   (v80+)
  for (auto &t : threats[who-1].threats2m) {
    if (t.min_win2 && t.isSafe()) {
      urgent.push_back(t.where0);
      int count = t.min_win2;
      while (count >= 3) {
	urgent.push_back(t.where0);
	count -= 4;
      }
    }
  }
  for (auto &t : threats[2-who].threats2m) {
    if (t.min_win2 && t.isSafe() && threats[2-who].is_in_terr[t.where0]==0 && threats[2-who].is_in_encl[t.where0]==0) {
      urgent.push_back(t.where0);
      int count = t.min_win2;
      while (count >= 3) {
	urgent.push_back(t.where0);
	count -= 4;
      }
    }
  }
  // select randomly one of the moves
  int total = urgent.size();  //*COEFF_URGENT + non_urgent.size()*COEFF_NONURGENT;
  Move m;
  m.who = who;
  // for debug, save all choices
#ifdef DEBUG_SGF
  /*
  for (pti p : urgent) {
    m.ind = p;
    sgf_tree.addChild({getRandomEncl(m).toSgfString(), {"C", {"chooseAtariMove:urgent,total==" + std::to_string(total)}}});
  }
  */
#endif
  //
  if (total) {
    std::uniform_int_distribution<int> di(0, total-1);
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
  } else {
    m.ind = 0;
    return m;
  }
  return getRandomEncl(m);
}

/// This function selects enclosures using Game:::chooseRandomEncl().
Move
Game::chooseAtariResponse(pti lastMove, int who)
{
  boost::container::small_vector<pti, 16> urgent;
  for (auto &t : threats[2-who].threats) {
    if (t.singular_dots && (t.type & ThreatConsts::ENCL) && std::find(t.encl->border.begin(), t.encl->border.end(), lastMove) != t.encl->border.end()) {
      // TODO: check also the ladder
      if (threats[2-who].is_in_encl[t.where]==0 && threats[2-who].is_in_2m_miai[t.where]==0 && threats[2-who].is_in_2m_miai[t.where]==0) {
	urgent.push_back(t.where);
	int count = t.singular_dots +  (t.terr_points >= 3);
	while (count >= 3) {
	  urgent.push_back(t.where);
	  count -= 4;
	}
      }
      if (t.border_dots_in_danger) {
	for (auto &ourt : threats[who-1].threats) {
	  if ((ourt.type & ThreatConsts::ENCL) && ourt.opp_dots && (std::find(t.opp_thr.begin(), t.opp_thr.end(), ourt.zobrist_key) != t.opp_thr.end())) {
	    urgent.push_back(ourt.where);
	    urgent.push_back(ourt.where);
	    int count = t.singular_dots +  (t.terr_points >= 3);
	    while (count >= 2) {
	      urgent.push_back(ourt.where);
	      count -= 3;
	    }
	  }
	}
      }
    }
  }
  // check opp threats in 2 moves
  for (auto &t : threats[2-who].threats2m) {
    if (t.min_win2 && t.isSafe() && threats[2-who].is_in_terr[t.where0]==0 && threats[2-who].is_in_encl[t.where0]==0
	&& threats[who-1].is_in_terr[t.where0]==0 && threats[who-1].is_in_encl[t.where0]==0
	&& (coord.distBetweenPts_infty(t.where0, lastMove) <= 1 or
	    std::any_of(t.thr_list.begin(), t.thr_list.end(),
			[lastMove](const Threat& thr)
			{
			  return std::find(thr.encl->border.begin(), thr.encl->border.end(), lastMove) != thr.encl->border.end();
			}))) {
      urgent.push_back(t.where0);
      int count = t.min_win2;
      while (count >= 3) {
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
    sgf_tree.addChild({getRandomEncl(m).toSgfString(), {"C", {"chooseAtariMove:urgent,total==" + std::to_string(total)}}});
  }
  */
#endif
  //
  if (total) {
    std::uniform_int_distribution<int> di(0, total-1);
    int number = di(engine);
    m.ind = urgent[number];
  } else {
    m.ind = 0;
    return m;
  }
  return getRandomEncl(m);
}


/// Note: if move0 and move1 have common neighbours, then they have
///  the probability of being chosen doubled.
Move
Game::choosePattern3Move(pti move0, pti move1, int who)
{
  Move move;
  move.who = who;
  typedef std::pair<pti, pattern3_val> MoveValue;
  boost::container::small_vector<MoveValue,24> stack;
  int total = 0;
  for (pti m : {move0, move1}) {
    if (m) {
      for (int i=0; i<8; i++) {
	pti nb = m + coord.nb8[i];
	auto v = pattern3_value[who-1][nb];
	assert(v == getPattern3Value(nb, who));
	if (v > 0 && ((threats[2-who].is_in_encl[nb]==0 && threats[2-who].is_in_terr[nb]==0) || threats[who-1].is_in_border[nb])) {
	  stack.push_back( std::make_pair(nb, v) );
	  total += v;
	}
      }
    }
  }
  if (total <= 20) {
    for (pti m : {move0, move1}) {
      if (m && coord.dist[m] >= 1) {
	for (int i=0; i<4; i++) {
	  if (isEmptyInDirection(m, i)) {  // in particular,  whoseDotMarginAt(m + 2*coord.nb4[i]) == 0
	    pti nb = m + 2*coord.nb4[i];
	    assert(coord.dist[nb] >= 0);
	    auto v = pattern3_value[who-1][nb];
	    assert(v == getPattern3Value(nb, who));

	    if (v >= 0 &&  // here non-dame (>=0) is enough
		((threats[2-who].is_in_encl[nb]==0 && threats[2-who].is_in_terr[nb]==0) || threats[who-1].is_in_border[nb])) {
	      //	      v = std::min(80.0f, v+30);  // v will be in [50, 80]     // TODO! change type of pattern3_value to integer!
	      if (isEmptyInNeighbourhood(nb)) {
		v = 20;
	      } else {
		v = 10 + v/4;
	      }
	      stack.push_back( std::make_pair(nb, v) );
	      total += v;
	    }
	  }
	}
      }
    }
  }
  if (total) {
    // for debug, save all choices
#ifdef DEBUG_SGF
    /*
    for (auto p : stack) {
      move.ind = p.first;
      sgf_tree.addChild({getRandomEncl(move).toSgfString(), {"C", {"p3,t==" + std::to_string(int(100*total+0.01)) + ", v=" + std::to_string(int(100*p.second+0.01))}}});
    }
    */
#endif
    std::uniform_int_distribution<int> di(0, total-1);
    int number = di(engine);
    for (auto p : stack) {
      number -= p.second;
      if (number < 0) {
	move.ind = p.first;
	return getRandomEncl(move);   // TODO: do we have to set zobrist?
      }
    }
    move.ind = stack[0].first;
    return getRandomEncl(move);   // TODO: do we have to set zobrist?
  }
  move.ind = 0;
  return move;
}


Move
Game::chooseSafetyMove(int who)
{
  Move move;
  move.who = who;
  std::vector<pti> stack;
  std::set<pti> already_saved;
  using Tup = std::tuple<int, pti, pti>;
  const std::array<Tup, 4>
    p_vnorm_vside {
      Tup{coord.ind(1, 1), coord.N, coord.E},
	Tup{coord.ind(1, 1), coord.W, coord.S},
	  Tup{coord.ind(coord.wlkx - 2, 1), coord.E, coord.S},
      Tup{coord.ind(1, coord.wlky - 2), coord.S, coord.E} };
  // top margin
  for (auto [p, vnorm, vside] : p_vnorm_vside) {
    for (; coord.dist[p] == 1; p+=vside) {
      if (whoseDotMarginAt(p) != 0 and descr.at(worm[p]).safety == 1) {
	if (whoseDotMarginAt(p + vnorm) == 0) {
	  stack.push_back(p + vnorm);
	  continue;
	}
	if (whoseDotMarginAt(p + vside) == 0 and coord.dist[p+vside] == 1 and
	    whoseDotMarginAt(p + vside + vnorm) != whoseDotMarginAt(p) and
	    already_saved.find(p + vside + vnorm) == already_saved.end()) {
	  // x ?
	  // o .   ? = .o but not 'x'
	  stack.push_back(p + vside);
	  already_saved.insert(p + vside);
	}
	if (whoseDotMarginAt(p - vside) == 0 and coord.dist[p-vside] == 1 and
	    whoseDotMarginAt(p - vside + vnorm) != whoseDotMarginAt(p) and
	    already_saved.find(p - vside + vnorm) == already_saved.end()) {
	  // x ?
	  // o .   ? = .o but not 'x'
	  stack.push_back(p - vside);
	  already_saved.insert(p - vside);
	}
      }
    }
  }
  if (!stack.empty()) {
    std::uniform_int_distribution<int> di(0, stack.size()-1);
    int number = di(engine);
    move.ind = stack[number];
    return getRandomEncl(move);   // TODO: do we have to set zobrist?
  }
  move.ind = 0;
  return move;
}


Move
Game::chooseAnyMove(int who)
{
  Move move;
  move.who = who;
  std::vector<pti> stack;
  std::vector<pti> bad_moves;
  for (pti i = coord.first; i<=coord.last; i++) {
    if (whoseDotMarginAt(i) == 0) {
      if ((connects[who-1][i].groups_id[0] == 0) &&
	  (threats[who-1].is_in_terr[i] > 0 || threats[2-who].is_in_terr[i] > 0)) {
	bad_moves.push_back(i);
      } else {
	stack.push_back(i);
      }
    }
  }
  if (!stack.empty()) {
    std::uniform_int_distribution<int> di(0, stack.size()-1);
    int number = di(engine);
    move.ind = stack[number];
    return getRandomEncl(move);   // TODO: do we have to set zobrist?
  } else if (!bad_moves.empty()) {
    std::uniform_int_distribution<int> di(0, bad_moves.size()-1);
    int number = di(engine);
    move.ind = bad_moves[number];
    return getRandomEncl(move);   // TODO: do we have to set zobrist?
  }
  move.ind = 0;
  return move;
}


/// Finds possibly good moves for who among TERRMoves.
std::vector<pti>
Game::getGoodTerrMoves(int who) const
{
  std::vector<pti> good_moves;
  for (unsigned i=0; i<possible_moves.lists[PossibleMovesConsts::LIST_TERRM].size(); ++i) {
    auto p = possible_moves.lists[PossibleMovesConsts::LIST_TERRM][i];
    if ((connects[who-1][p].groups_id[0] == 0 &&
	 (threats[2-who].is_in_terr[p] > 0 || threats[2-who].is_in_encl[p] > 0)) ||
	(connects[2-who][p].groups_id[0] == 0 &&
	 (threats[who-1].is_in_terr[p] > 0 || threats[who-1].is_in_encl[p] > 0))) {
      continue;
    }
    // check if it closes something
    if (threats[who-1].is_in_border[p] || threats[2-who].is_in_border[p]) {
      good_moves.push_back(p);
      continue;
    }
    // check if it can be closed fast
    int count_opp = 0, count_our = 0;
    for (auto j : coord.nb4) {
      count_opp += (whoseDotMarginAt(p + j) == 3-who);
      count_our += (whoseDotMarginAt(p + j) == who);
    }
    if (count_opp >= 3 && count_our == 0) {
      continue;
    }
    if (count_our >= 3 && count_opp == 0) {
      // check whether the point may be dangerous for us
      assert(coord.nb8[0] == coord.NE);  // could be any corner
      int count_danger = 0;
      for (int j=0; j<8; j+=2) {
	count_danger += (whoseDotMarginAt(p+coord.nb8[j]) != who);
      }
      if (count_danger <= 1) {
	continue;   // opp will not connect through p
      }
    }
    // here we could still check whether the move makes sense, for example, if there's any chance of connecting something,
    // but for now, we assume p is a good move
    good_moves.push_back(p);
  }
  return good_moves;
}

/// Chooses any move using possible_moves.
Move
Game::chooseAnyMove_pm(int who)
{
  Move move;
  move.who = who;
  assert(checkPossibleMovesCorrectness(who));
  if (!possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].empty()) {
    unsigned number;
    if (!possible_moves.lists[PossibleMovesConsts::LIST_TERRM].empty()) {
      // if there are some TERRM moves, maybe we should consider one of them; throw a dice to decide
      std::uniform_int_distribution<unsigned> di(0, possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].size()
						 + std::min(int(possible_moves.lists[PossibleMovesConsts::LIST_TERRM].size()), 2) - 1);
      number = di(engine);
    } else {
      std::uniform_int_distribution<unsigned> di(0, possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].size() - 1);
      number = di(engine);
    }
    if (number < possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].size()) {
      move.ind = possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL][number];
      dame_moves_so_far = 0;
      return getRandomEncl(move);   // TODO: do we have to set zobrist?
    }
  }
  // check TERRM moves
  std::vector<pti> good_moves = getGoodTerrMoves(who);

  // v138
  // check if there are no edge moves
  /*
  if (possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].empty() &&
      possible_moves.lists[PossibleMovesConsts::LIST_DAME].empty()) {
    for (int i=coord.first; i<=coord.last; i++)
      if (coord.dist[i] == 0 && whoseDotMarginAt(i) == 0) {
	show();
	std::cerr << "neutral moves: " << possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].size() << std::endl;
	std::cerr << "good_moves = ";
	for (auto p : good_moves) {
	  std::cerr << coord.showPt(p) << " ";
	}
	std::cerr << std::endl << "bad moves = ";
	for (auto p : bad_moves) {
	  std::cerr << coord.showPt(p) << " ";
	}
	assert(0);
      }
  }
  */

  // try neutral or good
  int n_or_g = possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].size() + good_moves.size();
  if (n_or_g > 0) {
    std::uniform_int_distribution<unsigned> di(0, n_or_g - 1);
    unsigned number = di(engine);
    if (number < possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].size())
      move.ind = possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL][number];
    else
      move.ind = good_moves[number - possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].size()];
    dame_moves_so_far = 0;
    return getRandomEncl(move);   // TODO: do we have to set zobrist?
  }
  // try dame
  if (possible_moves.lists[PossibleMovesConsts::LIST_DAME].size() > 0) {
    std::uniform_int_distribution<unsigned> di(0, possible_moves.lists[PossibleMovesConsts::LIST_DAME].size() - 1);
    unsigned number = di(engine);
    move.ind = possible_moves.lists[PossibleMovesConsts::LIST_DAME][number];
    dame_moves_so_far++;
    return getRandomEncl(move);   // TODO: do we have to set zobrist?
  }
  // maybe there's some bad move left
  if (possible_moves.lists[PossibleMovesConsts::LIST_TERRM].size() > 0) {   // note: here possible_moves.lists[LIST_TERRM] == list of bad_moves
    std::uniform_int_distribution<int> di(0, possible_moves.lists[PossibleMovesConsts::LIST_TERRM].size() - 1);
    int number = di(engine);
    move.ind = possible_moves.lists[PossibleMovesConsts::LIST_TERRM][number];
    dame_moves_so_far++;   // not really dame, but we just want to stop the simulation
    return getRandomEncl(move);   // TODO: do we have to set zobrist?
  }
  // no legal move left...
  move.ind = 0;
  return move;
}

/// Chooses interesting_move.
Move
Game::chooseInterestingMove(int who)
{
  Move move;
  move.who = who;
  int which_list = InterestingMovesConsts::LIST_0;
  assert(InterestingMovesConsts::LIST_0 + 1 == InterestingMovesConsts::LIST_1 &&
	 InterestingMovesConsts::LIST_1 + 1 == InterestingMovesConsts::LIST_2 &&
	 InterestingMovesConsts::LIST_2 + 1 == InterestingMovesConsts::LIST_REMOVED);
  while (interesting_moves.lists[which_list].empty()) {
    which_list++;
    if (which_list == InterestingMovesConsts::LIST_REMOVED) {
      move.ind = 0;
      return move;
    }
  }
  std::uniform_int_distribution<int> di(0, interesting_moves.lists[which_list].size() - 1);
  int number = di(engine);
  move.ind = interesting_moves.lists[which_list][number];
  return getRandomEncl(move);   // TODO: do we have to set zobrist?
}

Move
Game::choosePatt3extraMove(int who)
{
  Move move;
  move.who = who;
  std::vector<pti> patt3extrav = getPatt3extraValues();
  int sum = 0;
  for (int i=coord.first; i<=coord.last; ++i)
    if (patt3extrav[i]) {
      if (threats[0].is_in_encl[i]==0 && threats[0].is_in_terr[i]==0 && threats[1].is_in_encl[i]==0 && threats[1].is_in_terr[i]==0) {
	sum += patt3extrav[i];
      } else {
	patt3extrav[i] = 0;
      }
    }
  if (sum == 0) {
    move.ind = 0;
    return move;
  }
  std::uniform_int_distribution<int> di(0, sum-1);
  int number = di(engine);
  for (int i=coord.first; i<=coord.last; ++i) {
    number -= patt3extrav[i];
    if (number < 0) {
      move.ind = i;
      return move;
    }
  }
  assert(0);
  return move;
}


Move
Game::getLastMove() const
{
  // we do not get last enclosure, but this does not matter
  Move m;
  m.ind = history.back() & history_move_MASK;
  m.who = (nowMoves ^ 3);
  return m;
}

Move
Game::getLastButOneMove() const
{
  // we do not get last enclosure, but this does not matter
  Move m;
  m.ind = history.at(history.size()-2) & history_move_MASK;
  m.who = nowMoves;
  return m;
}


real_t
Game::randomPlayout()
{
  Move m;
  std::uniform_int_distribution<int> di(0, 0xfff);
  for (;;) {
    int number = di(engine);
    if ((number & 0xc00) != 0) {
      m = chooseAtariResponse(history.back() & history_move_MASK, nowMoves);
      if (m.ind != 0) {
	dame_moves_so_far = 0;
	makeMove(m);
#ifdef DEBUG_SGF
	sgf_tree.addComment("ar");
#endif
	//std::cerr << "A";
	continue;
      }
    }
    if ((number & 0x1) != 0) {  // probability 1/2
      m = chooseSafetyMove(nowMoves);
      if (m.ind != 0) {
	dame_moves_so_far = 0;
	makeMove(m);
#ifdef DEBUG_SGF
	sgf_tree.addComment(std::string("saf"));
#endif
	continue;
      }
    }
    if ((number & 0x300) != 0) {
      auto it = history.end();
      --it;
      m = choosePattern3Move((*it)  & history_move_MASK, (*(it-1)) & history_move_MASK, nowMoves);
      if (m.ind != 0) {
	dame_moves_so_far = 0;
#ifdef DEBUG_SGF
	auto v = pattern3_value[m.who-1][m.ind];
#endif
	makeMove(m);
#ifdef DEBUG_SGF
	sgf_tree.addComment(std::string("pa:") + std::to_string(v));
#endif
	//std::cerr << "p";
	continue;
      }
    }
    if ((number & 0x2) != 0) {
      m = chooseAtariMove(nowMoves);
      if (m.ind != 0) {
	dame_moves_so_far = 0;
	makeMove(m);
#ifdef DEBUG_SGF
	sgf_tree.addComment("at");
#endif
	//std::cerr << "A";
	continue;
      }
    }
    if ((number & 0x80) != 0) {  // probability 1/2, could be 3/4 by changing 0x80 to 0xc0
      m = chooseInterestingMove(nowMoves);
      if (m.ind != 0) {
	dame_moves_so_far = 0;
	makeMove(m);
#ifdef DEBUG_SGF
	sgf_tree.addComment(std::string("cut"));
#endif
	continue;
      }
    }
    if ((number & 0x10) != 0) {  // probability 1/2, could be 3/4 by changing 0x10 to 0x30
      m = choosePatt3extraMove(nowMoves);
      if (m.ind != 0) {
	dame_moves_so_far = 0;
	makeMove(m);
#ifdef DEBUG_SGF
	sgf_tree.addComment(std::string("ext"));
#endif
	continue;
      }
    }
    /*
    if ((number & 0xc) != 0) {
      m = chooseInfluenceMove(nowMoves);
      if (m.ind != 0) {
	dame_moves_so_far = 0;
	makeMove(m);
#ifdef DEBUG_SGF
	sgf_tree.addComment(std::string("infl"));
#endif
	continue;
      }
    }
    */
    m = chooseAnyMove_pm(nowMoves);
    if (m.ind != 0) {
      makeMove(m);
#ifdef DEBUG_SGF
      sgf_tree.addComment(std::string(":") + std::to_string(dame_moves_so_far));
#endif
      //std::cerr << ".";
    } else {
      break;
    }
    if (dame_moves_so_far >= 2) break;
  }
  //std::cerr << std::endl;
  auto [res, res_small] = countTerritory_simple(nowMoves);
  real_t win_value;
  if (res == 0) {
    const real_t scale = 0.04;
    win_value = 0.5 + scale * res_small;
  } else {
    int range = (coord.wlkx + coord.wlky)/2;
    const real_t scale = 0.04;
    real_t scaled_score = scale*std::max( std::min( real_t(res + 0.5*res_small)/range, real_t(1.0)), real_t(-1.0));
    win_value = (res>0) * (1-2*scale) + scale + scaled_score;
  }
#ifdef DEBUG_SGF
  sgf_tree.addComment(std::string("res=") + std::to_string(res) + std::string(" v=") + std::to_string(win_value));
  sgf_tree.finishPartialMove();
#endif
  return win_value;
}

/// This function checks if p is an interesting move, because of a change of its pattern3_value or atari.
int
Game::checkInterestingMove(pti p) const
{
  if (threats[0].is_in_encl[p]==0 && threats[0].is_in_terr[p]==0 && threats[1].is_in_encl[p]==0 && threats[1].is_in_terr[p]==0) {
    auto v = global::patt3_symm.getValue(pattern3_at[p], 1);
    assert(v>=0 && v<8);
    int lists[8] = {InterestingMovesConsts::REMOVED, InterestingMovesConsts::REMOVED,
		    InterestingMovesConsts::MOVE_2, InterestingMovesConsts::MOVE_2,
		    InterestingMovesConsts::MOVE_2, InterestingMovesConsts::MOVE_1,
		    InterestingMovesConsts::MOVE_0, InterestingMovesConsts::MOVE_0};  // best moves goes to LIST_0
    return lists[v];
  }
  return InterestingMovesConsts::REMOVED;
}

/// This function checks if p is still dame or now dame, because of its pattern3_value change.
int
Game::checkDame(pti p) const
{
  if (threats[0].is_in_encl[p]==0 && threats[0].is_in_terr[p]==0 && threats[1].is_in_encl[p]==0 && threats[1].is_in_terr[p]==0) {
    if (pattern3_value[0][p] < 0) return PossibleMovesConsts::DAME;
    if (coord.dist[p] == 0) {
      int x = coord.x[p], y = coord.y[p];
      if ((x==0 || x==coord.wlkx-1) && (y==0 || y==coord.wlky-1)) return PossibleMovesConsts::DAME;   // corner
      if ((x==0 && possible_moves.left) || (x==coord.wlkx-1 && possible_moves.right) ||
	  (y==0 && possible_moves.top) || (y==coord.wlky-1 && possible_moves.bottom)) return PossibleMovesConsts::DAME;
      for (int i=0; i<4; i++) {
	pti nb = p + coord.nb4[i];
	if (coord.dist[nb]==1) {
	  if (worm[nb] && descr.at(worm[nb]).isSafe()) return PossibleMovesConsts::DAME;
	  break;
	}
      }
    }
    return PossibleMovesConsts::NEUTRAL;
  } else {
    return PossibleMovesConsts::TERRM;
  }
}

/// This function checks if neighbour(s) of p is/are still dame or now dame, because of the safety change of p.
void
Game::possibleMoves_updateSafety(pti p)
{
  assert(coord.dist[p] == 1);
  if (worm[p]!=0) {
    for (int i=0; i<4; i++) {
      pti nb = p + coord.nb4[i];
      if (coord.dist[nb] == 0 && whoseDotMarginAt(nb) == 0) {
	possible_moves.changeMove(nb, descr.at(worm[p]).isSafe() ? PossibleMovesConsts::DAME : PossibleMovesConsts::NEUTRAL);
      }
    }
  }
}

void
Game::possibleMoves_updateSafetyDame()
{
  if (!possible_moves.left) {
    pti p = coord.ind(0,1);
    for (int y=coord.wlky-2; y>0; --y) {
      if (worm[p + coord.E] != 0 && whoseDotMarginAt(p) == 0) {
	possible_moves.changeMove(p, descr.at(worm[p + coord.E]).isSafe() ? PossibleMovesConsts::DAME : PossibleMovesConsts::NEUTRAL);
      }
      p += coord.S;
    }
  }
  if (!possible_moves.right) {
    pti p = coord.ind(coord.wlkx-1, 1);
    for (int y=coord.wlky-2; y>0; --y) {
      if (worm[p + coord.W] != 0 && whoseDotMarginAt(p) == 0) {
	 possible_moves.changeMove(p, descr.at(worm[p + coord.W]).isSafe() ? PossibleMovesConsts::DAME : PossibleMovesConsts::NEUTRAL);
      }
      p += coord.S;
    }
  }
  if (!possible_moves.top) {
    pti p = coord.ind(1, 0);
    for (int x=coord.wlkx-2; x>0; --x) {
      if (worm[p + coord.S] != 0 && whoseDotMarginAt(p) == 0) {
	 possible_moves.changeMove(p, descr.at(worm[p + coord.S]).isSafe() ? PossibleMovesConsts::DAME : PossibleMovesConsts::NEUTRAL);
      }
      p += coord.E;
    }
  }
  if (!possible_moves.bottom) {
    pti p = coord.ind(1, coord.wlky-1);
    for (int x=coord.wlkx-2; x>0; --x) {
      if (worm[p + coord.N] != 0 && whoseDotMarginAt(p) == 0) {
	 possible_moves.changeMove(p, descr.at(worm[p + coord.N]).isSafe() ? PossibleMovesConsts::DAME : PossibleMovesConsts::NEUTRAL);
      }
      p += coord.E;
    }
  }
}

/// This function is for testing.
bool
Game::isDame_directCheck(pti p, int who) const
{
  if (coord.dist[p] == 0 && checkBorderMove(p, who) < 0) return true;
  return isDame_directCheck_symm(p);
}

/// This function is for testing.
/// This is 'symmetric' version (i.e. good for both players), NOT using checkBorderMove().
bool
Game::isDame_directCheck_symm(pti p) const
{
  if (coord.dist[p] == 0) {
    int x = coord.x[p], y = coord.y[p];
    if ((x==0 || x==coord.wlkx-1) && (y==0 || y==coord.wlky-1)) return true;   // corner
    if ((x==0 && possible_moves.left) || (x==coord.wlkx-1 && possible_moves.right) ||
	(y==0 && possible_moves.top) || (y==coord.wlky-1 && possible_moves.bottom)) return true;
    for (int i=0; i<4; i++) {
      pti nb = p + coord.nb4[i];
      if (coord.dist[nb]==1) {
	if (worm[nb] && descr.at(worm[nb]).isSafe()) return true;
	break;
      }
    }
  }
  if (threats[0].is_in_encl[p]==0 && threats[0].is_in_terr[p]==0 && threats[1].is_in_encl[p]==0 && threats[1].is_in_terr[p]==0
      && pattern3_value[0][p] < 0) return true;
  return false;
}

bool
Game::checkRootListOfMovesCorrectness(Treenode *children) const
{
  int count = 0;
  int outside_terr = 0;
  int dame = 0;
  if (children != nullptr) {
    for (auto *ch = children; true; ++ch) {
      ++count;
      auto p = ch->move.ind;
      if (threats[0].is_in_encl[p]==0 && threats[0].is_in_terr[p]==0 && threats[1].is_in_encl[p]==0 && threats[1].is_in_terr[p]==0) {
	++outside_terr;
      }
      if (isDame_directCheck_symm(p)) ++dame;
      if (ch->isLast()) break;
    }
  }
  if (debug_previous_count > 0 && debug_previous_count != count) {
    std::cerr << "Vanishing moves!!!" << std::endl;
    return false;
  }
  if (possible_moves.lists[PossibleMovesConsts::LIST_DAME].size() > 0 && possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].size() && dame == 0 && outside_terr == 0) {
    return false;   // no dame move among children, but there should be!
  }
  return true;
}

bool
Game::checkWormCorrectness() const
{
  std::vector<pti> groups(coord.getSize(), 0);
  std::map<pti, WormDescr>  test_descr;
  for (int ind=coord.first; ind<=coord.last; ind++)
    if (isDotAt(ind) && groups[ind]==0) {
      // visit this group
      WormDescr dsc;
      test_descr.insert({worm[ind], dsc});
      std::vector<pti> stack;
      stack.push_back(ind);
      groups[ind] = ind;
      int who = (worm[ind] & MASK_DOT);
      do {
	pti p = stack.back();  stack.pop_back();
	for (int i=0; i<8; i++) {
	  pti nb = p + coord.nb8[i];
	  if ((worm[nb] & MASK_DOT) == who ) {
	    if (worm[nb] != worm[p]) {    // diagonal connection
	      if (test_descr.find(worm[nb]) == test_descr.end()) {
		test_descr.insert({worm[nb], dsc});
	      }
	      if ( std::find( test_descr[worm[nb]].neighb.begin(), test_descr[worm[nb]].neighb.end(), worm[p]) == test_descr[worm[nb]].neighb.end()) {
		test_descr.at(worm[nb]).neighb.push_back(worm[p]);
		test_descr.at(worm[p]).neighb.push_back(worm[nb]);
	      }
	    }
	    if (groups[nb] == 0) {
	      groups[nb] = ind;
	      stack.push_back(nb);
	    }
	  }
	}
      } while (!stack.empty());
    }

  //std::cerr << coord.showBoard(groups) << std::endl;
  for (auto &d : test_descr) {
    if (descr.find(d.first) == descr.end()) {
	std::cerr << "blad " << d.first << " nie wystepuje w descr" << std::endl;
	return false;
      }
    d.second.safety = 0;
  }
  // add safety info
  for (int x=0; x<coord.wlkx; x++) {
    if (worm[coord.ind(x, 0)])
      test_descr.at(worm[coord.ind(x, 0)]).safety = WormDescr::SAFE_VALUE;
    if (worm[coord.ind(x, coord.wlky-1)])
      test_descr.at(worm[coord.ind(x, coord.wlky-1)]).safety = WormDescr::SAFE_VALUE;
    if (x>0 && x<coord.wlkx-1) {
      if (worm[coord.ind(x, 0)]==0 && worm[coord.ind(x, 1)])
	test_descr.at(worm[coord.ind(x, 1)]).safety++;
      if (worm[coord.ind(x,  coord.wlky-1)]==0 && worm[coord.ind(x,  coord.wlky-2)])
	test_descr.at(worm[coord.ind(x,  coord.wlky-2)]).safety++;
    }
  }
  for (int y=0; y<coord.wlky; y++) {
    if (worm[coord.ind(0, y)])
      test_descr.at(worm[coord.ind(0, y)]).safety = WormDescr::SAFE_VALUE;
    if (worm[coord.ind(coord.wlkx-1,y )])
      test_descr.at(worm[coord.ind(coord.wlkx-1, y)]).safety = WormDescr::SAFE_VALUE;
    if (y>0 && y<coord.wlky-1) {
      if (worm[coord.ind(0, y)]==0 && worm[coord.ind(1, y)])
	test_descr.at(worm[coord.ind(1, y)]).safety++;
      if (worm[coord.ind(coord.wlkx-1, y)]==0 && worm[coord.ind(coord.wlkx-2, y)])
	test_descr.at(worm[coord.ind(coord.wlkx-2, y)]).safety++;
    }
  }

  for (auto &d1 : descr) {
    // check keys
    if (test_descr.find(d1.first) == test_descr.end()) {
      std::cerr << "blad, brak " << d1.first << std::endl;
      return false;
    }
    // check opp_threats
    std::vector<uint64_t> zobr;
    assert((d1.first & MASK_DOT) == 1 || (d1.first & MASK_DOT) == 2);
    for (auto &t : threats[2 - (d1.first & MASK_DOT)].threats) {
      if (t.encl->isInInterior(d1.second.leftmost)) zobr.push_back(t.zobrist_key);
    }
    if (zobr.size() != d1.second.opp_threats.size()) {
      std::cerr << "blad, inne zagr dla robaka " << d1.first << ", powinny byc: " << std::endl;
      for (auto z : zobr) std::cerr << z << " ";
      std::cerr << std::endl << "sa: " << std::endl;
      for (auto z : d1.second.opp_threats) std::cerr << z << " ";
      std::cerr << std::endl;
      std::cerr << "is_in_terr[]==" << threats[2 - (d1.first & MASK_DOT)].is_in_terr[d1.second.leftmost] << ",  "
		<< "is_in_encl[]==" << threats[2 - (d1.first & MASK_DOT)].is_in_encl[d1.second.leftmost] << std::endl;
      show();
      return false;
    } else {
      for (auto z : d1.second.opp_threats) {
	if (std::find(zobr.begin(), zobr.end(), z) == zobr.end()) {
	  std::cerr << "blad, inne zagr dla robaka " << d1.first << ", zagrozenie " << z << " nie powinno wystepowac." << std::endl;
	  show();
	  return false;
	}
      }
    }
    // check pairs
    for (auto &d2 : descr) {
      if (  (std::find( d1.second.neighb.begin(), d1.second.neighb.end(), d2.first ) == d1.second.neighb.end()) !=
	    (std::find( test_descr.at(d1.first).neighb.begin(), test_descr.at(d1.first).neighb.end(), d2.first ) == test_descr.at(d1.first).neighb.end()) ) {
	std::cerr << "blad" << std::endl;
	return false;
      }
      if ((d1.second.group_id == d2.second.group_id) != (groups[ d1.second.leftmost] == groups[ d2.second.leftmost]) ) {
	std::cerr << "blad" << std::endl;
	return false;
      }
    }
    // safety
    if (d1.second.safety != test_descr.at(d1.first).safety && (d1.second.safety < WormDescr::SAFE_THRESHOLD || test_descr.at(d1.first).safety < WormDescr::SAFE_THRESHOLD)) {
      std::cerr << "blad safety " << d1.second.safety << " " << test_descr.at(d1.first).safety << std::endl;
      return false;
    }
  }
  return true;
}

bool
Game::checkThreatCorrectness()
{
  std::map<uint64_t, Threat> checked[2];
  for (int ind = coord.first+1; ind<coord.last; ind++) {
    //if (coord.dist[ind]<0) continue;
    if (worm[ind] == 0) {
      for (int who=1; who<=2; who++) {
	{
	Threat t;
	// first check territory enclosing [ind]
	t.encl = std::make_shared<Enclosure>(findEnclosure_notOptimised(ind, MASK_DOT, who));
	if (!t.encl->isEmpty()) {
	  t.type = ThreatConsts::TERR;
	  t.where = 0;
	  t.zobrist_key = t.encl->zobristKey(who);
	  // try to save it
	  if (checked[who-1].find(t.zobrist_key) == checked[who-1].end()) {
	    checked[who-1].insert({t.zobrist_key, std::move(t)});
	  }
	}
	}
	{
	Threat t;
	// now try to put a dot here and enclose
	worm[ind] = who;
	for (int j=0; j<4; j++) {
	  pti nb = ind + coord.nb4[j];
	  if (coord.dist[nb]>=0 && (worm[nb] & MASK_DOT) != who) {
	    t.encl = std::make_shared<Enclosure>(findEnclosure_notOptimised(nb, MASK_DOT, who));
	    if (!t.encl->isEmpty() && std::find(t.encl->border.begin(), t.encl->border.end(), ind) != t.encl->border.end() ) {
	      t.type = ThreatConsts::ENCL;
	      t.where = ind;
	      t.zobrist_key = t.encl->zobristKey(who);
	      // try to save it, if not yet saved
	      if (checked[who-1].find(t.zobrist_key) == checked[who-1].end()) {
		checked[who-1].insert({t.zobrist_key, std::move(t)});
	      }
	    }
	  }
	}
	}
      }
      worm[ind] = 0;
    } else if (coord.dist[ind]>0) {
      // try to enclose it
      Threat t;
      int who = (worm[ind] & MASK_DOT) ^ MASK_DOT;
      t.encl = std::make_shared<Enclosure>(findEnclosure_notOptimised(ind, MASK_DOT, who));
      if (!t.encl->isEmpty()) {
	t.type = ThreatConsts::TERR;
	t.where = 0;
	t.zobrist_key = t.encl->zobristKey(who);
	// try to save it
	if (checked[who-1].find(t.zobrist_key) == checked[who-1].end()) {
	  checked[who-1].insert({t.zobrist_key, t});
	}
      }
    }
  }
  // now check if they are the same
  bool shown = false;
  for (int pl=0; pl<2; pl++)
    for (Threat &thr : threats[pl].threats) {
      auto pos = checked[pl].find(thr.zobrist_key);
      if (pos == checked[pl].end()) {
	if (!shown) { shown=true;  show();  }
	std::cerr << "Nieaktualne zagrozenie (gracz=" << pl+1 << "):" << std::endl;
	std::cerr << thr.show() << std::endl;
      } else {
	if (pos->second.type & ThreatConsts::TO_REMOVE) {
	  if (!shown) { shown=true;  show();  }
	  std::cerr << "Powtórzone zagrożenie (gracz=" << pl+1 << "):" << std::endl;
	} else {
	  pos->second.type |= ThreatConsts::TO_REMOVE;
	}
      }
      int16_t true_opp_dots, true_terr;
      auto tmp = countDotsTerrInEncl(*thr.encl, 2-pl, false);
      true_opp_dots = std::get<0>(tmp);
      true_terr = std::get<1>(tmp);
      if (true_opp_dots != thr.opp_dots || true_terr != thr.terr_points) {
	if (!shown) { shown=true;  show();  }
	std::cerr << "Zla liczba kropek w zagrozeniu (gracz=" << pl+1 << "): jest " << int(thr.opp_dots) << ", powinno byc " << int(true_opp_dots) << std::endl;
	std::cerr << "lub liczba pktow terr: jest " << int(thr.terr_points) << ", powinno byc " << int(true_terr) << std::endl;
	std::cerr << thr.show() << std::endl;
      }
    }
  for (int pl=0; pl<2; pl++)
    for (auto &thr : checked[pl]) {
      if ((thr.second.type & ThreatConsts::TO_REMOVE)==0) {
	// maybe this is an uninteresting threat?
	if (thr.second.type & ThreatConsts::ENCL) {
	  auto zobr = thr.second.zobrist_key ^ coord.zobrist_encl[pl][thr.second.where];
	  Threat *tz = threats[pl].findThreatZobrist(zobr);
	  if (tz != nullptr && (tz->type & (ThreatConsts::TO_CHECK | ThreatConsts::TO_REMOVE))==0)
	    {
	    continue;  // trivial enclosure! (we put dot inside our territory and have an enclosure 1-dot smaller than the original territory)
	    }
	  // maybe this is a still uninteresting threat, but more difficult to check?
	  // check if (thr.interior) is a subset of some territory's (threats.interior), which differs only by our dots
	  {
	    bool is_boring = false;
	    std::vector<int8_t> interior(coord.last+1, 0);
	    for (auto i : thr.second.encl->interior) interior[i]=1;
	    for (auto &t : threats[pl].threats)
	      if (t.type & ThreatConsts::TERR) {
		bool subset = true;
		// check if every point in old terr is a point inside new one or our dot
		for (auto i : t.encl->interior) {
		  if (interior[i]==0 && (worm[i] & MASK_DOT) != (pl+1) && i!=thr.second.where) {
		    subset = false;
		    break;
		  }
		}
		if (subset) {
		  // and vice versa, check if each point of a new terr is a point in old one
		  std::vector<int8_t> interior_old(coord.last+1, 0);
		  for (auto i : t.encl->interior) interior_old[i]=1;
		  for (auto i : thr.second.encl->interior) {
		    if (interior_old[i]==0) {
		      subset = false;
		      break;
		    }
		  }
		}
		if (subset) {
		  /*
		  show();
		  std::cerr << "Równoważne zagrożenia (oryg, nowe): " << std::endl;
		  std::cerr << t.show() << std::endl;
		  std::cerr << thr.second.show() << std::endl;
		  */
		  is_boring = true;
		  break;
		}
	      }
	    if (is_boring) continue;
	  }
	}
	if (!shown) { shown=true;  show();  }
	std::cerr << "Nieznalezione zagrozenie (gracz=" << pl+1 << "):" << std::endl;
	std::cerr << thr.second.show() << std::endl;
      }
    }
  // check if is_in_terr, is_in_encl and is_in_border are correct
  // also check singular_dots and border_dots_in_danger
  for (int pl=0; pl<2; pl++) {
    std::vector<pti> te(coord.getSize(), 0);
    std::vector<pti> en(coord.getSize(), 0);
    std::vector<pti> bo(coord.getSize(), 0);
    for (Threat &thr : threats[pl].threats) {
      if (thr.type & ThreatConsts::TERR) {
	for (auto i : thr.encl->interior) ++te[i];
      } else if (thr.type & ThreatConsts::ENCL) {
	for (auto i : thr.encl->interior) ++en[i];
      }
      for (auto i : thr.encl->border) ++bo[i];
      bo[thr.encl->border[0]]--;   // this was counted twice
      if (thr.type & ThreatConsts::TO_REMOVE) {
	if (!shown) { shown=true;  show();  }
	std::cerr << "Nieusuniete zagrozenie (gracz=" << pl+1 << "):" << std::endl << thr.show() << std::endl;
      }
      // now check singular_dots
      int singular = 0;
      std::set<pti> counted;
      for (auto i : thr.encl->interior) {
	if (whoseDotMarginAt(i) == 2-pl && threats[pl].is_in_encl[i] + threats[pl].is_in_terr[i] == 1
	    && counted.find(descr.at(worm[i]).leftmost) == counted.end()) {
	  singular+= descr.at(worm[i]).dots[1-pl];
	  counted.insert(descr.at(worm[i]).leftmost);
	}
      }
      if (singular != thr.singular_dots) {
	std::cerr << "Bledne singular_dots (gracz=" << pl+1 << "), powinno byc: " << singular << ", jest: " << thr.singular_dots << std::endl;
	show();
	std::cerr << thr.show() << std::endl;
	return false;
      }
      // check border_dots_in_danger and opp_thr
      std::vector<uint64_t> true_ot;
      int in_danger = 0;
      for (auto it= thr.encl->border.begin()+1; it != thr.encl->border.end(); ++it) {
	pti i = *it;
	if (threats[1-pl].is_in_encl[i] > 0 || threats[1-pl].is_in_terr[i] > 0) {
	  in_danger++;
	  for (auto &ot : threats[1-pl].threats) {
	    if (ot.encl->isInInterior(i))
	      addOppThreatZobrist(true_ot, ot.zobrist_key);
	  }
	}
      }
      if (in_danger != thr.border_dots_in_danger) {
	std::cerr << "Bledne border_dots_in_danger (gracz=" << pl+1 << "), powinno byc: " << in_danger << ", jest: " << thr.border_dots_in_danger << std::endl;
	show();
	std::cerr << thr.show() << std::endl;
	return false;
      }
      std::vector<uint64_t> old_ot(thr.opp_thr);
      std::sort(true_ot.begin(), true_ot.end());
      std::sort(old_ot.begin(), old_ot.end());
      for (unsigned i=0; i<std::min(true_ot.size(), old_ot.size()); i++) {
	if (true_ot[i] != old_ot[i]) {
	  std::cerr << "Blad: jest " << old_ot[i] << ", powinno byc " << true_ot[i] <<std::endl;
	  show();
	  std::cerr << thr.show() << std::endl;
	  for (auto &ot : threats[1-pl].threats) {
	    if (ot.zobrist_key == old_ot[i]) { std::cerr << ot.show() << std::endl; }
	    if (ot.zobrist_key == true_ot[i]) { std::cerr << ot.show() << std::endl; }
	  }
	  return false;
	}
      }
      if (true_ot.size() != old_ot.size()) {
	if (true_ot.size() < old_ot.size()) {
	  for (unsigned i=true_ot.size(); i<old_ot.size(); i++) {
	    std::cerr << "Nadmiarowe opp_thr " << old_ot[i] << std::endl;
	    for (auto &ot : threats[1-pl].threats) {
	      if (ot.zobrist_key == old_ot[i]) thr.show();
	    }
	  }
	} else {
	  for (unsigned i=old_ot.size(); i<true_ot.size(); i++) {
	    std::cerr << "Nadmiarowe opp_thr " << true_ot[i] << std::endl;
	    for (auto &ot : threats[1-pl].threats) {
	      if (ot.zobrist_key == true_ot[i]) thr.show();
	    }
	  }
	}
	return false;
      }
    }
    //
    for (int i=coord.first; i<=coord.last; i++) {
      if (te[i] != threats[pl].is_in_terr[i]) {
	if (!shown) { shown=true;  show();  }
	std::cerr << "Bledne is_in_terr (gracz=" << pl+1 << "), powinno byc:" << std::endl << coord.showFullBoard(te) << std::endl;
	std::cerr << "jest: " << std::endl << coord.showFullBoard(threats[pl].is_in_terr) << std::endl;
	std::cerr << "Zagrozenia:" << std::endl;
	for (Threat &thr : threats[pl].threats) std::cerr << thr.show() << std::endl;
	break;
      }
      if (en[i] != threats[pl].is_in_encl[i]) {
	if (!shown) { shown=true;  show();  }
	std::cerr << "Bledne is_in_encl (gracz=" << pl+1 << "), powinno byc:" << std::endl << coord.showFullBoard(en) << std::endl;
	std::cerr << "jest: " << std::endl << coord.showFullBoard(threats[pl].is_in_encl) << std::endl;
	break;
      }
      if (bo[i] != threats[pl].is_in_border[i]) {
	if (!shown) { shown=true;  show();  }
	std::cerr << "Bledne is_in_border (gracz=" << pl+1 << "), powinno byc:" << std::endl << coord.showFullBoard(bo) << std::endl;
	std::cerr << "jest: " << std::endl << coord.showFullBoard(threats[pl].is_in_border) << std::endl;
	break;
      }

    }
  }

  return !shown;
}

bool
Game::checkThreat2movesCorrectness()
{
  std::set<std::array<pti,2>> pairs;
  std::vector<pti> is_in_2m_encl(coord.getSize(), 0);
  std::vector<pti> is_in_2m_miai(coord.getSize(), 0);
  for (int who=1; who<=2; who++) {
    for (auto &thr2 : threats[who-1].threats2m) {
      pti where0 = thr2.where0;
      std::vector<pti> is_in_encl2(coord.getSize(), 0);
      int found_safe = ((thr2.flags & Threat2mconsts::FLAG_SAFE) != 0);
      int real_safe = (threats[2-who].is_in_encl[where0] == 0 && threats[2-who].is_in_terr[where0] == 0);
      if (found_safe != real_safe) {
	std::cerr << "flag = " << thr2.flags << "; is_in_encl[where0]==" << threats[2-who].is_in_encl[where0]
		  << ", is_in_terr[where0]==" << threats[2-who].is_in_terr[where0] << std::endl;
	show();
	std::cerr << "where0 = " << coord.showPt(where0) << std::endl;
	std::cerr << "Threats: " << std::endl;
	for (auto t : thr2.thr_list) std::cerr << t.show();
	return false;
      }
      for (auto &thr : thr2.thr_list) {
	pti where = thr.where;
	pairs.insert({where0, where});
	bool correct = false;
	if (worm[where] == 0 && worm[where0] == 0) {
	  CleanupOneVar<pti> worm_where_cleanup0(&worm[where0], who);   //  worm[where0] = who;  with restored old value (0) by destructor
	  CleanupOneVar<pti> worm_where_cleanup(&worm[where], who);   //  worm[where] = who;  with restored old value (0) by destructor
	  int done[4] = {0,0,0,0};
	  for (int j=0; j<4; j++)
	    if (!done[j]) {
	      pti nb = where + coord.nb4[j];
	      if ((worm[nb] & MASK_DOT) != who && coord.dist[nb] >= 1 && thr.encl->isInInterior(nb)) {
		Threat t;
		t.encl = std::make_shared<Enclosure>(findEnclosure_notOptimised(nb, MASK_DOT, who));
		if (!t.encl->isEmpty() && t.encl->isInBorder(where) && t.encl->isInBorder(where0)) {
		  t.zobrist_key = t.encl->zobristKey(who);
		  if (t.zobrist_key == thr.zobrist_key) {   // our old threat is still valid
		    correct = true;
		    goto Thr2_correct;
		  }
		  // if some next neighbour has been enclosed, mark it as done
		  for (int k=j+1; k<4; k++) {
		    if (t.encl->isInInterior(where + coord.nb4[k]))
		      done[k]=1;
		  }
		}
	      }
	    }
	}
      Thr2_correct:;
	if (!correct) {
	  show();
	  std::cerr << "where0 = " << coord.showPt(where0) << ", where=" << coord.showPt(where) << std::endl;
	  thr.show();
	  return false;
	}
	// check stats
	for (pti p : thr.encl->interior) {
	  is_in_encl2[p] += Threat2mconsts::ENCL2_INSIDE_ADD;
	  if (thr2.win_move_count >= 2 || (thr2.win_move_count==1 && thr.opp_dots==0)) is_in_encl2[p] |= Threat2mconsts::ENCL2_MIAI;
	}
      }
      std::string err;
      bool correct = true;
      if (thr2.is_in_encl2.empty() && thr2.thr_list.size()>1) {
	err = "empty encl2";
	correct = false;
      } else if (!thr2.is_in_encl2.empty()) {
	for (int i=0; i<coord.getSize(); i++) {
	  if (is_in_encl2[i] != thr2.is_in_encl2[i]) {
	    std::stringstream out;
	    out << " error in is_in_encl2 at " << coord.showPt(i) << "  is: " << thr2.is_in_encl2[i] << ", should be: " << is_in_encl2[i] << std::endl;
	    err = out.str();
	    correct = false;
	    break;
	  }
	  if (thr2.flags & Threat2mconsts::FLAG_SAFE) {
	    is_in_2m_encl[i] += (is_in_encl2[i] > Threat2mconsts::ENCL2_INSIDE_THRESHOLD);
	    is_in_2m_miai[i] += (is_in_encl2[i] & Threat2mconsts::ENCL2_MIAI) != 0;
	  }
	}
      }
      if (!correct) {
	show();
	std::cerr << "where0 = " << coord.showPt(where0) << ", err = " << err  << std::endl;
	std::cerr << "Threats: " << std::endl;
	for (auto t : thr2.thr_list) std::cerr << t.show();
	return false;
      }
    }
  }
  bool correct = true;
  for (auto p : pairs) {
    std::array<pti,2> pair2 {p[1], p[0]};
    if (pairs.find(pair2)==pairs.end()) {
      if (correct) show();
      std::cerr << "Error: Threat2 " << coord.showPt(p[0]) << "--" << coord.showPt(p[1]) << " found, but not the reversed one." << std::endl;
      correct = false;
    }
  }
  return correct;
}



bool
Game::checkConnectionsCorrectness()
{
  std::vector<OneConnection> tmp[2] { connects[0], connects[1] };
  findConnections();
  for (int ind = 0; ind<coord.getSize(); ind++) {
    if (connects[0][ind] != tmp[0][ind] || connects[1][ind] != tmp[1][ind]) {
      std::cerr << "Zla tablica connections, indeks " << ind << " = (" << int(coord.x[ind]) << ", " << int(coord.y[ind]) << ")." << std::endl;
      show();
      std::cerr << "Stara tab 1" << std::endl;
      std::cerr << coord.showBoard(tmp[0]);
      std::cerr << "Stara tab 2" << std::endl;
      std::cerr << coord.showBoard(tmp[1]);
      showConnections();
      //
      showGroupsId();
      for (int g=0; g<=1; g++) {
	for (int j=0; j<4; j++) std::cerr << int(tmp[g][ind].groups_id[j]) << " ";
	if (g==0) std::cerr << ","; else std::cerr << std::endl;
      }
      std::cerr << "Nowe conn: ";
      for (int g=0; g<=1; g++) {
	for (int j=0; j<4; j++) std::cerr << int(connects[g][ind].groups_id[j]) << " ";
	if (g==0) std::cerr << ","; else std::cerr << std::endl;
      }

      return false;
    }
  }
  return true;
}


bool
Game::checkPattern3valuesCorrectness() const
{
  for (int ind = 0; ind<coord.getSize(); ind++) {
    real_t should_be[2];
    if (whoseDotMarginAt(ind) != 0) {
      should_be[0] = 0;        should_be[1] = 0;
    } else {
      should_be[0] = getPattern3Value(ind, 1);
      should_be[1] = getPattern3Value(ind, 2);
    }
    for (int who=1; who<=2; ++who) {
      auto is = pattern3_value[who-1][ind];
      if (is != should_be[who-1]) {
	std::cerr << "recalculate_list: ";
	for (auto i : recalculate_list) std::cerr << coord.showPt(i) << " ";
	std::cerr << std::endl;
	show();
	std::cerr << "Wrong patt3 value for who=" << who << " at " << coord.showPt(ind) << ", is: " << is << " should be: " << should_be[who-1] << std::endl;
	assert(is == should_be[who-1]);
	return false;
      }
    }
  }
  // check list of moves
  std::vector<pti> listm[3];
  for (pti ind = coord.first; ind<=coord.last; ind++) {
    if (whoseDotMarginAt(ind)==0) {
      if (threats[0].is_in_encl[ind]==0 && threats[0].is_in_terr[ind]==0 && threats[1].is_in_encl[ind]==0 && threats[1].is_in_terr[ind]==0) {
	if (isDame_directCheck_symm(ind))
	  listm[PossibleMovesConsts::LIST_DAME].push_back(ind);
	else
	  listm[PossibleMovesConsts::LIST_NEUTRAL].push_back(ind);
      } else {
	listm[PossibleMovesConsts::LIST_TERRM].push_back(ind);
      }
    }
  }
  std::string names[3] = { "neutral", "dame", "terrm" };
  bool status = true;
  for (int j=0; j<3; j++) {
    std::vector<pti> possm = possible_moves.lists[j];
    if (possm.size() != listm[j].size()) {
      std::cerr << "Size of possible_moves (" << names[j] << ") differ -- should be" << listm[j].size() << ", is: " << possm.size() << std::endl;
      status = false;
    }
    std::sort(listm[j].begin(), listm[j].end());
    std::sort(possm.begin(), possm.end());
    std::vector<pti> diff;
    std::set_difference(listm[j].begin(), listm[j].end(), possm.begin(), possm.end(), back_inserter(diff));
    if (!diff.empty()) {
      std::cerr << "[" << names[j] << "] Moves that are missing: ";
      for (auto p : diff) std::cerr << coord.showPt(p) << " ";
      std::cerr << std::endl;
      status = false;
    }
    diff.clear();
    std::set_difference(possm.begin(), possm.end(), listm[j].begin(), listm[j].end(), back_inserter(diff));
    if (!diff.empty()) {
      std::cerr << "[" << names[j] << "] Moves that are incorrecly contained: ";
      for (auto p : diff) std::cerr << coord.showPt(p) << " ";
      std::cerr << std::endl;
      status = false;
    }
  }
  if (!status) show();
  return status;
}

bool
Game::checkPossibleMovesCorrectness(int who) const
{
  int terr = 0, neutral_or_dame = 0;
  int ext = 0, exnd = 0;
  for (int i=coord.first; i<=coord.last; ++i) {
    if (whoseDotMarginAt(i) == 0) {
      if (threats[0].is_in_terr[i] || threats[0].is_in_encl[i] || threats[1].is_in_terr[i] || threats[1].is_in_encl[i]) {
	++terr;
	ext = i;
      } else {
	++neutral_or_dame;
	exnd = i;
      }
    }
  }
  if ( (neutral_or_dame > 0) != (!possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].empty() || !possible_moves.lists[PossibleMovesConsts::LIST_DAME].empty()) ) {
    if (neutral_or_dame > 0) {
      std::cerr << "Possible neutral or dame moves is empty, but there is such a move: " << coord.showPt(exnd) << std::endl;
    } else {
      if (!possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL].empty()) {
	std::cerr << "Possible neutral moves has: " << coord.showPt(possible_moves.lists[PossibleMovesConsts::LIST_NEUTRAL][0]) << std::endl;
      }
      if (!possible_moves.lists[PossibleMovesConsts::LIST_DAME].empty()) {
	std::cerr << "Possible dame moves has: " << coord.showPt(possible_moves.lists[PossibleMovesConsts::LIST_DAME][0]) << std::endl;
      }

    }
    show();
    return false;
  }
  if ( (terr > 0) != (!possible_moves.lists[PossibleMovesConsts::LIST_TERRM].empty()) ) {
    if (terr > 0) {
      std::cerr << "TERR moves is empty, but there is such a move: " << coord.showPt(ext) << std::endl;
    } else {
      std::cerr << "Possible TERR move: " << coord.showPt(possible_moves.lists[PossibleMovesConsts::LIST_TERRM][0]) << std::endl;
    }
    show();
    return false;
  }
  return true;
}

bool
Game::checkCorrectness(SgfSequence seq)
{
  if (seq[0].findProp("RE") == seq[0].props.end())
    return true;  // cannot check
  std::string re = seq[0].findProp("RE")->second[0];
  int res=0;
  if (re == "W+R" || re == "W+F" || re=="W+T" || re == "B+R" || re == "B+F" || re=="B+T" || re=="?")
    return true;  // cannot check
  if (re.substr(0,2) == "W+") {
    res = -stoi(re.substr(2));
  } else if (re.substr(0,2) == "B+") {
    res = stoi(re.substr(2));
  } else if (re == "0") {
    res = 0;
  } else
    return false;
  auto saved_komi = global::komi;  global::komi = 0;
  auto [ct, ct_small_score] = countTerritory_simple(nowMoves);
  if (ct==res) {
    global::komi = saved_komi;
    return true;
  }
  auto [ct2, ct2_small_score] = countTerritory(nowMoves);
  global::komi = saved_komi;
  std::cerr << "Blad, ct=" << ct << ", res=" << res << ", zwykle ct=" << ct2 << std::endl;
  return false;
}


void
Game::findConnections()
{
  for (int x=0; x<coord.wlkx; x++) {
    pti ind = coord.ind(x, 0);
    for (int y=0; y<coord.wlky; y++) {
      connectionsRecalculatePoint(ind, 1);
      connectionsRecalculatePoint(ind, 2);
      ind += coord.S;
    }
  }
}

