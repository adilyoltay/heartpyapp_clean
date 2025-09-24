# HeartPy Clean - React Native PPG Analysis App

A high-performance React Native application for real-time photoplethysmography (PPG) signal analysis and heart rate variability (HRV) measurement, powered by a custom C++ engine based on HeartPy algorithms.

## 🚀 Features

### Core Capabilities
- **Real-time PPG Analysis**: Process camera-based PPG signals at 30+ FPS
- **Advanced HRV Metrics**: SDNN, RMSSD, pNN50, frequency domain analysis
- **Native Performance**: C++ core with JSI bindings for optimal speed
- **Cross-platform**: iOS and Android support with platform-specific optimizations

### Technical Highlights
- **Signal Processing**: Butterworth filtering, peak detection, RR interval analysis
- **Quality Assessment**: Real-time signal quality indicators and artifact rejection
- **Visualization**: Hardware-accelerated waveform rendering with Skia
- **Frame Processing**: Vision Camera integration with Worklets for efficient video processing

## 📱 Screenshots

<div align="center">
  <img src="docs/screenshot_measure.png" alt="Measurement Screen" width="250"/>
  <img src="docs/screenshot_analysis.png" alt="Analysis View" width="250"/>
  <img src="docs/screenshot_settings.png" alt="Settings" width="250"/>
</div>

## 🛠️ Tech Stack

### Frontend
- **React Native 0.74.3**: Latest stable version with New Architecture support
- **TypeScript**: Type-safe development
- **React Navigation**: Native navigation experience
- **Skia**: GPU-accelerated graphics for waveform rendering
- **Reanimated 3**: Smooth 60 FPS animations
- **Vision Camera**: Advanced camera processing with frame processors

### Native Modules
- **C++ Core**: High-performance signal processing engine
- **KissFFT**: Fast Fourier Transform for frequency analysis
- **JSI Bindings**: Direct JavaScript-to-C++ communication
- **Objective-C++/Java**: Platform-specific bridge code

### Architecture
```
┌─────────────────────────────────────┐
│         React Native UI              │
├─────────────────────────────────────┤
│     TypeScript Business Logic        │
├─────────────────────────────────────┤
│         JSI Bridge Layer             │
├─────────────────────────────────────┤
│      C++ HeartPy Core Engine         │
├─────────────────────────────────────┤
│   Platform Native APIs (iOS/Android) │
└─────────────────────────────────────┘
```

## 📋 Prerequisites

- **Node.js**: v18.0.0 or higher
- **React Native CLI**: Latest version
- **Xcode**: 15.0+ (for iOS development)
- **Android Studio**: Flamingo or newer (for Android development)
- **CocoaPods**: 1.12.0+ (iOS dependencies)
- **CMake**: 3.18+ (for Android native builds)

## 🔧 Installation

### 1. Clone the Repository
```bash
git clone https://github.com/yourusername/heartpyapp_clean.git
cd heartpyapp_clean
```

### 2. Install Dependencies
```bash
# Install npm packages
npm install

# iOS specific
cd ios && pod install
cd ..
```

### 3. Configure Native Modules
The project includes pre-configured native modules. Ensure the following paths are correctly set:
- iOS: `react-native-heartpy/ios/HeartPy.podspec`
- Android: `react-native-heartpy/android/build.gradle`

## 🏃‍♂️ Running the App

### iOS
```bash
# Run on simulator
npx react-native run-ios

# Run on device
npx react-native run-ios --device
```

### Android
```bash
# Start Metro bundler
npx react-native start

# Run on emulator/device
npx react-native run-android
```

## 🔍 Project Structure

```
heartpy-clean/
├── HeartPyApp/               # Main React Native application
│   ├── src/                  # TypeScript source code
│   │   ├── components/       # UI components
│   │   ├── screens/          # App screens
│   │   ├── core/             # Core business logic
│   │   └── hooks/            # Custom React hooks
│   ├── ios/                  # iOS native code
│   └── android/              # Android native code
├── react-native-heartpy/      # Native module package
│   ├── cpp/                  # Shared C++ code
│   ├── ios/                  # iOS-specific native code
│   └── android/              # Android-specific native code
├── cpp/                       # C++ HeartPy core engine
│   ├── heartpy_core.cpp      # Core analysis algorithms
│   └── heartpy_stream.cpp    # Real-time streaming
└── third_party/              # External dependencies
    └── kissfft/              # FFT library
```

## 🧪 Testing

### Unit Tests
```bash
npm test
```

### PPG Signal Validation
```bash
npm run check:ppg
```

### End-to-End Tests
```bash
# iOS
npm run e2e:ios

# Android
npm run e2e:android
```

## 📊 Performance

### Benchmarks
- **Frame Processing**: 30-60 FPS on modern devices
- **PPG Analysis Latency**: <50ms for real-time metrics
- **Memory Usage**: ~150MB baseline, ~200MB during active measurement
- **Battery Impact**: ~5-7% per 10-minute session

### Optimization Tips
1. Enable Hermes for improved JavaScript performance
2. Use release builds for production testing
3. Profile with Flipper for performance bottlenecks
4. Monitor with React DevTools Profiler

## 🐛 Troubleshooting

### Common Issues

#### EMFILE Error (Too many open files)
```bash
# Fix for macOS
./scripts/fix-file-limit.sh
```

#### Metro Bundler Issues
```bash
# Clear cache
npx react-native start --reset-cache

# Clean build
cd ios && xcodebuild clean
cd android && ./gradlew clean
```

#### @babel/runtime Not Found
```bash
npm install @babel/runtime --save
cd ios && pod install
```

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for details.

### Development Workflow
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Code Style
- **TypeScript**: Follow ESLint configuration
- **C++**: Google C++ Style Guide
- **Objective-C**: Apple's Coding Guidelines
- **Java/Kotlin**: Android Code Style

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **HeartPy**: Original Python implementation by Paul van Gent
- **KissFFT**: Mark Borgerding's FFT library
- **React Native Community**: For excellent documentation and tools
- **Vision Camera**: Marc Rousavy's camera framework

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/yourusername/heartpyapp_clean/issues)
- **Discussions**: [GitHub Discussions](https://github.com/yourusername/heartpyapp_clean/discussions)
- **Email**: support@heartpyapp.com

## 🚦 Status

![Build Status](https://img.shields.io/github/workflow/status/yourusername/heartpyapp_clean/CI)
![Version](https://img.shields.io/github/package-json/v/yourusername/heartpyapp_clean)
![License](https://img.shields.io/github/license/yourusername/heartpyapp_clean)
![Platform](https://img.shields.io/badge/platform-iOS%20%7C%20Android-blue)

---

<div align="center">
  Made with ❤️ using React Native and C++
</div>