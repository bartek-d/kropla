/********************************************************************************************************
 kropla -- a program to play Kropki; file board.h.
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

#if !defined(__BOARD_H)
#define __BOARD_H

#include <cstdint>    // intXX_t types
#include <array>
#include <sstream>
#include <iomanip>
#include <vector>

typedef int16_t  pti;  // point int, i.e., int suitable for coordinates
typedef float real_t;

/********************************************************************************************************
  Colour constants.
*********************************************************************************************************/
const std::string colour_off = "\033[0m";
const std::string black = "\033[30m";
const std::string red = "\033[31m";
const std::string green = "\033[32m";
const std::string yellow = "\033[33m";
const std::string blue = "\033[34m";
const std::string magenta = "\033[35m";
const std::string cyan = "\033[36m";
const std::string white = "\033[37m";
// background colours:
const std::string black_b = "\033[40m";
const std::string red_b = "\033[41m";
const std::string green_b = "\033[42m";
const std::string yellow_b = "\033[43m";
const std::string blue_b = "\033[44m";
const std::string magenta_b = "\033[45m";
const std::string cyan_b = "\033[46m";
const std::string white_b = "\033[47m";

/********************************************************************************************************
  Svg class for showing pretty diagrams
*********************************************************************************************************/
class Svg {
  int margin, grid, dot_size, sizex, sizey;   // parameters set by the constructor
  int xx, yy;
  std::string svg_prefix, svg_bkgrd, svg_grid, svg_dots, svg_suffix;
public:
  Svg(int x, int y);
  void drawDot(int i, int j, int who);
  void drawBkgrd(int i, int j, float r, float g, float b);
  void show();
  std::string to_str();
};


/********************************************************************************************************
  Coord class for manipulating coordinates.
*********************************************************************************************************/

class Coord {
public:
  int wlkx=15;
  int wlky=15;
  const static int maxx = 40;
  const static int maxy = 40;
  const static int maxSize = (maxx+2)*(maxy+1) +1;

  uint64_t zobrist_dots[2][maxSize];
  uint64_t zobrist_encl[2][maxSize];
  pti N;
  pti S;
  pti W;
  pti E;
  pti NE, NW, SE, SW;
  pti NN, WW, SS, EE;
  pti NNE, NNW, NWW, SWW, SSE, SSW, SEE, NEE;
  pti nb4[4];  // neighbours (N, E, S, W), order is important
  pti nb8[32];  // 8 neighbours repeated 4 times, order is important for some functions (NE, E, ... -- clockwise), repeat is to avoid nb8[(i+...)&7]
  pti nb25[25];  // the central point and its 24 neighbours
  pti first, last;  // smallest and largest coordinate possible for a point inside the board

  std::array<int8_t, maxSize> x;
  std::array<int8_t, maxSize> y;
  std::array<int8_t, maxSize> dist;
#include "connections_tab02.cc"  // std::array<pti, 256*256> connections_tab = {...}; see szabl_neighb02.cc
#include "connections_tab03_simple.cc"  // std::array<std::array<pti,4>, 256> connections_tab_simple = {...}, see szabl_neighb03.cc
  std::vector<pti> edge_points;
  std::vector<pti> edge_neighb_points;
  
  Coord(int x, int y);
  pti ind(int x, int y) const { return (x+1)*(wlky+1) + y+1; };
  pti findNextOnRight(pti x0, pti y) const;
  int findDirectionNo(pti x0, pti y) const;
  int find_nb25ind(pti delta) const;
  void initPtTabs();
  int getSize() const { return (wlkx+2)*(wlky+1) +1; };
  void changeSize(int x, int y);
  int distBetweenPts_infty(pti p1, pti p2) const;
  bool isInNeighbourhood(pti p1, pti p2) const;
  int distBetweenPts_1(pti p1, pti p2) const;
  template <typename Container> std::string showBoard(Container const &b);
  template <typename Container> std::string showColouredBoard(Container const &b);
  template <typename Container> std::string showColouredBoardWithDots(Container const &b);
  template <typename Container> std::string showFullBoard(Container const &b);
  std::string showPt(pti p) const;
  int sgfToX(std::string s) const;
  int sgfToY(std::string s) const;
  pti sgfToPti(std::string s) const;
  int sgfCoordToInt(char s) const;
  std::string indToSgf(pti p) const;
  std::string numberToLetter(pti p) const;
  bool isOnBoardSgf(std::string pt) const;
  std::string dindToStr(pti p) const;
};

