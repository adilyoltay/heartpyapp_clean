#import "PPGCameraManager.h"
#import <React/RCTLog.h>
#import <React/RCTUtils.h>

@implementation PPGCameraManager

RCT_EXPORT_MODULE();

- (NSArray<NSString *> *)supportedEvents {
    return @[@"PPGCameraEvent", @"PPGSample"];
}

+ (BOOL)requiresMainQueueSetup {
    return YES;
}

- (instancetype)init {
    if (self = [super init]) {
        // Find the back camera
        NSArray *devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
        for (AVCaptureDevice *device in devices) {
            if (device.position == AVCaptureDevicePositionBack) {
                self.captureDevice = device;
                break;
            }
        }
        
        if (!self.captureDevice) {
            RCTLogWarn(@"❌ PPGCameraManager: Back camera not found");
        } else {
            RCTLogInfo(@"✅ PPGCameraManager: Back camera found: %@", self.captureDevice.localizedName);
        }
        
        // Listen for PPG samples from PPGMeanPlugin
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(handlePPGSample:)
                                                     name:@"HeartPyPPGSample"
                                                   object:nil];
        RCTLogInfo(@"✅ PPGCameraManager: Listening for HeartPyPPGSample notifications");
    }
    return self;
}

RCT_EXPORT_METHOD(lockCameraSettings:(nonnull NSDictionary *)settings
                  resolver:(nonnull RCTPromiseResolveBlock)resolve
                  rejecter:(nonnull RCTPromiseRejectBlock)reject) {
    
    if (!self.captureDevice) {
        reject(@"NO_CAMERA", @"Camera device not available", nil);
        return;
    }
    
    NSError *error;
    if (![self.captureDevice lockForConfiguration:&error]) {
        reject(@"LOCK_FAILED", @"Could not lock camera for configuration", error);
        return;
    }
    
    @try {
        NSMutableDictionary *result = [NSMutableDictionary dictionary];
        
        // 1. FPS Lock
        NSNumber *targetFPS = settings[@"fps"];
        if (targetFPS && [targetFPS doubleValue] > 0) {
            double fps = [targetFPS doubleValue];
            CMTime frameDuration = CMTimeMake(1, (int32_t)fps);
            
            // Check if FPS is supported
            if ([self isFPSSupported:fps]) {
                self.captureDevice.activeVideoMinFrameDuration = frameDuration;
                self.captureDevice.activeVideoMaxFrameDuration = frameDuration;
                result[@"fps"] = @(fps);
                RCTLogInfo(@"🔒 FPS locked to %.1f", fps);
            } else {
                RCTLogWarn(@"⚠️ FPS %.1f not supported, using default", fps);
                result[@"fps"] = @"not_supported";
            }
        }
        
        // 2. Exposure Lock
        NSNumber *exposureDuration = settings[@"exposureDuration"];  // in seconds
        NSNumber *iso = settings[@"iso"];
        if (exposureDuration && iso) {
            double expDur = [exposureDuration doubleValue];
            float isoValue = [iso floatValue];
            
            // Clamp values to supported ranges
            CMTime expTime = CMTimeMakeWithSeconds(expDur, 1000000);
            expTime = [self clampExposureTime:expTime];
            isoValue = [self clampISO:isoValue];
            
            if ([self.captureDevice isExposureModeSupported:AVCaptureExposureModeCustom]) {
                [self.captureDevice setExposureModeCustomWithDuration:expTime
                                                                 ISO:isoValue
                                                   completionHandler:^(CMTime syncTime) {
                    RCTLogInfo(@"🔒 Exposure locked: %.4fs, ISO:%.0f", CMTimeGetSeconds(expTime), isoValue);
                }];
                result[@"exposure"] = @{@"duration": @(CMTimeGetSeconds(expTime)), @"iso": @(isoValue)};
            } else {
                result[@"exposure"] = @"not_supported";
            }
        }
        
        // 3. White Balance Lock  
        NSString *whiteBalanceMode = settings[@"whiteBalance"];
        if (whiteBalanceMode && [whiteBalanceMode isEqualToString:@"locked"]) {
            if ([self.captureDevice isWhiteBalanceModeSupported:AVCaptureWhiteBalanceModeLocked]) {
                // Get current white balance gains and lock them
                AVCaptureWhiteBalanceGains currentGains = self.captureDevice.deviceWhiteBalanceGains;
                AVCaptureWhiteBalanceGains normalizedGains = [self normalizeGains:currentGains];
                
                [self.captureDevice setWhiteBalanceModeLockedWithDeviceWhiteBalanceGains:normalizedGains
                                                                       completionHandler:^(CMTime syncTime) {
                    RCTLogInfo(@"🔒 White balance locked");
                }];
                result[@"whiteBalance"] = @"locked";
            } else {
                result[@"whiteBalance"] = @"not_supported";
            }
        }
        
        // 4. Focus Lock
        NSString *focusMode = settings[@"focus"];
        if (focusMode && [focusMode isEqualToString:@"locked"]) {
            if ([self.captureDevice isFocusModeSupported:AVCaptureFocusModeLocked]) {
                // First set to auto to get good focus, then lock
                if ([self.captureDevice isFocusModeSupported:AVCaptureFocusModeAutoFocus]) {
                    self.captureDevice.focusMode = AVCaptureFocusModeAutoFocus;
                    
                    // Wait a bit then lock
                    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                        if ([self.captureDevice lockForConfiguration:nil]) {
                            self.captureDevice.focusMode = AVCaptureFocusModeLocked;
                            self.captureDevice.subjectAreaChangeMonitoringEnabled = NO;
                            [self.captureDevice unlockForConfiguration];
                            RCTLogInfo(@"🔒 Focus locked");
                        }
                    });
                }
                result[@"focus"] = @"locked";
            } else {
                result[@"focus"] = @"not_supported";
            }
        }
        
        // 5. Torch Level
        NSNumber *torchLevel = settings[@"torchLevel"];
        if (torchLevel) {
            float level = [torchLevel floatValue];
            if ([self.captureDevice hasTorch] && [self.captureDevice isTorchModeSupported:AVCaptureTorchModeOn]) {
                level = fmax(0.0, fmin(1.0, level));  // Clamp 0-1
                NSError *torchError;
                if ([self.captureDevice setTorchModeOnWithLevel:level error:&torchError]) {
                    result[@"torch"] = @{@"level": @(level), @"status": @"on"};
                    RCTLogInfo(@"🔦 Torch set to level %.2f", level);
                } else {
                    result[@"torch"] = @{@"error": torchError.localizedDescription};
                }
            } else {
                result[@"torch"] = @"not_supported";
            }
        }
        
        [self.captureDevice unlockForConfiguration];
        resolve(result);
        
    } @catch (NSException *exception) {
        [self.captureDevice unlockForConfiguration];
        reject(@"CONFIGURATION_ERROR", exception.reason, nil);
    }
}

