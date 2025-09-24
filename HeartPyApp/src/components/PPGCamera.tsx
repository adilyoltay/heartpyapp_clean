import React, {useCallback, useEffect, useMemo, useRef, useState} from 'react';
import {
  NativeEventEmitter,
  NativeModules,
  Platform,
  StyleSheet,
  Text,
  View,
} from 'react-native';
import {
  Camera,
  type Frame,
  type FrameProcessorPlugin,
  useCameraDevice,
  useCameraPermission,
  useFrameProcessor,
  VisionCameraProxy,
} from 'react-native-vision-camera';
import {PPG_CONFIG} from '../core/PPGConfig';
import type {PPGSample} from '../types/PPGTypes';

const {PPGCameraManager} = NativeModules;

type Props = {
  onSample: (sample: PPGSample) => Promise<void>;
  isActive: boolean;
  onFpsUpdate?: (fps: number) => void; // FPS callback for dynamic sampleRate
  hidden?: boolean;
};

export function PPGCamera({
  onSample,
  isActive,
  onFpsUpdate,
  hidden = false,
}: Props): JSX.Element {
  const device = useCameraDevice('back');
  const {hasPermission, requestPermission} = useCameraPermission();
  const enableProcessorTimerRef = useRef<NodeJS.Timeout | null>(null);
  const torchTimerRef = useRef<NodeJS.Timeout | null>(null);
  const cameraRef = useRef<Camera | null>(null);
  const [androidTorchMode, setAndroidTorchMode] = useState<'off' | 'on'>('off');
  const requireTorch = PPG_CONFIG.ppgChannel === 'red';

  // FPS Monitoring
  const frameTimestamps = useRef<number[]>([]);
  const fpsRef = useRef<number>(0);

  // Calculate FPS from frame timestamps
  const calculateFPS = useCallback(
    (timestamp: number) => {
      frameTimestamps.current.push(timestamp);

      // Keep only last 30 timestamps (1 second at 30fps)
      if (frameTimestamps.current.length > 30) {
        frameTimestamps.current.shift();
      }

      // Calculate FPS if we have enough samples
      if (frameTimestamps.current.length >= 10) {
        const timeSpan =
          frameTimestamps.current[frameTimestamps.current.length - 1] -
          frameTimestamps.current[0];
        const frameCount = frameTimestamps.current.length - 1;
        const fps = frameCount / timeSpan; // Keep in seconds (VisionCamera timestamps are in seconds)
        if (Number.isFinite(fps) && fps > 0) {
          fpsRef.current = fps;
          // Notify parent component about FPS update
          if (onFpsUpdate) {
            onFpsUpdate(fps);
          }

          // Log FPS periodically
          if (
            PPG_CONFIG.debug.enabled &&
            frameTimestamps.current.length % 30 === 0
          ) {
            console.log('[PPGCamera] FPS calculated:', {
              fps: fps.toFixed(1),
              frameCount,
              timeSpan: timeSpan.toFixed(3) + 's',
            });
          }
        }
      }
    },
    [onFpsUpdate],
  );

  // SAMPLE FLOW DEBUG: Check onSample prop
  useEffect(() => {
    console.log('[PPGCamera] Props received', {
      hasOnSample: typeof onSample === 'function',
      isActive,
      hidden,
    });
  }, [onSample, isActive, hidden]);

  // NATIVE MODULES EVENT LISTENER: Listen for samples from PPGCameraManager
  useEffect(() => {
    console.log('[PPGCamera] Setting up NativeModules event listener');
    console.log('[PPGCamera] PPGCameraManager available:', !!PPGCameraManager);
    console.log(
      '[PPGCamera] onSample available:',
      typeof onSample === 'function',
    );

    if (!PPGCameraManager) {
      console.warn('[PPGCamera] PPGCameraManager not available');
      return;
    }

    const eventEmitter = new NativeEventEmitter(PPGCameraManager);
    console.log('[PPGCamera] EventEmitter created, adding PPGSample listener');
    const subscription = eventEmitter.addListener('PPGSample', event => {
      console.log('[PPGCamera] PPGSample event received:', event);

      // FIXED: Reduce log flooding for NaN values during warm-up
      if (typeof event.value === 'number' && !isNaN(event.value)) {
        console.log('[PPGCamera] Received valid sample from NativeModules', {
          value: event.value,
          timestamp: event.timestamp,
          confidence: event.confidence,
        });
      } else {
        console.log('[PPGCamera] Received NaN sample (warm-up/low signal):', {
          value: event.value,
          timestamp: event.timestamp,
          confidence: event.confidence,
        });
      }

      // FPS Monitoring: Calculate FPS from JS event timestamps (safe from worklet context)
      calculateFPS(event.timestamp);

      // NOTE: Camera confidence is always ~0.85, so we don't filter here
      // Poor signal detection is handled by PPGAnalyzer based on metrics/SNR

      const sample: PPGSample = {
        value: event.value,
        timestamp: event.timestamp,
        confidence: event.confidence,
      };
      // CRITICAL: Handle async addSample to prevent race conditions
      onSample(sample).catch(error => {
        console.warn('[PPGCamera] Sample processing failed:', error);
      });
    });

    return () => {
      console.log('[PPGCamera] Clearing NativeModules event listener');
      subscription.remove();
    };
  }, [onSample, calculateFPS]);

  useEffect(() => {
    console.log('[PPGCamera] Permission status:', hasPermission);
    if (!hasPermission) {
      console.log('[PPGCamera] Requesting camera permission...');
      requestPermission().catch(error => {
        console.error('[PPGCamera] Permission request failed:', error);
      });
    }
  }, [hasPermission, requestPermission]);

  const plugin = useMemo<FrameProcessorPlugin | null>(() => {
    try {
      console.log('[PPGCamera] Initializing frame processor plugin');
      const created =
        VisionCameraProxy.initFrameProcessorPlugin('ppgMean', {}) ?? null;
      console.log('[PPGCamera] Frame processor plugin ready:', !!created);
      console.log('[PPGCamera] Plugin type:', typeof created);
      console.log(
        '[PPGCamera] Plugin callable:',
        typeof created?.call === 'function',
      );
      return created;
    } catch (error) {
      console.warn('[PPGCamera] frame processor unavailable', error);
      return null;
    }
  }, []);

  const hasTorch = device?.hasTorch === true;
  const [frameProcessorEnabled, setFrameProcessorEnabled] = useState(false);

  const pluginParams = useMemo(
    () => ({
      enableAgc: PPG_CONFIG.enableAGC,
      targetRms: PPG_CONFIG.amplitudeTargetRMS,
      alphaRms: PPG_CONFIG.agcAlphaRms,
      alphaGain: PPG_CONFIG.agcAlphaGain,
      gainMin: PPG_CONFIG.agcGainMin,
      gainMax: PPG_CONFIG.agcGainMax,
      roi: PPG_CONFIG.roiBoxPct,
      channel: PPG_CONFIG.ppgChannel,
      torch: requireTorch,
      simdEnabled: PPG_CONFIG.camera.simdEnabled,
      performanceLogging: PPG_CONFIG.camera.performanceLogging,
    }),
    [requireTorch],
  );

  const frameProcessor = useFrameProcessor(
    (frame: Frame) => {
      'worklet';
      console.log('[PPGCamera] Frame processor called with frame:', {
        timestamp: frame.timestamp,
        width: frame.width,
        height: frame.height,
      });

      if (!plugin || typeof plugin.call !== 'function') {
        console.log('[PPGCamera] Frame processor: plugin not ready');
        return;
      }

      console.log(
        '[PPGCamera] Frame processor: calling plugin with params:',
        pluginParams,
      );
      const value = plugin.call(frame, pluginParams) as unknown;

      console.log('[PPGCamera] Frame processor: plugin returned value:', {
        value,
        type: typeof value,
        isNumber: typeof value === 'number',
        isNaN: Number.isNaN(value),
        isFinite: Number.isFinite(value),
      });

      if (typeof value !== 'number' || Number.isNaN(value)) {
        console.log('[PPGCamera] Frame processor: invalid value', {
          value,
          type: typeof value,
        });
        return;
      }

      const timestamp = frame.timestamp ?? Date.now();
      console.log('[PPGCamera] Frame processor: emitting sample', {
        value,
        timestamp,
      });

      // WORKLET CRASH FIX: FPS monitoring moved to JS event listener
      // No runOnJS calls from frame processor to prevent iOS crashes

      // NATIVE NOTIFICATION: PPGMeanPlugin sends NSNotification, PPGCameraManager forwards to JS
      // No worklet callback needed - samples come via NativeModules event
      console.log(
        '[PPGCamera] Frame processor: sample processed by PPGMeanPlugin',
      );
    },
    [onSample, plugin, pluginParams],
  );

  useEffect(() => {
    if (!plugin) {
      setFrameProcessorEnabled(false);
      return;
    }
    if (enableProcessorTimerRef.current) {
      clearTimeout(enableProcessorTimerRef.current);
      enableProcessorTimerRef.current = null;
    }
    if (isActive) {
      enableProcessorTimerRef.current = setTimeout(() => {
        console.log('[PPGCamera] Enabling frame processor');
        setFrameProcessorEnabled(true);
      }, 200);
    } else {
      console.log('[PPGCamera] Disabling frame processor');
      setFrameProcessorEnabled(false);
    }
    return () => {
      if (enableProcessorTimerRef.current) {
        clearTimeout(enableProcessorTimerRef.current);
        enableProcessorTimerRef.current = null;
      }
    };
  }, [isActive, plugin]);

  // FLASH FIX: Use native torch control safely
  const setTorchProp = useCallback((mode: 'on' | 'off') => {
    if (Platform.OS !== 'android') {
      return;
    }
    if (!cameraRef.current) {
      return;
    }
    try {
      setAndroidTorchMode(mode);
      (cameraRef.current as any).setTorch(mode);
      if (PPG_CONFIG.debug.enabled) {
        console.log('[PPGCamera] Android torch set', {mode});
      }
    } catch (error) {
      console.warn('[PPGCamera] Android torch fallback failed', error);
    }
  }, []);

  useEffect(() => {
    if (!hasTorch) {
      return undefined;
    }

    const setTorch = async (level: number) => {
      if (Platform.OS === 'android') {
        setTorchProp(level > 0 ? 'on' : 'off');
        return;
      }

      try {
        if (typeof PPGCameraManager?.setTorchLevel === 'function') {
          console.log('[PPGCamera] Native torch request', {level});
          await PPGCameraManager.setTorchLevel(level);
          console.log('[PPGCamera] Native torch applied', {level});
        }
      } catch (error) {
        console.warn('[PPGCamera] Native torch failed', error);
      }
    };

    if (torchTimerRef.current) {
      clearTimeout(torchTimerRef.current);
      torchTimerRef.current = null;
    }

    if (isActive && requireTorch) {
      torchTimerRef.current = setTimeout(() => {
        setTorch(PPG_CONFIG.cameraTorchLevel);
      }, 1000);
    } else {
      setTorch(0);
    }

    return () => {
      if (torchTimerRef.current) {
        clearTimeout(torchTimerRef.current);
        torchTimerRef.current = null;
      }
      setTorch(0);
    };
  }, [hasTorch, isActive, requireTorch, setTorchProp]);

  useEffect(() => {
    console.log('[PPGCamera] Device/permission check', {
      hasDevice: !!device,
      hasTorch,
      hasPermission,
      isActive,
    });
  }, [device, hasPermission, isActive, hasTorch]);

  useEffect(() => {
    let cancelled = false;

    const lock = async () => {
      if (Platform.OS === 'android') {
        if (PPG_CONFIG.debug.enabled) {
          console.log(
            '[PPGCamera] Android fallback: camera lock not available; relying on auto settings',
          );
        }
        return;
      }

      if (typeof PPGCameraManager?.lockCameraSettings !== 'function') {
        return;
      }
      try {
        const result = await PPGCameraManager.lockCameraSettings({
          whiteBalance: 'locked',
          focus: 'locked',
          torchLevel: requireTorch ? PPG_CONFIG.cameraTorchLevel : 0,
        });
        if (!cancelled && PPG_CONFIG.debug.enabled) {
          console.log('[PPGCamera] Camera settings locked', result);
        }
      } catch (error) {
        if (!cancelled) {
          console.warn('[PPGCamera] lockCameraSettings failed', error);
        }
      }
    };

    const unlock = async () => {
      if (Platform.OS === 'android') {
        return;
      }
      if (typeof PPGCameraManager?.unlockCameraSettings !== 'function') {
        return;
      }
      try {
        await PPGCameraManager.unlockCameraSettings();
        if (PPG_CONFIG.debug.enabled) {
          console.log('[PPGCamera] Camera settings unlocked');
        }
      } catch (error) {
        console.warn('[PPGCamera] unlockCameraSettings failed', error);
      }
    };

    if (isActive) {
      lock();
    } else {
      unlock();
    }

    return () => {
      cancelled = true;
    };
  }, [isActive, requireTorch]);

  useEffect(() => {
    console.log('[PPGCamera] Frame processor status', {
      hasPlugin: !!plugin,
      isActive,
    });
  }, [plugin, isActive]);

  const cameraProps: Partial<React.ComponentProps<typeof Camera>> = {
    fps: PPG_CONFIG.sampleRate,
    torch: Platform.OS === 'ios' ? 'off' : androidTorchMode,
  };

  useEffect(() => {
    console.log('[PPGCamera] Torch mode update', {
      platform: Platform.OS,
      hasTorch,
      isActive,
      torchStrategy:
        Platform.OS === 'ios' ? 'PPGCameraManager' : 'Camera.setTorch',
    });
  }, [hasTorch, isActive]);

  if (!device || !hasPermission) {
    console.log('[PPGCamera] Camera not ready:', {
      hasDevice: !!device,
      hasPermission,
      hidden,
    });

    if (hidden) {
      return (
        <View style={styles.hiddenPlaceholder}>
          <Text style={styles.hiddenPlaceholderText}>
            {!device ? 'Kamera bulunamadı' : 'Kamera izni gerekli'}
          </Text>
        </View>
      );
    }
    return (
      <View style={styles.placeholder}>
        <Text style={styles.placeholderText}>
          {!device ? 'Kamera bulunamadı' : 'Kamera izni gerekli'}
        </Text>
      </View>
    );
  }

  return (
    <Camera
      style={hidden ? styles.hiddenCamera : styles.camera}
      device={device}
      isActive={isActive}
      frameProcessor={
        plugin && frameProcessorEnabled ? frameProcessor : undefined
      }
      ref={cameraRef}
      {...cameraProps}
    />
  );
}

const styles = StyleSheet.create({
  camera: {
    width: '100%',
    aspectRatio: 3 / 4,
    borderRadius: 12,
    overflow: 'hidden',
    backgroundColor: '#000',
  },
  hiddenCamera: {
    position: 'absolute',
    width: 1,
    height: 1,
    opacity: 0,
    top: -100,
    left: -100,
  },
  placeholder: {
    width: '100%',
    aspectRatio: 3 / 4,
    borderRadius: 12,
    borderWidth: 1,
    borderColor: '#ccc',
    alignItems: 'center',
    justifyContent: 'center',
  },
  placeholderText: {
    color: '#666',
  },
  hiddenPlaceholder: {
    position: 'absolute',
    width: 1,
    height: 1,
    top: -100,
    left: -100,
  },
  hiddenPlaceholderText: {
    color: '#666',
    fontSize: 10,
  },
});
