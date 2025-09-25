#import <React/RCTBridgeModule.h>
#import <React/RCTEventEmitter.h>

// #import "generated/HeartPyModule.h"  // Temporarily disabled for New Architecture testing

@class RCTBridge;

@interface HeartPyModule : RCTEventEmitter <RCTBridgeModule>

@property (nonatomic, weak) RCTBridge *bridge;

// JSI installation method
- (void)installJSIBindingsWithRuntime:(void*)runtime;

@end