RCT_EXPORT_METHOD(unlockCameraSettings:(nonnull RCTPromiseResolveBlock)resolve
                  rejecter:(nonnull RCTPromiseRejectBlock)reject) {
    
    if (!self.captureDevice) {
        reject(@"NO_CAMERA", @"Camera device not available", nil);
        return;
    }
    
    NSError *error;
    if (![self.captureDevice lockForConfiguration:&error]) {
        reject(@"LOCK_FAILED", @"Could not lock camera for configuration", error);
        return;
    }
    
    // Reset to auto modes
    if ([self.captureDevice isExposureModeSupported:AVCaptureExposureModeContinuousAutoExposure]) {
        self.captureDevice.exposureMode = AVCaptureExposureModeContinuousAutoExposure;
    }
    
    if ([self.captureDevice isWhiteBalanceModeSupported:AVCaptureWhiteBalanceModeContinuousAutoWhiteBalance]) {
        self.captureDevice.whiteBalanceMode = AVCaptureWhiteBalanceModeContinuousAutoWhiteBalance;
    }
    
    if ([self.captureDevice isFocusModeSupported:AVCaptureFocusModeContinuousAutoFocus]) {
        self.captureDevice.focusMode = AVCaptureFocusModeContinuousAutoFocus;
        self.captureDevice.subjectAreaChangeMonitoringEnabled = YES;
    }
    
    // Turn off torch
    if ([self.captureDevice hasTorch] && [self.captureDevice isTorchModeSupported:AVCaptureTorchModeOff]) {
        self.captureDevice.torchMode = AVCaptureTorchModeOff;
    }
    
    [self.captureDevice unlockForConfiguration];
    
    RCTLogInfo(@"🔓 Camera settings unlocked - returned to auto modes");
    resolve(@{@"status": @"unlocked"});
}

RCT_EXPORT_METHOD(setTorchLevel:(nonnull NSNumber *)level
                  resolver:(nonnull RCTPromiseResolveBlock)resolve
                  rejecter:(nonnull RCTPromiseRejectBlock)reject) {
    
    if (!self.captureDevice || ![self.captureDevice hasTorch]) {
        reject(@"NO_TORCH", @"Torch not available", nil);
        return;
    }
    
    float torchLevel = fmax(0.0, fmin(1.0, [level floatValue]));
    
    NSError *error;
    if (![self.captureDevice lockForConfiguration:&error]) {
        reject(@"LOCK_FAILED", @"Could not lock camera for configuration", error);
        return;
    }
    
    if (torchLevel > 0.0) {
        if ([self.captureDevice isTorchModeSupported:AVCaptureTorchModeOn]) {
            if ([self.captureDevice setTorchModeOnWithLevel:torchLevel error:&error]) {
                resolve(@{@"level": @(torchLevel), @"status": @"on"});
            } else {
                reject(@"TORCH_ERROR", error.localizedDescription, error);
            }
        } else {
            reject(@"TORCH_NOT_SUPPORTED", @"Torch mode not supported", nil);
        }
    } else {
        self.captureDevice.torchMode = AVCaptureTorchModeOff;
        resolve(@{@"level": @(0.0), @"status": @"off"});
    }
    
    [self.captureDevice unlockForConfiguration];
}

