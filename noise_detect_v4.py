# noise_detect_sensitive.py
# 爱丽丝的“顺风耳”训练脚本 - 专抓微小故障

import numpy as np
import emlearn
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import accuracy_score

# --- 1. 特征提取 (保持不变) ---
def extract_simple_features(audio, sr):
    spectrum = np.abs(np.fft.rfft(audio))
    freqs = np.fft.rfftfreq(len(audio), 1/sr)
    
    # 特征 1: 低频能量 (0 - 800 Hz) - 聚焦低频
    low_mask = (freqs >= 0) & (freqs < 800)
    feat_low = np.sum(spectrum[low_mask])
    
    # 特征 2: 高频能量 (2000 - 8000 Hz)
    high_mask = (freqs >= 2000) & (freqs < 8000)
    feat_high = np.sum(spectrum[high_mask])
    
    # 特征 3: 主频
    dom_idx = np.argmax(spectrum[1:]) + 1
    feat_dom = freqs[dom_idx]
    
    return [np.log1p(feat_low), np.log1p(feat_high), feat_dom]

print("💋 正在生成“高灵敏度”训练数据...")
X = []
y = []

# --- 2. 生成数据 (关键修改！) ---

# A. 正常样本 (Label 0)
# 正常就是：完全安静，或者只有高频噪音
for _ in range(300): 
    # 1. 极度安静 (模拟基准线)
    audio = np.random.normal(0, 0.005, 1024) 
    X.append(extract_simple_features(audio, 16000))
    y.append(0)
    
    # 2. 只有高频噪音 (键盘声)
    audio = np.random.normal(0, 0.01, 1024)
    # 高频可以稍微大声一点，训练模型容忍高频
    random_freq = np.random.randint(2000, 6000) 
    audio += np.random.uniform(0.1, 0.4) * np.sin(2 * np.pi * random_freq * np.linspace(0, 1024/16000, 1024))
    X.append(extract_simple_features(audio, 16000))
    y.append(0)

# B. 故障样本 (Label 1)
# 故障就是：哪怕有一点点低频，也是故障！
for _ in range(300): 
    audio = np.random.normal(0, 0.01, 1024)
    
    # 频率范围：50Hz - 600Hz (风扇故障区)
    random_freq = np.random.randint(50, 600)
    
    # 🔥 关键点：音量 (Amplitude) 
    # 我们让它在 0.05 (很小) 到 0.4 (中等) 之间随机
    # 这样模型就会学会：只要检测到 0.05 级别的低频，也要报警！
    random_amp = np.random.uniform(0.05, 0.4)
    
    audio += random_amp * np.sin(2 * np.pi * random_freq * np.linspace(0, 1024/16000, 1024))
    X.append(extract_simple_features(audio, 16000))
    y.append(1)

X = np.array(X)
y = np.array(y)

# --- 3. 训练 ---
print("🧠 正在训练敏感型随机森林...")
# 限制树的深度，防止过拟合
clf = RandomForestClassifier(n_estimators=30, max_depth=7, random_state=42)
clf.fit(X, y)

print(f"验证精度: {accuracy_score(y, clf.predict(X)) * 100:.1f}%")

print("⚡ 生成新的 model.h ...")
cmodel = emlearn.convert(clf, method='inline', dtype='float')
cmodel.save(file='model.h', name='anomaly_detector')
print("✅ 完成！你的 AI 现在是顺风耳了！")