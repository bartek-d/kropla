/********************************************************************************************************
 kropla -- a program to play Kropki; file cnn_workers.h -- reading NN in
multiple processes. Copyright (C) 2022 Bartek Dyda, email: bartekdyda (at)
protonmail (dot) com

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

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace workers
{
int setupWorkers(std::size_t memory_needed, uint32_t wlkx);
std::pair<bool, std::vector<float>> getCnnInfo(std::vector<float>& input,
                                               uint32_t wlkx);

}  // namespace workers
