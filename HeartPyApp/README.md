This is a [**React Native**](https://reactnative.dev) sample app bootstrapped with [`@react-native-community/cli`](https://github.com/react-native-community/cli) and wired to `react-native-heartpy` for on‑device HR/RR/HRV.

# Getting Started

>**Note**: Make sure you have completed the [React Native - Environment Setup](https://reactnative.dev/docs/environment-setup) instructions till "Creating a new application" step, before proceeding.

## Quick Start (with `react-native-heartpy`)

Install the local module in this workspace root:

```
yarn add file:../react-native-heartpy
```

Enable the New Architecture (Hermes recommended). Then:

### Step 1: Start the Metro Server

First, you will need to start **Metro**, the JavaScript _bundler_ that ships _with_ React Native.

To start Metro, run the following command from the _root_ of your React Native project:

```bash
# using npm
npm start

# OR using Yarn
yarn start
```

## Step 2: Start your Application

Let Metro Bundler run in its _own_ terminal. Open a _new_ terminal from the _root_ of your React Native project. Run the following command to start your _Android_ or _iOS_ app:

### For Android

```bash
# using npm
npm run android

# OR using Yarn
yarn android
```

### For iOS

```bash
# using npm
npm run ios

# OR using Yarn
yarn ios
```

If everything is set up _correctly_, you should see your new app running in your _Android Emulator_ or _iOS Simulator_ shortly provided you have set up your emulator/simulator correctly.

This is one way to run your app — you can also run it directly from within Android Studio and Xcode respectively.

## Calling HeartPy from the App

```ts
import {analyzeAsync, installJSI} from 'react-native-heartpy';

installJSI();

const run = async (ppg: number[]) => {
  const fs = 50;
  const res = await analyzeAsync(ppg, fs, {
    bandpass: {lowHz: 0.5, highHz: 5, order: 2},
    welch: {nfft: 1024, overlap: 0.5},
    peak: {refractoryMs: 320, thresholdScale: 0.5},
    quality: {rejectSegmentwise: true, segmentRejectWindowBeats: 10, segmentRejectMaxRejects: 3},
  });
  console.log('BPM', res.bpm, 'conf', res.quality.confidence);
};
```

For more details see `../docs/mobile_integration.md`.

Now that you have successfully run the app, let's modify it.

1. Open `App.tsx` in your text editor of choice and edit some lines.
2. For **Android**: Press the <kbd>R</kbd> key twice or select **"Reload"** from the **Developer Menu** (<kbd>Ctrl</kbd> + <kbd>M</kbd> (on Window and Linux) or <kbd>Cmd ⌘</kbd> + <kbd>M</kbd> (on macOS)) to see your changes!

   For **iOS**: Hit <kbd>Cmd ⌘</kbd> + <kbd>R</kbd> in your iOS Simulator to reload the app and see your changes!

## Congratulations! :tada:

You've successfully run and modified your React Native App. :partying_face:

### Now what?

- If you want to add this new React Native code to an existing application, check out the [Integration guide](https://reactnative.dev/docs/integration-with-existing-apps).
- If you're curious to learn more about React Native, check out the [Introduction to React Native](https://reactnative.dev/docs/getting-started).

## HeartPy Runtime Notes

### SNR Cadence

- HeartPy'nın SNR yenileme döngüsü yaklaşık 2 saniyelik aralıklarla çalışır.
- Ardışık iki örnek arasındaki süre `dt < 2.0s` olduğunda aşağıdaki gibi bir log görürsünüz ve önceki kalite değeri yeniden kullanılır:

  ```
  [HeartPySNR] updateSNR cadence skip: dt=1.265 < 2.000, reuse previous quality
  ```
- Bu davranış tasarımsaldır; SNR serisini yumuşatır ve gereksiz dalgalanmaları engeller.

### Watchdog Uyarıları

- Warm-up aşamasında (rezervuar hazır olana kadar) watchdog otomatik olarak sessizdir; ilk başarılı poll sonrası aktifleşir.
- Geliştirici modunda dahi uyarıları tamamen kapatmak isterseniz `PPG_CONFIG.debug.watchdogLogsEnabled` bayrağını `false` yapabilirsiniz.

### Metro & DevTools İpuçları

- React DevTools için: `npx react-devtools` (varsayılan port 8097).
- Aynı ağdaki cihazların bağlanabilmesi için Metro'yu herkese açık IP üzerinde başlatın: `npx react-native start --host 0.0.0.0`.
- 8097 portuna erişim sorunları yaşıyorsanız firewall ayarlarını ve cihazların aynı Wi‑Fi ağında olduğunu kontrol edin.

### Gerçek Zamanlı HRV Metrik Eşikleri

- **RR örnek sayısı (rrCount)** belirli eşiklere ulaşmadan kartlar `—` gösterir:
  - `rmssd` / `sdsd`: rrCount ≥ **3**
  - `sdnn` / `pnn20` / `pnn50`: rrCount ≥ **8**
  - `breathingRate`: rrCount ≥ **10**
- `HeartPyWrapper` bu eşiklere göre alanları `undefined` bırakır; UI değeri doldurmak için yeni poll’ü bekler.

### pNN Normalizasyonu

- Native HeartPy pNN değerleri bazı platformlarda oran (0..1), bazılarında yüzde (0..100) döndürebilir.
- Wrapper her zaman bir **oran** döndürür:
  - `value > 1.5` ise `value / 100`
  - Sonuç 0..1 arasında tutulur
- UI kartları bu oranı `×100` yapıp `%` formatında gösterir, böylece çift ölçekleme engellenir.

### LF/HF Detay Modu

- LF/HF analizi **Frequency Domain (LF/HF)** toggle’ı ile açılır.
- Toggle **OFF** (varsayılan):
  - Analiz penceresi ≈ **12 s** (360 örnek)
  - LF/HF kartı `—` gösterir
- Toggle **ON**:
  - Analiz penceresi ≈ **30 s** (900 örnek)
  - rrCount ≥ **17** toplandığında LF/HF hesaplanır ve kart dolmaya başlar
  - Welch PSD kadansı ≈ **2 s**; CPU/pil maliyeti artabileceği için uzun ölçümlerde kullanım tavsiye edilir
- Toggle değiştirildiğinde analyzer otomatik olarak yeniden başlatılır (Stop/Start), yeni ayarlar native köprüye aktarılır.

# Troubleshooting

If you can't get this to work, see the [Troubleshooting](https://reactnative.dev/docs/troubleshooting) page.

# Learn More

To learn more about React Native, take a look at the following resources:

- [React Native Website](https://reactnative.dev) - learn more about React Native.
- [Getting Started](https://reactnative.dev/docs/environment-setup) - an **overview** of React Native and how setup your environment.
- [Learn the Basics](https://reactnative.dev/docs/getting-started) - a **guided tour** of the React Native **basics**.
- [Blog](https://reactnative.dev/blog) - read the latest official React Native **Blog** posts.
- [`@facebook/react-native`](https://github.com/facebook/react-native) - the Open Source; GitHub **repository** for React Native.
