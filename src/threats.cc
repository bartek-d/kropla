/********************************************************************************************************
 kropla -- a program to play Kropki; file threats.cc.
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

#include "threats.h"
#include <cassert>
#include <algorithm>
#include <iostream>

bool
Threat::isShortcut(pti x) const
{
  for (int i=0; i<4; ++i)
    if (shortcuts[i] == x) return true;
  return false;
}

void
Threat::addShortcuts(pti ind0, pti ind1)
{
  shortcuts.fill(-1);
  int count = 0;
  assert(encl->border[0] == encl->border[encl->border.size()-1]);
  for (pti ind : {ind0, ind1}) {
    int nr=0;
    while (encl->border[nr] != ind && nr < encl->border.size()) nr++;
    pti prev, next;
    if (nr > 0) {
      assert(nr+1 < encl->border.size());  // this should hold because border[0] == border[border.size()-1]
      prev = encl->border[nr-1];
      next = encl->border[nr+1];
    } else {
      prev = encl->border[encl->border.size()-2];
      next = encl->border[1];
    }
    for (int i=0; i<4; i++) {
      pti nb = ind + coord.nb4[i];
      if (coord.isInNeighbourhood(nb, prev) && coord.isInNeighbourhood(nb, next)) {  // && !shortcuts.contains(nb)) {
	//shortcuts.insert(nb);
	assert(count<4);
	shortcuts[count++] = nb;
      }
    }
  }
}

std::string
Threat::show() const
{
  std::stringstream out;
  out << "Typ = " << type << ", zobr = " << /*std::hex << */ zobrist_key << ", gdzie=" << coord.showPt(where) << std::endl;
  return out.str() + encl->show();
}

void removeMarkedThreats(std::vector<Threat> &thr_list)
{
  int s = thr_list.size();
  for (int i=0; i<s; ++i) {
    if (thr_list[i].type & ThreatConsts::TO_REMOVE) {
      do { --s; } while (i<s && (thr_list[s].type & ThreatConsts::TO_REMOVE));
      if (i<s) std::swap(thr_list[i], thr_list[s]);
    }
  }
  thr_list.erase( thr_list.begin() + s, thr_list.end() );
}



std::string
Threat2m::show() const
{
  std::stringstream out;
  out << "Threat2m: where0 = " << coord.showPt(where0) << ", flags = " << std::hex << flags << ", win_move_count=" << win_move_count << ", threat count = " << thr_list.size() << std::endl;
  for (auto &t : thr_list) {
    out << t.show() << std::endl;
  }
  return out.str();
}

/*
void
Threat2m::removeMarked()
{
  int s = thr_list.size();
  for (int i=0; i<s; ++i) {
    if (thr_list[i].type & ThreatConsts::TO_REMOVE) {
      do { --s; } while (i<s && (thr_list[s].type & ThreatConsts::TO_REMOVE));
      if (i<s) std::swap(thr_list[i], thr_list[s]);
    }
  }
  thr_list.erase( thr_list.begin() + s, thr_list.end() );
}
*/

void removeEmptyThreats2m(std::vector<Threat2m> &thr_list)
{
  int s = thr_list.size();
  for (int i=0; i<s; ++i) {
    if (thr_list[i].thr_list.empty()) {
      do { --s; } while (i<s && thr_list[s].thr_list.empty());
      if (i<s) std::swap(thr_list[i], thr_list[s]);
    }
  }
  thr_list.erase( thr_list.begin() + s, thr_list.end() );
}

/*
AllThreats::AllThreats(const AllThreats& other)
{
  threats.reserve(other.threats.size() + 50);
  threats2m.reserve(other.threats2m.size() + 250);
  std::copy( other.threats.begin(), other.threats.end(), std::back_inserter(threats) );
  std::copy( other.threats2m.begin(), other.threats2m.end(), std::back_inserter(threats2m) );
  is_in_encl = other.is_in_encl;
  is_in_terr = other.is_in_terr;
  is_in_border = other.is_in_border;
  is_in_2m_encl = other.is_in_2m_encl;
  is_in_2m_miai = other.is_in_2m_miai;
}
*/

