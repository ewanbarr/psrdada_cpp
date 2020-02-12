#ifndef PSRDADA_CPP_EFFELSBERG_RFI_CHAMBER_RSSPECTROMETERTESTER_CUH
#define PSRDADA_CPP_EFFELSBERG_RFI_CHAMBER_RSSPECTROMETERTESTER_CUH

#include "psrdada_cpp/meerkat/fbfuse/PipelineConfig.hpp"
#include "psrdada_cpp/meerkat/fbfuse/CoherentBeamformer.cuh"
#include "thrust/host_vector.h"
#include <gtest/gtest.h>

namespace psrdada_cpp {
namespace effeslberg {
namespace rfi_chamber {
namespace test {

class RSSpectrometerTester: public ::testing::Test
{
public:
    typedef CoherentBeamformer::VoltageVectorType DeviceVoltageVectorType;
    typedef thrust::host_vector<char2> HostVoltageVectorType;
    typedef CoherentBeamformer::PowerVectorType DevicePowerVectorType;
    typedef thrust::host_vector<char> HostPowerVectorType;
    typedef CoherentBeamformer::WeightsVectorType DeviceWeightsVectorType;
    typedef thrust::host_vector<char2> HostWeightsVectorType;

protected:
    void SetUp() override;
    void TearDown() override;

public:
    RSSpectrometerTester();
    ~RSSpectrometerTester();

protected:
    PipelineConfig _config;
    cudaStream_t _stream;
};

} //namespace test
} //namespace rfi_chamber
} //namespace effeslberg
} //namespace psrdada_cpp

#endif //PSRDADA_CPP_EFFELSBERG_RFI_CHAMBER_RSSPECTROMETERTESTER_CUH
