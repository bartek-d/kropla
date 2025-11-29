/********************************************************************************************************
 kropla -- a program to play Kropki; file mcnn.h
    Copyright (C) 2021, 2023 Bartek Dyda,
    email: bartekdyda (at) protonmail (dot) com

    This file is a rewritten file from Pachi http://pachi.or.cz/
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

#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct CnnException : public std::runtime_error
{
    CnnException(const std::string& error_message)
        : std::runtime_error(error_message){};
};

class CnnProxy
{
   public:
    virtual ~CnnProxy() = default;
    virtual bool is_ready() const = 0;
    virtual void load(const std::string& model_file,
                      const std::string& weights_file, int default_size) = 0;
    virtual void init(int size, const std::string& model_file,
                      const std::string& weights_file, int default_size) = 0;
    virtual std::vector<float> get_data(float* data, int size, int planes,
                                        int psize) = 0;
};

std::unique_ptr<CnnProxy> buildTorch();
