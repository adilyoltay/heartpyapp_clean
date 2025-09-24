# New Architecture Readiness

_Status: React Native 0.74.3 baseline across app and module._

## Dependency Compatibility Matrix

| Dependency | Version (package.json) | Fabric / TurboModule Readiness | Notes & Required Actions |
| --- | --- | --- | --- |
| `react-native-vision-camera` | ^4.7.2 | Fabric frame processors supported behind `newArchEnabled` but still beta | Keep manual pod; pair with `react-native-worklets-core` ≥1.6.0. Apply latest `VisionCamera-FrameProcessor` patch before toggling Fabric. |
| `react-native-reanimated` | 3.8.1 | Fabric-compatible via JSI runtime; TurboModule not applicable | Ensure Babel plugin stays enabled. Requires Hermes for best parity; enable `REANIMATED_FABRIC=1` env when flipping new-arch. |
| `@shopify/react-native-skia` | ^0.1.214 | Fabric renderer stable; TurboModule N/A | Requires C++17 (already set) and `use_frameworks!` disabled. Recommend running `pod install --repo-update` when bumping. |
| `react-native-worklets-core` | ^1.6.2 | Fabric-ready; powers VisionCamera frame processors | No extra config beyond Reanimated plugin. Align version with VisionCamera minor to avoid ABI drift. |
| `react-native-heartpy` (local) | workspace | Native modules still Fabric-in-progress | CMake now repo-relative; add generated code from TurboModule codegen under `react-native-heartpy/cpp/generated`. |
| `react-native-gesture-handler` | ^2.28.0 | Fabric-ready; TurboModule not used | Keep `GestureHandlerRootView` wrapping root. No extra patches needed. |

> For other dependencies, follow the RN upgrade helper notes for 0.74.x. Add rows here as additional native packages are introduced.

## Migration Checklist

### Completed in PR #1
- Converted Android CMake to repo-relative paths with optional `cpp/generated` auto-discovery.
- Cleaned the iOS `Podfile`, enabled Hermes, and documented manual pods for VisionCamera/Reanimated.
- Captured current dependency readiness and documented validation commands below.

### Completed in PR #2
- Introduced shared TypeScript types under `src/types/heartpy.ts` for API + TurboModule parity.
- Added `NativeHeartPy` spec wrapper that prefers TurboModules before falling back to `NativeModules`.
- Refactored `src/index.ts` to use a single `getNativeModule()` helper and flow typed arrays as `ReadonlyArray<number>` when available.
- Added `codegenConfig` to `package.json` pointing to module metadata (name, jsSrcsDir, Android/iOS settings).
- Declared `@babel/runtime` in both the app and module so Metro resolves compiled helpers without manual linking.

### Completed in PR #3
- Added the TS TurboModule spec under `src/specs/NativeHeartPy.ts` and refreshed codegen outputs in `ios/generated` + `cpp/generated`.
- Refactored `HeartPyModule.{h,mm}` to conform to the generated `NativeHeartPySpec`, including sync/async analyzers, realtime APIs, config accessors, and event-emitter hooks.
- Updated the iOS Podspec to compile the generated sources and ensured the new registry path works alongside the legacy bridge.
- Wired TypeScript bindings to cast codegen returns to the shared `HeartPyResult` shape and tightened runtime config handling.

### Completed in PR #4
- Ran Android codegen and checked in the generated `NativeHeartPySpec.java` plus the accompanying JNI stubs under `cpp/generated`.
- Updated `HeartPyModule.java` to extend the codegenerated spec, adapting every sync/async analyzer, preprocessing call, and realtime entry point to the TurboModule signatures while reusing the existing JNI helpers.
- Switched `HeartPyPackage` to a `TurboReactPackage` so the module registers as a TurboModule while still supporting the legacy bridge list.

### Remaining Before Enabling TurboModules/Fabric
- Update the iOS target to opt into the New Architecture (`RCT_NEW_ARCH_ENABLED=1`) once CI smoke passes with the new module.
- Audit Metro config (asset and sourceExt tweaks) once VisionCamera + Skia begin emitting Fabric components.
- Add CI jobs for `yarn lint`, Android build (Debug & Release), and iOS Debug build.
- Keep `HeartPyApp/node_modules` cache warm; Metro reports missing helpers if the app is linked before dependencies install.

## Validation Commands

Run these after each migration step to guard against regressions:

```bash
# Android library & sample app
cd react-native-heartpy/android
./gradlew :react-native-heartpy:assembleDebug
./gradlew :react-native-heartpy:assembleRelease

# iOS pods & build
cd ../../HeartPyApp/ios
pod install
xcodebuild -workspace HeartPyApp.xcworkspace -scheme HeartPyApp -configuration Debug -sdk iphonesimulator

# JavaScript checks
cd ..
yarn lint
# Smoke test: launch metro & run app
yarn start
# In another terminal
npx react-native run-ios   # or run-android
```

Document test results in sprint notes or update this file as coverage expands (e.g., camera streaming scenarios, wearable integrations).