void
AllThreats::changeEnclToTerr(const Threat& t)
{
  for (auto i : t.encl->interior) {
    --is_in_encl[i];
    ++is_in_terr[i];
  }
}

void
AllThreats::addThreat2moves_toStats(Threat2m &t2, Threat &t)
{
  assert(!t2.is_in_encl2.empty());
  if (t2.win_move_count >= 2 || (t2.win_move_count == 1 && t.opp_dots==0)) {
    // miai possibility
    if (t2.flags & Threat2mconsts::FLAG_SAFE) {
      for (pti p : t.encl->interior) {
	if ((t2.is_in_encl2[p] & Threat2mconsts::ENCL2_MIAI)==0) {
	  t2.is_in_encl2[p] |= Threat2mconsts::ENCL2_MIAI;
	  is_in_2m_miai[p]++;
	}
	bool was_not = (t2.is_in_encl2[p] < Threat2mconsts::ENCL2_INSIDE_THRESHOLD);
	t2.is_in_encl2[p] += Threat2mconsts::ENCL2_INSIDE_ADD;
	if (was_not && t2.is_in_encl2[p] >= Threat2mconsts::ENCL2_INSIDE_THRESHOLD)
	  is_in_2m_encl[p]++;
      }
    } else {
      for (pti p : t.encl->interior) {
	t2.is_in_encl2[p] |= Threat2mconsts::ENCL2_MIAI;
	t2.is_in_encl2[p] += Threat2mconsts::ENCL2_INSIDE_ADD;
      }
    }
  } else {
    // no miai
    if (t2.flags & Threat2mconsts::FLAG_SAFE) {
      for (pti p : t.encl->interior) {
	bool was_not = (t2.is_in_encl2[p] < Threat2mconsts::ENCL2_INSIDE_THRESHOLD);
	t2.is_in_encl2[p] += Threat2mconsts::ENCL2_INSIDE_ADD;
	if (was_not && t2.is_in_encl2[p] >= Threat2mconsts::ENCL2_INSIDE_THRESHOLD)
	  is_in_2m_encl[p]++;
      }
    } else {
      for (pti p : t.encl->interior) {
	t2.is_in_encl2[p] += Threat2mconsts::ENCL2_INSIDE_ADD;
      }
    }
  }
}

void
AllThreats::addThreat2moves_toMiai(Threat2m &t2, Threat &t)
{
  assert(!t2.is_in_encl2.empty());
  if (t2.win_move_count >= 2 || (t2.win_move_count == 1 && t.opp_dots==0)) {
    // miai possibility
    if (t2.flags & Threat2mconsts::FLAG_SAFE) {
      for (pti p : t.encl->interior) {
	if ((t2.is_in_encl2[p] & Threat2mconsts::ENCL2_MIAI)==0) {
	  t2.is_in_encl2[p] |= Threat2mconsts::ENCL2_MIAI;
	  is_in_2m_miai[p]++;
	}
      }
    } else {
      for (pti p : t.encl->interior) {
	t2.is_in_encl2[p] |= Threat2mconsts::ENCL2_MIAI;
      }
    }
  } else {
    // no miai
  }
}

int debug_foundt2m = 0;

