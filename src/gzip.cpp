#include "gzip.hpp"

#include "lz77.h"

std::string unzip(const std::string& compressed)
{
  lz77::decompress_t decompress;
  std::string temp;
  decompress.feed(compressed, temp);
  std::string uncompressed = decompress.result();
  return uncompressed;
}

std::string zip(const std::string& uncompressed)
{
  lz77::compress_t compress;
  std::string compressed = compress.feed(uncompressed);
  return compressed;
}
  
