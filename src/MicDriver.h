/**
 * @file MicDriver.h
 * @brief I2S Microphone Driver for ESP32
 * @brief ESP32 I2S 麦克风驱动程序
 * * Handles the initialization and reading of I2S MEMS microphones.
 * 处理 I2S MEMS 麦克风的初始化和读取。
 */

#ifndef MIC_DRIVER_H
#define MIC_DRIVER_H

#include <Arduino.h>
#include <driver/i2s.h>

// =============================================================================
// Hardware Pin Configuration / 硬件引脚配置
// Encapsulated here so main program doesn't need to know details.
// 封装在此处，主程序无需知道细节。
// =============================================================================
#define MIC_SAMPLE_RATE  16000     // 16kHz Sampling / 16kHz 采样率
#define MIC_BUF_LEN      256       // Buffer length / 缓冲区长度
#define MIC_BUF_COUNT    4         // Buffer count / 缓冲区数量

// Group A: Left Satellite Board / 左侧卫星板
#define I2S_PORT_L       I2S_NUM_0
#define PIN_SCK_L        19
#define PIN_WS_L         23
#define PIN_SD_L         4

// Group B: Right Satellite Board / 右侧卫星板
#define I2S_PORT_R       I2S_NUM_1
#define PIN_SCK_R        35
#define PIN_WS_R         32 // Note: Sometimes EN is on 32 / 注意：有时 EN 在 32
#define PIN_SD_R         26

/**
 * @struct MicData
 * @brief Structure to neatly return 4-channel microphone data.
 * @brief 用于整洁地返回 4 路麦克风数据的结构体。
 */
struct MicData {
    float left_top;  // Mic 1
    float left_bot;  // Mic 2
    float right_top; // Mic 3
    float right_bot; // Mic 4
    bool valid;      // Data validity flag / 数据是否有效标志
};

class MicDriver {
private:
    /**
     * @brief Helper function to configure a single I2S port.
     * @brief 辅助函数：配置单个 I2S 端口。
     */
    void install_port(i2s_port_t port, int sck, int ws, int sd) {
        i2s_config_t i2s_config = {
            .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = MIC_SAMPLE_RATE,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = MIC_BUF_COUNT,
            .dma_buf_len = MIC_BUF_LEN,
            .use_apll = false
        };
        i2s_driver_install(port, &i2s_config, 0, NULL);
        
        i2s_pin_config_t pin_config = {
            .bck_io_num = sck,
            .ws_io_num = ws,
            .data_out_num = I2S_PIN_NO_CHANGE,
            .data_in_num = sd
        };
        i2s_set_pin(port, &pin_config);
    }

public:
    /**
     * @brief Initialize both I2S ports (Left/Right ears).
     * @brief 初始化双耳 I2S 端口。
     */
    void begin() {
        install_port(I2S_PORT_L, PIN_SCK_L, PIN_WS_L, PIN_SD_L);
        install_port(I2S_PORT_R, PIN_SCK_R, PIN_WS_R, PIN_SD_R);
    }

    /**
     * @brief Read data from all microphones simultaneously.
     * @brief 一键读取 4 路麦克风数据。
     * @return MicData Struct containing normalized float values / 包含归一化浮点值的结构体
     */
    MicData read() {
        MicData result = {0, 0, 0, 0, false};
        int32_t buf_a[2]; // Temp buffer for single frame / 单帧临时缓存
        int32_t buf_b[2];
        size_t bytes_a = 0, bytes_b = 0;

        // Parallel Read / 并行读取
        // Using short timeout (10ms) to avoid blocking / 使用短超时 (10ms) 避免阻塞
        esp_err_t err_a = i2s_read(I2S_PORT_L, &buf_a, sizeof(buf_a), &bytes_a, 10); 
        esp_err_t err_b = i2s_read(I2S_PORT_R, &buf_b, sizeof(buf_b), &bytes_b, 10);

        if (err_a == ESP_OK && err_b == ESP_OK && bytes_a > 0 && bytes_b > 0) {
            // Data Normalization / 数据归一化
            // Scale down raw 32-bit integer to float / 将原始 32 位整数缩放到浮点数
            float scale = 10000.0f;
            result.left_top  = (float)buf_a[0] / scale;
            result.left_bot  = (float)buf_a[1] / scale;
            result.right_top = (float)buf_b[0] / scale;
            result.right_bot = (float)buf_b[1] / scale;
            result.valid = true;
        }
        return result;
    }
};

// Global Instance Declaration / 全局实例声明
extern MicDriver Mic; 

#endif