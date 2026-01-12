/**
 * @file main.cpp
 * @brief Main Application Logic
 * @brief 主程序逻辑
 * * Integrates MicDriver, SignalProcessor, and AnomalyDetector.
 * 整合麦克风驱动、信号处理和异常检测模块。
 */

#include <Arduino.h>
#include "MicDriver.h"
#include "AnomalyDetector.h"
#include "SignalProcessor.h" 

// Global Instances / 全局实例
MicDriver Mic;
AnomalyDetector Detector;
SignalProcessor DSP; 

// Configuration / 配置宏
#define FFT_SIZE 1024
#define CALIBRATION_FRAMES 100 // Frames needed for baseline calibration / 基准校准所需帧数
#define LED_PIN 2              // Status LED / 状态指示灯

// Buffers / 缓冲区
float audio_buffer[FFT_SIZE]; 
int buffer_index = 0;

// Adaptive Calibration State / 自适应校准状态
bool is_calibrated = false;
int calibration_counter = 0;
float baseline_low = 0;  // Baseline for low frequencies / 低频基准
float baseline_high = 0; // Baseline for high frequencies / 高频基准

// =============================================================================
// Sliding Window Logic (Hysteresis Filter)
// 滑动窗口逻辑 (迟滞滤波)
// Purpose: Prevents flickering alarms due to transient noise.
// 目的：防止因瞬态噪声导致的警报闪烁。
// =============================================================================
#define WINDOW_SIZE 6      // Window size: Look at last 6 frames / 窗口大小：看最近 6 帧
#define ALARM_THRESHOLD 2   // Threshold: Alarm if >= 2 anomalies / 报警阈值：>= 2 帧异常则报警

int history_buffer[WINDOW_SIZE] = {0}; // Circular buffer storing 0 or 1 / 存 0 或 1 的循环数组
int history_idx = 0;                   // Current write pointer / 当前写入指针

void setup() {
    Serial.begin(9600);
    pinMode(LED_PIN, OUTPUT);
    
    // Boot Blink Sequence / 开机闪烁序列
    digitalWrite(LED_PIN, HIGH); delay(100); digitalWrite(LED_PIN, LOW); delay(100);
    digitalWrite(LED_PIN, HIGH); delay(100); digitalWrite(LED_PIN, LOW);

    Serial.println("(Please keep quiet for calibration)");
    Serial.println("(请保持安静以进行校准)");

    Mic.begin();
    Detector.begin();
}

void loop() {
    MicData data = Mic.read();

    if (data.valid) {
        // 🔥 Hardware Compensation Gain / 硬件补偿增益
        // Patch for signal loss due to breadboard resistance.
        // 针对万用板电阻损耗的软件补丁，放大 23 倍。
        audio_buffer[buffer_index] = data.left_top * 23.0; 
        buffer_index++;

        // Process when buffer is full / 缓冲区满时处理
        if (buffer_index >= FFT_SIZE) {
            
            // 1. Remove DC Offset (Center signal at 0)
            // 1. 去除直流偏移 (将信号中心对准 0)
            float dc_offset = 0;
            for(int i=0; i<FFT_SIZE; i++) dc_offset += audio_buffer[i];
            dc_offset /= FFT_SIZE;
            for(int i=0; i<FFT_SIZE; i++) audio_buffer[i] -= dc_offset;

            // 2. FFT Processing / FFT 处理
            DSP.compute(audio_buffer);
            float* spectrum = DSP.getSpectrum();
            float dom_freq = DSP.getMajorPeak();

            // 3. Extract Energy Features (Log Scale)
            // 3. 提取能量特征 (对数刻度)
            float raw_low = 0;
            for(int i=1; i<64; i++) raw_low += spectrum[i]; // Low Band / 低频段
            raw_low = log(raw_low + 1.0); 

            float raw_high = 0;
            for(int i=128; i<512; i++) raw_high += spectrum[i]; // High Band / 高频段
            raw_high = log(raw_high + 1.0);

            // --- State Machine: Calibration vs Inference ---
            // --- 状态机：校准 vs 推理 ---
            if (!is_calibrated) {
                // Learning Phase / 学习阶段
                baseline_low += raw_low;
                baseline_high += raw_high;
                calibration_counter++;

                if (calibration_counter % 10 == 0) {
                    Serial.print("Learning... "); Serial.println(calibration_counter);
                }

                if (calibration_counter >= CALIBRATION_FRAMES) {
                    baseline_low /= CALIBRATION_FRAMES;
                    baseline_high /= CALIBRATION_FRAMES;
                    is_calibrated = true;
                    Serial.println("✅ Learning Complete! Sensitivity Optimized.");
                    Serial.println("✅ 学习完成！灵敏度已优化。");
                }

            } else {
                // Inference Phase / 推理阶段
                
                // Subtract baseline (Background Noise Cancellation)
                // 减去基准 (背景噪声消除)
                float feat_low = raw_low - baseline_low;
                if (feat_low < 0) feat_low = 0;
                
                // Digital Amplifier / 数字放大器
                feat_low = feat_low * 7.0; 

                float feat_high = raw_high - baseline_high;
                if (feat_high < 0) feat_high = 0;
                float feat_dom = dom_freq;

                // Prepare features for model / 准备模型特征
                float features[3] = {feat_low, feat_high, feat_dom};
                int result = Detector.predict(features);

                // Safety Net: Force Trigger if energy is too high
                // 强制防线：如果能量过高，强制触发 (适应万用板特性)
                if (feat_low > 0.6) result = 1;

                // ===============================================
                //  Sliding Window Filter Implementation
                //  滑动窗口滤波器实现 
                // ===============================================
                
                // A. Update History / 更新历史记录
                history_buffer[history_idx] = result;
                
                // B. Move Pointer (Circular Buffer) / 移动指针 (循环缓冲区)
                history_idx++;
                if (history_idx >= WINDOW_SIZE) history_idx = 0;

                // C. Count Faults in Window / 统计窗口内的故障数
                int total_faults = 0;
                for(int i=0; i<WINDOW_SIZE; i++) {
                    total_faults += history_buffer[i];
                }

                // D. Debug Output / 调试输出
                Serial.print("LowΔ:"); Serial.print(feat_low); 
                Serial.print(" | Window: ["); 
                Serial.print(total_faults);
                Serial.print("/");
                Serial.print(WINDOW_SIZE);
                Serial.print("]");

                // E. Final Decision / 最终判定
                if (total_faults >= ALARM_THRESHOLD) {
                    Serial.println(" -> 🔴 ALARM! (Fault Detected)");
                    
                    // Fast Flash Logic / 极速闪烁逻辑
                    // 20ms flash, no blocking delay / 20ms 闪烁，无阻塞延迟
                    digitalWrite(LED_PIN, HIGH); 
                    delay(20); 
                    digitalWrite(LED_PIN, LOW);
                } 
                else if (total_faults > 0) {
                    Serial.println(" -> ⚠️ Observing...");
                    digitalWrite(LED_PIN, LOW);
                } 
                else {
                    Serial.println(" -> 🟢 Normal");
                    digitalWrite(LED_PIN, LOW);
                }
            }

            buffer_index = 0; // Reset buffer / 重置缓冲区
        }
    }
}