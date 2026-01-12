/**
 * @file AnomalyDetector.h
 * @brief Machine Learning Inference Wrapper
 * @brief 机器学习推理封装
 * * Interfaces with the emlearn-generated C code.
 * 对接 emlearn 生成的 C 代码。
 */

#ifndef ANOMALY_DETECTOR_H
#define ANOMALY_DETECTOR_H

#include <Arduino.h>
#include "model.h" // Include the generated model / 引用生成的模型文件

// Feature vector size (Must match Python training configuration)
// 特征向量长度 (必须和 Python 训练时一致，例如 67)
#define FEATURE_SIZE 67 

class AnomalyDetector {
private:
    float features[FEATURE_SIZE]; // Internal feature buffer / 内部特征缓存

public:
    void begin() {
        // Initialization if needed / 如果需要初始化在此处执行
    }

    /**
     * @brief Run inference on input features.
     * @brief 对输入特征进行推理。
     * * @param input_features Array of features / 特征数组
     * @return int 0 = Normal, 1 = Anomaly / 0 = 正常, 1 = 异常
     */
    int predict(float* input_features) {
        // Call the underlying generated function
        // 调用底层生成函数
        return anomaly_detector_predict(input_features, FEATURE_SIZE);
    }
};

// Global Instance Declaration / 全局实例声明
extern AnomalyDetector Detector;

#endif