
#ifndef PSRDADA_CPP_SIGPROCHEADER_HPP
#define PSRDADA_CPP_SIGPROCHEADER_HPP

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <stdlib.h>
#include"psrdada_cpp/psrdadaheader.hpp"
#include"psrdada_cpp/raw_bytes.hpp"

/* @detail: A SigProc Header writer class. This class will parse values
 *          from a PSRDADA header object and write that out as a standard
 *          SigProc format. This is specific for PSRDADA stream.
 */

namespace psrdada_cpp
{

    struct FilHead {
        std::string rawfile;
        std::string source;

        double az;                      // azimuth angle in deg
        double dec;                     // source declination
        double fch1;                    // frequency of the top channel in MHz
        double foff;                    // channel bandwidth in MHz
        double ra;                      // source right ascension
        double rdm;                     // reference DM
        double tsamp;                   // sampling time in seconds
        double tstart;                  // observation start time in MJD format
        double za;                      // zenith angle in deg

        int datatype;                  // data type ID
        int ibeam;                      // beam number
        int machineid;
        int nbeams;
        int nbits;
        int nchans;
        int nifs;
        int telescopeid;
    };

class SigprocHeader
{
public:
    SigprocHeader();
    ~SigprocHeader();
    void write_header(RawBytes& block,PsrDadaHeader ph);
    void read_header(std::ifstream &infile, FilHead &header);

    std::size_t header_size() const;

private:
    std::size_t _header_size;
    /*
     * @brief write string to the header
     */
    void header_write(char*& ptr, std::string const& str);
    void header_write(char*& ptr, std::string const& str, std::string const& name);

    /*
     * @brief write a value to the stream
     */
    template<typename NumericT>
    void header_write(char*& ptr, std::string const& name, NumericT val);

};

} // namespace psrdada_cpp
#include "psrdada_cpp/detail/sigprocheader.cpp"
#endif //PSRDADA_CPP_SIGPROCHEADER_HPP
