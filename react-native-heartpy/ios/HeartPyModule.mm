#import "HeartPyModule.h"

#import <React/RCTBridge.h>
#import <React/RCTUtils.h>
#import <jsi/jsi.h>

#include "heartpy_core.h"
// Realtime streaming API
#include "heartpy_stream.h"
// Options validator (RN step 1)
#include "rn_options_builder.h"

using namespace facebook;

@implementation HeartPyModule

@synthesize bridge = _bridge;

// Event emitter support
- (NSArray<NSString *> *)supportedEvents {
    return @[@"PPGSample"];
}

// Global PPG buffer for frame processor data
static NSMutableArray<NSNumber*>* globalPPGBuffer = nil;
static NSMutableArray<NSNumber*>* globalPPGTsBuffer = nil;
static dispatch_queue_t ppgBufferQueue = nil;
static double lastPPGConfidence = 0.0;

+ (void)initialize {
    if (self == [HeartPyModule class]) {
        globalPPGBuffer = [[NSMutableArray alloc] init];
        globalPPGTsBuffer = [[NSMutableArray alloc] init];
        ppgBufferQueue = dispatch_queue_create("heartpy.ppg.buffer", DISPATCH_QUEUE_SERIAL);
        // Subscribe to native PPG notifications from VisionCamera frame processor
        [[NSNotificationCenter defaultCenter] addObserverForName:@"HeartPyPPGSample"
                                                          object:nil
                                                           queue:nil  // Process on posting thread for speed
                                                      usingBlock:^(__unused NSNotification * _Nonnull note) {
            NSNumber* value = note.userInfo[@"value"];
            if (!value) return;
            NSNumber* tsNum = note.userInfo[@"timestamp"];
            double ts = tsNum ? [tsNum doubleValue] : [[NSDate date] timeIntervalSince1970];
            NSNumber* confNum = note.userInfo[@"confidence"];
            if (confNum) {
                double c = [confNum doubleValue];
                if (isfinite(c)) lastPPGConfidence = fmax(0.0, fmin(1.0, c));
            }
            
            // Add to buffer on dedicated queue
            dispatch_async(ppgBufferQueue, ^{
                [globalPPGBuffer addObject:value];
                [globalPPGTsBuffer addObject:@(ts)];
                // Keep last 300 samples (~10-20 seconds of data at 15-30 fps)
                if (globalPPGBuffer.count > 300) {
                    NSRange removeRange = NSMakeRange(0, globalPPGBuffer.count - 300);
                    [globalPPGBuffer removeObjectsInRange:removeRange];
                }
                if (globalPPGTsBuffer.count > 300) {
                    NSRange removeRange2 = NSMakeRange(0, globalPPGTsBuffer.count - 300);
                    [globalPPGTsBuffer removeObjectsInRange:removeRange2];
                }
                
                // Debug log every 30 samples
                if (globalPPGBuffer.count % 30 == 0) {
                    NSLog(@"📦 Native PPG buffer size: %lu samples", (unsigned long)globalPPGBuffer.count);
                }
            });
        }];
    }
}

// Store PPG sample from frame processor
RCT_EXPORT_METHOD(storePPGSample:(double)value timestamp:(double)timestamp) {
  dispatch_async(ppgBufferQueue, ^{
    [globalPPGBuffer addObject:@(value)];
    // Keep last 100 samples
    if (globalPPGBuffer.count > 100) {
      [globalPPGBuffer removeObjectAtIndex:0];
    }
  });
}

// Get latest PPG samples for UI
RCT_EXPORT_METHOD(getLatestPPGSamples:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject) {
    dispatch_async(ppgBufferQueue, ^{
        NSArray* samples = [globalPPGBuffer copy];
        [globalPPGBuffer removeAllObjects]; // drain buffer on poll
        resolve(samples ?: @[]);
    });
}

// Get latest PPG samples with timestamps
RCT_EXPORT_METHOD(getLatestPPGSamplesTs:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject) {
    dispatch_async(ppgBufferQueue, ^{
        NSArray* samples = [globalPPGBuffer copy];
        NSArray* ts = [globalPPGTsBuffer copy];
        [globalPPGBuffer removeAllObjects];
        [globalPPGTsBuffer removeAllObjects];
        NSDictionary* out = @{ @"samples": samples ?: @[], @"timestamps": ts ?: @[] };
        resolve(out);
    });
}

RCT_EXPORT_METHOD(getLastPPGConfidence:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject) {
    // Return last confidence as a simple value (no need for queue)
    resolve(@(lastPPGConfidence));
}

