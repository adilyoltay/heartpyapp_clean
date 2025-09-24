#import <React/RCTBridgeModule.h>
#import <React/RCTEventEmitter.h>

@class RCTBridge;

@interface HeartPyModule : RCTEventEmitter <RCTBridgeModule>

@property (nonatomic, weak) RCTBridge *bridge;

// JSI installation method
- (void)installJSIBindingsWithRuntime:(void*)runtime;

@end


