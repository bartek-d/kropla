/********************************************************************************************************
 kropla -- a program to play Kropki; file mcaffe.cc -- file for handling caffe NN.
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

#include "mcaffe.h"

#include "glog/logging.h"

#include <cassert>
#include <iostream>
#include <filesystem>

using namespace caffe;


int
MCaffe::shape_size(const vector<int>& shape) const
{
  int size = 1;
  for (unsigned int i = 0; i < shape.size(); i++)
    size *= shape[i];
  return size;
}
	
/* Make caffe quiet */
void
MCaffe::quiet_caffe(char *name) const
{
  google::InitGoogleLogging(name);
}
	
void
MCaffe::caffe_load(const std::string& model_file, const std::string& weights_file, int default_size)
{
  if (not std::filesystem::exists(model_file) or not std::filesystem::exists(weights_file)) {
    throw CaffeException("File " + model_file + " or " + weights_file + " does not exist");
  }
  Caffe::set_mode(Caffe::CPU);       
  /* Load the network. */
  net = std::make_shared<Net<float>>(model_file, TEST);
  net->CopyTrainedLayersFrom(weights_file);
  net_size = default_size;
}

void
MCaffe::caffe_init(int size, const std::string& model_file, const std::string& weights_file, int default_size)
{
  if (net && net_size == size)  return;   /* Nothing to do. */
  if (!net) {
    caffe_load(model_file, weights_file, default_size);
  }
	
  /* If network is fully convolutional it can handle any boardsize,
   * just need to resize the input layer. */
  if (net_size != size) {
    const vector<int>& shape = net->input_blobs()[0]->shape();
    net->input_blobs()[0]->Reshape(shape[0], shape[1], size, size);
    net->Reshape();   /* Forward the dimension change. */
    net_size = size;
  }
}

	
std::vector<float>
MCaffe::caffe_get_data(float *data, int size, int planes, int psize)
{
  assert(net && net_size == size);
  //	Blob<float> *blob = new Blob<float>(1, planes, psize, psize);
  auto blob = std::make_unique<Blob<float>>(1, planes, psize, psize);
  blob->set_cpu_data(data);
  std::vector<Blob<float>*> bottom;
  bottom.push_back(blob.get());
  const std::vector<Blob<float>*>& rr = net->Forward(bottom);
  
  assert(shape_size(rr[0]->shape()) >= size * size);
  
  std::vector<float> result;
  result.resize(size * size);
  
  for (int i = 0; i < size * size; i++) {
    result[i] = rr[0]->cpu_data()[i];
    if (result[i] < 0.00001)
      result[i] = 0.00001;
  }
  return result;
}