static void installBinding(jsi::Runtime &rt) {
	auto analyzeFunc = jsi::Function::createFromHostFunction(
		rt,
		jsi::PropNameID::forAscii(rt, "__HeartPyAnalyze"),
		3,
		[](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args, size_t count) -> jsi::Value {
			if (count < 2) {
				throw jsi::JSError(rt, "analyze() requires signal and fs");
			}
			auto arrObj = args[0].asObject(rt);
			size_t len = (size_t)arrObj.getProperty(rt, "length").asNumber();
			std::vector<double> signal; signal.reserve(len);
			for (size_t i = 0; i < len; ++i) signal.push_back(arrObj.getProperty(rt, jsi::PropNameID::forUtf8(rt, std::to_string(i))).asNumber());
			double fs = args[1].asNumber();
			heartpy::Options opt{};
			
			// Parse options if provided
			if (count > 2 && !args[2].isUndefined() && args[2].isObject()) {
				auto optObj = args[2].asObject(rt);
				
				// Bandpass options
				if (optObj.hasProperty(rt, "bandpass")) {
					auto bp = optObj.getProperty(rt, "bandpass").asObject(rt);
					if (bp.hasProperty(rt, "lowHz")) opt.lowHz = bp.getProperty(rt, "lowHz").asNumber();
					if (bp.hasProperty(rt, "highHz")) opt.highHz = bp.getProperty(rt, "highHz").asNumber();
					if (bp.hasProperty(rt, "order")) opt.iirOrder = bp.getProperty(rt, "order").asNumber();
				}
				
				// Welch options
				if (optObj.hasProperty(rt, "welch")) {
					auto w = optObj.getProperty(rt, "welch").asObject(rt);
					if (w.hasProperty(rt, "nfft")) opt.nfft = w.getProperty(rt, "nfft").asNumber();
					if (w.hasProperty(rt, "overlap")) opt.overlap = w.getProperty(rt, "overlap").asNumber();
				}
				
				// Peak detection options
				if (optObj.hasProperty(rt, "peak")) {
					auto p = optObj.getProperty(rt, "peak").asObject(rt);
					if (p.hasProperty(rt, "refractoryMs")) opt.refractoryMs = p.getProperty(rt, "refractoryMs").asNumber();
					if (p.hasProperty(rt, "thresholdScale")) opt.thresholdScale = p.getProperty(rt, "thresholdScale").asNumber();
					if (p.hasProperty(rt, "bpmMin")) opt.bpmMin = p.getProperty(rt, "bpmMin").asNumber();
					if (p.hasProperty(rt, "bpmMax")) opt.bpmMax = p.getProperty(rt, "bpmMax").asNumber();
				}
				
				// Preprocessing options
				if (optObj.hasProperty(rt, "preprocessing")) {
					auto prep = optObj.getProperty(rt, "preprocessing").asObject(rt);
					if (prep.hasProperty(rt, "interpClipping")) opt.interpClipping = prep.getProperty(rt, "interpClipping").asBool();
					if (prep.hasProperty(rt, "clippingThreshold")) opt.clippingThreshold = prep.getProperty(rt, "clippingThreshold").asNumber();
					if (prep.hasProperty(rt, "hampelCorrect")) opt.hampelCorrect = prep.getProperty(rt, "hampelCorrect").asBool();
					if (prep.hasProperty(rt, "hampelWindow")) opt.hampelWindow = prep.getProperty(rt, "hampelWindow").asNumber();
					if (prep.hasProperty(rt, "hampelThreshold")) opt.hampelThreshold = prep.getProperty(rt, "hampelThreshold").asNumber();
					if (prep.hasProperty(rt, "removeBaselineWander")) opt.removeBaselineWander = prep.getProperty(rt, "removeBaselineWander").asBool();
					if (prep.hasProperty(rt, "enhancePeaks")) opt.enhancePeaks = prep.getProperty(rt, "enhancePeaks").asBool();
				}
				
				// Quality options
				if (optObj.hasProperty(rt, "quality")) {
					auto qual = optObj.getProperty(rt, "quality").asObject(rt);
					if (qual.hasProperty(rt, "rejectSegmentwise")) opt.rejectSegmentwise = qual.getProperty(rt, "rejectSegmentwise").asBool();
					if (qual.hasProperty(rt, "segmentRejectThreshold")) opt.segmentRejectThreshold = qual.getProperty(rt, "segmentRejectThreshold").asNumber();
					if (qual.hasProperty(rt, "cleanRR")) opt.cleanRR = qual.getProperty(rt, "cleanRR").asBool();
					if (qual.hasProperty(rt, "cleanMethod")) {
						std::string method = qual.getProperty(rt, "cleanMethod").asString(rt).utf8(rt);
						if (method == "iqr") opt.cleanMethod = heartpy::Options::CleanMethod::IQR;
						else if (method == "z-score") opt.cleanMethod = heartpy::Options::CleanMethod::Z_SCORE;
						else opt.cleanMethod = heartpy::Options::CleanMethod::QUOTIENT_FILTER;
					}
				}
				
				// High precision options
				if (optObj.hasProperty(rt, "highPrecision")) {
					auto hp = optObj.getProperty(rt, "highPrecision").asObject(rt);
					if (hp.hasProperty(rt, "enabled")) opt.highPrecision = hp.getProperty(rt, "enabled").asBool();
					if (hp.hasProperty(rt, "targetFs")) opt.highPrecisionFs = hp.getProperty(rt, "targetFs").asNumber();
				}
				
				// Segmentwise options
				if (optObj.hasProperty(rt, "segmentwise")) {
					auto seg = optObj.getProperty(rt, "segmentwise").asObject(rt);
					if (seg.hasProperty(rt, "width")) opt.segmentWidth = seg.getProperty(rt, "width").asNumber();
					if (seg.hasProperty(rt, "overlap")) opt.segmentOverlap = seg.getProperty(rt, "overlap").asNumber();
					if (seg.hasProperty(rt, "minSize")) opt.segmentMinSize = seg.getProperty(rt, "minSize").asNumber();
					if (seg.hasProperty(rt, "replaceOutliers")) opt.replaceOutliers = seg.getProperty(rt, "replaceOutliers").asBool();
				}
			}
			
			auto res = heartpy::analyzeSignal(signal, fs, opt);
			jsi::Object out(rt);
			
			// Basic metrics
			out.setProperty(rt, "bpm", res.bpm);
			jsi::Array ibi(rt, res.ibiMs.size());
			for (size_t i = 0; i < res.ibiMs.size(); ++i) ibi.setValueAtIndex(rt, i, res.ibiMs[i]);
			out.setProperty(rt, "ibiMs", ibi);
			
			jsi::Array rrList(rt, res.rrList.size());
			for (size_t i = 0; i < res.rrList.size(); ++i) rrList.setValueAtIndex(rt, i, res.rrList[i]);
			out.setProperty(rt, "rrList", rrList);
			
			jsi::Array peakList(rt, res.peakList.size());
			for (size_t i = 0; i < res.peakList.size(); ++i) peakList.setValueAtIndex(rt, i, res.peakList[i]);
			out.setProperty(rt, "peakList", peakList);
			
			// Time domain metrics
			out.setProperty(rt, "sdnn", res.sdnn);
			out.setProperty(rt, "rmssd", res.rmssd);
			out.setProperty(rt, "sdsd", res.sdsd);
			out.setProperty(rt, "pnn20", res.pnn20);
			out.setProperty(rt, "pnn50", res.pnn50);
			out.setProperty(rt, "nn20", res.nn20);
			out.setProperty(rt, "nn50", res.nn50);
			out.setProperty(rt, "mad", res.mad);
			
		// Poincare analysis
		out.setProperty(rt, "sd1", res.sd1);
		out.setProperty(rt, "sd2", res.sd2);
		out.setProperty(rt, "sd1sd2Ratio", res.sd1sd2Ratio);
		out.setProperty(rt, "ellipseArea", res.ellipseArea);
		// Binary quality mask & raw peaks
		jsi::Array peakListRaw(rt, res.peakListRaw.size());
		for (size_t i = 0; i < res.peakListRaw.size(); ++i) peakListRaw.setValueAtIndex(rt, i, res.peakListRaw[i]);
		out.setProperty(rt, "peakListRaw", peakListRaw);
		jsi::Array binaryPeakMask(rt, res.binaryPeakMask.size());
		for (size_t i = 0; i < res.binaryPeakMask.size(); ++i) binaryPeakMask.setValueAtIndex(rt, i, res.binaryPeakMask[i]);
		out.setProperty(rt, "binaryPeakMask", binaryPeakMask);
		jsi::Array binSegs(rt, res.binarySegments.size());
		for (size_t i = 0; i < res.binarySegments.size(); ++i) {
			const auto &bs = res.binarySegments[i];
			jsi::Object o(rt);
			o.setProperty(rt, "index", bs.index);
			o.setProperty(rt, "startBeat", bs.startBeat);
			o.setProperty(rt, "endBeat", bs.endBeat);
			o.setProperty(rt, "totalBeats", bs.totalBeats);
			o.setProperty(rt, "rejectedBeats", bs.rejectedBeats);
			o.setProperty(rt, "accepted", bs.accepted);
			binSegs.setValueAtIndex(rt, i, o);
		}
		out.setProperty(rt, "binarySegments", binSegs);
			
			// Frequency domain
			out.setProperty(rt, "vlf", res.vlf);
			out.setProperty(rt, "lf", res.lf);
			out.setProperty(rt, "hf", res.hf);
			out.setProperty(rt, "lfhf", res.lfhf);
			out.setProperty(rt, "totalPower", res.totalPower);
			out.setProperty(rt, "lfNorm", res.lfNorm);
			out.setProperty(rt, "hfNorm", res.hfNorm);
			
			// Breathing analysis
			out.setProperty(rt, "breathingRate", res.breathingRate);
			
		// Quality info
		jsi::Object quality(rt);
		quality.setProperty(rt, "totalBeats", res.quality.totalBeats);
		quality.setProperty(rt, "rejectedBeats", res.quality.rejectedBeats);
		quality.setProperty(rt, "rejectionRate", res.quality.rejectionRate);
		quality.setProperty(rt, "goodQuality", res.quality.goodQuality);
		// rejectedIndices (if available)
		{
			jsi::Array rej(rt, res.quality.rejectedIndices.size());
			for (size_t i = 0; i < res.quality.rejectedIndices.size(); ++i) rej.setValueAtIndex(rt, i, res.quality.rejectedIndices[i]);
			quality.setProperty(rt, "rejectedIndices", rej);
		}
			if (!res.quality.qualityWarning.empty()) {
				quality.setProperty(rt, "qualityWarning", jsi::String::createFromUtf8(rt, res.quality.qualityWarning));
			}
			out.setProperty(rt, "quality", quality);
			
			return out;
		});
	rt.global().setProperty(rt, "__HeartPyAnalyze", analyzeFunc);
	
	// Realtime analyzer create function
	auto rtCreateFunc = jsi::Function::createFromHostFunction(
		rt,
		jsi::PropNameID::forAscii(rt, "rtCreate"),
		2,
		[](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args, size_t count) -> jsi::Value {
			if (count < 1) {
				throw jsi::JSError(rt, "rtCreate() requires fs parameter");
			}
			double fs = args[0].asNumber();
			
    // Parse options if provided (full parsing via RN builder)
    heartpy::Options opt{};
    if (count > 1 && !args[1].isUndefined() && args[1].isObject()) {
        auto optObj = args[1].asObject(rt);
        const char* err_code = nullptr;
        std::string err_msg;
        try {
            opt = hp_build_options_from_jsi(rt, optObj, &err_code, &err_msg);
        } catch (const std::exception& e) {
            throw jsi::JSError(rt, e.what());
        } catch (...) {
            throw jsi::JSError(rt, "Unknown error building options");
        }
        if (err_code) {
            throw jsi::JSError(rt, err_msg);
        }
    }

    // Validate options centrally to avoid native crashes
    {
        const char* v_code = nullptr; std::string v_msg;
        if (!hp_validate_options(fs, opt, &v_code, &v_msg)) {
            throw jsi::JSError(rt, v_msg);
        }
    }
			
			// Create realtime analyzer instance
			auto analyzer = std::make_shared<heartpy::RealtimeAnalyzer>(fs, opt);
			
			// Create JSI object with analyzer methods
			jsi::Object rtObj(rt);
			
			// Store analyzer pointer (simplified - in production, use proper memory management)
			rtObj.setProperty(rt, "_ptr", jsi::Value((double)reinterpret_cast<uintptr_t>(analyzer.get())));
			rtObj.setProperty(rt, "fs", fs);
			
			// Add methods
			auto pushFunc = jsi::Function::createFromHostFunction(
				rt,
				jsi::PropNameID::forAscii(rt, "push"),
				1,
				[analyzer](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args, size_t count) -> jsi::Value {
					if (count < 1) return jsi::Value::undefined();
					
					if (args[0].isNumber()) {
						// Single sample
						double sample = args[0].asNumber();
						std::vector<double> samples = {sample};
						analyzer->push(samples);
					} else if (args[0].isObject()) {
						// Array of samples
						auto arrObj = args[0].asObject(rt);
						size_t len = (size_t)arrObj.getProperty(rt, "length").asNumber();
						std::vector<double> samples;
						samples.reserve(len);
						for (size_t i = 0; i < len; ++i) {
							samples.push_back(arrObj.getProperty(rt, jsi::PropNameID::forUtf8(rt, std::to_string(i))).asNumber());
						}
						analyzer->push(samples);
					}
					return jsi::Value::undefined();
				});
			rtObj.setProperty(rt, "push", pushFunc);
			
			auto pollFunc = jsi::Function::createFromHostFunction(
				rt,
				jsi::PropNameID::forAscii(rt, "poll"),
				0,
				[analyzer](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args, size_t count) -> jsi::Value {
					heartpy::HeartMetrics result;
					bool hasUpdate = analyzer->poll(result);
					
					jsi::Object out(rt);
					out.setProperty(rt, "hasUpdate", hasUpdate);
					if (hasUpdate) {
						out.setProperty(rt, "bpm", result.bpm);

						auto makeArray = [&rt](const std::vector<double>& vec) {
							jsi::Array arr(rt, vec.size());
							for (size_t i = 0; i < vec.size(); ++i) arr.setValueAtIndex(rt, i, vec[i]);
							return arr;
						};
						auto makeArrayInt = [&rt](const std::vector<int>& vec) {
							jsi::Array arr(rt, vec.size());
							for (size_t i = 0; i < vec.size(); ++i) arr.setValueAtIndex(rt, i, vec[i]);
							return arr;
						};
						out.setProperty(rt, "ibiMs", makeArray(result.ibiMs));
						out.setProperty(rt, "rrList", makeArray(result.rrList));
						out.setProperty(rt, "peakList", makeArrayInt(result.peakList));
						out.setProperty(rt, "peakListRaw", makeArrayInt(result.peakListRaw));
						out.setProperty(rt, "peakTimestamps", makeArray(result.peakTimestamps));
						out.setProperty(rt, "waveform_values", makeArray(result.waveform_values));
						out.setProperty(rt, "waveform_timestamps", makeArray(result.waveform_timestamps));
						
						// Quality info as object
						jsi::Object quality(rt);
						quality.setProperty(rt, "totalBeats", result.quality.totalBeats);
						quality.setProperty(rt, "rejectedBeats", result.quality.rejectedBeats);
						quality.setProperty(rt, "rejectionRate", result.quality.rejectionRate);
						quality.setProperty(rt, "goodQuality", result.quality.goodQuality);
						quality.setProperty(rt, "confidence", result.quality.confidence);
						quality.setProperty(rt, "snrDb", result.quality.snrDb);
						quality.setProperty(rt, "qualityWarning", jsi::String::createFromUtf8(rt, result.quality.qualityWarning));
						out.setProperty(rt, "quality", quality);
					}
					return out;
				});
			rtObj.setProperty(rt, "poll", pollFunc);
			
			return rtObj;
		});
	rt.global().setProperty(rt, "rtCreate", rtCreateFunc);
}

