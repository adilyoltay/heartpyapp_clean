import {useCallback, useEffect, useMemo, useRef, useState} from 'react';
import {unstable_batchedUpdates} from 'react-native';
import {
  PPGAnalyzer,
  DEFAULT_ANALYZER_OPTIONS,
  type AnalyzerTuningOptions,
} from '../core/PPGAnalyzer';
import {PPG_CONFIG} from '../core/PPGConfig';
import {useMasterTimer, usePPGReducer} from '../core/state/ppgStore';
import type {
  PPGAnalysisFrame,
  PPGSample,
  PPGState as PPGLifecycleState,
} from '../types/PPGTypes';

export function useAnalyzer() {
  const analyzerRef = useRef<PPGAnalyzer | null>(null);
  const [ppgState, dispatch] = usePPGReducer();
  const [options, setOptions] = useState<AnalyzerTuningOptions>({
    ...DEFAULT_ANALYZER_OPTIONS,
  });
  const masterIntervalMs = Math.min(
    Math.max(PPG_CONFIG.uiUpdateIntervalMs ?? 50, 16),
    50,
  );
  const {
    addTask,
    removeTask,
    stats: masterStats,
  } = useMasterTimer(masterIntervalMs);
  const tasksRegisteredRef = useRef(false);
  const ppgStateRef = useRef(ppgState);
  const lastPollAtRef = useRef<number>(Date.now());
  const schedulerStatsRef = useRef({ticks: 0, skipped: 0});
  const lastDropLogRef = useRef(0);

  useEffect(() => {
    ppgStateRef.current = ppgState;
  }, [ppgState]);

  const teardownSchedulerTasks = useCallback(() => {
    removeTask('ppg.poll');
    removeTask('ppg.telemetry');
    removeTask('ppg.watchdog');
    tasksRegisteredRef.current = false;
  }, [removeTask]);

  const decimateWaveform = useCallback(
    (waveform: ReadonlyArray<{value: number; timestamp: number}>) => {
      const limit = PPG_CONFIG.waveformTailSamples ?? 200;
      if (waveform.length <= limit) {
        return waveform;
      }
      const stride = Math.ceil(waveform.length / limit);
      const decimated: Array<{value: number; timestamp: number}> = [];
      for (let i = 0; i < waveform.length; i += stride) {
        decimated.push(waveform[i]);
      }
      return decimated;
    },
    [],
  );

  const handleFrame = useCallback(
    (frame: PPGAnalysisFrame) => {
      const payloadWaveform = decimateWaveform(frame.waveform);
      unstable_batchedUpdates(() => {
        dispatch({type: 'SET_METRICS', payload: frame.metrics});
        dispatch({type: 'APPEND_WAVEFORM', payload: payloadWaveform});
      });
    },
    [decimateWaveform, dispatch],
  );

  const registerSchedulerTasks = useCallback(() => {
    const analyzer = analyzerRef.current;
    if (!analyzer) {
      return;
    }

    const pollTask = {
      id: 'ppg.poll',
      intervalMs: masterIntervalMs,
      description: 'PPG analyzer polling',
      run: async () => {
        const summary = await analyzer.processTick();
        if (summary.droppedSamples && summary.droppedSamples > 0) {
          const now = Date.now();
          if (
            now - lastDropLogRef.current > 1_000 &&
            PPG_CONFIG.debug.enabled
          ) {
            lastDropLogRef.current = now;
            console.warn('[useAnalyzer] Back-pressure dropping samples', {
              dropped: summary.droppedSamples,
              pendingSamples: summary.pendingSamples,
            });
          }
        }

        if (summary.polled || summary.reservoirReady) {
          lastPollAtRef.current = Date.now();
        }

        unstable_batchedUpdates(() => {
          dispatch({
            type: 'TICK_DEBUG',
            payload: {
              schedulerTicks: 1,
              polls: summary.polled ? 1 : 0,
              framesEmitted: summary.emittedFrame ? 1 : 0,
            },
          });
        });
      },
    } as const;

    const telemetryTask = {
      id: 'ppg.telemetry',
      intervalMs: 10_000,
      description: 'PPG telemetry flush',
      run: () => {
        const {ticks, skipped} = masterStats;
        const deltaSkipped = skipped - schedulerStatsRef.current.skipped;
        schedulerStatsRef.current = {ticks, skipped};
        const payload: Record<string, number> = {lastFlushTs: Date.now()};
        if (deltaSkipped > 0) {
          payload.schedulerSkips = deltaSkipped;
        }
        if (PPG_CONFIG.debug.enableSchedulerLogging) {
          console.log('[useAnalyzer] Scheduler telemetry snapshot', {
            ticks,
            skipped,
            deltaSkipped,
          });
        }
        unstable_batchedUpdates(() => {
          dispatch({type: 'TICK_DEBUG', payload});
        });
      },
    } as const;

    const watchdogTask = {
      id: 'ppg.watchdog',
      intervalMs: 1_000,
      description: 'PPG watchdog',
      run: () => {
        const now = Date.now();
        const lifecycle = ppgStateRef.current.lifecycle;
        if (lifecycle !== 'running') {
          return;
        }
        const lastPollDelta = now - lastPollAtRef.current;
        if (lastPollDelta > 15_000) {
          if (PPG_CONFIG.debug.enabled) {
            console.warn('[useAnalyzer] Watchdog: analyzer poll stalled', {
              lastPollDelta,
            });
          }
          unstable_batchedUpdates(() => {
            dispatch({type: 'SET_ERROR', payload: 'poll-timeout'});
          });
        } else if (ppgStateRef.current.lastError === 'poll-timeout') {
          unstable_batchedUpdates(() => {
            dispatch({type: 'SET_ERROR', payload: null});
          });
        }
      },
    } as const;

    removeTask(pollTask.id);
    removeTask(telemetryTask.id);
    removeTask(watchdogTask.id);

    addTask(pollTask);
    addTask(telemetryTask);
    addTask(watchdogTask);

    tasksRegisteredRef.current = true;
  }, [addTask, removeTask, dispatch, masterIntervalMs, masterStats]);

  const handleStateChange = useCallback(
    (nextState: PPGLifecycleState) => {
      if (PPG_CONFIG.debug.enabled) {
        console.log('[useAnalyzer] Analyzer state changed', {nextState});
      }

      unstable_batchedUpdates(() => {
        dispatch({type: 'SET_LIFECYCLE', payload: nextState});
        dispatch({type: 'SET_ACTIVE', payload: nextState !== 'idle'});
        dispatch({type: 'SET_ANALYZING', payload: nextState === 'running'});
        if (nextState === 'idle') {
          dispatch({type: 'RESET_WAVEFORM'});
        }
      });

      if (nextState === 'running') {
        registerSchedulerTasks();
      }
      if (nextState === 'idle') {
        teardownSchedulerTasks();
      }
    },
    [dispatch, registerSchedulerTasks, teardownSchedulerTasks],
  );

  const handleWarmupProgress = useCallback(
    (progress: {
      isWarmingUp: boolean;
      progress: number;
      samplesPushed: number;
      samplesRequired: number;
    }) => {
      dispatch({type: 'SET_WARMUP_PROGRESS', payload: progress});
    },
    [dispatch],
  );

  useEffect(() => {
    console.log('[useAnalyzer] Initializing analyzer');
    analyzerRef.current = new PPGAnalyzer({
      onStateChange: handleStateChange,
      onFrame: handleFrame,
      onHeartRateUpdate: update => {
        if (PPG_CONFIG.debug.enabled) {
          console.log('[useAnalyzer] Heart rate update', update);
        }
      },
      onWarmupProgress: handleWarmupProgress,
    });
    return () => {
      console.log('[useAnalyzer] Cleaning up analyzer');
      teardownSchedulerTasks();
      analyzerRef.current?.stop().catch(console.warn);
      analyzerRef.current = null;
    };
  }, [
    handleFrame,
    handleStateChange,
    handleWarmupProgress,
    teardownSchedulerTasks,
  ]);

  const start = useCallback(async () => {
    console.log(
      '[useAnalyzer] Start button pressed, current state:',
      ppgStateRef.current.lifecycle,
    );
    if (tasksRegisteredRef.current) {
      teardownSchedulerTasks();
    }
    try {
      await analyzerRef.current?.start();
      console.log('[useAnalyzer] Start completed successfully');
    } catch (error) {
      console.error('[useAnalyzer] Start failed:', error);
      unstable_batchedUpdates(() => {
        dispatch({type: 'SET_ERROR', payload: 'start-failed'});
        dispatch({type: 'SET_ACTIVE', payload: false});
      });
    }
  }, [dispatch, teardownSchedulerTasks]);

  const stop = useCallback(async () => {
    console.log('[useAnalyzer] Stop button pressed');
    try {
      await analyzerRef.current?.stop();
      console.log('[useAnalyzer] Stop completed successfully');
    } catch (error) {
      console.error('[useAnalyzer] Stop failed:', error);
    } finally {
      teardownSchedulerTasks();
    }
  }, [teardownSchedulerTasks]);

  const addSample = useCallback(async (sample: PPGSample) => {
    analyzerRef.current?.addSample(sample);
  }, []);

  const updateSampleRate = useCallback((fps: number) => {
    analyzerRef.current?.updateSampleRate(fps);
  }, []);

  const updateOptions = useCallback((patch: Partial<AnalyzerTuningOptions>) => {
    setOptions(prev => ({...prev, ...patch}));
    analyzerRef.current?.configure(patch).catch(error => {
      console.warn('[useAnalyzer] Failed to configure analyzer', error);
    });
  }, []);

  const resetOptions = useCallback(() => {
    setOptions({...DEFAULT_ANALYZER_OPTIONS});
    analyzerRef.current?.resetOptions().catch(error => {
      console.warn('[useAnalyzer] Failed to reset analyzer options', error);
    });
  }, []);

  const analysisData = useMemo<PPGAnalysisFrame>(
    () => ({
      metrics: ppgState.metrics,
      waveform: ppgState.waveform,
      warmupProgress: ppgState.warmupProgress,
    }),
    [ppgState.metrics, ppgState.waveform, ppgState.warmupProgress],
  );

  return {
    analysisData,
    state: ppgState.lifecycle,
    start,
    stop,
    addSample,
    updateSampleRate,
    options,
    updateOptions,
    resetOptions,
  };
}
