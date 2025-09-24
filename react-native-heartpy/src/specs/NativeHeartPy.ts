import {TurboModuleRegistry} from 'react-native';
import type {TurboModule} from 'react-native';

type JsonObject = {[key: string]: unknown};

export interface Spec extends TurboModule {
  analyze(signal: ReadonlyArray<number>, fs: number, options?: JsonObject | null): JsonObject;
  analyzeSegmentwise(signal: ReadonlyArray<number>, fs: number, options?: JsonObject | null): JsonObject;
  analyzeRR(rrIntervals: ReadonlyArray<number>, options?: JsonObject | null): JsonObject;
  analyzeAsync(signal: ReadonlyArray<number>, fs: number, options?: JsonObject | null): Promise<JsonObject>;
  analyzeSegmentwiseAsync(signal: ReadonlyArray<number>, fs: number, options?: JsonObject | null): Promise<JsonObject>;
  analyzeRRAsync(rrIntervals: ReadonlyArray<number>, options?: JsonObject | null): Promise<JsonObject>;
  analyzeTyped(signal: ReadonlyArray<number>, fs: number, options?: JsonObject | null): JsonObject;
  analyzeSegmentwiseTyped(signal: ReadonlyArray<number>, fs: number, options?: JsonObject | null): JsonObject;
  analyzeRRTyped(rrIntervals: ReadonlyArray<number>, options?: JsonObject | null): JsonObject;
  analyzeAsyncTyped(signal: ReadonlyArray<number>, fs: number, options?: JsonObject | null): Promise<JsonObject>;
  analyzeSegmentwiseAsyncTyped(signal: ReadonlyArray<number>, fs: number, options?: JsonObject | null): Promise<JsonObject>;
  analyzeRRAsyncTyped(rrIntervals: ReadonlyArray<number>, options?: JsonObject | null): Promise<JsonObject>;
  interpolateClipping(signal: ReadonlyArray<number>, fs: number, threshold?: number): ReadonlyArray<number>;
  hampelFilter(signal: ReadonlyArray<number>, windowSize?: number, threshold?: number): ReadonlyArray<number>;
  scaleData(signal: ReadonlyArray<number>, newMin?: number, newMax?: number): ReadonlyArray<number>;
  installJSI(): boolean;
  getConfig(): JsonObject;
  setConfig(config: JsonObject): void;
  rtCreate(fs: number, options?: JsonObject | null): Promise<number>;
  rtPush(handle: number, samples: ReadonlyArray<number>, t0?: number): Promise<void>;
  rtPushTs(handle: number, samples: ReadonlyArray<number>, timestamps: ReadonlyArray<number>): Promise<void>;
  rtPoll(handle: number): Promise<JsonObject | null>;
  rtSetWindow(handle: number, windowSeconds: number): Promise<void>;
  rtDestroy(handle: number): Promise<void>;
  addListener(eventType: string): void;
  removeListeners(count: number): void;
}

export default TurboModuleRegistry.getEnforcing<Spec>('HeartPyModule');
