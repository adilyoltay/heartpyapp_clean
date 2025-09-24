// Minimal sanity probe for Android JSI streaming
import { installJSI } from '../src/index';

export async function probeJSIStreamingOnce() {
  const ok = installJSI();
  console.log('installJSI():', ok);
  const g: any = global as any;
  const has = ['__hpRtCreate','__hpRtPush','__hpRtPoll','__hpRtDestroy'].map(k => [k, typeof g[k]]);
  console.log('JSI functions:', has);
  if (!ok || has.some(([,t]) => t !== 'function')) return false;

  const fs = 50;
  const id = g.__hpRtCreate(fs, { bandpass: { lowHz: 0.5, highHz: 5, order: 2 }, peak: { refractoryMs: 320, bpmMin: 40, bpmMax: 180 } });
  const len = Math.floor(fs * 1.0);
  const x = new Float32Array(len);
  const freq = 1.2;
  for (let i = 0; i < len; i++) x[i] = 0.6 * Math.sin((2*Math.PI*freq) * (i/fs));
  g.__hpRtPush(id, x, 0);
  const out = g.__hpRtPoll(id);
  console.log('JSI poll result:', out);
  g.__hpRtDestroy(id);
  return true;
}

