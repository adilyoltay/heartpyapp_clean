const {
  computeMetrics,
  parseAnalyzerEntries,
  parsePluginSamples,
} = require('../check_ppg_acceptance.js');

describe('PPG acceptance utilities', () => {
  it('computes warm-up timing and amplitude stats', () => {
    const series = [
      {hasResult: false, quality: {goodQuality: false}, snrDb: -10},
      {hasResult: true, quality: {goodQuality: true}, snrDb: -2},
      {hasResult: true, quality: {goodQuality: true}, snrDb: 4},
    ];
    const samples = new Float32Array([0.01, -0.01, 0.02, -0.02, 0.03, -0.03]);
    const metrics = computeMetrics(series, samples, 0.05);
    expect(metrics.warmUpSeconds).toBeCloseTo(0.05);
    expect(metrics.ampRms).toBeGreaterThan(0.01);
    expect(metrics.peakToPeak).toBeGreaterThan(0.05);
    expect(metrics.goodQualityFrac).toBeGreaterThan(0.5);
  });

  it('parses analyzer and plugin entries from logs', () => {
    const log = [
      'LOG  [PPGAnalyzer] Metrics polled {"hasResult": false, "quality": {"goodQuality": false}, "snrDb": -10}',
      'LOG  [PPGAnalyzer] Metrics polled {"hasResult": true, "quality": {"goodQuality": true}, "snrDb": 0}',
      '📸 PPGMeanPlugin gain=1.20 rms=0.0123 sample=0.0200',
    ].join('\n');
    const analyzerEntries = parseAnalyzerEntries(log);
    const pluginSamples = parsePluginSamples(log);
    expect(analyzerEntries).toHaveLength(2);
    expect(analyzerEntries[1].hasResult).toBe(true);
    expect(pluginSamples).toHaveLength(1);
    expect(pluginSamples[0]).toBeCloseTo(0.02, 5);
  });
});
