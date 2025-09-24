// PPG Configuration Constants
// All configuration values in one place with type safety

// Environment-based debug flag
const isDebugMode = __DEV__ || process.env.NODE_ENV === 'development';

export const PPG_CONFIG = {
  // Sampling & buffering
  sampleRate: 30,
  // LF/HF için dinamik olarak seçilecek pencere değerleri
  analysisWindowBaseline: 360, // samples (~12 s @ 30 Hz)
  analysisWindowLfHf: 900, // samples (~30 s @ 30 Hz)
  analysisWindow: 900, // varsayılan olarak uzun pencereyi kullan (LF/HF modu)
  ringBufferSize: 1350, // analyzer/history buffer length (~45 s)
  waveformTailSamples: 180, // UI waveform tail displayed (~6 s)
  expectedBpm: 75, // average BPM used for segment rejection tuning

  // Reliability & gating (OPTIMIZED SNR thresholds)
  reliabilityThreshold: 0.6,
  snrDbThresholdUI: -2, // UI display threshold (-3'ten +33% iyileştirme)
  snrDbThresholdHaptic: -4, // Haptic feedback threshold (-6'dan +33% iyileştirme)
  snrDbThresholdReliable: -1, // High confidence threshold (yeni eklendi)
  snrDbThresholdPoor: -8, // Poor signal threshold (-8'den +25% iyileştirme)

  // Haptic feedback settings
  hapticDebounceMs: 600, // Minimum interval between haptic triggers (300ms'den 600ms'ye)
  hapticMinConfidence: 0.6, // Minimum confidence for haptic (0.5'ten 0.6'ya)
  hapticIntensity: 'impactHeavy', // Haptic intensity type

  // Adaptive SNR parameters (dinamik SNR ayarlaması)
  adaptiveSnrEnabled: true, // Enable adaptive SNR adjustments
  snrAdaptationRate: 0.1, // How quickly SNR thresholds adapt (0-1)
  snrStabilityWindowSec: 5.0, // Time window for SNR stability analysis
  snrMinThreshold: -5, // Minimum SNR threshold (absolute floor)
  snrMaxThreshold: 15, // Maximum SNR threshold (absolute ceiling)
  snrQualityWeight: 0.7, // Weight for SNR in overall quality score

  // Adaptive gain control (AGC)
  enableAGC: true,
  amplitudeTargetRMS: 0.02,
  agcAlphaRms: 0.05,
  agcAlphaGain: 0.1,
  agcGainMin: 0.5,
  agcGainMax: 20,

  // Analyzer warm-up / batching
  minSamplesBeforePollSec: 10.0, // Require ~10 s reservoir before polling native
  microBatchSamples: 16,
  microBatchLatencyMs: 150,

  // Native analyzer tightening
  rrOutlierPercent: 0.25,
  rrOutlierMinMs: 180,
  rrOutlierMaxMs: 320,
  refractoryMs: 350, // Minimum refractory period between peaks (ms)
  peakMinSpacingMs: 350,
  thresholdRR: true, // Enable HeartPy-style RR masking by default
  calcFreqEnabled: false, // LF/HF varsayılan olarak kapalı (detay modunda açılır)
  filterMode: 'butter-filtfilt', // Use zero-phase Butterworth by default
  filterOrder: 3, // Order for the Butterworth cascades (HP-like)

  // Camera preferences
  ppgChannel: 'red', // 'red' (torch) | 'green'
  roiBoxPct: 0.5, // central box (fraction of width/height)
  cameraTorchLevel: 1.0,

  // Camera processing optimization
  camera: {
    simdEnabled: true, // Enable SIMD optimizations for iOS BGRA processing
    performanceLogging: false, // Enable p50/p95 performance logging
  },

  // UI refresh cadence
  uiUpdateIntervalMs: 50,

  // UI feature flags & design tokens
  ui: {
    minimalMode: true,
    autoStart: true,
    progressiveDisclosure: false,
    adaptiveColor: true,
    breathingGuide: false,
    confidenceCollapseThreshold: 0.95,
    unifiedPrimaryCard: true,
    waveformGradient: true,
    tabletTwoColumnLayout: true,
    themeMode: 'system' as const,
  },

  debug: {
    enabled: isDebugMode, // Enable debug logs to see reservoir status
    sampleLogThrottle: 30,
    enableSnrLogging: false, // Surface C++ SNR log stream
    enableDetailedSnrLogging: false, // Verbose SNR logs (default kapalı, ihtiyaç halinde aç)
    enableAdaptivePsd: true, // Toggle adaptive PSD + fallbacks during QA
    enableSchedulerLogging: true, // Enable scheduler logging
    watchdogLogsEnabled: true, // Enable watchdog logs
  },
} as const;
