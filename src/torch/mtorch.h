/********************************************************************************************************
 kropla -- a program to play Kropki; file mtorch.h
    Copyright (C) 2023 Bartek Dyda,
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

#include "../mcnn.h"

#include <torch/torch.h>
#include <memory>

struct Net;

class MTorch : public CnnProxy
{
   public:
    MTorch();
    bool is_ready() const override;
    void load(const std::string& model_file, const std::string& weights_file,
              int default_size) override;
    void init(int size, const std::string& model_file,
              const std::string& weights_file, int default_size) override;
    std::vector<float> get_data(float* data, int size, int planes,
                                int psize) override;

   private:
    std::shared_ptr<Net> net = nullptr;
};
