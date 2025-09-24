import {useCallback, useEffect, useMemo, useReducer, useRef} from 'react';
import type {
  PPGAnalysisFrame,
  PPGState as PPGLifecycleState,
  PPGWarmupProgress,
} from '../../types/PPGTypes';

export interface DebugCounters {
  readonly schedulerTicks: number;
  readonly schedulerSkips: number;
  readonly framesEmitted: number;
  readonly polls: number;
  readonly lastFlushTs: number;
}

export interface PPGState {
  readonly lifecycle: PPGLifecycleState;
  readonly isActive: boolean;
  readonly isAnalyzing: boolean;
  readonly metrics: PPGAnalysisFrame['metrics'];
  readonly waveform: ReadonlyArray<{value: number; timestamp: number}>;
  readonly lastError: string | null;
  readonly debugCounters: DebugCounters;
  readonly warmupProgress: PPGWarmupProgress;
}

export type PPGAction =
  | {type: 'SET_LIFECYCLE'; payload: PPGLifecycleState}
  | {type: 'SET_ACTIVE'; payload: boolean}
  | {type: 'SET_ANALYZING'; payload: boolean}
  | {type: 'SET_METRICS'; payload: PPGAnalysisFrame['metrics']}
  | {
      type: 'APPEND_WAVEFORM';
      payload: ReadonlyArray<{value: number; timestamp: number}>;
    }
  | {type: 'SET_ERROR'; payload: string | null}
  | {type: 'RESET_WAVEFORM'}
  | {type: 'SET_WARMUP_PROGRESS'; payload: PPGWarmupProgress}
  | {type: 'TICK_DEBUG'; payload: Partial<DebugCounters>};

const MAX_WAVEFORM_POINTS = 240;

export const initialDebugCounters: DebugCounters = {
  schedulerTicks: 0,
  schedulerSkips: 0,
  framesEmitted: 0,
  polls: 0,
  lastFlushTs: 0,
};

export const initialPPGState: PPGState = {
  lifecycle: 'idle',
  isActive: false,
  isAnalyzing: false,
  metrics: null,
  waveform: [],
  lastError: null,
  debugCounters: initialDebugCounters,
  warmupProgress: {
    isWarmingUp: false,
    progress: 0,
    samplesPushed: 0,
    samplesRequired: 0,
  },
};

const clampWaveform = (
  existing: ReadonlyArray<{value: number; timestamp: number}>,
  incoming: ReadonlyArray<{value: number; timestamp: number}>,
): ReadonlyArray<{value: number; timestamp: number}> => {
  if (incoming.length >= MAX_WAVEFORM_POINTS) {
    return incoming.slice(-MAX_WAVEFORM_POINTS);
  }

  const next = existing.concat(incoming);
  if (next.length <= MAX_WAVEFORM_POINTS) {
    return next;
  }

  return next.slice(-MAX_WAVEFORM_POINTS);
};

const mergeDebugCounters = (
  prev: DebugCounters,
  patch: Partial<DebugCounters>,
): DebugCounters => ({
  schedulerTicks: prev.schedulerTicks + (patch.schedulerTicks ?? 0),
  schedulerSkips: prev.schedulerSkips + (patch.schedulerSkips ?? 0),
  framesEmitted: prev.framesEmitted + (patch.framesEmitted ?? 0),
  polls: prev.polls + (patch.polls ?? 0),
  lastFlushTs: patch.lastFlushTs ?? prev.lastFlushTs,
});

export function ppgReducer(state: PPGState, action: PPGAction): PPGState {
  switch (action.type) {
    case 'SET_LIFECYCLE': {
      const lifecycle = action.payload;
      return {
        ...state,
        lifecycle,
        isActive: lifecycle !== 'idle',
        isAnalyzing: lifecycle === 'running',
      };
    }
    case 'SET_ACTIVE':
      return {
        ...state,
        isActive: action.payload,
      };
    case 'SET_ANALYZING':
      return {
        ...state,
        isAnalyzing: action.payload,
      };
    case 'SET_METRICS':
      return {
        ...state,
        metrics: action.payload,
      };
    case 'APPEND_WAVEFORM':
      return {
        ...state,
        waveform: clampWaveform(state.waveform, action.payload),
      };
    case 'RESET_WAVEFORM':
      return {
        ...state,
        waveform: [],
      };
    case 'SET_ERROR':
      return {
        ...state,
        lastError: action.payload,
      };
    case 'TICK_DEBUG':
      return {
        ...state,
        debugCounters: mergeDebugCounters(state.debugCounters, action.payload),
      };
    case 'SET_WARMUP_PROGRESS':
      return {
        ...state,
        warmupProgress: action.payload,
      };
    default:
      return state;
  }
}

export const usePPGReducer = () => useReducer(ppgReducer, initialPPGState);

export type MasterTask = {
  readonly id: string;
  readonly intervalMs: number;
  readonly run: () => Promise<void> | void;
  readonly description?: string;
};

export interface MasterTimerStats {
  ticks: number;
  skipped: number;
}

export interface MasterTimerApi {
  readonly addTask: (task: MasterTask) => void;
  readonly removeTask: (id: string) => void;
  readonly updateTaskInterval: (id: string, intervalMs: number) => void;
  readonly stats: MasterTimerStats;
}

export function useMasterTimer(baseIntervalMs = 32): MasterTimerApi {
  const tasksRef = useRef(
    new Map<string, MasterTask & {lastRun: number; running: boolean}>(),
  );
  const timerRef = useRef<NodeJS.Timeout | null>(null);
  const statsRef = useRef<MasterTimerStats>({ticks: 0, skipped: 0});

  const clearTimer = useCallback(() => {
    if (timerRef.current) {
      clearInterval(timerRef.current);
      timerRef.current = null;
    }
  }, []);

  const runTasks = useCallback(() => {
    const now = Date.now();
    statsRef.current.ticks += 1;

    tasksRef.current.forEach(task => {
      if (task.running) {
        statsRef.current.skipped += 1;
        return;
      }
      if (now - task.lastRun < task.intervalMs) {
        return;
      }
      task.running = true;
      Promise.resolve()
        .then(() => task.run())
        .catch(error => {
          console.warn('[MasterTimer] Task failed', {
            id: task.id,
            description: task.description,
            error,
          });
        })
        .finally(() => {
          task.lastRun = Date.now();
          task.running = false;
        });
    });
  }, []);

  const ensureTimer = useCallback(() => {
    if (!timerRef.current && tasksRef.current.size > 0) {
      timerRef.current = setInterval(runTasks, baseIntervalMs);
    }
  }, [baseIntervalMs, runTasks]);

  const addTask = useCallback(
    (task: MasterTask) => {
      tasksRef.current.set(task.id, {
        ...task,
        lastRun: 0,
        running: false,
      });
      ensureTimer();
    },
    [ensureTimer],
  );

  const removeTask = useCallback(
    (id: string) => {
      tasksRef.current.delete(id);
      if (tasksRef.current.size === 0) {
        clearTimer();
      }
    },
    [clearTimer],
  );

  const updateTaskInterval = useCallback((id: string, intervalMs: number) => {
    const existing = tasksRef.current.get(id);
    if (!existing) {
      return;
    }
    tasksRef.current.set(id, {
      ...existing,
      intervalMs,
    });
  }, []);

  useEffect(
    () => () => {
      clearTimer();
      tasksRef.current.clear();
    },
    [clearTimer],
  );

  return useMemo(
    () => ({
      addTask,
      removeTask,
      updateTaskInterval,
      stats: statsRef.current,
    }),
    [addTask, removeTask, updateTaskInterval],
  );
}
