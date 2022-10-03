/********************************************************************************************************
 kropla -- a program to play Kropki; file mcaffe.h
    Copyright (C) 2021 Bartek Dyda,
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

#define CPU_ONLY 1
#include "caffe/caffe.hpp"

#include <vector>
#include <memory>

struct CaffeException : public std::runtime_error {
    CaffeException(const std::string& error_message) : std::runtime_error(error_message) {};
};

class MCaffe {
public:
  MCaffe();
  void quiet_caffe(const char *name) const;
  bool caffe_ready() const { return (net != nullptr); }
  void caffe_load(const std::string& model_file, const std::string& weights_file, int default_size);
  void caffe_init(int size, const std::string& model_file, const std::string& weights_file, int default_size);
  std::vector<float> caffe_get_data(float *data, int size, int planes, int psize);
private:
  int  shape_size(const std::vector<int>& shape) const;
  std::shared_ptr<caffe::Net<float>> net = nullptr;
  int net_size = 0;

};

