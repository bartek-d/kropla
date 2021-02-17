/********************************************************************************************************
 kropla -- a program to play Kropki; file patterns.cc.
    Copyright (C) 2015,2016,2017,2018,2019 Bartek Dyda,
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

#include <string>
#include <array>
#include <vector>
#include <map>
#include <iostream>
#include <cassert>
#include <algorithm>

#include "board.h"

/********************************************************************************************************
  Pattern3 class for handling 3x3 patterns
*********************************************************************************************************/
typedef uint32_t pattern3_t;
typedef int16_t pattern3_val;
/* Each pattern is encoded in 20 bits.
   There is 2 bits for each of the 8 neighbours plus 4 for direct neighbours to save ataris:
    6 7 0   atari:   b
    5 . 1          a   8
    4 3 2            9
   Bits for neighbours: 0=empty, 1,2=dot, 3=outside,
   bits in atari may be set ONLY if there's a dot at a point, and set bit denotes dot in atari.
*/

/* Source pattern encoding:   (taken from Pachi: playout/moggy.c)
 * X: black;  O: white;  .: empty;  #: edge
 * x: !black; o: !white; ?: any;  *: any but edge
 *
 * |/=: black in atari/anything but black in atari
 * @/0: white in atari/
 * Y/y: black notin atari/; Q/q: white notin atari/
 *
 * extra X: pattern valid only for one side (black);
 * middle point ignored. */
/*
 Atoms:
  with atari-bit == 0:
   . Y Q #
  with atari-bit == 1:
    | @
*/

const pattern3_t PATTERN3_SIZE = 0x100000;
const pattern3_t PATTERN3_IMPOSSIBLE = PATTERN3_SIZE + 2;

class Pattern3 {
  std::array<pattern3_val, PATTERN3_SIZE> values;
  int add_type;
  void addPatterns(pattern3_t p, pattern3_val value);
  void generateFromStr(std::string sarg, pattern3_val value, int depth);
  friend struct Pattern3full;
  friend class Pattern3extra_array;
public:
  pattern3_val getValue(pattern3_t p, int who) const;
  void generate(std::vector<std::string> vs, int type = TYPE_REPLACE);
  void readFromFile(const std::string &filename);
  static pattern3_t rotate(pattern3_t p);
  static pattern3_t reflect(pattern3_t p);
  static pattern3_t reverseColour(pattern3_t p);
  static std::string show(pattern3_t p);
  void showCode() const;
  //  static const constexpr T RECALCULATE = 10000.0;  // this constant should be greater than any possible pattern value
  static bool isPatternPossible(pattern3_t p) { return p<PATTERN3_SIZE; };
  //  static const constexpr T RECALCULATE_TO_0 = 10001.0;  // this constant should be greater than RECALCULATE
  static const int TYPE_REPLACE = 0;
  static const int TYPE_MAX = 1;
  static pattern3_t getCodeOfPattern(std::string s);
};

/********************************************************************************************************
  Pattern3extra class for handling additional conditions in 5x5 square for 3x3 patterns
*********************************************************************************************************/
struct Pattern3extra {
  int8_t scored_point, score;
  std::array<uint8_t, 6> conditions;
  static const int8_t NO_SCORED_POINT = -1;
  static const uint8_t EQUAL_MASK = 0x80;
  static const uint8_t IS_SET_MASK = 0x40;
  static const uint8_t WHICH_POINT_MASK = 0x3c;
  static const uint8_t WHICH_POINT_SHIFT = 2;
  static const uint8_t VALUE_MASK = 3;
  static const pti MASK_DOT = 3;  // should equal Game::MASK_DOT !
  bool checkConditions(const std::vector<pti> &w, pti ind) const;
  void rotate();
  void reflect();
  void reverseColour();
  void parseConditions(std::string s, pattern3_val value);
  Pattern3extra() { scored_point = NO_SCORED_POINT; };
  bool isScored() const { return scored_point != NO_SCORED_POINT; };
  bool operator==(const Pattern3extra& other) const;
  std::string show() const;
};


/********************************************************************************************************
  Pattern3full class for 3x3 patterns with additional conditions in 5x5 square
*********************************************************************************************************/
struct Pattern3full {
  pattern3_t p3;
  Pattern3extra extra;
  void rotate();
  void reflect();
  void reverseColour();
  Pattern3full() : extra() {};
};

/********************************************************************************************************
  Pattern3extra_array -- an array with 3x3 patterns with additional conditions in 5x5 square
*********************************************************************************************************/
class Pattern3extra_array {
  static const int PATT_COUNT = 32;
  std::array<std::array<Pattern3extra, PATT_COUNT>, PATTERN3_SIZE> values[2];
  int max_occupied;
  void addPatterns(Pattern3full p);
  void generateFromStr(std::string sarg, pattern3_val value, int depth);
  void show(pattern3_t p) const;
public:
  //pattern3_val getValue(pattern3_t p, int who) const;
  void generate(std::vector<std::string> vs);
  void setValues(std::vector<pti> &val, const std::vector<pti> &w, pattern3_t patt3_at, pti ind, int who) const;
  Pattern3extra_array();
};


/********************************************************************************************************
  Pattern52 class for handling 5xx patterns
*********************************************************************************************************/
typedef uint32_t pattern52_t;
/* Each pattern is encoded in 18 bits.
   There is 2 bits for each of the 9 neighbours.
     0 2 * 7 5
     1 3 4 8 6
     #########
   or
     0 2 4 7 5
     1 3 * 8 6
     #########
    point * is the one we are about to play.
   Bits for neighbours: 0=empty, 1,2=dot, 3=outside.
*/

/* Source pattern encoding:   (taken from Pachi: playout/moggy.c)
 * X: black;  O: white;  .: empty;  #: edge
 * x: !black; o: !white; ?: any;  *: any but edge
 * H: where to play (denoted by * above)
 *
 * |/=: black in atari/anything but black in atari
 * @/0: white in atari/
 * Y/y: black notin atari/; Q/q: white notin atari/
 * (We do not use atari in Pattern52, but we still use the same notation).
 *
 * extra X: pattern valid only for one side (black);
 */
/*
 Atoms:
  with atari-bit == 0:
   . Y Q #
  with atari-bit == 1: (unused currently in Pattern52!)
    | @
*/

class Pattern52 {
  std::array<real_t, 0x40000> values;
  pattern52_t reflect(pattern52_t p) const;
  pattern52_t reverseColour(pattern52_t p) const;
  void addPatterns(pattern52_t p, real_t value);
  void generateFromStr(std::string sarg, real_t value, int depth);
public:
  real_t getValue(pattern52_t p, int who) const;
  void generate(std::vector<std::string> vs);
  std::string show(pattern52_t p) const;
  void showCode() const;
  Pattern52(std::array<real_t, 0x40000> v) { values = v; };
};