RCT_EXPORT_METHOD(getCameraCapabilities:(nonnull RCTPromiseResolveBlock)resolve
                  rejecter:(nonnull RCTPromiseRejectBlock)reject) {
    
    if (!self.captureDevice) {
        reject(@"NO_CAMERA", @"Camera device not available", nil);
        return;
    }
    
    NSMutableDictionary *capabilities = [NSMutableDictionary dictionary];
    
    // FPS capabilities
    NSMutableArray *supportedFPS = [NSMutableArray array];
    for (AVFrameRateRange *range in self.captureDevice.activeFormat.videoSupportedFrameRateRanges) {
        [supportedFPS addObject:@{
            @"min": @(range.minFrameRate),
            @"max": @(range.maxFrameRate)
        }];
    }
    capabilities[@"fps"] = supportedFPS;
    
    // Exposure capabilities
    CMTime minExp = self.captureDevice.activeFormat.minExposureDuration;
    CMTime maxExp = self.captureDevice.activeFormat.maxExposureDuration;
    capabilities[@"exposure"] = @{
        @"minDuration": @(CMTimeGetSeconds(minExp)),
        @"maxDuration": @(CMTimeGetSeconds(maxExp)),
        @"minISO": @(self.captureDevice.activeFormat.minISO),
        @"maxISO": @(self.captureDevice.activeFormat.maxISO)
    };
    
    // Supported modes
    capabilities[@"modes"] = @{
        @"customExposure": @([self.captureDevice isExposureModeSupported:AVCaptureExposureModeCustom]),
        @"lockedWhiteBalance": @([self.captureDevice isWhiteBalanceModeSupported:AVCaptureWhiteBalanceModeLocked]),
        @"lockedFocus": @([self.captureDevice isFocusModeSupported:AVCaptureFocusModeLocked]),
        @"torch": @([self.captureDevice hasTorch])
    };
    
    resolve(capabilities);
}

#pragma mark - Helper Methods

- (BOOL)isFPSSupported:(double)fps {
    for (AVFrameRateRange *range in self.captureDevice.activeFormat.videoSupportedFrameRateRanges) {
        if (fps >= range.minFrameRate && fps <= range.maxFrameRate) {
            return YES;
        }
    }
    return NO;
}

- (CMTime)clampExposureTime:(CMTime)time {
    CMTime minTime = self.captureDevice.activeFormat.minExposureDuration;
    CMTime maxTime = self.captureDevice.activeFormat.maxExposureDuration;
    
    if (CMTimeCompare(time, minTime) < 0) return minTime;
    if (CMTimeCompare(time, maxTime) > 0) return maxTime;
    return time;
}

- (float)clampISO:(float)iso {
    float minISO = self.captureDevice.activeFormat.minISO;
    float maxISO = self.captureDevice.activeFormat.maxISO;
    
    return fmax(minISO, fmin(maxISO, iso));
}

- (AVCaptureWhiteBalanceGains)normalizeGains:(AVCaptureWhiteBalanceGains)gains {
    gains.redGain = fmax(1.0, fmin(self.captureDevice.maxWhiteBalanceGain, gains.redGain));
    gains.greenGain = fmax(1.0, fmin(self.captureDevice.maxWhiteBalanceGain, gains.greenGain));
    gains.blueGain = fmax(1.0, fmin(self.captureDevice.maxWhiteBalanceGain, gains.blueGain));
    return gains;
}

RCT_EXPORT_METHOD(processSample:(nonnull NSNumber *)value timestamp:(nonnull NSNumber *)timestamp) {
    // Forward sample to React Native via event
    [self sendEventWithName:@"PPGSample" body:@{
        @"value": value,
        @"timestamp": timestamp
    }];
}

- (void)handlePPGSample:(NSNotification *)notification {
    NSDictionary *userInfo = notification.userInfo;
    NSNumber *value = userInfo[@"value"];
    NSNumber *timestamp = userInfo[@"timestamp"];
    NSNumber *confidence = userInfo[@"confidence"];

    RCTLogInfo(@"📸 PPGCameraManager: Received notification with userInfo: %@", userInfo);
    RCTLogInfo(@"📸 PPGCameraManager: value: %@, timestamp: %@, confidence: %@",
               value, timestamp, confidence);

    if (value && timestamp) {
        RCTLogInfo(@"📸 PPGCameraManager: Received PPG sample - value: %.3f, timestamp: %.0f, confidence: %.2f",
                   value.doubleValue, timestamp.doubleValue, confidence.doubleValue);

        // Forward sample to React Native via event
        [self sendEventWithName:@"PPGSample" body:@{
            @"value": value,
            @"timestamp": timestamp,
            @"confidence": confidence ?: @(0.0)
        }];
        RCTLogInfo(@"📸 PPGCameraManager: Successfully sent PPGSample event to React Native");
    } else {
        RCTLogWarn(@"📸 PPGCameraManager: Invalid sample data - value: %@, timestamp: %@", value, timestamp);
    }
}

@end
