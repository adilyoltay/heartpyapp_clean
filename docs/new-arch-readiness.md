# New Architecture Readiness

_Status: React Native 0.81.4 baseline with New Architecture enabled across app and module._

## Dependency Compatibility Matrix

| Dependency | Version (package.json) | Fabric / TurboModule Readiness | Notes & Required Actions |
| --- | --- | --- | --- |
| `react-native-vision-camera` | ^4.9.3 | Fabric frame processors stable with RN 0.81 | Manual pod remains required; keep paired with `react-native-worklets-core` ≥1.9.0. Apply upstream 4.9 frame processor patches when regenerating native plugins. |
| `react-native-reanimated` | ^3.12.0 | Fabric-compatible via JSI runtime; TurboModule not applicable | Babel plugin must stay enabled. Hermes is required; set `REANIMATED_FABRIC=1` in CI for release builds. |
| `@shopify/react-native-skia` | ^0.1.325 | Fabric renderer stable; TurboModule N/A | Requires C++17 (matched on both platforms) and `use_frameworks!` disabled. Run `pod install --repo-update` when bumping. |
| `react-native-worklets-core` | ^1.9.0 | Fabric-ready; powers VisionCamera frame processors | No extra config beyond Reanimated plugin. Stay aligned with VisionCamera minor to avoid ABI drift. |
| `react-native-heartpy` (local) | workspace | TurboModule + JSI path production-ready | CMake enforces C++17; timestamps preserved in JSI pushes with fallback to `push()` when binding missing. |
| `react-native-gesture-handler` | ^2.28.1 | Fabric-ready; TurboModule not used | Keep `GestureHandlerRootView` wrapping root. No extra patches needed. |

> For other dependencies, follow the RN upgrade helper notes for 0.81.x. Add rows here as additional native packages are introduced.

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

### Remaining Follow-ups
- Reinstate CI coverage (`yarn lint`, Android assemble, iOS Debug build) on the RN 0.81.4 toolchain.
- Track VisionCamera/Worklets release notes for additional Fabric toggles or plugin changes.
- Evaluate enabling release-mode measurements for Skia/JSI paths once QA matrix is refreshed.

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
yarn test --watch=false
# Smoke test: launch metro & run app (New Architecture enabled by default)
yarn start
# In another terminal
npx react-native run-ios   # or run-android
```

Document test results in sprint notes or update this file as coverage expands (e.g., camera streaming scenarios, wearable integrations).
