/********************************************************************************************************
 kropla -- a program to play Kropki; file threats.h.
    Copyright (C) 2015,2016,2017,2018 Bartek Dyda,
    email: bartekdyda (at) protonmail (dot) com

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

#include "board.h"
#include "enclosure.h"
#include <vector>
#include <list>
#include <memory>

/********************************************************************************************************
  Threat class
*********************************************************************************************************/
namespace ThreatConsts
{
  // threat types
  constexpr int TERR=1;
  constexpr int ENCL=2;
  constexpr int TO_REMOVE=64;
  constexpr int TO_CHECK=128;
};

struct Threat {
  pti where;   // where to put dot, or 0 if no need to put a dot (so a TERRitory)
  uint16_t type {0};
  int16_t terr_points {0};     // == number of empty places inside
  int16_t opp_dots {0};
  int16_t singular_dots {0};   // == number of opp's dots which are inside only this threat
  int16_t border_dots_in_danger {0};  // == number of dots in border that are in some opp's threats
  uint64_t zobrist_key;
  std::shared_ptr<Enclosure> encl;
  std::vector<uint64_t> opp_thr;  // zobrist keys of opp's threats that enclose some border points of this threat
  std::array<pti, 4>  shortcuts;  // this is only used for Threat2m (TODO: maybe inherit a new class with this field?)
  // SmallMultiset<pti, 4> shortcuts;  // this is only used for Threat2m (TODO: maybe inherit a new class with this field?)
  //Threat() : encl(nullptr) { }
  Threat() : shortcuts()  {}
  bool isShortcut(pti x) const;
  void addShortcuts(pti ind0, pti ind1);
  std::string show() const;
};

/********************************************************************************************************
  Threat2m (threat in 2 moves) class
*********************************************************************************************************/
namespace Threat2mconsts
{
  constexpr uint16_t FLAG_SAFE=1;
  constexpr uint16_t FLAG_RECALCULATE=2;   // should we recalcuate min_wins and miai flag after removal
  constexpr uint16_t ENCL2_MIAI=1;   // this bit we set in is_in_encl2 to denote miai (i.e., exists other threat with opp-dots win)
  constexpr uint16_t ENCL2_INSIDE_ADD=2;  // this number we add for each point inside threat
  constexpr uint16_t ENCL2_INSIDE_THRESHOLD=2*ENCL2_INSIDE_ADD;  // the threshold to see whether a point can be for sure captured

};

struct Threat2m {
  pti where0;   // where to put the first dot
  int16_t  min_win {0}, min_win2 {0}; // minimal win, second minimal win (i.e., how many opponent dots are captured)
  uint16_t flags {0};           // currently only FLAG_SAFE, in which case the threats get counted in AllThreats::is_in_2m_encl/is_in_2m_miai
  int16_t  win_move_count {0};      // number of threats in thr_list with opp-dot capture
  std::vector<pti> is_in_encl2;   // this we assign to 0 only after we have at least 2 threats
  std::vector<Threat> thr_list;
  bool isSafe() const { return (flags & Threat2mconsts::FLAG_SAFE)!=0; };
  //void removeMarked();
  std::string show() const;
};

/********************************************************************************************************
  AllThreats class
*********************************************************************************************************/
struct AllThreats {
  std::vector<Threat> threats;
  //  std::vector<Threat2m> threats2m;
  std::list<Threat2m> threats2m;
  std::vector<pti> is_in_encl;
  std::vector<pti> is_in_terr;
  std::vector<pti> is_in_border;
  std::vector<pti> is_in_2m_encl;
  std::vector<pti> is_in_2m_miai;
  AllThreats() : is_in_encl(coord.getSize(), 0), is_in_terr(coord.getSize(), 0), is_in_border(coord.getSize(), 0),
		 is_in_2m_encl(coord.getSize(), 0), is_in_2m_miai(coord.getSize(), 0) {};
  //AllThreats(const AllThreats& other);
  int addThreat2moves(pti ind0, pti ind1, bool safe0, bool safe1, int who, Threat &t);
  void changeEnclToTerr(const Threat& t);
  void removeMarkedAndAtPoint2moves(pti ind);
  void removeMarked2moves();
  void changeFlagSafe(Threat2m &t2);
  Threat* findThreatWhichContains(pti ind);
  Threat* findThreatZobrist(uint64_t zobr);
  const Threat* findThreatZobrist_const(uint64_t zobr) const;
private:
  void subtractThreat2moves(Threat2m &t2, const Threat& t);
  void deleteMiai(Threat2m &t2);
  void recalculateMiai(Threat2m &t2);
  void addThreat2moves_toStats(Threat2m &t2, Threat &t);
  void addThreat2moves_toMiai(Threat2m &t2, Threat &t);
};

extern int debug_foundt2m;


/********************************************************************************************************
  ThrInfo class for finding necessary enclosures
*********************************************************************************************************/
namespace ThrInfoConsts {
  constexpr pti MINF = -20000;
  constexpr pti VALUE_WON_DOT = 8;
  constexpr pti VALUE_SAVED_DOT = 8;
};

struct ThrInfo {
  std::vector<uint64_t> opp_thr;
  std::vector<pti> saved_worms;  // list of our saved worms, to calculated saved_dots correctly
  uint64_t zobrist_key;
  const Threat* thr_pointer;
  pti type;
  pti move;
  pti lost_terr_points, won_dots, saved_dots, priority_value;
  bool operator<(const ThrInfo& other) const { return priority_value > other.priority_value; };  // warning: > to sort descending!
  ThrInfo() { thr_pointer = nullptr; type=0; move=0; lost_terr_points= won_dots= saved_dots= priority_value= 0; };
  std::string show() const;
  pti calculatePriorityValue() const;
};

