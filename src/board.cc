/********************************************************************************************************
 kropla -- a program to play Kropki; file board.cc.
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

#include "board.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>

/********************************************************************************************************
  Svg class for showing pretty diagrams
*********************************************************************************************************/

Svg::Svg(int x, int y)
    : xx{x},
      yy{y},
      margin{30},
      grid{20},
      sizex{(x - 1) * grid + 2 * margin},
      sizey{(y - 1) * grid + 2 * margin},
      svg_prefix{R"delim(<svg version="1.1"
     baseProfile="full"
     width="WIDTH" height="HEIGHT"
     xmlns="http://www.w3.org/2000/svg">
   <rect width="100%" height="100%" fill="rgb(255,255,255)" />  <!-- " in comment to please the editor -->)delim"},
      svg_bkgrd{},
      svg_dots{},
      svg_suffix{"</svg>"}
{
    svg_prefix.replace(svg_prefix.find("WIDTH"), std::string("WIDTH").length(),
                       std::to_string(sizex));
    svg_prefix.replace(svg_prefix.find("HEIGHT"),
                       std::string("HEIGHT").length(), std::to_string(sizey));
    std::stringstream out;
    for (int i = 0; i < x; i++)
    {
        out << "<line x1=\"" << margin + i * grid << "\" x2=\""
            << margin + i * grid << "\" y1=\"" << margin << "\" y2=\""
            << sizey - margin
            << "\" stroke=\"rgb(238,238,238)\" stroke-width=\"1\"/>\n";
    }
    for (int j = 0; j < y; j++)
    {
        out << "<line x1=\"" << margin << "\" x2=\"" << sizex - margin
            << "\" y1=\"" << margin + j * grid << "\" y2=\""
            << margin + j * grid
            << "\" stroke=\"rgb(238,238,238)\" stroke-width=\"1\"/>\n";
    }
    svg_grid = out.str();
}

void Svg::drawDot(int i, int j, int who)
{
    std::stringstream out;
    std::string colours[] = {"0,0,0", "0,0,255", "255,0,0", "0,200,0"};
    out << "<circle cx=\"" << margin + i * grid << "\" cy=\""
        << margin + j * grid << "\" r=\"" << grid / 5 << "\" fill=\"rgb("
        << colours[who] << ")\" />\n";
    svg_dots += out.str();
}

void Svg::drawBkgrd(int i, int j, float r, float g, float b)
{
    std::stringstream out;
    out << "<rect x=\"" << margin + i * grid - grid / 3 << "\" y=\""
        << margin + j * grid - grid / 3 << "\" width=\"" << 2 * (grid / 3)
        << "\" height=\"" << 2 * (grid / 3) << "\" stroke=\"rgb("
        << int(r * 255) << "," << int(g * 255) << "," << int(b * 255)
        << ")\" stroke-width=\"5\" fill=\"transparent\" />\n";
    svg_bkgrd += out.str();
}

void Svg::show() {}

std::string Svg::to_str()
{
    return svg_prefix + svg_bkgrd + svg_grid + svg_dots + svg_suffix;
}

/********************************************************************************************************
  Coord class for manipulating coordinates.
*********************************************************************************************************/

Coord::Coord(int x, int y)
{
    changeSize(x, y);
    std::default_random_engine dre;
    std::uniform_int_distribution<uint64_t> di(
        std::numeric_limits<uint64_t>::min(),
        std::numeric_limits<uint64_t>::max());
    for (int i = 0; i < maxSize; i++)
    {
        zobrist_dots[0][i] = di(dre);
        zobrist_dots[1][i] = di(dre);
        zobrist_encl[0][i] = di(dre);
        zobrist_encl[1][i] = di(dre);
    }
}

void Coord::changeSize(int x, int y)
{
    wlkx = x;
    wlky = y;
    // board cannot be larger than 45x45 because of Game::lastWormNo, which
    // allows only for 1023 worms for each player
    assert(x >= 5 && x <= 45 && y >= 5 && y <= 45);
    initPtTabs();
    N = -1;
    S = 1;
    W = -wlky - 1;
    E = wlky + 1;
    NE = N + E;
    NW = N + W;
    SE = S + E;
    SW = S + W;
    NN = N + N;
    WW = W + W;
    SS = S + S;
    EE = E + E;
    NNE = NN + E;
    NNW = NN + W;
    NWW = N + WW;
    SWW = S + WW;
    SSE = SS + E;
    SSW = SS + W;
    SEE = S + EE;
    NEE = N + EE;
    nb4[0] = N;
    nb4[1] = E;
    nb4[2] = S;
    nb4[3] = W;
    nb8[0] = NE;
    nb8[1] = E;
    nb8[2] = SE;
    nb8[3] = S;
    nb8[4] = SW;
    nb8[5] = W;
    nb8[6] = NW;
    nb8[7] = N;
    for (int i = 0; i < 24; i++) nb8[i + 8] = nb8[i];
    nb25[0] = 0;
    for (int i = 0; i < 8; i++)
    {
        nb25[i + 1] = nb8[i];
        nb25[9 + 2 * i] = 2 * nb8[i];
        nb25[9 + 2 * i + 1] =
            nb8[i] + nb8[i + 1];  // here it is important that nb8[8] == nb8[0]
    }
    first = ind(0, 0);
    last = ind(wlkx - 1, wlky - 1);
}

