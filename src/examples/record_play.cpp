#include <lime/LimeSuite.h>
#include <iostream>
#include <vector>
#include <complex>
#include <chrono>
#include <thread>
#include <algorithm>
#include <deque>
#include <cmath>

const double frequency = 2.44e9; // Frequency of 2.44 GHz
const double sampleRate = 2e6;   // Sampling rate of 2 MHz

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

    // Configure streams
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

    std::cout << "press enter to record" << std::endl;
    std::cin.ignore();

    LMS_StartStream(&rxStream);

    // Recording phase: Receive and store samples for 3 seconds
    std::vector<std::complex<float>> recordingBuffer(3 * sampleRate);
    auto startTime = std::chrono::steady_clock::now();
    int totalSamplesReceived = 0;
    std::deque<float> amplitudes(1000000);

    while (totalSamplesReceived < recordingBuffer.size()) {
        int samplesRead = LMS_RecvStream(&rxStream, recordingBuffer.data() + totalSamplesReceived,
                                         std::min((size_t)100, recordingBuffer.size() - totalSamplesReceived), nullptr, 1000);
        if (samplesRead > 0) {
            totalSamplesReceived += samplesRead;
            for (int i = 0; i < samplesRead; i++) {
                float real = recordingBuffer[totalSamplesReceived - samplesRead + i].real();
                float imag = recordingBuffer[totalSamplesReceived - samplesRead + i].imag();
                float amp = std::sqrt(real * real + imag * imag);
                amplitudes.push_back(amp);
                if (amp > 0.2) {
                    std::cout << amp << std::endl;
                }
                
                if (amplitudes.size() > 1000000) {
                    amplitudes.pop_front();
                }
            }
        } else {
            std::cerr << "Failed to receive samples" << std::endl;
            break;
        }
    }

    LMS_StopStream(&rxStream);
    LMS_DestroyStream(device, &rxStream);

    while (true) {

      std::cout << "press enter to play" << std::endl;
      std::cin.ignore();

      // Replay phase: Transmit the recorded samples for 3 seconds
      LMS_StartStream(&txStream);
      int rtn = LMS_SendStream(&txStream, recordingBuffer.data(), recordingBuffer.size(), nullptr, 3000);

      std::cout << rtn << std::endl;

    }

    LMS_StopStream(&txStream);
    LMS_DestroyStream(device, &txStream);

    // Cleanup
    LMS_EnableChannel(device, LMS_CH_TX, 0, false);
    LMS_EnableChannel(device, LMS_CH_RX, 0, false);
    LMS_Close(device);

    std::cout << "Program completed" << std::endl;
    return 0;
}
