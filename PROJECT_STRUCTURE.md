# HeartPy Clean Project Structure

## 📁 Proje Organizasyonu

```
heartpy-clean/
├── cpp/                          # C++ Core Library
│   ├── heartpy_core.cpp         # Ana HRV analiz motoru
│   ├── heartpy_core.h           # Core API tanımlamaları
│   ├── heartpy_stream.cpp       # Streaming analiz
│   └── heartpy_stream.h         # Stream API tanımlamaları
│
├── third_party/                  # Üçüncü parti kütüphaneler
│   └── kissfft/                 # FFT kütüphanesi
│
├── react-native-heartpy/         # React Native modülü
│   ├── src/                     # TypeScript kaynak kodları
│   │   └── index.ts            # Ana export dosyası
│   ├── android/                # Android native kod
│   │   ├── src/main/
│   │   │   ├── cpp/           # Android C++ JNI
│   │   │   └── java/          # Android Java/Kotlin
│   ├── ios/                    # iOS native kod
│   │   ├── HeartPyModule.mm   # iOS Objective-C++
│   │   └── HeartPyModule.h    
│   └── package.json
│
├── HeartPyApp/                   # React Native Uygulaması
│   ├── src/
│   │   ├── components/         # UI bileşenleri
│   │   │   ├── PPGCamera.tsx  # Kamera görüntüleme
│   │   │   ├── PPGDisplay.tsx # Metrik gösterimi
│   │   │   └── SkiaWaveform.tsx # Dalga formu çizimi
│   │   ├── core/              # İş mantığı
│   │   │   ├── PPGAnalyzer.ts # PPG analiz yöneticisi
│   │   │   ├── PPGConfig.ts   # Konfigürasyon
│   │   │   └── HeartPyWrapper.ts # Native köprü
│   │   ├── hooks/             # React hooks
│   │   └── styles/            # Stil tanımlamaları
│   ├── android/               # Android proje dosyaları
│   ├── ios/                   # iOS proje dosyaları
│   ├── App.tsx               # Ana uygulama
│   └── package.json
│
├── CMakeLists.txt               # C++ build konfigürasyonu
├── README.md                    # Proje dokümantasyonu
└── LICENSE                      # Lisans dosyası
```

## 🚀 Kurulum Adımları

### 1. Bağımlılıkları Yükle

```bash
# React Native modülü için
cd react-native-heartpy
npm install

# Ana uygulama için
cd ../HeartPyApp
npm install

# iOS için Pod kurulumu
cd ios
pod install
cd ..
```

### 2. Uygulamayı Çalıştır

#### iOS
```bash
cd HeartPyApp
npx react-native run-ios
```

#### Android
```bash
cd HeartPyApp
npx react-native run-android
```

## 🎯 Temel Özellikler

- **C++ Core:** Yüksek performanslı HRV analiz motoru
- **Real-time PPG:** Gerçek zamanlı PPG sinyal işleme
- **Cross-platform:** iOS ve Android desteği
- **Native Bridge:** Optimize edilmiş native köprü
- **UI Components:** Modern React Native UI

## 📝 Notlar

- Tüm gereksiz dosyalar (test data, build artifacts, vb.) kaldırıldı
- Sadece çalışması için gerekli minimum dosyalar tutuldu
- node_modules ve build klasörleri .gitignore'da

## 📚 Dokümantasyon

- [New Architecture Readiness](docs/new-arch-readiness.md)

## ⚠️ Önemli

- **C++ kodlarına dokunmayın** (repo kuralı)
- Sadece React Native tarafında değişiklik yapılabilir
- Gerçek PPG verisi kullanılmalı, simülasyon yasak
