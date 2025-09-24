import React, { useEffect, useRef, useState } from 'react';
import { View, Text, Button, ScrollView, StyleSheet } from 'react-native';
import { RealtimeAnalyzer } from 'react-native-heartpy';

type Poll = { t: number; bpm: number; conf: number; snr: number };

function median(xs: number[]) {
  const a = xs.slice().sort((x, y) => x - y);
  const n = a.length;
  if (!n) return NaN;
  return n % 2 ? a[(n - 1) / 2] : 0.5 * (a[n / 2 - 1] + a[n / 2]);
}

export default function RealtimeDemo() {
  const [running, setRunning] = useState(false);
  const [last5, setLast5] = useState<Poll[]>([]);
  const [medians, setMedians] = useState<{ bpm?: number; conf?: number; snr?: number } | null>(null);
  const [error, setError] = useState<string | null>(null);

  const analyzerRef = useRef<RealtimeAnalyzer | null>(null);
  const pushTimerRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const pollTimerRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const t0Ref = useRef<number>(0);
  const pollsRef = useRef<Poll[]>([]);

  async function start() {
    if (running) return;
    setError(null);
    setMedians(null);
    pollsRef.current = [];
    setLast5([]);
    t0Ref.current = Date.now();

    try {
      const fs = 50;
      analyzerRef.current = await RealtimeAnalyzer.create(fs, {
        bandpass: { lowHz: 0.5, highHz: 5, order: 2 },
        welch: { nfft: 1024, overlap: 0.5 },
        peak: { refractoryMs: 320, thresholdScale: 0.5, bpmMin: 40, bpmMax: 180 },
      });

      // Producer: 200 ms blocks
      const freq = 1.2; // ~72 bpm
      const blockSec = 0.2;
      const blockN = Math.floor(fs * blockSec);

      pushTimerRef.current = setInterval(async () => {
        try {
          const x = new Float32Array(blockN);
          const baseIdx = Math.floor(((Date.now() - t0Ref.current) / 1000) * fs);
          for (let i = 0; i < blockN; i++) {
            const n = baseIdx + i;
            const t = n / fs;
            x[i] = 0.6 * Math.sin(2 * Math.PI * freq * t) + 0.05 * Math.sin(2 * Math.PI * 0.25 * t);
          }
          await analyzerRef.current!.push(x);
        } catch (e: any) {
          setError(e?.message ?? String(e));
        }
      }, blockSec * 1000);

      // Poller: 1 Hz
      pollTimerRef.current = setInterval(async () => {
        try {
          const elapsed = (Date.now() - t0Ref.current) / 1000;
          const m = await analyzerRef.current!.poll();
          if (m) {
            let bpm = m.bpm;
            if (m.rrList?.length) {
              const meanRR = m.rrList.reduce((s, v) => s + v, 0) / m.rrList.length;
              if (meanRR > 1e-6) bpm = 60000.0 / meanRR;
            }
            const poll = {
              t: elapsed,
              bpm,
              conf: (m as any)?.quality?.confidence ?? NaN,
              snr: (m as any)?.quality?.snrDb ?? NaN,
            };
            pollsRef.current.push(poll);
            setLast5(pollsRef.current.slice(-5));
            console.log(
              `RT poll t=${elapsed.toFixed(1)}s bpm=${bpm.toFixed(1)} conf=${(poll.conf as number)?.toFixed?.(2)} snr=${(poll.snr as number)?.toFixed?.(2)}dB`
            );
          }
        } catch (e: any) {
          setError(e?.message ?? String(e));
        }
      }, 1000);

      setRunning(true);
    } catch (e: any) {
      setError(e?.message ?? String(e));
      await stop();
    }
  }

  async function stop() {
    if (pushTimerRef.current) { clearInterval(pushTimerRef.current); pushTimerRef.current = null; }
    if (pollTimerRef.current) { clearInterval(pollTimerRef.current); pollTimerRef.current = null; }
    if (analyzerRef.current) {
      try { await analyzerRef.current.destroy(); } catch {}
      analyzerRef.current = null;
    }
    setRunning(false);
    // Compute medians excluding first 20 s
    const usable = pollsRef.current.filter(p => p.t >= 20);
    const bpm = median(usable.map(p => p.bpm));
    const conf = median(usable.map(p => p.conf));
    const snr = median(usable.map(p => p.snr));
    setMedians({ bpm, conf, snr });
    console.log('MEDIANS:', { bpm, conf, snr });
  }

  useEffect(() => {
    return () => { stop(); };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return (
    <ScrollView contentContainerStyle={styles.root}>
      <Text style={styles.title}>HeartPy Realtime Demo</Text>
      <View style={styles.row}>
        <Button title={running ? 'Stop' : 'Start'} onPress={running ? stop : start} />
      </View>
      {error && <Text style={styles.err}>Error: {error}</Text>}
      {!!medians && (
        <View style={styles.card}>
          <Text>Medians (t ≥ 20s):</Text>
          <Text>BPM: {medians.bpm?.toFixed?.(2)}</Text>
          <Text>Confidence: {medians.conf?.toFixed?.(2)}</Text>
          <Text>SNR (dB): {medians.snr?.toFixed?.(2)}</Text>
        </View>
      )}
      <Text style={styles.subtitle}>Last 5 Polls</Text>
      {last5.map((p, i) => (
        <Text key={i}>
          t={p.t.toFixed(1)}s bpm={p.bpm.toFixed(1)} conf={p.conf?.toFixed?.(2)} snr={p.snr?.toFixed?.(2)}dB
        </Text>
      ))}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  root: { padding: 16 },
  title: { fontSize: 18, fontWeight: '600', marginBottom: 8 },
  subtitle: { fontSize: 16, fontWeight: '600', marginTop: 16 },
  row: { flexDirection: 'row', alignItems: 'center', marginBottom: 12 },
  card: { padding: 12, borderRadius: 8, borderWidth: StyleSheet.hairlineWidth, marginBottom: 12 },
  err: { color: '#b00020', marginVertical: 8 },
});

