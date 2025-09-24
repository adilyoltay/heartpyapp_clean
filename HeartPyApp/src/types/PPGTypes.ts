// PPG Type Definitions
// Type-safe interfaces and types for PPG data flow

export interface PPGSample {
  readonly value: number;
  readonly timestamp: number;
  readonly confidence?: number; // Optional confidence from PPGMeanPlugin
}

export type PPGState = 'idle' | 'starting' | 'running' | 'stopping';

export type PPGError = 'camera' | 'native' | 'buffer' | 'config';

export type PPGHeartRateUpdate = {
  readonly bpm: number;
  readonly confidence: number;
};

export type PPGWarmupProgress = {
  readonly isWarmingUp: boolean;
  readonly progress: number;
  readonly samplesPushed: number;
  readonly samplesRequired: number;
};

export type PPGAnalysisFrame = {
  readonly metrics: Record<string, any> | null;
  readonly waveform: ReadonlyArray<{value: number; timestamp: number}>;
  readonly warmupProgress?: PPGWarmupProgress;
};
