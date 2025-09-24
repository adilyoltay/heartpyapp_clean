import React, { useCallback, useMemo, useState } from 'react';
import { View, Text, Button, ScrollView, StyleSheet } from 'react-native';
import { analyzeAsync, type HeartPyOptions, type HeartPyResult } from 'react-native-heartpy';

function generateSinePPG(fs: number, seconds: number): number[] {
  const n = Math.max(1, Math.floor(fs * seconds));
  const out = new Array<number>(n);
  // 1.2 Hz heart rate (~72 bpm) + small noise
  const freq = 1.2;
  for (let i = 0; i < n; i++) {
    const t = i / fs;
    const s = Math.sin(2 * Math.PI * freq * t);
    const noise = 0.05 * Math.sin(2 * Math.PI * 0.25 * t);
    out[i] = 0.6 * s + noise;
  }
  return out;
}

export default function AppUsage() {
  const [fs] = useState(50);
  const [seconds, setSeconds] = useState(60); // try 300+ for stable FD
  const [loading, setLoading] = useState(false);
  const [res, setRes] = useState<HeartPyResult | null>(null);
  const [error, setError] = useState<string | null>(null);

  const isFDShort = useMemo(() => seconds < 240, [seconds]);

  const options: HeartPyOptions = useMemo(
    () => ({
      bandpass: { lowHz: 0.5, highHz: 5, order: 2 },
      welch: { wsizeSec: 240, overlap: 0.5 },
      peak: { refractoryMs: 250, thresholdScale: 0.5, bpmMin: 40, bpmMax: 180 },
      breathingAsBpm: false, // Hz to match HeartPy
    }),
    []
  );

  const run = useCallback(async (durSec: number) => {
    setSeconds(durSec);
    setLoading(true);
    setError(null);
    setRes(null);
    try {
      const signal = generateSinePPG(fs, durSec);
      const r = await analyzeAsync(signal, fs, options);
      setRes(r);
    } catch (e: any) {
      setError(e?.message ?? String(e));
    } finally {
      setLoading(false);
    }
  }, [fs, options]);

  return (
    <ScrollView contentContainerStyle={styles.root}>
      <Text style={styles.title}>HeartPy Enhanced — Quick Demo</Text>
      <View style={styles.row}>
        <Button title="Run 60s (TD only)" onPress={() => run(60)} disabled={loading} />
        <View style={{ width: 12 }} />
        <Button title="Run 300s (FD stable)" onPress={() => run(300)} disabled={loading} />
      </View>
      {loading && <Text style={styles.info}>Analyzing…</Text>}
      {error && <Text style={styles.err}>Error: {error}</Text>}
      {isFDShort && !loading && (
        <Text style={styles.warn}>Short window (&lt;240s): frequency metrics may be unreliable.</Text>
      )}
      {!!res && (
        <View style={styles.card}>
          <Text style={styles.subtitle}>Time Domain</Text>
          <Text>BPM: {res.bpm.toFixed(1)}</Text>
          <Text>SDNN: {res.sdnn.toFixed(2)} ms</Text>
          <Text>RMSSD: {res.rmssd.toFixed(2)} ms</Text>
          <Text>pNN50: {res.pnn50.toFixed(3)}</Text>

          <Text style={[styles.subtitle, { marginTop: 12 }]}>Frequency Domain</Text>
          <Text>VLF: {res.vlf?.toFixed?.(2) ?? String(res.vlf)}</Text>
          <Text>LF: {res.lf?.toFixed?.(2) ?? String(res.lf)}</Text>
          <Text>HF: {res.hf?.toFixed?.(2) ?? String(res.hf)}</Text>
          <Text>LF/HF: {res.lfhf?.toFixed?.(2) ?? String(res.lfhf)}</Text>

          <Text style={[styles.subtitle, { marginTop: 12 }]}>Breathing</Text>
          <Text>Breathing Rate (Hz): {res.breathingRate?.toFixed?.(3) ?? String(res.breathingRate)}</Text>
        </View>
      )}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  root: { padding: 16 },
  row: { flexDirection: 'row', alignItems: 'center', marginBottom: 12 },
  title: { fontSize: 18, fontWeight: '600', marginBottom: 8 },
  subtitle: { fontSize: 16, fontWeight: '600' },
  card: { padding: 12, borderRadius: 8, borderWidth: StyleSheet.hairlineWidth },
  info: { color: '#444', marginBottom: 8 },
  warn: { color: '#a36f00', marginBottom: 8 },
  err: { color: '#b00020', marginBottom: 8 },
});

