#pragma once

#include <string>
#include <cstdint>

std::string unzip(const std::string& compressed);
std::string zip(const std::string& uncompressed);
