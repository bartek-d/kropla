#include "gzip.hpp"

#include <iostream>
#include <fstream>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/copy.hpp>

std::vector<char> unzip(const std::vector<char>& compressed)
{
  std::vector<char> decompressed;

   boost::iostreams::filtering_ostream os;

   os.push(boost::iostreams::gzip_decompressor());
   os.push(boost::iostreams::back_inserter(decompressed));

   boost::iostreams::write(os, compressed.data(), compressed.size());

   return decompressed;
}

std::vector<char> zip(const std::vector<char>& uncompressed)
{
  std::vector<char> compressed;

  boost::iostreams::filtering_ostream os;

  os.push(boost::iostreams::gzip_compressor(boost::iostreams::gzip_params(9)));
  os.push(boost::iostreams::back_inserter(compressed));
  boost::iostreams::write(os, uncompressed.data(), uncompressed.size());
  return compressed;
}