RCT_EXPORT_MODULE();

- (BOOL)requiresMainQueueSetup { return YES; }

static heartpy::Options optionsFromNSDictionary(NSDictionary* optDict) {
    heartpy::Options opt;
    if (!optDict) return opt;
    NSDictionary* bp = optDict[@"bandpass"];
    if ([bp isKindOfClass:[NSDictionary class]]) {
        if (bp[@"lowHz"]) opt.lowHz = [bp[@"lowHz"] doubleValue];
        if (bp[@"highHz"]) opt.highHz = [bp[@"highHz"] doubleValue];
        if (bp[@"order"]) opt.iirOrder = [bp[@"order"] intValue];
    }
    NSDictionary* w = optDict[@"welch"];
    if ([w isKindOfClass:[NSDictionary class]]) {
        if (w[@"nfft"]) opt.nfft = [w[@"nfft"] intValue];
        if (w[@"overlap"]) opt.overlap = [w[@"overlap"] doubleValue];
        if (w[@"wsizeSec"]) opt.welchWsizeSec = [w[@"wsizeSec"] doubleValue];
    }
    NSDictionary* p = optDict[@"peak"];
    if ([p isKindOfClass:[NSDictionary class]]) {
        if (p[@"refractoryMs"]) opt.refractoryMs = [p[@"refractoryMs"] doubleValue];
        if (p[@"thresholdScale"]) opt.thresholdScale = [p[@"thresholdScale"] doubleValue];
        if (p[@"bpmMin"]) opt.bpmMin = [p[@"bpmMin"] doubleValue];
        if (p[@"bpmMax"]) opt.bpmMax = [p[@"bpmMax"] doubleValue];
    }
    NSDictionary* prep = optDict[@"preprocessing"];
    if ([prep isKindOfClass:[NSDictionary class]]) {
        if (prep[@"interpClipping"]) opt.interpClipping = [prep[@"interpClipping"] boolValue];
        if (prep[@"clippingThreshold"]) opt.clippingThreshold = [prep[@"clippingThreshold"] doubleValue];
        if (prep[@"hampelCorrect"]) opt.hampelCorrect = [prep[@"hampelCorrect"] boolValue];
        if (prep[@"hampelWindow"]) opt.hampelWindow = [prep[@"hampelWindow"] intValue];
        if (prep[@"hampelThreshold"]) opt.hampelThreshold = [prep[@"hampelThreshold"] doubleValue];
        if (prep[@"removeBaselineWander"]) opt.removeBaselineWander = [prep[@"removeBaselineWander"] boolValue];
        if (prep[@"enhancePeaks"]) opt.enhancePeaks = [prep[@"enhancePeaks"] boolValue];
    }
    NSDictionary* qual = optDict[@"quality"];
    if ([qual isKindOfClass:[NSDictionary class]]) {
        if (qual[@"rejectSegmentwise"]) opt.rejectSegmentwise = [qual[@"rejectSegmentwise"] boolValue];
        if (qual[@"segmentRejectThreshold"]) opt.segmentRejectThreshold = [qual[@"segmentRejectThreshold"] doubleValue];
        if (qual[@"segmentRejectMaxRejects"]) opt.segmentRejectMaxRejects = [qual[@"segmentRejectMaxRejects"] intValue];
        if (qual[@"segmentRejectWindowBeats"]) opt.segmentRejectWindowBeats = [qual[@"segmentRejectWindowBeats"] intValue];
        if (qual[@"segmentRejectOverlap"]) opt.segmentRejectOverlap = [qual[@"segmentRejectOverlap"] doubleValue];
        if (qual[@"cleanRR"]) opt.cleanRR = [qual[@"cleanRR"] boolValue];
        if (qual[@"thresholdRR"]) opt.thresholdRR = [qual[@"thresholdRR"] boolValue];
        if ([qual[@"cleanMethod"] isKindOfClass:[NSString class]]) {
            NSString* method = (NSString*)qual[@"cleanMethod"];
            if ([method isEqualToString:@"iqr"]) opt.cleanMethod = heartpy::Options::CleanMethod::IQR;
            else if ([method isEqualToString:@"z-score"]) opt.cleanMethod = heartpy::Options::CleanMethod::Z_SCORE;
            else opt.cleanMethod = heartpy::Options::CleanMethod::QUOTIENT_FILTER;
        }
    }
    NSDictionary* td = optDict[@"timeDomain"];
    if ([td isKindOfClass:[NSDictionary class]]) {
        if ([td[@"sdsdMode"] isKindOfClass:[NSString class]]) {
            NSString* m = (NSString*)td[@"sdsdMode"];
            if ([m isEqualToString:@"signed"]) opt.sdsdMode = heartpy::Options::SdsdMode::SIGNED; else opt.sdsdMode = heartpy::Options::SdsdMode::ABS;
        }
        if (td[@"pnnAsPercent"]) opt.pnnAsPercent = [td[@"pnnAsPercent"] boolValue];
    }
    NSDictionary* pc = optDict[@"poincare"];
    if ([pc isKindOfClass:[NSDictionary class]]) {
        if ([pc[@"mode"] isKindOfClass:[NSString class]]) {
            NSString* m = (NSString*)pc[@"mode"];
            if ([m isEqualToString:@"masked"]) opt.poincareMode = heartpy::Options::PoincareMode::MASKED; else opt.poincareMode = heartpy::Options::PoincareMode::FORMULA;
        }
    }
    NSDictionary* hp = optDict[@"highPrecision"];
    if ([hp isKindOfClass:[NSDictionary class]]) {
        if (hp[@"enabled"]) opt.highPrecision = [hp[@"enabled"] boolValue];
        if (hp[@"targetFs"]) opt.highPrecisionFs = [hp[@"targetFs"] doubleValue];
    }
    NSDictionary* rr = optDict[@"rrSpline"];
    if ([rr isKindOfClass:[NSDictionary class]]) {
        if (rr[@"s"]) opt.rrSplineS = [rr[@"s"] doubleValue];
        if (rr[@"targetSse"]) opt.rrSplineSTargetSse = [rr[@"targetSse"] doubleValue];
        if (rr[@"smooth"]) opt.rrSplineSmooth = [rr[@"smooth"] doubleValue];
    }
    NSDictionary* seg = optDict[@"segmentwise"];
    if ([seg isKindOfClass:[NSDictionary class]]) {
        if (seg[@"width"]) opt.segmentWidth = [seg[@"width"] doubleValue];
        if (seg[@"overlap"]) opt.segmentOverlap = [seg[@"overlap"] doubleValue];
        if (seg[@"minSize"]) opt.segmentMinSize = [seg[@"minSize"] doubleValue];
        if (seg[@"replaceOutliers"]) opt.replaceOutliers = [seg[@"replaceOutliers"] boolValue];
    }
    if (optDict[@"breathingAsBpm"]) opt.breathingAsBpm = [optDict[@"breathingAsBpm"] boolValue];
    // Global FD toggle (calc_freq parity)
    if (optDict[@"calcFreq"]) opt.calcFreq = [optDict[@"calcFreq"] boolValue];
    if (optDict[@"snrTauSec"]) opt.snrTauSec = [optDict[@"snrTauSec"] doubleValue];
    if (optDict[@"snrActiveTauSec"]) opt.snrActiveTauSec = [optDict[@"snrActiveTauSec"] doubleValue];
    if (optDict[@"adaptivePsd"]) opt.adaptivePsd = [optDict[@"adaptivePsd"] boolValue];
    NSDictionary* filt = optDict[@"filter"];
    if ([filt isKindOfClass:[NSDictionary class]]) {
        id mode = filt[@"mode"];
        if ([mode isKindOfClass:[NSString class]]) {
            NSString* m = (NSString*)mode;
            if ([m isEqualToString:@"rbj"]) opt.filterMode = heartpy::Options::FilterMode::RBJ;
            else if ([m isEqualToString:@"butter"] || [m isEqualToString:@"butter-filtfilt"]) opt.filterMode = heartpy::Options::FilterMode::BUTTER_FILTFILT;
            else opt.filterMode = heartpy::Options::FilterMode::AUTO;
        }
        if (filt[@"order"]) opt.iirOrder = [filt[@"order"] intValue];
    }
    return opt;
}

