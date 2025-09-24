import { analyze, analyzeAsync, analyzeRRAsync, installJSI, analyzeJSI } from '../src/index';

describe('react-native-heartpy TS API', () => {
  it('converts Float64Array to number[] and calls native analyze', () => {
    const arr = new Float64Array([1, 2, 3]);
    const res = analyze(arr as any, 50, {} as any);
    expect(res).toHaveProperty('bpm');
    // @ts-ignore access mock
    const mock = require('react-native').NativeModules.HeartPyModule.analyze as jest.Mock;
    expect(mock).toHaveBeenCalled();
    const args = mock.mock.calls[0][0];
    expect(Array.isArray(args)).toBe(true);
    expect(args).toEqual([1, 2, 3]);
  });

  it('supports async analyze and RR', async () => {
    const arr = [800, 820, 780, 810];
    const r1 = await analyzeAsync(arr, 50, {} as any);
    expect(r1).toHaveProperty('rmssd');
    const rr = [800, 820, 780, 810];
    const r2 = await analyzeRRAsync(rr, {} as any);
    expect(r2).toHaveProperty('sdnn');
  });

  it('installs JSI and can call analyzeJSI when available', () => {
    expect(installJSI()).toBe(true);
    // Provide fake global for JSI path
    (global as any).__HeartPyAnalyze = (signal: number[], fs: number) => ({ bpm: 60 });
    const res = analyzeJSI([1, 2], 50, {} as any);
    expect(res).toHaveProperty('bpm');
  });
});