/// @param[in] t  Threat after ind0-ind1 moves, with t.type, t.zobrist_key, t.encl and t.opp_dots set.
/// @param[in] safe0  Is placing dot at ind0 safe for who.
/// @param[in] safe1  Is placing dot at ind1 safe for who.
int
AllThreats::addThreat2moves(pti ind0, pti ind1, bool safe0, bool safe1, int who, Threat &t)
{
#ifndef NDEBUG
  // DEBUG: check whether there is a 
  if (t.encl->checkIfRedundant(ind0)) {
    //game.show();
    std::cerr << "Redundant point0 in thr2m: " << coord.showPt(ind0) << " second point: " << coord.showPt(ind1) <<
      " enclosure:" << std::endl << t.encl->show() << std::endl;
    assert(0);
  }
  if (t.encl->checkIfRedundant(ind1)) {
    //game.show();
    std::cerr << "Redundant point1 in thr2m: " << coord.showPt(ind1) << " second point: " << coord.showPt(ind0) <<
      " enclosure:" << std::endl << t.encl->show() << std::endl;
    assert(0);
  }
#endif

  // add 2 threats: (ind0, ind1) and (ind1, ind0)
  int added = 0;
  t.addShortcuts(ind0, ind1);
  for (int i=0; i<2; i++, std::swap(ind0, ind1), std::swap(safe0, safe1)) {
    auto pos = std::find_if(threats2m.begin(), threats2m.end(),
			    [ind0](Threat2m &t) { return (t.where0 == ind0); } );
    if (pos == threats2m.end()) {
      Threat2m t2;
      t2.where0 = ind0;
      t2.flags = safe0 ? Threat2mconsts::FLAG_SAFE : 0;
      t.where = ind1;
      t2.thr_list = std::vector<Threat> {t};
      if (t.opp_dots) {
	t2.min_win = t.opp_dots;
	t2.win_move_count++;
      }
      threats2m.push_back(t2);
      ++added;
    } else {
      //if (!pos->is_in_encl2.empty()) std::cerr << "is_in_encl2[" << coord.showPt(ind0) << "] przed:" << coord.showBoard(pos->is_in_encl2) << std::endl;
      auto pos2 = std::find_if(pos->thr_list.begin(), pos->thr_list.end(),
			       [t](Threat &tt) { return (tt.zobrist_key == t.zobrist_key); } );
#ifndef NDEBUG
      if (((pos->flags & Threat2mconsts::FLAG_SAFE)!=0) != safe0) {
	std::cerr << pos->show() << std::endl;
      }
      assert(((pos->flags & Threat2mconsts::FLAG_SAFE)!=0) == safe0);
#endif      
      if (pos2 != pos->thr_list.end()) {
	
	debug_foundt2m++;
	if (pos2->type & ThreatConsts::TO_REMOVE) {
	  pos2->type &= ~ThreatConsts::TO_REMOVE;
	  continue;   // this threat has been found before;  used to be 'break' (v120-), we now (v121+) use continue to reset TO_REMOVE also in the second copy of this threat
	} else {
	  break;     // v121+: now we only break if bit TO_REMOVE was not set
	}
      }
      t.where = ind1;
      // recalculate points in danger, etc.
      if (t.opp_dots) {
	pos->win_move_count++;
	if (t.opp_dots > pos->min_win) {
	  pos->min_win2 = pos->min_win;
	  pos->min_win = t.opp_dots;
	} else if (t.opp_dots > pos->min_win2) {
	  pos->min_win2 = t.opp_dots;
	}
	switch (pos->win_move_count) {
	case 1:
	  if (pos->is_in_encl2.empty()) {
	    pos->is_in_encl2.assign(coord.getSize(), 0);
	    for (auto &tt : pos->thr_list) addThreat2moves_toStats(*pos, tt);
	  } else {
	    for (auto &tt : pos->thr_list) addThreat2moves_toMiai(*pos, tt);
	  }
	  break;
	case 2:
	  if (pos->is_in_encl2.empty()) {
	    pos->is_in_encl2.assign(coord.getSize(), 0);
	    assert(pos->thr_list.size() == 1);
	    addThreat2moves_toStats(*pos, pos->thr_list[0]);
	  } else {
	    // set miai in the only capture so far
	    for (auto &tt : pos->thr_list) {
	      if (tt.opp_dots) {
		addThreat2moves_toMiai(*pos, tt);
		break;
	      }
	    }
	  }
	  break;
	}
      } else {
	if (pos->is_in_encl2.empty()) {
	  pos->is_in_encl2.assign(coord.getSize(), 0);
	  assert(pos->thr_list.size() == 1);
	  for (pti p : pos->thr_list[0].encl->interior) {
	    pos->is_in_encl2[p] = Threat2mconsts::ENCL2_INSIDE_ADD;
	  }
	}
      }
      // add the threat
      addThreat2moves_toStats(*pos, t);
      pos->thr_list.push_back(t);
      ++added;
      //
      //if (!pos->is_in_encl2.empty()) std::cerr << "is_in_encl2[" << coord.showPt(ind0) << "] po:" << coord.showBoard(pos->is_in_encl2) << std::endl;
    }
  }
  return added;
}


