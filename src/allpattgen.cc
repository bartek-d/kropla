/********************************************************************************************************
 kropla -- a program to play Kropki; file allpattgen.cc.
    Copyright (C) 2021 Bartek Dyda,
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

#include "allpattgen.h"

#include <string>
#include <array>
#include <algorithm>
#include <set>

#include <iostream>

std::string getPattStr(const pattern3_t p)
{
  const auto code = Pattern3::show(p);
  const std::set<char> symbols{ '-',                   // centre point
				'.', 'Y', 'Q', '#',    // no atari values
				'E', '|', '@', 'F' };  // atari values; E,F denote invalid values
  std::string res;
  for (auto c : code) {
    if (symbols.find(c) != symbols.end())
      res += std::string{c};
  }
  const std::string out = res.substr(1, 2) + res.substr(5, 1) + res.substr(8,1) + res.substr(7,1) + res.substr(6,1) + res.substr(3,1) + res.substr(0, 1);
  //  std::cout << code << " ---> " << out << std::endl;
  return out;
}

bool checkBottomEdge(const std::string& ps)
{
  if (ps.at(4) == '#' and (ps.at(3) != '#' or ps.at(5) != '#'))
    return false;
  if (ps.at(4) != '#' and (ps.at(3) == '#' and ps.at(5) == '#'))
    return false;
  if (ps.at(4) == '#' and ps.at(0) == '#')
    return false;
  if (ps.at(4) == '#' and (ps.at(2) == '|' or ps.at(2) == '@' or ps.at(6) == '|' or ps.at(6) == '@'))
    return false;
  if (ps.at(3) == '#'
      and (ps.at(2) != '#' or ps.at(1) != '#')
      and (ps.at(4) != '#' or ps.at(5) != '#'))
    return false;
  return true;
}


bool checkCommonFate(const std::string& ps)
{
  if (ps.at(4) != 'Y' and ps.at(4) != '|')
    return true;
  for (int i=5; i<=10; ++i) {
    if (ps.at(i % 8) != 'Y' and ps.at(i % 8) != '|' and ps.at(i % 8) != '.')
      break;
    if (ps.at(i % 8) == '.')
      continue;
    if (i % 2 == 0 and ps.at(i % 8) != ps.at(4))
      return false;
  }
  return true;
}

std::set<pattern3_t>
generateAllPossibleSmallest()
{
  std::set<pattern3_t> all{};
  std::array<pattern3_t, 8> rotations;
  std::array<std::string, 8> rot_str;
  for (unsigned p = 0; p < PATTERN3_SIZE; ++p) {
    auto ps = getPattStr(p);
    if (ps.find('E') != std::string::npos or ps.find('F') != std::string::npos)
      continue;
    rotations[0] = p;
    rotations[1] = Pattern3::reflect(p);
    for (int i=2; i<8; i+=2) {
      rotations[i] = Pattern3::rotate(rotations[i-2]);
      rotations[i+1] = Pattern3::rotate(rotations[i-1]);
    }
    if (std::any_of(rotations.begin()+1, rotations.end(), [p](auto q) { return q<p; }))
      continue;
    for (int i=0; i<8; ++i) {
      rot_str[i] = getPattStr(rotations[i]);
    }
    if (not std::all_of(rot_str.begin(), rot_str.end(), checkBottomEdge))
      continue;
    if (not std::all_of(rot_str.begin(), rot_str.end(), checkCommonFate))
      continue;
    for (int i=0; i<8; ++i) {
      rotations[i] = Pattern3::reverseColour(rotations[i]);
      rot_str[i] = getPattStr(rotations[i]);
    }
    if (not std::all_of(rot_str.begin(), rot_str.end(), checkCommonFate))
      continue;
    all.insert(p);
  }
  return all;
}
