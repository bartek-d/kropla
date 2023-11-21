/********************************************************************************************************
 kropla -- a program to play Kropki; file mtorch.cc
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

#include "mtorch.h"

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cmath>
#include <optional>
#include <charconv>

enum class LayerType {
  Conv, Batch, Relu
};

struct LayerInfo
{
  LayerType layer_type;
  int kernels;  // tylko dla conv, TODO: zmienic na variant
  int size;     // tylko dla conv
};

std::ostream& operator<<(std::ostream& os, const LayerInfo& l)
{
  switch (l.layer_type) {
  case LayerType::Conv:
    os << "conv" << l.kernels << ":" << l.size;
    break;
  case LayerType::Batch:
    os << "B";
    break;
  case LayerType::Relu:
    os << "r";
    break;
  }
  return os;
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v)
{
  for (const auto& el : v)
    os << el << ' ';
  return os;
}

std::optional<LayerInfo> string2layer_info(const std::string& s)
{
  if (s == "B") return LayerInfo{LayerType::Batch};
  if (s == "r") return LayerInfo{LayerType::Relu};
  const char* prefix = "conv";
  if (not s.starts_with(prefix) or s.size() < 7) return {};
  const char* beg = s.data() + std::strlen(prefix);
  int kernels{};
  auto [ptr, ec] = std::from_chars(beg, s.data() + s.size(), kernels);
  if (ec != std::errc()) return {};
  if (*ptr != ':')  return {};
  int size{};
  auto [ptr2, ec2] = std::from_chars(ptr+1, s.data() + s.size(), size);
  if (ec2 != std::errc()) return {};
  return  LayerInfo{LayerType::Conv, kernels, size};
}


struct Net : torch::nn::Module {
  Net(int in_channels, const std::vector<LayerInfo>& netdef_)
    : netdef{netdef_}
  {
    int inch = in_channels;
    for (std::size_t i = 0; i<netdef.size(); ++i) {
      switch (netdef[i].layer_type) {
	case LayerType::Conv:
	  {
	  const int pad = netdef[i].size / 2;
	  const bool not_before_batchn = (i == netdef.size() - 1 || netdef[i+1].layer_type != LayerType::Batch);
	  conv.push_back(register_module("conv"  + std::to_string(i), torch::nn::Conv2d(torch::nn::Conv2dOptions(inch, netdef[i].kernels, netdef[i].size).stride(1).padding(pad).bias(not_before_batchn))));
	  inch = netdef[i].kernels;
	  }
	  break;
	case LayerType::Batch:
	  batchn.push_back(register_module("batchn" + std::to_string(i), torch::nn::BatchNorm2d(torch::nn::BatchNorm2dOptions(inch).affine(true))));
	  break;
	case LayerType::Relu:
	  break;
      }
    }
  }
  torch::Tensor forward(torch::Tensor x) {
    std::size_t batch_i = 0;
    std::size_t conv_i = 0;
    for (const auto& layer : netdef) {
      switch (layer.layer_type) {
	case LayerType::Conv:
	  x = conv[conv_i++](x);
	  break;
	case LayerType::Batch:
	  x = batchn[batch_i++](x);
	  break;
	case LayerType::Relu:
	  x = torch::relu(x);
      }
    }
    return x.flatten(-2);
  }
  //torch::nn::Linear linear;
  //torch::Tensor another_bias;
  std::vector<torch::nn::BatchNorm2d> batchn;
  std::vector<torch::nn::Conv2d> conv;
  std::vector<LayerInfo> netdef;
};

std::pair<int, std::vector<LayerInfo>> read_cnn_def(const std::string& filename)
{
  std::ifstream is(filename, std::ios::in);
  std::vector<LayerInfo> v;
  int input_chan{};
  is >> input_chan;
  while (is) {
    std::string s;
    is >> s;
    if (auto layer_opt = string2layer_info(s))
      v.push_back(*layer_opt);
    else if (not s.empty()) {
      std::cerr << "Ostrzezenie, ignoruje ciag: " << s << ".\n";
    }
  }
  return {input_chan, v};
}


MTorch::MTorch()
{
  setenv("OMP_NUM_THREADS", "1", true);
  torch::globalContext().setFlushDenormal(true);
}

bool MTorch::is_ready() const
{ return (net != nullptr); }

void MTorch::load(const std::string& model_file, const std::string& weights_file,
		  int default_size)
{
  auto [in_channels, netdef] = read_cnn_def(model_file);
  std::cerr << "netdef: " << netdef << '\n';
  net = std::make_shared<Net>(in_channels, netdef);
  torch::load(net, weights_file);
  net->eval();
}

void MTorch::init(int size, const std::string& model_file,
		  const std::string& weights_file, int default_size)
{
  if (not is_ready())
    load(model_file, weights_file, default_size);
}

std::vector<float> MTorch::get_data(float* data, int size, int planes,
				    int psize)
{
  auto options = torch::TensorOptions().dtype(torch::kFloat32);
  torch::Tensor input = torch::from_blob(data, {1, planes, size, psize}, options);
  torch::Tensor prediction = net->forward(input);
  std::vector<float> result;
  result.resize(size * size);

  float sum = 0.0;
  for (int i=0; i< result.size(); ++i) {
    result[i] =  std::exp(prediction[0][0][i].item<float>());
    sum += result[i];
  }

  for (int i = 0; i < result.size(); i++)
    {
      result[i] /= sum;
    }
  return result;
}

std::unique_ptr<CnnProxy> buildTorch()
{
  return std::make_unique<MTorch>();
}