void
AllThreats::subtractThreat2moves(Threat2m &t2, const Threat& t)
{
  if (!t2.is_in_encl2.empty()) {
    for (pti p : t.encl->interior) {
      bool was = (t2.is_in_encl2[p] >= Threat2mconsts::ENCL2_INSIDE_THRESHOLD);
      t2.is_in_encl2[p] -= Threat2mconsts::ENCL2_INSIDE_ADD;
      if (was && t2.is_in_encl2[p] < Threat2mconsts::ENCL2_INSIDE_THRESHOLD &&
	  (t2.flags & Threat2mconsts::FLAG_SAFE)) {
	is_in_2m_encl[p]--;
      }
    }
  }
  if (t2.win_move_count) {
    if (t.opp_dots) t2.win_move_count--;
    t2.flags |= Threat2mconsts::FLAG_RECALCULATE;
  }
}


void
AllThreats::deleteMiai(Threat2m &t2) {
  // delete miai caused by the about to delete threat2m t2
  if (t2.is_in_encl2.empty()) return;
  // delete all miai's
  for (int i=coord.first; i<=coord.last; i++) {
    if (t2.is_in_encl2[i] & Threat2mconsts::ENCL2_MIAI) {
      if (t2.flags & Threat2mconsts::FLAG_SAFE) is_in_2m_miai[i]--;
    }
  }
}

void
AllThreats::recalculateMiai(Threat2m &t2) {
  // recalculate miai and wins
  assert(!t2.is_in_encl2.empty());
  // delete all miai's
  for (int i=coord.first; i<=coord.last; i++) {
    if (t2.is_in_encl2[i] & Threat2mconsts::ENCL2_MIAI) {
      t2.is_in_encl2[i] &= ~Threat2mconsts::ENCL2_MIAI;
      if (t2.flags & Threat2mconsts::FLAG_SAFE) is_in_2m_miai[i]--;
    }
  }
  // do them again
  t2.min_win = t2.min_win2 = 0;
  for (auto &t : t2.thr_list) {
    if (t.opp_dots >= t2.min_win) {
      t2.min_win2 = t.opp_dots;  t2.min_win = t.opp_dots;
    } else if (t.opp_dots > t2.min_win2) {
      t2.min_win2 = t.opp_dots;
    }
    addThreat2moves_toMiai(t2, t);
  }
  t2.flags &= ~Threat2mconsts::FLAG_RECALCULATE;
}


void
AllThreats::changeFlagSafe(Threat2m &t2)
{
  t2.flags ^= Threat2mconsts::FLAG_SAFE;
  if (t2.thr_list.size()>=2 && t2.win_move_count>=1) {
    pti change = (t2.flags & Threat2mconsts::FLAG_SAFE) ? 1 : -1;
    for (int ind=coord.first; ind<=coord.last; ind++) {
      if (t2.is_in_encl2[ind] & Threat2mconsts::ENCL2_MIAI)
	is_in_2m_miai[ind] += change;
      if (t2.is_in_encl2[ind] >= Threat2mconsts::ENCL2_INSIDE_THRESHOLD)
	is_in_2m_encl[ind] += change;
    }
  }
}

/// Finds first threat of who containing [ind].
Threat*
AllThreats::findThreatWhichContains(pti ind)
{
  for (auto &t : threats) {
    if (t.encl->isInInterior(ind)) return &t;
  }
  return nullptr;
}

