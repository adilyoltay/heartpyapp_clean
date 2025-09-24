import React from 'react';
import { View, Text, StyleSheet } from 'react-native';

type Segment = { index: number; startBeat: number; endBeat: number; totalBeats: number; rejectedBeats: number; accepted: boolean };

type Props = {
  peakListRaw: number[];
  binaryPeakMask: number[]; // 1 or 0
  binarySegments: Segment[];
  height?: number;
};

export const BinaryMaskDemo: React.FC<Props> = ({ peakListRaw, binaryPeakMask, binarySegments, height = 18 }) => {
  const n = Math.max(peakListRaw?.length || 0, binaryPeakMask?.length || 0) || 1;
  return (
    <View style={styles.container}>
      <Text style={styles.title}>Beat Acceptance Mask</Text>
      <View style={[styles.row, { height }]}> {
        binaryPeakMask.map((m, i) => (
          <View key={i} style={{ width: 2, height, backgroundColor: m ? '#2ecc71' : '#e74c3c', marginRight: 1 }} />
        ))
      } </View>
      <Text style={[styles.title, { marginTop: 12 }]}>Segments ({binarySegments.length})</Text>
      <View style={[styles.row, { height }]}> {
        binarySegments.map((s) => {
          const w = Math.max(1, s.totalBeats * 3);
          return (
            <View key={s.index} style={{ width: w, height, backgroundColor: s.accepted ? '#27ae60' : '#c0392b', marginRight: 2 }} />
          );
        })
      } </View>
      <Text style={styles.caption}>
        Total beats: {n} • Rejected beats: {binaryPeakMask.filter(v => v === 0).length}
      </Text>
    </View>
  );
};

const styles = StyleSheet.create({
  container: { padding: 12 },
  row: { flexDirection: 'row', alignItems: 'center', flexWrap: 'nowrap' },
  title: { fontWeight: '600', marginBottom: 6 },
  caption: { marginTop: 8, color: '#666' },
});

export default BinaryMaskDemo;

