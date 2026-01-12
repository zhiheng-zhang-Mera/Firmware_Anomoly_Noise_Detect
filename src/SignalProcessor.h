/**
 * @file SignalProcessor.h
 * @brief Audio Signal Processing Helper
 * @brief 音频信号处理辅助类
 * * Wraps arduinoFFT to perform spectral analysis.
 * 封装 arduinoFFT 以执行频谱分析。
 */

#ifndef SIGNAL_PROCESSOR_H
#define SIGNAL_PROCESSOR_H

#include <Arduino.h>
#include <arduinoFFT.h> // Dependency: arduinoFFT library

// FFT Configuration / FFT 配置
#define FFT_SAMPLES 1024    // Must be a power of 2 / 必须是 2 的幂次 (128, 256, 512, 1024)
#define SAMPLING_FREQ 16000 // Must match MicDriver / 必须与 MicDriver 采样率一致

class SignalProcessor {
private:
    // Arrays for Real and Imaginary parts / 实部和虚部数组
    float vReal[FFT_SAMPLES];
    float vImag[FFT_SAMPLES];
    ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, FFT_SAMPLES, SAMPLING_FREQ);

public:
    SignalProcessor() {}

    /**
     * @brief Compute FFT Spectrum from raw audio buffer.
     * @brief 输入原始音频数据，计算频谱。
     * * @param input_buffer Array of length FFT_SAMPLES / 长度为 FFT_SAMPLES 的数组
     */
    void compute(float* input_buffer) {
        // 1. Load Data / 装载数据
        for (int i = 0; i < FFT_SAMPLES; i++) {
            vReal[i] = input_buffer[i]; // Audio data to Real part / 音频数据填入实部
            vImag[i] = 0.0;             // Imaginary part zeroed / 虚部清零
        }

        // 2. Windowing (Hamming) / 加窗 (汉明窗)
        // Reduces spectral leakage by tapering edges / 减少频谱泄露
        FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);

        // 3. Compute FFT / 计算 FFT
        // Transforms Time Domain -> Frequency Domain / 时域 -> 频域转换
        FFT.compute(FFT_FORWARD);

        // 4. Compute Magnitude / 计算幅度
        // We need energy (magnitude), not phase / 我们只关心能量大小，不关心相位
        // Result stored back in vReal / 结果存回 vReal
        FFT.complexToMagnitude();
    }

    /**
     * @brief Get magnitude of specific frequency bin (Debug usage).
     * @brief 获取某个频率点的能量值 (调试用)。
     */
    float getMagnitude(int index) {
        if (index < FFT_SAMPLES / 2) {
            return vReal[index];
        }
        return 0.0f;
    }

    /**
     * @brief Get the entire spectrum array pointer.
     * @brief 获取整个频谱数组指针 (用于后续特征提取)。
     */
    float* getSpectrum() {
        return vReal;
    }
    
    /**
     * @brief Identify the dominant frequency.
     * @brief 获取主频 (用于测试麦克风准确性)。
     */
    float getMajorPeak() {
        return FFT.majorPeak();
    }
};

// Global Instance Declaration / 全局实例声明
extern SignalProcessor DSP;

#endif