// Synchronous bridge method to align with Android/TypeScript usage
RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(analyze:(NSArray<NSNumber*>*)signal
                                    fs:(nonnull NSNumber*)fs
                                    options:(NSDictionary*)options)
{
    std::vector<double> x; x.reserve(signal.count);
    for (NSNumber* n in signal) x.push_back([n doubleValue]);
    heartpy::Options opt = optionsFromNSDictionary(options);
    auto res = heartpy::analyzeSignal(x, [fs doubleValue], opt);

    NSMutableDictionary* out = [NSMutableDictionary new];
    out[@"bpm"] = @(res.bpm);
    // Arrays
    NSMutableArray* ibi = [NSMutableArray arrayWithCapacity:res.ibiMs.size()];
    for (double v : res.ibiMs) [ibi addObject:@(v)];
    out[@"ibiMs"] = ibi;
    NSMutableArray* rr = [NSMutableArray arrayWithCapacity:res.rrList.size()];
    for (double v : res.rrList) [rr addObject:@(v)];
    out[@"rrList"] = rr;
    NSMutableArray* peaks = [NSMutableArray arrayWithCapacity:res.peakList.size()];
    for (int idx : res.peakList) [peaks addObject:@(idx)];
    out[@"peakList"] = peaks;
    NSMutableArray* peakListRaw = [NSMutableArray arrayWithCapacity:res.peakListRaw.size()];
    for (int idx : res.peakListRaw) [peakListRaw addObject:@(idx)];
    out[@"peakListRaw"] = peakListRaw;
    NSMutableArray* binaryMask = [NSMutableArray arrayWithCapacity:res.binaryPeakMask.size()];
    for (int idx : res.binaryPeakMask) [binaryMask addObject:@(idx)];
    out[@"binaryPeakMask"] = binaryMask;
    NSMutableArray* peakTs = [NSMutableArray arrayWithCapacity:res.peakTimestamps.size()];
    for (double t : res.peakTimestamps) [peakTs addObject:@(t)];
    out[@"peakTimestamps"] = peakTs;
    NSMutableArray* waveVals = [NSMutableArray arrayWithCapacity:res.waveform_values.size()];
    for (double v : res.waveform_values) [waveVals addObject:@(v)];
    out[@"waveform_values"] = waveVals;
    NSMutableArray* waveTs = [NSMutableArray arrayWithCapacity:res.waveform_timestamps.size()];
    for (double t : res.waveform_timestamps) [waveTs addObject:@(t)];
    out[@"waveform_timestamps"] = waveTs;
    // Time domain
    out[@"sdnn"] = @(res.sdnn);
    out[@"rmssd"] = @(res.rmssd);
    out[@"sdsd"] = @(res.sdsd);
    out[@"pnn20"] = @(res.pnn20);
    out[@"pnn50"] = @(res.pnn50);
    out[@"nn20"] = @(res.nn20);
    out[@"nn50"] = @(res.nn50);
    out[@"mad"] = @(res.mad);
    // Poincare
    out[@"sd1"] = @(res.sd1);
    out[@"sd2"] = @(res.sd2);
    out[@"sd1sd2Ratio"] = @(res.sd1sd2Ratio);
    out[@"ellipseArea"] = @(res.ellipseArea);
    // Frequency domain
    out[@"vlf"] = @(res.vlf);
    out[@"lf"] = @(res.lf);
    out[@"hf"] = @(res.hf);
    out[@"lfhf"] = @(res.lfhf);
    out[@"totalPower"] = @(res.totalPower);
    out[@"lfNorm"] = @(res.lfNorm);
    out[@"hfNorm"] = @(res.hfNorm);
    // Breathing
    out[@"breathingRate"] = @(res.breathingRate);
    // Quality
    NSMutableDictionary* q = [NSMutableDictionary new];
    q[@"totalBeats"] = @(res.quality.totalBeats);
    q[@"rejectedBeats"] = @(res.quality.rejectedBeats);
    q[@"rejectionRate"] = @(res.quality.rejectionRate);
    q[@"goodQuality"] = @(res.quality.goodQuality);
    q[@"snrDb"] = @(res.quality.snrDb);
    q[@"confidence"] = @(res.quality.confidence);
    q[@"f0Hz"] = @(res.quality.f0Hz);
    q[@"maPercActive"] = @(res.quality.maPercActive);
    q[@"doublingFlag"] = @(res.quality.doublingFlag);
    q[@"softDoublingFlag"] = @(res.quality.softDoublingFlag);
    q[@"doublingHintFlag"] = @(res.quality.doublingHintFlag);
    q[@"hardFallbackActive"] = @(res.quality.hardFallbackActive);
    q[@"rrFallbackModeActive"] = @(res.quality.rrFallbackModeActive);
    q[@"snrWarmupActive"] = @(res.quality.snrWarmupActive);
    q[@"snrSampleCount"] = @(res.quality.snrSampleCount);
    q[@"refractoryMsActive"] = @(res.quality.refractoryMsActive);
    q[@"minRRBoundMs"] = @(res.quality.minRRBoundMs);
    q[@"pairFrac"] = @(res.quality.pairFrac);
    q[@"rrShortFrac"] = @(res.quality.rrShortFrac);
    q[@"rrLongMs"] = @(res.quality.rrLongMs);
    q[@"pHalfOverFund"] = @(res.quality.pHalfOverFund);
    if (!res.quality.qualityWarning.empty()) {
        q[@"qualityWarning"] = [NSString stringWithUTF8String:res.quality.qualityWarning.c_str()];
    }
    out[@"quality"] = q;
    NSMutableArray* binarySegments = [NSMutableArray arrayWithCapacity:res.binarySegments.size()];
    for (const auto& seg : res.binarySegments) {
        [binarySegments addObject:@{
            @"index": @(seg.index),
            @"startBeat": @(seg.startBeat),
            @"endBeat": @(seg.endBeat),
            @"totalBeats": @(seg.totalBeats),
            @"rejectedBeats": @(seg.rejectedBeats),
            @"accepted": @(seg.accepted)
        }];
    }
    out[@"binarySegments"] = binarySegments;
    return out;
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(analyzeTyped:(NSArray<NSNumber*>*)signal
                                     fs:(nonnull NSNumber*)fs
                                     options:(NSDictionary*)options)
{
    return [self analyze:signal fs:fs options:options];
}

RCT_EXPORT_METHOD(analyzeAsyncTyped:(NSArray<NSNumber*>*)signal
                  fs:(nonnull NSNumber*)fs
                  options:(NSDictionary*)options
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    [self analyzeAsync:signal fs:fs options:options resolver:resolve rejecter:reject];
}

// Typed segmentwise/rr wrappers (NSDictionary already typed)
RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(analyzeSegmentwiseTyped:(NSArray<NSNumber*>*)signal
                                     fs:(nonnull NSNumber*)fs
                                     options:(NSDictionary*)options)
{
    return [self analyzeSegmentwise:signal fs:fs options:options];
}

RCT_EXPORT_METHOD(analyzeSegmentwiseAsyncTyped:(NSArray<NSNumber*>*)signal
                  fs:(nonnull NSNumber*)fs
                  options:(NSDictionary*)options
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    [self analyzeSegmentwiseAsync:signal fs:fs options:options resolver:resolve rejecter:reject];
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(analyzeRRTyped:(NSArray<NSNumber*>*)rr
                                    options:(NSDictionary*)options)
{
    return [self analyzeRR:rr options:options];
}

RCT_EXPORT_METHOD(analyzeRRAsyncTyped:(NSArray<NSNumber*>*)rr
                  options:(NSDictionary*)options
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    [self analyzeRRAsync:rr options:options resolver:resolve rejecter:reject];
}

// Async Promise-based variants to avoid blocking the JS thread
RCT_EXPORT_METHOD(analyzeAsync:(NSArray<NSNumber*>*)signal
                  fs:(nonnull NSNumber*)fs
                  options:(NSDictionary*)options
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        @try {
            id res = [self analyze:signal fs:fs options:options];
            resolve(res);
        } @catch (NSException* e) {
            reject(@"analyze_error", e.reason, nil);
        }
    });
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(analyzeRR:(NSArray<NSNumber*>*)rr
                                    options:(NSDictionary*)options)
{
    std::vector<double> rrms; rrms.reserve(rr.count);
    for (NSNumber* n in rr) rrms.push_back([n doubleValue]);
    heartpy::Options opt = optionsFromNSDictionary(options);
    auto res = heartpy::analyzeRRIntervals(rrms, opt);
    NSMutableDictionary* out = [NSMutableDictionary new];
    out[@"bpm"] = @(res.bpm);
    // Arrays
    NSMutableArray* rrList = [NSMutableArray arrayWithCapacity:res.rrList.size()];
    for (double v : res.rrList) [rrList addObject:@(v)];
    out[@"rrList"] = rrList;
    // Time domain & poincare
    out[@"sdnn"] = @(res.sdnn);
    out[@"rmssd"] = @(res.rmssd);
    out[@"sdsd"] = @(res.sdsd);
    out[@"pnn20"] = @(res.pnn20);
    out[@"pnn50"] = @(res.pnn50);
    out[@"nn20"] = @(res.nn20);
    out[@"nn50"] = @(res.nn50);
    out[@"mad"] = @(res.mad);
    out[@"sd1"] = @(res.sd1);
    out[@"sd2"] = @(res.sd2);
    out[@"sd1sd2Ratio"] = @(res.sd1sd2Ratio);
    out[@"ellipseArea"] = @(res.ellipseArea);
    out[@"breathingRate"] = @(res.breathingRate);
    return out;
}

RCT_EXPORT_METHOD(analyzeRRAsync:(NSArray<NSNumber*>*)rr
                  options:(NSDictionary*)options
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        @try {
            id res = [self analyzeRR:rr options:options];
            resolve(res);
        } @catch (NSException* e) {
            reject(@"analyzeRR_error", e.reason, nil);
        }
    });
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(analyzeSegmentwise:(NSArray<NSNumber*>*)signal
                                      fs:(nonnull NSNumber*)fs
                                      options:(NSDictionary*)options)
{
    std::vector<double> x; x.reserve(signal.count);
    for (NSNumber* n in signal) x.push_back([n doubleValue]);
    heartpy::Options opt = optionsFromNSDictionary(options);
    auto res = heartpy::analyzeSignalSegmentwise(x, [fs doubleValue], opt);
    NSMutableDictionary* out = [NSMutableDictionary new];
    out[@"bpm"] = @(res.bpm);
    out[@"sdnn"] = @(res.sdnn);
    out[@"rmssd"] = @(res.rmssd);
    NSMutableArray* segs = [NSMutableArray new];
    for (const auto& s : res.segments) {
        NSMutableDictionary* d = [NSMutableDictionary new];
        d[@"bpm"] = @(s.bpm);
        d[@"sdnn"] = @(s.sdnn);
        d[@"rmssd"] = @(s.rmssd);
        [segs addObject:d];
    }
    out[@"segments"] = segs;
    return out;
}

