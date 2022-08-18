/********************************************************************************************************
 kropla -- a program to play Kropki; file command.h
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

#pragma once

#include <stdexcept>
#include <string>
#include <vector>

class CommandParser
{
    std::vector<std::string> parsed;
    std::string buf;
    std::string::size_type pos;
    int eatWS();
    int parseCommandName();
    void parseUint();
    void parseCoord();
    bool isEOL() const { return pos == buf.length(); };
    std::string spaces(int n) const;

   public:
    std::vector<std::string> parse(const std::string& b);
    class CPException : public std::runtime_error
    {
       public:
        CPException(const std::string& error_message)
            : std::runtime_error(error_message){};
    };
};
