#include <lime/LimeSuite.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <thread>
#include <complex>
#include <rerun.hpp>
#include <rerun/demo_utils.hpp>

const double frequency = 433e6; // Frequency of 433 MHz
const double sampleRate = 2e6;  // Sampling rate of 2 MHz
const int bufferSize = 1024;    // Buffer size for transmitted and received samples

// Generate a square wave signal
void generateSquareWave(std::vector<std::complex<float>> &buffer) {
    bool high = true;
    for (int i = 0; i < buffer.size(); i++) {
        buffer[i] = high ? std::complex<float>(0.7, 0) : std::complex<float>(-0.7, 0);
        if (i % 16 == 0) {  // Toggle every few samples for a ~square wave
            high = !high;
        }
    }
}

int main() {
    // Initialize the LimeSDR device
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

    // Set sample rate for both TX and RX
    if (LMS_SetSampleRate(device, sampleRate, 0) != 0) {
        std::cerr << "Failed to set sample rate" << std::endl;
        LMS_Close(device);
        return -1;
    }

    // Set frequency for TX and RX
    if (LMS_SetLOFrequency(device, LMS_CH_TX, 0, frequency) != 0 ||
        LMS_SetLOFrequency(device, LMS_CH_RX, 0, frequency) != 0) {
        std::cerr << "Failed to set frequency" << std::endl;
        LMS_Close(device);
        return -1;
    }

    // Enable TX and RX channels
    if (LMS_EnableChannel(device, LMS_CH_TX, 0, true) != 0 ||
        LMS_EnableChannel(device, LMS_CH_RX, 0, true) != 0) {
        std::cerr << "Failed to enable channels" << std::endl;
        LMS_Close(device);
        return -1;
    }

    // Set TX and RX gains
    LMS_SetNormalizedGain(device, LMS_CH_TX, 0, 0.7);
    LMS_SetNormalizedGain(device, LMS_CH_RX, 0, 0.7);

    // Generate the square wave signal
    std::vector<std::complex<float>> txBuffer(bufferSize);
    generateSquareWave(txBuffer);

    // Transmit the square wave and simultaneously receive
    lms_stream_t txStream, rxStream;
    txStream.channel = 0;
    txStream.fifoSize = 1024 * 1024;
    txStream.throughputVsLatency = 0.5;
    txStream.isTx = true;
    txStream.dataFmt = lms_stream_t::LMS_FMT_F32;

    rxStream.channel = 0;
    rxStream.fifoSize = 1024 * 1024;
    rxStream.throughputVsLatency = 0.5;
    rxStream.isTx = false;
    rxStream.dataFmt = lms_stream_t::LMS_FMT_F32;

    if (LMS_SetupStream(device, &txStream) != 0 || LMS_SetupStream(device, &rxStream) != 0) {
        std::cerr << "Failed to set up streams" << std::endl;
        LMS_Close(device);
        return -1;
    }

    LMS_StartStream(&txStream);
    LMS_StartStream(&rxStream);

    // Initialize rerun recording stream
    const auto rec = rerun::RecordingStream("rerun_limesdr_square_wave");
    rec.spawn().exit_on_failure();

    std::vector<std::complex<float>> rxBuffer(bufferSize);
    for (int i = 0; i < 100; i++) {  // Transmit/Receive loop
        LMS_SendStream(&txStream, txBuffer.data(), txBuffer.size(), nullptr, 1000);
        int samplesRead = LMS_RecvStream(&rxStream, rxBuffer.data(), rxBuffer.size(), nullptr, 1000);

        // Convert received samples to rerun compatible points
        std::vector<rerun::Position3D> points;
        for (int j = 0; j < samplesRead; j++) {
            points.emplace_back(j, std::real(rxBuffer[j]), std::imag(rxBuffer[j]));
        }

        // Log the received samples as points
        rec.log("received_waveform", rerun::Points3D(points).with_radii({0.1f}));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup
    LMS_StopStream(&txStream);
    LMS_StopStream(&rxStream);
    LMS_DestroyStream(device, &txStream);
    LMS_DestroyStream(device, &rxStream);
    LMS_EnableChannel(device, LMS_CH_TX, 0, false);
    LMS_EnableChannel(device, LMS_CH_RX, 0, false);
    LMS_Close(device);

    std::cout << "Program completed" << std::endl;
    return 0;
}
