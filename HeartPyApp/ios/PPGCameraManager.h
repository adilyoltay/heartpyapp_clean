#import <React/RCTBridgeModule.h>
#import <React/RCTEventEmitter.h>
#import <AVFoundation/AVFoundation.h>

@interface PPGCameraManager : RCTEventEmitter <RCTBridgeModule>

@property (nonatomic, strong) AVCaptureDevice *captureDevice;

// Camera lock methods  
- (void)lockCameraSettings:(nonnull NSDictionary *)settings resolver:(nonnull RCTPromiseResolveBlock)resolve rejecter:(nonnull RCTPromiseRejectBlock)reject;
- (void)unlockCameraSettings:(nonnull RCTPromiseResolveBlock)resolve rejecter:(nonnull RCTPromiseRejectBlock)reject;
- (void)setTorchLevel:(nonnull NSNumber *)level resolver:(nonnull RCTPromiseResolveBlock)resolve rejecter:(nonnull RCTPromiseRejectBlock)reject;
- (void)getCameraCapabilities:(nonnull RCTPromiseResolveBlock)resolve rejecter:(nonnull RCTPromiseRejectBlock)reject;

// Sample processing method
- (void)processSample:(nonnull NSNumber *)value timestamp:(nonnull NSNumber *)timestamp;

@end
