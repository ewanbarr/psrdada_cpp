#include "psrdada_cpp/multilog.hpp"

namespace psrdada_cpp {

    MultiLog::MultiLog(std::string name)
    : _name(name)
    , _log(0)
    , _open(false)
    {
        open();
    }

    MultiLog::~MultiLog()
    {
        close();
    }

    void MultiLog::open()
    {
        _log = multilog_open(_name.c_str(),0);
        _open = true;
    }

    void MultiLog::close()
    {
        multilog_close(_log);
        _open = false;
    }

    multilog_t* MultiLog::native_handle()
    {
        return _log;
    }

    std::string const& MultiLog::name() const
    {
        return _name;
    }


} //namespace psrdada_cpp