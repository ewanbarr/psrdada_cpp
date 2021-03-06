#ifndef PSRDADA_CPP_DADA_JUNK_SOURCE_HPP
#define PSRDADA_CPP_DADA_JUNK_SOURCE_HPP

#include "psrdada_cpp/raw_bytes.hpp"
#include "psrdada_cpp/common.hpp"
#include <vector>

namespace psrdada_cpp
{
    template <class Handler>
    void junk_source(Handler& handler,
        std::size_t header_size,
        std::size_t nbytes_per_write,
        std::size_t total_bytes)
    {
        std::vector<char> junk_header(header_size);
        std::vector<char> junk_block(nbytes_per_write);
        RawBytes header(junk_header.data(),header_size,header_size,false);
        handler.init(header);
        std::size_t bytes_written = 0;
        while (bytes_written < total_bytes)
        {
            RawBytes data(junk_block.data(),nbytes_per_write,nbytes_per_write,false);
            handler(data);
            bytes_written += data.used_bytes();
        }
    }
} //namespace psrdada_cpp
#endif //PSRDADA_CPP_DADA_JUNK_SOURCE_HPP