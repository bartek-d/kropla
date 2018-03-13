/********************************************************************************************************
 kropla -- a program to play Kropki; file command.cc
    Copyright (C) 2018 Bartek Dyda,
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


#include <vector>
#include <string>
#include <cassert>

#include <cctype>   // std::isdigit()
#include "board.h"  // numberToLetter()
#include "command.h"


int
CommandParser::eatWS()
{
  int eaten = 0;
  while (pos < buf.length()) {
    if (std::isspace(buf[pos])) {
      ++pos;
      ++eaten;
    } else {
      break;
    }
  }
  return eaten;
}

// Returns number of obligatory uint parameteres, or -1 if no command found.
int
CommandParser::parseCommandName()
{
  auto saved = pos;
  eatWS();
  std::pair<std::string, int> commands[] = { {"threads", 1}, {"iters", 1}, {"first", 1}, {"second", 1},
					     {"new", 0}, {"move", 0}, {"quit", 0}, {"bye", 0},
					     {"back", 0}, {"show", 0}, {"help", 0}, {"?", 0}};
  for (auto c : commands) {
    if (buf.find(c.first, pos) == 0) {
      pos += c.first.length();
      parsed.push_back(c.first);
      return c.second;
    }
  }
  pos = saved;
  return -1;
}

// Parses uint, throws an exception if no uint to parse.
void
CommandParser::parseUint()
{
  auto saved = pos;
  while (pos < buf.length() && std::isdigit(buf[pos])) ++pos;
  if (pos > saved) {
    parsed.push_back(buf.substr(saved, pos - saved));
  } else {
    throw CPException(spaces(pos) + "^\n" "uint: digit expected");
  }
}

// Parses coord, throws an exception if no coord to parse or the coord is invalid.
void
CommandParser::parseCoord()
{
  std::string point;
  for (int c=0; c<2; ++c) {
    if (isEOL()) {
      throw CPException(spaces(pos) + "^\n" "coord: unexpected EOL");
    }
    if (std::isdigit(buf[pos])) {
      // '<uint>' format of the coordinate
      parseUint();
      int x_or_y = std::stoi(parsed.back());
      parsed.pop_back();
      try {
	point += coord.numberToLetter(x_or_y);
      } catch (const std::out_of_range& e) {
	throw CPException(spaces(pos) + "^\n" "coord: number too large");
      }
    } else {
      // '<char>' format
      point += buf[pos];
      ++pos;
    }
    if (c==0) eatWS();
  }
  bool is_on_board;
  try {
    is_on_board =  coord.isOnBoardSgf(point);
  } catch (const std::runtime_error& e) {
    throw CPException(spaces(pos) + "^\n" "coord: unexpected character / number too large");
  }
  if (!is_on_board)
    throw CPException(spaces(pos) + "^\n" "coord: point outside the board");
  parsed.push_back(point);
}

std::string
CommandParser::spaces(int n) const
{
  std::string s;
  for (;n > 0; --n) s += " ";
  return s;
}

std::vector<std::string>
CommandParser::parse(std::string b)
{
  buf = b;
  parsed.clear();
  pos = 0;
  int uint_parameters = parseCommandName();
  if (uint_parameters >= 0) {
    // command found, parse parameters
    int eaten = eatWS();
    int parsed_uints = 0;
    while (!isEOL() || parsed_uints < uint_parameters) {
      if (eaten == 0) {
	throw CPException(spaces(pos) + "^\n" "command_uints: space or digit expected");
      }
      parseUint();
      ++parsed_uints;
      eaten = eatWS();
    }
  } else {
    // command not found, parse coordinates list
    parsed.push_back("_play");
    parseCoord();
    int eaten = eatWS();
    while (!isEOL()) {
      if (eaten == 0) {
	throw CPException(spaces(pos) + "^\n" "coordinateList: space or EOL expected");
      }
      parseCoord();
      eaten = eatWS();
    }
  }
  return parsed;
}