int Coord::distBetweenPts_infty(pti p1, pti p2) const
{
    return std::max(abs(x[p1] - x[p2]), abs(y[p1] - y[p2]));
}
bool Coord::isInNeighbourhood(pti p1, pti p2) const
{
    pti diff = p1 - p2;
    if (diff > 1)
        diff -= E;
    else if (diff < -1)
        diff += E;
    return (diff >= -1 && diff <= 1);
}
int Coord::distBetweenPts_1(pti p1, pti p2) const
{
    return abs(x[p1] - x[p2]) + abs(y[p1] - y[p2]);
}

void Coord::initPtTabs()
{
    int ind = 0;
    edge_points.clear();
    edge_neighb_points.clear();
    edge_points.reserve(2 * (wlkx + wlky - 2));
    edge_neighb_points.reserve(2 * (wlkx + wlky - 6));
    for (int i = 0; i < wlkx + 2; i++)
        for (int j = 0; j < wlky + 1; j++)
        {
            if (i == 0 || i == wlkx + 1 || j == 0)
            {
                x[ind] = -1;
                y[ind] = -1;
                dist[ind] = -1;
            }
            else
            {
                x[ind] = i - 1;
                y[ind] = j - 1;
                dist[ind] = std::min(std::min(i - 1, wlkx - i),
                                     std::min(j - 1, wlky - j));
                if (dist[ind] == 0)
                    edge_points.push_back(ind);
                else if (dist[ind] == 1)
                    edge_neighb_points.push_back(ind);
            }
            ind++;
        }
    x[ind] = -1;
    y[ind] = -1;
    dist[ind] = -1;
    assert(ind == getSize() - 1);
}

pti Coord::findNextOnRight(pti x0, pti y) const
// in: x0 = centre, y = a neighbour of x0
// returns point z next to right to y (as seen from x0)
{
    auto v = y - x0;
    for (int i = (v < 0) ? 4 : 0; i < 8; i++)
    {
        if (nb8[i] == v)
        {
            return x0 + nb8[i + 1];  // (i+1) & 7
        }
    }
    std::cerr << coord.showPt(x0) << "  " << coord.showPt(y) << std::endl;
    assert(0);
    return 0;
}

int Coord::findDirectionNo(pti x0, pti y) const
// in: x0 = centre, y = a neighbour of x0
// returns i such that y == x0 + nb8[i]
{
    auto v = y - x0;
    for (int i = (v < 0) ? 4 : 0; i < 8; i++)
    {
        if (nb8[i] == v)
        {
            return i;
        }
    }
    std::cerr << coord.showPt(x0) << "  " << coord.showPt(y) << std::endl;
    assert(0);
    return 0;
}

int Coord::find_nb25ind(pti delta) const
{
    for (int i = 0; i < 25; ++i)
    {
        if (nb25[i] == delta) return i;
    }
    assert(0);
    return 0;
}

std::string Coord::showPt(pti p) const
{
    std::stringstream out;
    out << "(" << int(coord.x[p]) << ", " << int(coord.y[p]) << ")";
    return out.str();
}

int Coord::sgfToX(const std::string& s) const { return sgfCoordToInt(s.at(0)); }

int Coord::sgfToY(const std::string& s) const { return sgfCoordToInt(s.at(1)); }

pti Coord::sgfToPti(const std::string& s) const
{
    return ind(sgfToX(s), sgfToY(s));
}

int Coord::sgfCoordToInt(char s) const
{
    if (s >= 'a' && s <= 'z')
    {
        return s - 'a';
    }
    if (s >= 'A' && s <= 'Z')
    {
        return s - 'A' + 26;
    }
    throw std::runtime_error("sgfCoordToInt: Unexpected character");
}

std::string Coord::indToSgf(pti p) const
{
    return numberToLetter(coord.x[p]) + numberToLetter(coord.y[p]);
}

/// converts a number-x-coord to letter-x-coord
std::string Coord::numberToLetter(pti p) const
{
    std::string crd("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    return std::string({crd.at(p)});
}

bool Coord::isOnBoardSgf(const std::string& pt) const
{
    return (sgfToX(pt) < coord.wlkx) && (sgfToY(pt) < coord.wlky);
}

std::string Coord::dindToStr(pti p) const
{
    pti centre = ind(wlkx / 2, wlky / 2);
    pti other = centre + p;
    auto dx = x[other] - x[centre];
    auto dy = y[other] - y[centre];
    std::string s;
    if (dy < 0)
    {
        for (int i = 0; i < -dy; ++i) s += 'N';
    }
    else
    {
        for (int i = 0; i < dy; ++i) s += 'S';
    }
    if (dx < 0)
    {
        for (int i = 0; i < -dx; ++i) s += 'W';
    }
    else
    {
        for (int i = 0; i < dx; ++i) s += 'E';
    }
    return s;
}

/********************************************************************************************************
  Score class
*********************************************************************************************************/

std::string Score::show() const
{
    std::stringstream out;
    out << "(d=" << dots
        << /* ", t=" << terr << ", dit=" << dots_in_terr << */ ")";
    return out.str();
}
