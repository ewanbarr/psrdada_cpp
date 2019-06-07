
#include "psrdada_cpp/meerkat/fbfuse/test/BufferDumpTester.hpp"
#include "psrdada_cpp/dada_db.hpp"
#include "psrdada_cpp/dada_null_sink.hpp"
#include "psrdada_cpp/dada_output_stream.hpp"

namespace psrdada_cpp {
namespace meerkat {
namespace fbfuse {
namespace test {

BufferDumpTester::BufferDumpTester()
    : ::testing::Test()
{

}

BufferDumpTester::~BufferDumpTester()
{
}

void BufferDumpTester::SetUp()
{
}

void BufferDumpTester::TearDown()
{
}

TEST_F(BufferDumpTester, do_nothing)
{

    std::size_t nchans = 64;
    std::size_t total_nchans = 4096;
    std::size_t nantennas = 64;
    std::size_t ngroups = 8;
    std::size_t nblocks = 64;
    std::size_t block_size = nchans * nantennas * ngroups * 256 * sizeof(unsigned);

    float cfreq = 856e6;
    float bw = 856e6 / (total_nchans / nchans);
    float max_fill_level = 0.8;

    DadaDB buffer(nblocks, block_size, 4, 4096);
    buffer.create();
    MultiLog log("log");
    DadaOutputStream ostream(buffer.key(), log);

    std::vector<char> input_header_buffer(4096, 0);
    RawBytes input_header_rb(input_header_buffer.data(), 4096, 4096);
    Header header(input_header_rb);
    header.set<long double>("SAMPLE_CLOCK", 856000000.0);
    header.set<long double>("SYNC_TIME", 0.0);
    header.set<std::size_t>("SAMPLE_CLOCK_START", 0);
    ostream.init(input_header_rb);
    std::vector<char> input_data_buffer(block_size, 0);
    RawBytes input_data_rb(&input_data_buffer[0], block_size, block_size);
    for (uint32_t ii=0; ii < nblocks-1; ++ii)
    {
        ostream(input_data_rb);
    }

    NullSink sink;
    DadaReadClient reader(buffer.key(), log);
    BufferDump<decltype(sink)> dumper(reader, sink, "/tmp/buffer_dump_test.sock",
                                      max_fill_level, nantennas, nchans,
                                      total_nchans, cfreq, bw);

    std::thread dumper_thread([&](){
        dumper.start();
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    dumper.stop();

    dumper_thread.join();
}

} //namespace test
} //namespace fbfuse
} //namespace meerkat
} //namespace psrdada_cpp
