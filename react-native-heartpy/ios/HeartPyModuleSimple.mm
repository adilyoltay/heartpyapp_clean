#import "HeartPyModuleSimple.h"
#import <React/RCTLog.h>

@implementation HeartPyModuleSimple

RCT_EXPORT_MODULE(HeartPyModule);

- (BOOL)requiresMainQueueSetup { 
    return YES; 
}

RCT_EXPORT_METHOD(analyze:(NSArray *)signal 
                  fs:(double)fs 
                  options:(NSDictionary *)options
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject) {
    // Minimal test implementation
    NSDictionary *result = @{
        @"bpm": @(72.5),
        @"sdnn": @(45.2),
        @"rmssd": @(35.8),
        @"pnn50": @(12.3),
        @"quality": @{
            @"totalBeats": @(50),
            @"rejectedBeats": @(2),
            @"rejectionRate": @(0.04),
            @"goodQuality": @(YES)
        }
    };
    resolve(result);
}

RCT_EXPORT_METHOD(installJSI:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject) {
    // JSI not implemented in simple version
    resolve(@(NO));
}

@end
