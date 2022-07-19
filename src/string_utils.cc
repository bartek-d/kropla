/********************************************************************************************************
 kropla -- a program to play Kropki; file string_utils.cc
    Copyright (C) 2022 Bartek Dyda,
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

#include "string_utils.h"

#include <charconv>
#include <string_view>
#include <algorithm>

int robust_stoi(const std::string& s)
{
  // find a sequence of digits
  const auto begin = std::find_if(s.begin(), s.end(), [](auto c) { return std::isdigit(c); });
  if (begin == s.end())
    return -1;
  const auto end = std::find_if(begin, s.end(), [](auto c) { return not std::isdigit(c); });
  // convert
  int result;
  auto [ptr, ec] = std::from_chars(&*begin, &*end, result);
  if (ec == std::errc())
    return result;
  return -1;
}