RCT_EXPORT_METHOD(analyzeSegmentwiseAsync:(NSArray<NSNumber*>*)signal
                  fs:(nonnull NSNumber*)fs
                  options:(NSDictionary*)options
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        @try {
            id res = [self analyzeSegmentwise:signal fs:fs options:options];
            resolve(res);
        } @catch (NSException* e) {
            reject(@"analyzeSegmentwise_error", e.reason, nil);
        }
    });
}

// Preprocessing exports
RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(interpolateClipping:(NSArray<NSNumber*>*)signal
                                      fs:(nonnull NSNumber*)fs
                                      threshold:(nonnull NSNumber*)threshold)
{
    std::vector<double> x; x.reserve(signal.count);
    for (NSNumber* n in signal) x.push_back([n doubleValue]);
    auto y = heartpy::interpolateClipping(x, [fs doubleValue], [threshold doubleValue]);
    NSMutableArray* out = [NSMutableArray arrayWithCapacity:y.size()];
    for (double v : y) [out addObject:@(v)];
    return out;
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(hampelFilter:(NSArray<NSNumber*>*)signal
                                      windowSize:(nonnull NSNumber*)windowSize
                                      threshold:(nonnull NSNumber*)threshold)
{
    std::vector<double> x; x.reserve(signal.count);
    for (NSNumber* n in signal) x.push_back([n doubleValue]);
    auto y = heartpy::hampelFilter(x, [windowSize intValue], [threshold doubleValue]);
    NSMutableArray* out = [NSMutableArray arrayWithCapacity:y.size()];
    for (double v : y) [out addObject:@(v)];
    return out;
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(scaleData:(NSArray<NSNumber*>*)signal
                                   newMin:(nonnull NSNumber*)newMin
                                   newMax:(nonnull NSNumber*)newMax)
{
    std::vector<double> x; x.reserve(signal.count);
    for (NSNumber* n in signal) x.push_back([n doubleValue]);
    auto y = heartpy::scaleData(x, [newMin doubleValue], [newMax doubleValue]);
    NSMutableArray* out = [NSMutableArray arrayWithCapacity:y.size()];
    for (double v : y) [out addObject:@(v)];
    return out;
}

// JSI installation method - called from bridge
- (void)installJSIBindingsWithRuntime:(void*)runtime {
	NSLog(@"[HeartPyModule] installJSIBindingsWithRuntime called with runtime: %p", runtime);
	
	if (runtime) {
		try {
			jsi::Runtime* jsiRuntime = (jsi::Runtime*)runtime;
			installBinding(*jsiRuntime);
			NSLog(@"[HeartPyModule] JSI bindings installed successfully!");
		} catch (const std::exception& e) {
			NSLog(@"[HeartPyModule] ERROR installing JSI bindings: %s", e.what());
		} catch (...) {
			NSLog(@"[HeartPyModule] ERROR: Unknown exception installing JSI bindings");
		}
	} else {
		NSLog(@"[HeartPyModule] ERROR: Runtime is null");
	}
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(installJSI)
{
	NSLog(@"[HeartPyModule] installJSI called - React Native 0.74+ has JSI limitations");
	
	// In RN 0.74+, JSI runtime access is restricted
	// For now, return NO to use NativeModule path which works perfectly
	NSLog(@"[HeartPyModule] Using NativeModule fallback (recommended for RN 0.74+)");
	return @NO;
}

// MARK: - Realtime Streaming (NativeModules P0)

// Create realtime analyzer and return opaque handle (as number)
RCT_EXPORT_METHOD(rtCreate:(double)fs
                  options:(NSDictionary *)options
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    @try {
        if (fs <= 0.0) { reject(@"HEARTPY_E001", @"Invalid sample rate: must be 1-10000 Hz", nil); return; }
        heartpy::Options opt = optionsFromNSDictionary(options);
        // Validate options centrally
        const char* code = nullptr; std::string msg;
        if (!hp_validate_options(fs, opt, &code, &msg)) {
            NSString* nscode = code ? [NSString stringWithUTF8String:code] : @"HEARTPY_E015";
            NSString* nsmsg = [NSString stringWithUTF8String:msg.c_str()];
            reject(nscode, nsmsg, nil);
            return;
        }
        void* handle = hp_rt_create(fs, &opt);
        if (!handle) { reject(@"HEARTPY_E004", @"hp_rt_create returned null", nil); return; }
        resolve(@((long)handle));
    } @catch (NSException* e) {
        reject(@"HEARTPY_E900", e.reason, nil);
    }
}

RCT_EXPORT_METHOD(rtSetWindow:(nonnull NSNumber*)handle
                  windowSeconds:(nonnull NSNumber*)windowSeconds
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    @try {
        if (handle == nil || windowSeconds == nil) {
            reject(@"rt_set_window_invalid_args", @"Invalid handle or windowSeconds", nil);
            return;
        }
        hp_rt_set_window((void*)[handle longValue], [windowSeconds doubleValue]);
        resolve(nil);
    } @catch (NSException* e) {
        reject(@"rt_set_window_exception", e.reason, nil);
    }
}

// Push a chunk of samples (number[])
RCT_EXPORT_METHOD(rtPush:(nonnull NSNumber*)handle
                  samples:(NSArray<NSNumber*>*)samples
                  timestamp:(nonnull NSNumber*)t0
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    @try {
        if (handle == nil || samples == nil || samples.count == 0) { reject(@"rt_push_invalid_args", @"Invalid handle or empty samples", nil); return; }
        void* h = (void*)[handle longValue];
        const NSUInteger n = samples.count;
        std::vector<float> x; x.reserve(n);
        for (NSNumber* v in samples) x.push_back([v floatValue]);
        double ts0 = (t0 && [t0 doubleValue] != 0) ? [t0 doubleValue] : [[NSDate date] timeIntervalSince1970];
        hp_rt_push(h, x.data(), (size_t)x.size(), ts0);
        resolve(nil);
    } @catch (NSException* e) {
        reject(@"rt_push_exception", e.reason, nil);
    }
}

// ✅ CRITICAL P0 FIX: Push samples with per-sample timestamps
RCT_EXPORT_METHOD(rtPushTs:(nonnull NSNumber*)handle
                  samples:(NSArray<NSNumber*>*)xs
                  timestamps:(NSArray<NSNumber*>*)ts
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    @try {
        if (handle == nil || xs == nil || ts == nil || xs.count != ts.count || xs.count == 0) {
            reject(@"rt_push_ts_invalid_args", @"Invalid handle or mismatched arrays", nil);
            return;
        }
        
        void* h = (void*)[handle longValue];
        const NSUInteger n = xs.count;
        
        std::vector<float> samples; samples.reserve(n);
        for (NSNumber* v in xs) samples.push_back([v floatValue]);
        
        std::vector<double> timestamps; timestamps.reserve(n);
        for (NSNumber* t in ts) timestamps.push_back([t doubleValue]);
        
        hp_rt_push_ts(h, samples.data(), timestamps.data(), (size_t)n);
        resolve(nil);
    } @catch (NSException* e) {
        reject(@"rt_push_ts_exception", e.reason, nil);
    }
}

// Poll for latest metrics; returns object or null
RCT_EXPORT_METHOD(rtPoll:(nonnull NSNumber*)handle
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    @try {
        if (handle == nil) { reject(@"rt_poll_invalid_args", @"Invalid handle", nil); return; }
        void* h = (void*)[handle longValue];
        heartpy::HeartMetrics res;
        if (!hp_rt_poll(h, &res)) { resolve(nil); return; }

        NSMutableDictionary* out = [NSMutableDictionary new];
        out[@"bpm"] = @(res.bpm);
        // Arrays
        {
            NSMutableArray* ibi = [NSMutableArray arrayWithCapacity:res.ibiMs.size()];
            for (double v : res.ibiMs) [ibi addObject:@(v)];
            out[@"ibiMs"] = ibi;
            NSMutableArray* rr = [NSMutableArray arrayWithCapacity:res.rrList.size()];
            for (double v : res.rrList) [rr addObject:@(v)];
            out[@"rrList"] = rr;
            NSMutableArray* peaks = [NSMutableArray arrayWithCapacity:res.peakList.size()];
            for (int idx : res.peakList) [peaks addObject:@(idx)];
            out[@"peakList"] = peaks;
            // Peak timestamps (if native provided)
            NSMutableArray* peakTs = [NSMutableArray arrayWithCapacity:res.peakTimestamps.size()];
            for (double t : res.peakTimestamps) [peakTs addObject:@(t)];
            out[@"peakTimestamps"] = peakTs;
            NSMutableArray* waveVals = [NSMutableArray arrayWithCapacity:res.waveform_values.size()];
            for (double v : res.waveform_values) [waveVals addObject:@(v)];
            out[@"waveform_values"] = waveVals;
            NSMutableArray* waveTs = [NSMutableArray arrayWithCapacity:res.waveform_timestamps.size()];
            for (double t : res.waveform_timestamps) [waveTs addObject:@(t)];
            out[@"waveform_timestamps"] = waveTs;
        }
        // Time domain
        out[@"sdnn"] = @(res.sdnn);
        out[@"rmssd"] = @(res.rmssd);
        out[@"sdsd"] = @(res.sdsd);
        out[@"pnn20"] = @(res.pnn20);
        out[@"pnn50"] = @(res.pnn50);
        out[@"nn20"] = @(res.nn20);
        out[@"nn50"] = @(res.nn50);
        out[@"mad"] = @(res.mad);
        // Poincaré
        out[@"sd1"] = @(res.sd1);
        out[@"sd2"] = @(res.sd2);
        out[@"sd1sd2Ratio"] = @(res.sd1sd2Ratio);
        out[@"ellipseArea"] = @(res.ellipseArea);
        // Frequency domain
        out[@"vlf"] = @(res.vlf);
        out[@"lf"] = @(res.lf);
        out[@"hf"] = @(res.hf);
        out[@"lfhf"] = @(res.lfhf);
        out[@"totalPower"] = @(res.totalPower);
        out[@"lfNorm"] = @(res.lfNorm);
        out[@"hfNorm"] = @(res.hfNorm);
        // Breathing
        out[@"breathingRate"] = @(res.breathingRate);
        // Quality
        {
            NSMutableDictionary* q = [NSMutableDictionary new];
            q[@"totalBeats"] = @(res.quality.totalBeats);
            q[@"rejectedBeats"] = @(res.quality.rejectedBeats);
            q[@"rejectionRate"] = @(res.quality.rejectionRate);
            q[@"goodQuality"] = @(res.quality.goodQuality);
            // Streaming quality fields (if available)
            q[@"snrDb"] = @(res.quality.snrDb);
            q[@"confidence"] = @(res.quality.confidence);
            q[@"f0Hz"] = @(res.quality.f0Hz);
            q[@"maPercActive"] = @(res.quality.maPercActive);
            q[@"doublingFlag"] = @(res.quality.doublingFlag);
            q[@"softDoublingFlag"] = @(res.quality.softDoublingFlag);
            q[@"doublingHintFlag"] = @(res.quality.doublingHintFlag);
            q[@"hardFallbackActive"] = @(res.quality.hardFallbackActive);
            q[@"rrFallbackModeActive"] = @(res.quality.rrFallbackModeActive);
            q[@"snrWarmupActive"] = @(res.quality.snrWarmupActive);
            q[@"snrSampleCount"] = @(res.quality.snrSampleCount);
            q[@"refractoryMsActive"] = @(res.quality.refractoryMsActive);
            q[@"minRRBoundMs"] = @(res.quality.minRRBoundMs);
            q[@"pairFrac"] = @(res.quality.pairFrac);
            q[@"rrShortFrac"] = @(res.quality.rrShortFrac);
            q[@"rrLongMs"] = @(res.quality.rrLongMs);
            q[@"pHalfOverFund"] = @(res.quality.pHalfOverFund);
            if (!res.quality.qualityWarning.empty()) {
                q[@"qualityWarning"] = [NSString stringWithUTF8String:res.quality.qualityWarning.c_str()];
            }
            out[@"quality"] = q;
        }
        // P1 FIX: Add peakListRaw and remove faulty windowStartAbs calculation
        {
            NSMutableArray* peakListRaw = [NSMutableArray arrayWithCapacity:res.peakListRaw.size()];
            for (int idx : res.peakListRaw) [peakListRaw addObject:@(idx)];
            out[@"peakListRaw"] = peakListRaw;
            
            // P1 FIX: Remove faulty windowStartAbs calculation
            // The previous heuristic (peakListRaw.size() - 150) was incorrect and caused overflow
            // For now, set to 0 to indicate early detection phase
            // In production, the native core should provide the actual window start
            double windowStartAbs = 0.0; // Default for early detection
            out[@"windowStartAbs"] = @(windowStartAbs);
        }
        // Binary segments (if any)
        {
            NSMutableArray* segs = [NSMutableArray arrayWithCapacity:res.binarySegments.size()];
            for (const auto& s : res.binarySegments) {
                NSMutableDictionary* d = [NSMutableDictionary new];
                d[@"index"] = @(s.index);
                d[@"startBeat"] = @(s.startBeat);
                d[@"endBeat"] = @(s.endBeat);
                d[@"totalBeats"] = @(s.totalBeats);
                d[@"rejectedBeats"] = @(s.rejectedBeats);
                d[@"accepted"] = @(s.accepted);
                [segs addObject:d];
            }
            out[@"binarySegments"] = segs;
        }
        resolve(out);
    } @catch (NSException* e) {
        reject(@"rt_poll_exception", e.reason, nil);
    }
}

// Destroy analyzer and release native resources
RCT_EXPORT_METHOD(rtDestroy:(nonnull NSNumber*)handle
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    @try {
        if (handle == nil) { reject(@"rt_destroy_invalid_args", @"Invalid handle", nil); return; }
        void* h = (void*)[handle longValue];
        hp_rt_destroy(h);
        resolve(nil);
    } @catch (NSException* e) {
        reject(@"rt_destroy_exception", e.reason, nil);
    }
}

@end
