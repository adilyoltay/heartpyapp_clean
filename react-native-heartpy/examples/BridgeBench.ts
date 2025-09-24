import {performance} from 'perf_hooks';
import {
  analyze,
  analyzeAsync,
  analyzeTyped,
  analyzeAsyncTyped,
  type HeartPyOptions,
  type HeartPyResult,
} from '../src/index';

const fs = 50;
const seconds = 60;
const samples = fs * seconds;

const signal = Array.from({length: samples}, (_, i) => {
  const t = i / fs;
  const base = 512;
  const amplitude = 120;
  const hz = 1.2; // ~72 bpm
  return base + amplitude * Math.sin(2 * Math.PI * hz * t);
});

const options: HeartPyOptions = {
  bandpass: {lowHz: 0.5, highHz: 5, order: 2},
  calcFreq: false,
  quality: {thresholdRR: true},
};

function stats(values: number[]) {
  const sorted = [...values].sort((a, b) => a - b);
  const p50 = sorted[Math.floor(sorted.length * 0.5)];
  const p95 = sorted[Math.floor(sorted.length * 0.95)];
  const avg = sorted.reduce((acc, cur) => acc + cur, 0) / sorted.length;
  return {avg, p50, p95};
}

function bench(label: string, run: () => HeartPyResult) {
  const iterations = 50;
  const durations: number[] = [];
  for (let i = 0; i < iterations; i++) {
    const start = performance.now();
    run();
    const end = performance.now();
    durations.push(end - start);
  }
  const s = stats(durations);
  console.log(`${label}: avg=${s.avg.toFixed(2)}ms p50=${s.p50.toFixed(2)}ms p95=${s.p95.toFixed(2)}ms`);
}

async function benchAsync(label: string, run: () => Promise<HeartPyResult>) {
  const iterations = 50;
  const durations: number[] = [];
  for (let i = 0; i < iterations; i++) {
    const start = performance.now();
    await run();
    const end = performance.now();
    durations.push(end - start);
  }
  const s = stats(durations);
  console.log(`${label}: avg=${s.avg.toFixed(2)}ms p50=${s.p50.toFixed(2)}ms p95=${s.p95.toFixed(2)}ms`);
}

export async function runBenchmarks() {
  console.log('--- HeartPy bridge benchmark (60s signal @ 50Hz) ---');
  bench('sync-json', () => analyze(signal, fs, options));
  bench('sync-typed', () => analyzeTyped(signal, fs, options));
  await benchAsync('async-json', () => analyzeAsync(signal, fs, options));
  await benchAsync('async-typed', () => analyzeAsyncTyped(signal, fs, options));
}

if (require.main === module) {
  runBenchmarks().catch(err => {
    console.error(err);
    process.exitCode = 1;
  });
}
