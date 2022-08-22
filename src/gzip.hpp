#pragma once

#include <string>
#include <vector>
#include <cstdint>

std::vector<char> unzip(const std::vector<char>& compressed);
std::vector<char> zip(const std::vector<char>& uncompressed);
