/**
 * ParityCheck.ts — quick JSON vs Typed parity probe
 * Run inside a React Native environment (device/emulator). Not for CI.
 */
import {
  analyze,
  analyzeTyped,
  analyzeSegmentwiseTyped,
  analyzeRRTyped,
  type HeartPyOptions,
} from '../src/index';

function makeSignal(fs: number, seconds: number, bpm: number): number[] {
  const n = Math.floor(fs * seconds);
  const f = bpm / 60.0;
  const out: number[] = new Array(n);
  for (let i = 0; i < n; i++) {
    const t = i / fs;
    out[i] = 512 + 0.7 * Math.sin(2 * Math.PI * f * t) + 0.2 * Math.sin(4 * Math.PI * f * t);
  }
  return out;
}

async function main() {
  const fs = 50;
  const seconds = 60;
  const signal = makeSignal(fs, seconds, 72);
  const opts: HeartPyOptions = {
    bandpass: { lowHz: 0.5, highHz: 5, order: 2 },
    quality: { thresholdRR: true },
    calcFreq: false,
    filter: { mode: 'butter-filtfilt', order: 3 },
  };

  const jsonRes = await analyze(signal, fs, opts);
  const typedRes = await analyzeTyped(signal, fs, opts);

  const keys = ['bpm','sdnn','rmssd','sdsd','pnn20','pnn50','lfhf'];
  const diffs: Record<string, number> = {} as any;
  for (const k of keys) {
    const a = (jsonRes as any)[k];
    const b = (typedRes as any)[k];
    diffs[k] = Math.abs((a ?? 0) - (b ?? 0));
  }

  console.log('[ParityCheck] json vs typed core diffs', diffs);
  console.log('[ParityCheck] rrList length parity', jsonRes.rrList.length, typedRes.rrList.length);

  // Segmentwise (small window for demo)
  const segTyped = await analyzeSegmentwiseTyped(signal, fs, { segmentwise: { width: 20, overlap: 0.0 } });
  console.log('[ParityCheck] segmentwise typed: segments', segTyped.segments?.length ?? 0);

  // RR-only
  const rr: number[] = [];
  for (let i = 1; i < jsonRes.peakList.length; i++) {
    const dt = (jsonRes.peakList[i] - jsonRes.peakList[i - 1]) * 1000 / fs;
    rr.push(dt);
  }
  const rrTyped = await analyzeRRTyped(rr, { quality: { thresholdRR: true } });
  console.log('[ParityCheck] rr typed bpm', rrTyped.bpm.toFixed(2));
}

// eslint-disable-next-line @typescript-eslint/no-floating-promises
main();