Threat*
AllThreats::findThreatZobrist(uint64_t zobr)
{
  auto pos = std::find_if(threats.begin(), threats.end(),
			  [zobr](const Threat &t) { return (t.zobrist_key == zobr); });
  return (pos != threats.end()) ? (&*pos) : nullptr;
}

const Threat*
AllThreats::findThreatZobrist_const(uint64_t zobr) const
{
  auto pos = std::find_if(threats.begin(), threats.end(),
			  [zobr](const Threat &t) { return (t.zobrist_key == zobr); });
  return (pos != threats.end()) ? (&*pos) : nullptr;
}

void
AllThreats::removeMarkedAndAtPoint2moves(pti ind)
{
  if (!threats2m.empty()) {
    bool to_remove2 = false;
    for (auto &t2 : threats2m) {
      bool to_remove = false;
      for (auto &t : t2.thr_list) {
	if (t2.where0 == ind || t.where == ind || (t.type & ThreatConsts::TO_REMOVE)) {
	  subtractThreat2moves(t2, t);
	  t.type |= ThreatConsts::TO_REMOVE;
	  to_remove = true;
	}
      }
      if (to_remove) {
	//	t2.thr_list.erase( std::remove_if( t2.thr_list.begin(), t2.thr_list.end(), [](Threat &t) { return (t.type & ThreatConsts::TO_REMOVE); } ), t2.thr_list.end() );
	removeMarkedThreats(t2.thr_list);
	//t2.removeMarked();
	//
	if (t2.thr_list.empty()) {
	  to_remove2=true;
	  if (t2.flags & Threat2mconsts::FLAG_RECALCULATE) deleteMiai(t2);
	}
	else if (t2.flags & Threat2mconsts::FLAG_RECALCULATE) recalculateMiai(t2);
      }
    }
    if (to_remove2) {
      // removeEmptyThreats2m(threats2m); -- for vectors
      threats2m.erase( std::remove_if( threats2m.begin(), threats2m.end(), [](Threat2m &t2) { return t2.thr_list.empty(); } ),
		       threats2m.end() );
    }
  }
}

void
AllThreats::removeMarked2moves()
{
  if (!threats2m.empty()) {
    bool to_remove2 = false;
    for (auto &t2 : threats2m) {
      bool to_remove = false;
      for (auto &t : t2.thr_list) {
	if (t.type & ThreatConsts::TO_REMOVE) {
	  subtractThreat2moves(t2, t);
	  t.type |= ThreatConsts::TO_REMOVE;
	  to_remove = true;
	}
      }
      if (to_remove) {
	// t2.thr_list.erase( std::remove_if( t2.thr_list.begin(), t2.thr_list.end(), [](Threat &t) { return (t.type & ThreatConsts::TO_REMOVE); } ),  t2.thr_list.end() );
	removeMarkedThreats(t2.thr_list);
	//t2.removeMarked();
	if (t2.thr_list.empty()) {
	  to_remove2=true;
	  if (t2.flags & Threat2mconsts::FLAG_RECALCULATE) deleteMiai(t2);
	}
	else if (t2.flags & Threat2mconsts::FLAG_RECALCULATE) recalculateMiai(t2);
      }
    }
    if (to_remove2) {
      //removeEmptyThreats2m(threats2m);
      threats2m.erase( std::remove_if( threats2m.begin(), threats2m.end(), [](Threat2m &t2) { return t2.thr_list.empty(); } ),
		       threats2m.end() );
    }
  }
}


std::string
ThrInfo::show() const
{
  std::stringstream out;
  out << "lost_terr=" << lost_terr_points << ", won_dots=" << won_dots << ",  saved_dots=" <<  saved_dots << ", priority_value=" << priority_value;
  return out.str();
}

pti
ThrInfo::calculatePriorityValue() const
{
  return -lost_terr_points + saved_dots * ThrInfoConsts::VALUE_SAVED_DOT + won_dots * ThrInfoConsts::VALUE_WON_DOT;
}