extern class Coord coord;


/********************************************************************************************************
  Define template functions here to avoid linking problems
*********************************************************************************************************/

template <typename Container>
std::string
Coord::showBoard(Container const &b)
{
  std::stringstream out;
  for (int y=-2; y<wlky; y++) {
    for (int x=-1; x<wlkx; x++) {
      if (y >= 0) {
	if (x >= 0) {
	  if (b[ind(x,y)]==0) out << colour_off;
	  else if (b[ind(x,y)] & 1) out << blue;
	  else out << red;
	  out << std::setw(4) << int(b[ind(x,y)]) << colour_off;
	} else {
	  out << std::setw(3) << int(y) << ":" << colour_off;
	}
      } else {
	if (x >= 0) {
	  if (y == -2) {
	    out << std::setw(4) << int(x) << colour_off;
	  } else {
	    out << std::setw(4) << "----" << colour_off;
	  }
	} else
	  out << std::setw(4) << "" << colour_off;
      }
    }
    out << std::endl;
  }
  return out.str();
}

/// shows numbers, for debug purposes
template <typename Container>
std::string
Coord::showColouredBoard(Container const &b)
{
  std::stringstream out;
  for (int y=-2; y<wlky; y++) {
    for (int x=-1; x<wlkx; x++) {
      if (y >= 0) {
	if (x >= 0) {
	  switch (b[ind(x,y)]) {
	  case 0: out << colour_off; break;
	  case 1: out << blue; break;
	  case 2: out << red; break;
	  case 3: out << magenta; break;
	  case 4: out << green; break;
	  default: out << cyan; break;
	  };
	  out << std::setw(4) << int(b[ind(x,y)]) << colour_off;
	} else {
	  out << std::setw(3) << int(y) << ":" << colour_off;
	}
      } else {
	if (x >= 0) {
	  if (y == -2) {
	    out << std::setw(4) << int(x) << colour_off;
	  } else {
	    out << std::setw(4) << "----" << colour_off;
	  }
	} else
	  out << std::setw(4) << "" << colour_off;
      }
    }
    out << std::endl;
  }
  return out.str();
}


/// shows dots, for ascii-mode play
template <typename Container>
std::string
Coord::showColouredBoardWithDots(Container const &b)
{
  std::stringstream out;
  std::string ascii_dots[3] = {"\u2027 ", "\u25cf ", "\u25cf "};  // \u2022 = 'BULLET'  \u25cf = Black Circle, \u2027 = Hyphenation Point
  for (int y=-2; y<wlky; y++) {
    for (int x=-1; x<wlkx; x++) {
      if (y >= 0) {
	if (x >= 0) {
	  int what = (b[ind(x,y)] == 0) ? 0 : (b[ind(x,y)] % 4);
	  switch (what) {
	  case 0: out << colour_off; break;
	  case 1: out << blue; break;
	  case 2: out << red; break;
	  case 3: out << green; break;
	  };
	  out << ascii_dots[what] << colour_off;
	} else {
	  out << std::setw(2) << int(y) << ":" << colour_off;
	}
      } else {
	if (x >= 0) {
	  if (y == -2) {
	    out << std::setw(2) << numberToLetter(x) << colour_off;
	  } else {
	    out << std::setw(2) << "--" << colour_off;
	  }
	} else
	  out << std::setw(2) << "" << colour_off;
      }
    }
    out << std::endl;
  }
  return out.str();
}


template <typename Container>
std::string
Coord::showFullBoard(Container const &b)
{
  std::stringstream out;
  for (int y=-1; y<=wlky; y++) {
    for (int x=-1; x<=wlkx; x++) {
      if (y<wlky || x==wlkx) {
	out << std::setw(4) << int(b[ind(x,y)]);
      } else {
	out << std::setw(4) << "";
      }
    }
    out << std::endl;
  }
  return out.str();
}


/********************************************************************************************************
  Score class
*********************************************************************************************************/
struct Score {
  int dots;   // enclosed dots
  //  int terr;   // territory
  //int dots_in_terr;  // dots not enclosed yet, but captured inside territory
  Score() { dots=0; /*terr=dots_in_terr=0;*/ } ;
  std::string show() const;
};


#endif
