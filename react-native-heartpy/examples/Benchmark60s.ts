import { RealtimeAnalyzer, HeartPyDebugger } from '../src/index';

type Path = 'jsi' | 'nm';

function median(xs: number[]) {
  const a = xs.slice().sort((x, y) => x - y);
  const n = a.length; if (!n) return NaN;
  return n % 2 ? a[(n - 1) / 2] : 0.5 * (a[n / 2 - 1] + a[n / 2]);
}

export async function runBenchmark60s(path: Path, fs = 50) {
  if (path === 'jsi') {
    RealtimeAnalyzer.setConfig({ jsiEnabled: true, debug: true });
  } else {
    RealtimeAnalyzer.setConfig({ jsiEnabled: false, debug: true });
  }

  const analyzer = await RealtimeAnalyzer.create(fs, {
    bandpass: { lowHz: 0.5, highHz: 5, order: 2 },
    welch: { nfft: 1024, overlap: 0.5 },
    peak: { refractoryMs: 320, thresholdScale: 0.5, bpmMin: 40, bpmMax: 180 },
  });

  const polls: { t: number; bpm: number; conf: number; snr: number }[] = [];
  const t0 = Date.now();
  const freq = 1.2;
  const blockSec = 0.2;
  const blockN = Math.floor(fs * blockSec);

  const pushTimer = setInterval(async () => {
    const x = new Float32Array(blockN);
    const baseIdx = Math.floor(((Date.now() - t0) / 1000) * fs);
    for (let i = 0; i < blockN; i++) {
      const n = baseIdx + i;
      const t = n / fs;
      x[i] = 0.6 * Math.sin(2 * Math.PI * freq * t) + 0.05 * Math.sin(2 * Math.PI * 0.25 * t);
    }
    try { await analyzer.push(x); } catch {}
  }, blockSec * 1000);

  const pollTimer = setInterval(async () => {
    const elapsed = (Date.now() - t0) / 1000;
    try {
      const m: any = await analyzer.poll();
      if (m) {
        let bpm = m.bpm;
        if (m.rrList?.length) {
          const meanRR = m.rrList.reduce((s: number, v: number) => s + v, 0) / m.rrList.length;
          if (meanRR > 1e-6) bpm = 60000.0 / meanRR;
        }
        polls.push({ t: elapsed, bpm, conf: m.quality?.confidence ?? NaN, snr: m.quality?.snrDb ?? NaN });
      }
    } catch {}
  }, 1000);

  await new Promise(res => setTimeout(res, 60000));
  clearInterval(pushTimer); clearInterval(pollTimer);
  await analyzer.destroy();

  const usable = polls.filter(p => p.t >= 20);
  const med_bpm = median(usable.map(p => p.bpm));
  const med_conf = median(usable.map(p => p.conf));
  const med_snr = median(usable.map(p => p.snr));
  const stats = HeartPyDebugger.getStats();
  const report = {
    path,
    med_bpm,
    med_conf,
    med_snr,
    push_p50: stats.pushMsP50,
    push_p95: stats.pushMsP95,
    poll_p50: stats.pollMsP50,
    poll_p95: stats.pollMsP95,
    last5: usable.slice(-5),
  } as const;
  console.log('Benchmark60s report:', report);
  return report;
}

