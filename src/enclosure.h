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

#pragma once

#include "board.h"
#include "sgf.h"
#include <vector>
#include <string>
#include <memory>

/********************************************************************************************************
  Enclosure class
*********************************************************************************************************
  There are two lists: interior, border with the list of interior, border points of the enclosure.
  (getInteriorSize() == 0) indicates no enclosure (empty enclosure).
*********************************************************************************************************/
struct Enclosure {
  std::vector<pti> interior;
  std::vector<pti> border;
  Enclosure(std::vector<pti> const &i, std::vector<pti> const &b) : interior(i), border(b) {};
  Enclosure(std::vector<pti> &&i, std::vector<pti> &&b) : interior(std::move(i)), border(std::move(b)) {};
  /* set version seems to be slower:
  std::set<pti,std::greater<pti>> interior;
  std::set<pti,std::greater<pti>> border;

  Enclosure(std::vector<pti> i, std::vector<pti> b) : interior(i.begin(), i.end()), border(b.begin(), b.end()) {};
  */
  Enclosure() = default;
  ~Enclosure() = default;
  bool isInInterior(pti p) const;
  bool isInBorder(pti p) const;
  bool isEmpty() const;
  pti getBorderElement() const;
  pti getInteriorElement() const;
  int getInteriorSize() const;
  uint64_t zobristKey(int who) const;
  bool checkShortcut(pti p1, pti x) const;
  bool checkIfRedundant(pti p1) const;
  std::string show() const;
  std::string toSgfString() const;
};

extern Enclosure empty_enclosure;

/********************************************************************************************************
  Move class
*********************************************************************************************************/
struct Move {
  std::vector<std::shared_ptr<Enclosure> > enclosures;
  uint64_t zobrist_key{0};
  pti ind;
  pti who {-1};
  bool operator==(const Move& other) const { return zobrist_key == other.zobrist_key; };
  SgfProperty toSgfString() const;
  std::string show() const;
};

