#include <lime/LimeSuite.h>
#include <iostream>
#include <vector>
#include <complex>
#include <chrono>
#include <thread>

// Frequency to transmit at (e.g., 2.44 GHz)
const double targetFrequency = 2.44e9;  // 2.44 GHz
const double sampleRate = 10e6;         // Set a sample rate, e.g., 10 MHz
const int bufferSize = 1024;            // Buffer size

// Generate a noise signal or constant wave
void generateNoiseSignal(std::vector<std::complex<float>> &buffer) {
    for (auto &sample : buffer) {
        sample = std::complex<float>(0.9f * (rand() / (float)RAND_MAX - 0.5f), 
                                     0.9f * (rand() / (float)RAND_MAX - 0.5f));
    }
}

int main() {
    lms_device_t* device = nullptr;
    if (LMS_Open(&device, nullptr, nullptr) != 0) {
        std::cerr << "Failed to open LimeSDR device" << std::endl;
        return -1;
    }

    if (LMS_Init(device) != 0) {
        std::cerr << "Failed to initialize LimeSDR device" << std::endl;
        LMS_Close(device);
        return -1;
    }

    // Set the sample rate
    if (LMS_SetSampleRate(device, sampleRate, 0) != 0) {
        std::cerr << "Failed to set sample rate" << std::endl;
        LMS_Close(device);
        return -1;
    }

    // Set the transmission frequency
    if (LMS_SetLOFrequency(device, LMS_CH_TX, 0, targetFrequency) != 0) {
        std::cerr << "Failed to set frequency" << std::endl;
        LMS_Close(device);
        return -1;
    }

    // Enable the TX channel
    if (LMS_EnableChannel(device, LMS_CH_TX, 0, true) != 0) {
        std::cerr << "Failed to enable TX channel" << std::endl;
        LMS_Close(device);
        return -1;
    }

    // Set TX gain
    LMS_SetNormalizedGain(device, LMS_CH_TX, 0, 1.0);

    // Set up the transmission stream
    lms_stream_t txStream;
    txStream.channel = 0;
    txStream.fifoSize = 1024 * 1024;
    txStream.throughputVsLatency = 0.5;
    txStream.isTx = true;
    txStream.dataFmt = lms_stream_t::LMS_FMT_F32;

    if (LMS_SetupStream(device, &txStream) != 0) {
        std::cerr << "Failed to set up TX stream" << std::endl;
        LMS_Close(device);
        return -1;
    }

    LMS_StartStream(&txStream);

    // Create a noise signal buffer
    std::vector<std::complex<float>> txBuffer(bufferSize);
    generateNoiseSignal(txBuffer);

    // Transmit loop
    for (int i = 0; i < 1000; i++) {  // Arbitrary loop count for demonstration
        LMS_SendStream(&txStream, txBuffer.data(), txBuffer.size(), nullptr, 1000);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Cleanup
    LMS_StopStream(&txStream);
    LMS_DestroyStream(device, &txStream);
    LMS_EnableChannel(device, LMS_CH_TX, 0, false);
    LMS_Close(device);

    std::cout << "Transmission completed" << std::endl;
    return 0;
}
