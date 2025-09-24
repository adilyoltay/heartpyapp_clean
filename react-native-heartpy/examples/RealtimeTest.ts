import { RealtimeAnalyzer } from '../src/index';

function median(xs: number[]) {
  const a = xs.slice().sort((a, b) => a - b);
  const n = a.length; if (!n) return NaN;
  return n % 2 ? a[(n - 1) / 2] : 0.5 * (a[n / 2 - 1] + a[n / 2]);
}

export async function runRealtimeTest60s() {
  const fs = 50;
  const analyzer = await RealtimeAnalyzer.create(fs, {
    bandpass: { lowHz: 0.5, highHz: 5, order: 2 },
    welch: { nfft: 1024, overlap: 0.5 },
    peak: { refractoryMs: 320, thresholdScale: 0.5, bpmMin: 40, bpmMax: 180 },
  });

  const freq = 1.2; // 72 bpm
  const blockSec = 0.2; // 200 ms
  const blockN = Math.floor(fs * blockSec);
  const t0 = Date.now();
  const polls: { t: number; bpm: number; conf: number; snr: number }[] = [];

  const pushTimer = setInterval(async () => {
    const x = new Float32Array(blockN);
    const baseIdx = Math.floor(((Date.now() - t0) / 1000) * fs);
    for (let i = 0; i < blockN; i++) {
      const n = baseIdx + i;
      const t = n / fs;
      x[i] = 0.6 * Math.sin(2 * Math.PI * freq * t) + 0.05 * Math.sin(2 * Math.PI * 0.25 * t);
    }
    try { await analyzer.push(x); } catch (e) { console.warn('push error', e); }
  }, blockSec * 1000);

  const pollTimer = setInterval(async () => {
    const elapsed = (Date.now() - t0) / 1000;
    try {
      const m = await analyzer.poll();
      if (m) {
        let bpm = m.bpm;
        if (m.rrList?.length) {
          const meanRR = m.rrList.reduce((s, v) => s + v, 0) / m.rrList.length;
          if (meanRR > 1e-6) bpm = 60000.0 / meanRR;
        }
        const conf = (m as any).quality?.confidence ?? NaN;
        const snr = (m as any).quality?.snrDb ?? NaN;
        polls.push({ t: elapsed, bpm, conf, snr });
        console.log(`RT poll t=${elapsed.toFixed(1)}s bpm=${bpm.toFixed(1)} conf=${(conf as number)?.toFixed?.(2)} snr=${(snr as number)?.toFixed?.(2)}dB`);
      }
    } catch (e) { console.warn('poll error', e); }
  }, 1000);

  await new Promise(res => setTimeout(res, 60000));
  clearInterval(pushTimer); clearInterval(pollTimer);
  await analyzer.destroy();

  // Warm-up: exclude first 20 s
  const usable = polls.filter(p => p.t >= 20.0);
  const med_bpm = median(usable.map(p => p.bpm));
  const med_conf = median(usable.map(p => p.conf));
  const med_snr = median(usable.map(p => p.snr));
  console.log('MEDIANS: bpm=', med_bpm.toFixed(2), ' conf=', med_conf.toFixed(2), ' snr=', med_snr.toFixed(2));
  console.log('Last 5 polls:', usable.slice(-5));

  return { med_bpm, med_conf, med_snr, last5: usable.slice(-5) };
}

