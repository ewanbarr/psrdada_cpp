#include "psrdada_cpp/dada_read_client.hpp"

namespace psrdada_cpp {

    DadaReadClient::DadaReadClient(key_t key, MultiLog& log)
    : DadaClientBase(key, log)
    , _locked(false)
    , _header_stream(*this)
    , _data_stream(*this)
    {
        lock();
    }

    DadaReadClient::~DadaReadClient()
    {
        release();
    }

    void DadaReadClient::lock()
    {
        if (!_connected)
        {
            throw std::runtime_error("Lock requested on unconnected HDU\n");
        }
        BOOST_LOG_TRIVIAL(debug) << this->id() << "Acquiring reading lock on dada buffer";
        if (dada_hdu_lock_read (_hdu) < 0)
        {
            _log.write(LOG_ERR, "open_hdu: could not lock read\n");
            throw std::runtime_error(std::string("Error locking HDU with key: ")
                + std::to_string(_key));
        }
        _locked = true;
    }

    void DadaReadClient::release()
    {
        if (!_locked)
        {
            throw std::runtime_error("Release requested on unlocked HDU\n");
        }
        BOOST_LOG_TRIVIAL(debug) << this->id() << "Releasing reading lock on dada buffer";
        if (dada_hdu_unlock_read (_hdu) < 0)
        {
            _log.write(LOG_ERR, "open_hdu: could not release read\n");
            throw std::runtime_error(std::string("Error releasing HDU with key: ")
                + std::to_string(_key));
        }
        _locked = false;
    }

    DadaReadClient::HeaderStream& DadaReadClient::header_stream()
    {
        return _header_stream;
    }

    DadaReadClient::DataStream& DadaReadClient::data_stream()
    {
        return _data_stream;
    }

    DadaReadClient::HeaderStream::HeaderStream(DadaReadClient& parent)
    : _parent(parent)
    , _current_block(nullptr)
    {
    }

    DadaReadClient::HeaderStream::~HeaderStream()
    {
    }

    RawBytes& DadaReadClient::HeaderStream::next()
    {
        if (_current_block)
        {
            throw std::runtime_error("Previous header block not released");
        }
        BOOST_LOG_TRIVIAL(debug) << _parent.id() << "Acquiring next header block";
        std::size_t nbytes = 0;
        char* tmp = ipcbuf_get_next_read(_parent._hdu->header_block, &nbytes);
        if (!tmp)
        {
            _parent._log.write(LOG_ERR, "Could not get header\n");
            throw std::runtime_error("Could not get header");
        }
        _current_block.reset(new RawBytes(tmp, _parent.header_buffer_size(), nbytes));
        BOOST_LOG_TRIVIAL(debug) << _parent.id() << "Header block used/total bytes = "
            << _current_block->used_bytes() <<"/"<<_current_block->total_bytes();
        BOOST_LOG_TRIVIAL(debug) << _parent.id() << "Header content\n" << _current_block->ptr();
        return *_current_block;
    }

    void  DadaReadClient::HeaderStream::release()
    {
        if (!_current_block)
        {
            throw std::runtime_error("No header block to be released");
        }
        BOOST_LOG_TRIVIAL(debug) << _parent.id() << "Releasing header block";
        if (ipcbuf_mark_cleared(_parent._hdu->header_block) < 0)
        {
            _parent._log.write(LOG_ERR, "Could not mark cleared header block\n");
            throw std::runtime_error("Could not mark cleared header block");
        }
        _current_block.reset(nullptr);
    }

    bool DadaReadClient::HeaderStream::at_end() const
    {
        return (bool) ipcbuf_eod(_parent._hdu->header_block);
    }

    void DadaReadClient::HeaderStream::purge()
    {
        std::size_t nheader = ipcbuf_get_nfull((ipcbuf_t *) _parent._hdu->header_block);
        BOOST_LOG_TRIVIAL(debug) << _parent.id() << nheader << " header blocks are full";
        for (std::size_t ii=0; ii < nheader; ++ii)
        {
            next();
            release();
        }
    }

    DadaReadClient::DataStream::DataStream(DadaReadClient& parent)
    : _parent(parent)
    , _current_block(nullptr)
    , _block_idx(0)
    {
    }

    DadaReadClient::DataStream::~DataStream()
    {
    }

    RawBytes& DadaReadClient::DataStream::next()
    {
        if (_current_block)
        {
            throw std::runtime_error("Previous data block not released");
        }
        BOOST_LOG_TRIVIAL(debug) << _parent.id() << "Acquiring next data block";
        std::size_t nbytes = 0;
        char* tmp = ipcio_open_block_read(_parent._hdu->data_block, &nbytes, &_block_idx);
        if (!tmp)
        {
            _parent._log.write(LOG_ERR, "Could not get data block\n");
            throw std::runtime_error("Could not open block to read");
        }
        _current_block.reset(new RawBytes(tmp, _parent.data_buffer_size(), nbytes));
        BOOST_LOG_TRIVIAL(debug) << _parent.id() << "Data block used/total bytes = "
            << _current_block->used_bytes() <<"/"<<_current_block->total_bytes();
        return *_current_block;
    }

    void DadaReadClient::DataStream::release()
    {
        if (!_current_block)
        {
             throw std::runtime_error("No data block to be released");
        }
        BOOST_LOG_TRIVIAL(debug) << _parent.id() << "Releasing data block";
        if (ipcio_close_block_read (_parent._hdu->data_block, _current_block->used_bytes()) < 0)
        {
            _parent._log.write(LOG_ERR, "close_buffer: ipcio_close_block_read failed\n");
            throw std::runtime_error("Could not close ipcio data block");
        }
        _current_block.reset(nullptr);
    }

    std::size_t DadaReadClient::DataStream::block_idx() const
    {
        if (!_current_block)
        {
             throw std::runtime_error("No data block currently acquired");
        }
        return _block_idx;
    }

    bool DadaReadClient::DataStream::at_end() const
    {
        return (bool) ipcbuf_eod((ipcbuf_t *)(_parent._hdu->data_block));
    }

    void DadaReadClient::DataStream::purge()
    {
        std::size_t ndata = ipcbuf_get_nfull((ipcbuf_t *) _parent._hdu->data_block);
        BOOST_LOG_TRIVIAL(debug) << _parent.id() << ndata << " data blocks are full";
        for (std::size_t ii=0; ii < ndata; ++ii)
        {
            next();
            release();
            if (at_end())
            {
                _parent.reconnect();
            }
        }
    }

} //namespace psrdada_cpp





