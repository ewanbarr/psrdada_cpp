#include "psrdada_cpp/effelsberg/rfi_chamber/RSSpectrometer.cuh"
#include "psrdada_cpp/cuda_utils.hpp"
#include <thrust/system/cuda/execution_policy.h>
#include "thrust/functional.h"
#include "thrust/transform.h"
#include "thrust/fill.h"
#include <cassert>

namespace psrdada_cpp {
namespace effelsberg {
namespace rfi_chamber {
namespace kernels {

struct short2_to_float2
    : public thrust::unary_function<short2, float2>
{
    __host__ __device__
    float2 operator()(short2 in)
    {
        float2 out;
        out.x = (float) in.x;
        out.y = (float) in.y;
        return out;
    }
};

struct detect_accumulate
    : public thrust::binary_function<float2, float, float>
{
    __host__ __device__
    float operator()(float2 voltage, float power_accumulator)
    {
        float power = voltage.x * voltage.x + voltage.y * voltage.y;
        return power_accumulator + power;
    }
};

} // namespace kernels


RSSpectrometer::RSSpectrometer(std::size_t input_nchans, std::size_t fft_length,
    std::size_t naccumulate, std::size_t nskip)
    : _input_nchans(input_nchans)
    , _fft_length(fft_length)
    , _naccumulate(naccumulate)
    , _nskip(nskip)
    , _output_nchans(_fft_length * _input_nchans)
    , _bytes_per_input_spectrum(_input_nchans * sizeof(InputType))
{

    BOOST_LOG_TRIVIAL(info) << "Initialising RSSpectrometer instance";
    BOOST_LOG_TRIVIAL(debug) << "Available GPU memory: " << MEM_LIMIT << " bytes";
    std::size_t mem_budget = MEM_LIMIT;
    // Memory required for accumulation buffer
    mem_budget -= _output_nchans * sizeof(OutputType);
    // Memory required per input channel
    std::size_t mem_per_input_channel = (_fft_length *
        (sizeof(FftType) + sizeof(InputType)));
    BOOST_LOG_TRIVIAL(debug) << "Memory required per input channel: " << mem_per_input_channel << " bytes";
    _chans_per_copy = mem_budget / mem_per_input_channel;
    while (_input_nchans % _chans_per_copy != 0)
    {
        _chans_per_copy -= 1;
    }
    assert(_chans_per_copy > 0); /** Must be able to process at least 1 channel */
    BOOST_LOG_TRIVIAL(debug) << "Nchannels per GPU transfer: " << _chans_per_copy;
    // Resize all buffers.
    BOOST_LOG_TRIVIAL(debug) << "Resizing all memory buffers";
    _accumulation_buffer.resize(_output_nchans, 0.0f);
    _h_accumulation_buffer.resize(_output_nchans, 0.0f);
    _fft_buffer.resize(_chans_per_copy * _fft_length);
    _copy_buffer.resize(_chans_per_copy * _fft_length);

    // Allocate streams
    BOOST_LOG_TRIVIAL(debug) << "Allocating CUDA streams";
    CUDA_ERROR_CHECK(cudaStreamCreate(&_copy_stream));
    CUDA_ERROR_CHECK(cudaStreamCreate(&_proc_stream));

    // Configure CUFFT for FFT execution
    BOOST_LOG_TRIVIAL(debug) << "Generating CUFFT plan";
    CUFFT_ERROR_CHECK(cufftPlanMany(
        *_fft_plan, 1, {_fft_length},
        {_input_nchans}, _input_nchans
        1, {_fft_length}, 1, _fft_length,
        CUFFT_C2C, _input_nchans));
    BOOST_LOG_TRIVIAL(debug) << "Setting CUFFT stream";
    CUFFT_ERROR_CHECK(cufftSetStream(_proc_stream));
    BOOST_LOG_TRIVIAL(info) << "RSSpectrometer instance initialised";
}

RSSpectrometer::~RSSpectrometer()
{
    BOOST_LOG_TRIVIAL(info) << "Destroying RSSpectrometer instance";
    BOOST_LOG_TRIVIAL(debug) << "Destroying CUDA streams";
    CUDA_ERROR_CHECK(cudaStreamDestroy(_copy_stream));
    CUDA_ERROR_CHECK(cudaStreamDestroy(_proc_stream));
    BOOST_LOG_TRIVIAL(debug) << "Destroying CUFFT plan";
    CUFFT_ERROR_CHECK(cufftDestroy(plan));
    BOOST_LOG_TRIVIAL(info) << "RSSpectrometer instance destroyed";
}

void RSSpectrometer::init(RawBytes &header)
{
    BOOST_LOG_TRIVIAL(debug) << "RSSpectrometer received header block";
}

void RSSpectrometer::operator()(RawBytes &block)
{
    BOOST_LOG_TRIVIAL(debug) << "RSSpectrometer received data block";
    if (_nskip > 0)
    {
        BOOST_LOG_TRIVIAL(debug) << "Skipping block while stream stabilizes";
        --_nskip;
        return false
    }
    assert(block.used_bytes() % _bytes_per_input_spectrum == 0) /** Block is not a multiple of the heap group size */
    std::size_t nspectra_in = block.used_bytes() / _bytes_per_input_spectrum;
    BOOST_LOG_TRIVIAL(debug) << "Number of input spectra per block: " << nspectra_in;
    assert(block.used_bytes() % _output_nchans * sizeof(InputType) == 0) /** Block is not a multiple of the spectrum size */
    std::size_t nspectra_out = block.used_bytes() / (_output_nchans * sizeof(InputType));
    BOOST_LOG_TRIVIAL(debug) << "Number of output spectra per block: " << nspectra_out;

    std::size_t n_to_accumulate;
    if (nspectra_out > _naccumulate)
    {
        n_to_accumulate = _naccumulate;
    }
    else
    {
        n_to_accumulate = nspectra;
    }
    BOOST_LOG_TRIVIAL(debug) << "Number of spectra to accumulate in current block: " << n_to_accumulate;
    BOOST_LOG_TRIVIAL(debug) << "Entering processing loop";
    std::size_t nchan_blocks = _input_nchans / _chans_per_copy;
    for (std::size_t spec_idx = 0; spec_idx < nspectra; ++spec_idx)
    {
        copy(spec_idx, 0, nspectra_in);
        for (std::size_t chan_block_idx = 1;
            chan_block_idx < nchan_blocks - 1;
            ++chan_block_idx)
        {
            copy(spec_idx, chan_block_idx, nspectra_in);
            process(chan_block_idx);
        }
        CUDA_ERROR_CHECK(cudaStreamSynchronize(_copy_stream));
        _copy_buffer.swap();
        process(nchan_blocks - 1);
    }
    BOOST_LOG_TRIVIAL(debug) << "Processing loop complete";
    _naccumulate -= n_to_accumulate;
    BOOST_LOG_TRIVIAL(info) << "Accumulated " << n_to_accumulate
    << " spectra ("<< _naccumulate << " remaining)";
    if (_naccumulate == 0)
    {
        BOOST_LOG_TRIVIAL(debug) << "Processing loop complete";
        return true;
    }
    return false;
}

void RSSpectrometer::process(std::size_t chan_block_idx)
{
    /** Note streams do not actually work as expected
     *  with Thrust. The code is synchronous with respect
     *  to the host. The Thrust 1.9.4 (CUDA 10.1) release
     *  includes thrust::async which alleviates this problem.
     *  This can be included here if need be, but as it is the
     *  H2D copy should still run in parallel to the FFT, so
     *  there is no performance cost to blocking on the host.
     */
    CUDA_ERROR_CHECK(cudaStreamSynchronize(_proc_stream));
    // Convert shorts to floats
    BOOST_LOG_TRIVIAL(debug) << "Performing short2 to float2 conversion";
    thrust::transform(
        thrust::cuda::par.on(_proc_stream),
        _copy_buffer.b().begin(),
        _copy_buffer.b().end(),
        _fft_buffer.begin(),
        kernels::short2_to_float2());
    // Perform forward C2C transform
    BOOST_LOG_TRIVIAL(debug) << "Executing FFT";
    cufftFloatComplex* ptr = reinterpret_cast<cufftFloatComplex>(
        thrust::raw_pointer_cast(_fft_buffer.data()));
    CUFFT_ERROR_CHECK(cufftExecC2C(
        _fft_plan, ptr, ptr, CUFFT_FORWARD));
    std::size_t chan_offset = chan_block_idx * _chans_per_copy * _fft_length;
    // Detect FFT output and accumulate
    BOOST_LOG_TRIVIAL(debug) << "Detecting and accumulating";
    thrust::transform(
        thrust::cuda::par.on(_proc_stream),
        _fft_buffer.begin(),
        _fft_buffer.end(),
        _accumulation_buffer.begin() + chan_offset,
        _accumulation_buffer.begin() + chan_offset,
        kernels::detect_accumulate());
}

void RSSpectrometer::copy(std::size_t spec_idx, std::size_t chan_block_idx, std::size_t nspectra_in)
{
    BOOST_LOG_TRIVIAL(debug) << "Copying block to GPU";
    std::size_t spitch = _input_nchans * sizeof(short2); // Width of a row in bytes (so number of channels total)
    std::size_t dpitch = nspectra_in; // Total number of samples in the input
    std::size_t width = _chans_per_copy * sizeof(short2); // Width of row in bytes in the output
    std::size_t height = _fft_length; // Total number of samples to copy
    CUDA_ERROR_CHECK(cudaStreamSynchronize(_copy_stream));
    _copy_buffer.swap();
    char* src = block.ptr() + spec_id * spitch * height + chan_block_idx * width;
    CUDA_ERROR_CHECK(cudaMemcpy2DAsync(_copy_buffer.a_ptr(),
        dpitch, src, spitch, width, height,
        cudaMemcpyHostToDevice, _copy_stream));
}


} //namespace fbfuse
} //namespace effelsberg
} //namespace psrfi_chambercpp

#endif //PSRDADA_CPP_EFFELSBERG_RFI_CHAMBER_RSSPECTROMETER_CUH