export const Platform = {
  OS: 'android',
  select: (obj: any) => (obj && obj.android !== undefined ? obj.android : obj?.default),
};

export const NativeModules = {
  HeartPyModule: {
    analyze: jest.fn((signal: number[], fs: number, options: any) => defaultResult()),
    analyzeAsync: jest.fn((signal: number[], fs: number, options: any) => Promise.resolve(defaultResult() as any)),
    analyzeRR: jest.fn((rr: number[], options: any) => defaultResult()),
    analyzeRRAsync: jest.fn((rr: number[], options: any) => Promise.resolve(defaultResult() as any)),
    analyzeSegmentwise: jest.fn((signal: number[], fs: number, options: any) => defaultResult()),
    analyzeSegmentwiseAsync: jest.fn((signal: number[], fs: number, options: any) => Promise.resolve(defaultResult() as any)),
    interpolateClipping: jest.fn((signal: number[], fs: number, thr: number) => signal),
    hampelFilter: jest.fn((signal: number[], win: number, thr: number) => signal),
    scaleData: jest.fn((signal: number[], a: number, b: number) => signal),
    installJSI: jest.fn(() => true),
    getConfig: jest.fn(() => ({ jsiEnabled: true, zeroCopyEnabled: true, debug: false, maxSamplesPerPush: 5000 })),
    setConfig: jest.fn((_cfg: any) => {}),
    rtCreate: jest.fn(async (_fs: number, _opt: any) => 123),
    rtPush: jest.fn(async (_h: number, _arr: number[], _t0?: number) => {}),
    rtPoll: jest.fn(async (_h: number) => null),
    rtDestroy: jest.fn(async (_h: number) => {}),
  },
};

function defaultResult() {
  return {
    bpm: 60,
    ibiMs: [],
    rrList: [],
    peakList: [],
    sdnn: 30,
    rmssd: 25,
    sdsd: 25,
    pnn20: 0.1,
    pnn50: 0.05,
    nn20: 10,
    nn50: 5,
    mad: 15,
    sd1: 10,
    sd2: 20,
    sd1sd2Ratio: 0.5,
    ellipseArea: 628,
    vlf: 0,
    lf: 0,
    hf: 0,
    lfhf: 0,
    totalPower: 0,
    lfNorm: 0,
    hfNorm: 0,
    breathingRate: 0.2,
    quality: { totalBeats: 0, rejectedBeats: 0, rejectionRate: 0, goodQuality: true },
    segments: [],
  } as const;
}

export default { NativeModules, Platform };
