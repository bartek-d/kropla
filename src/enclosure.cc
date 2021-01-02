/********************************************************************************************************
 kropla -- a program to play Kropki; file enclosure.cc.
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

#include "enclosure.h"
#include <cassert>
#include <algorithm>

Enclosure empty_enclosure;

bool
Enclosure::isInInterior(pti p) const
{
  // return (std::binary_search(interior.begin(), interior.end(), p));
  // if (p <= border.front()) return false;  // to speed up on average
  return (std::find(interior.begin(), interior.end(), p)!=interior.end() );
}

bool
Enclosure::isInBorder(pti p) const
{
  return (std::find(border.begin(), border.end(), p)!=border.end() );
}

bool
Enclosure::isEmpty() const
{
  return border.empty();
}

// returns some element on the border
pti
Enclosure::getBorderElement() const
{
  return border.front();
}

// returns some element in the interior
pti
Enclosure::getInteriorElement() const
{
  return interior.front();
}

int
Enclosure::getInteriorSize() const
{
  return interior.size();
}

uint64_t
Enclosure::zobristKey(int who) const
{
  uint64_t key = 0;
  who--;
  for (auto &p : interior) {
    key ^= coord.zobrist_encl[who][p];
  }
  return key;
}

/// Given a point p1 from the border and a point x!=p1 where the player just played,
/// it finds previous (p0) and next (p2) point from the border and checks,
/// whether x touches p0, p1 and p2. In particular, it may happen that p1==x.
//  If yes, there is a shortcut.
/// @param[in] p1  Point from the border.
/// @param[in] x   Point where the player just played.
/// @return    bool value
bool
Enclosure::checkShortcut(pti p1, pti x) const
{
  //if (p1==x) return true;
  if (!coord.isInNeighbourhood(p1, x)) return false;
  assert(border[0] == border[border.size()-1]);
  unsigned nr=0;
  while (border[nr] != p1 && nr<border.size()) nr++;
  if (nr > 0) {
    assert(nr+1 < border.size());  // this should hold because border[0] == border[border.size()-1]
    return (coord.isInNeighbourhood(x, border[nr-1]) && coord.isInNeighbourhood(x, border[nr+1]));
  } else {
    return (coord.isInNeighbourhood(x, border[1]) && coord.isInNeighbourhood(x, border[border.size()-2]));
  }
}

/// Given a point p1 from the border,
/// it finds previous (p0) and next (p2) point from the border and checks,
/// whether p0 touches p2.
//  If yes, then p1 is redundant.
/// @param[in] p1  Point from the border.
/// @return    bool value
bool
Enclosure::checkIfRedundant(pti p1) const
{
  assert(border[0] == border[border.size()-1]);
  unsigned nr=0;
  while (border[nr] != p1 && nr<border.size()) nr++;
  if (nr > 0) {
    assert(nr+1 < border.size());  // this should hold because border[0] == border[border.size()-1]
    return coord.isInNeighbourhood(border[nr-1], border[nr+1]);
  } else {
    return coord.isInNeighbourhood(border[1], border[border.size()-2]);
  }
}


std::string
Enclosure::show() const
{
  std::vector<pti> tab(coord.getSize(), 0);
  for (auto &e : border) tab[e] = 1;
  for (auto &e : interior) tab[e] |= 2;
  std::stringstream out;
  out << "Border = ";
  for (auto e : border) out << coord.showPt(e) << " ";
  out << std::endl;
  return coord.showBoard(tab) + out.str();
}

std::string
Enclosure::toSgfString() const
{
  std::string es;
  assert(!border.empty());
  for (auto &p : border)
    es += coord.indToSgf(p);
  if (border[0] < border[1])
    return "." + es;
  else
    return "." + es.substr(2) + es.substr(2,2);  // could be always "."+es, but we want to save in a zagram.org's style
}


SgfProperty
Move::toSgfString() const
{
  std::string prop_name(who == 1 ? "B":"W");
  std::stringstream out;
  out << coord.indToSgf(ind);
  if (!enclosures.empty()) {
    for (auto &e : enclosures) {
      out << e->toSgfString();
    }
  }
  return {prop_name, {out.str()}};
}

std::string
Move::show() const
{
  return (who != -1) ? coord.showPt(ind) + " +" + std::to_string(enclosures.size()) + " encl(s)" /*, zobr=" + std::to_string(zobrist_key)*/ : "(none)";
}
