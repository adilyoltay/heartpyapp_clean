import React, {useEffect, useMemo, useRef, useState} from 'react';
import {Animated, Easing, StyleSheet, View} from 'react-native';
import ReactNativeHapticFeedback from 'react-native-haptic-feedback';
import {PPG_CONFIG} from '../core/PPGConfig';
import type {PPGAnalysisFrame, PPGState} from '../types/PPGTypes';
import SkiaWaveform from './SkiaWaveform';
import {PrimaryMetricsCard} from './PrimaryMetricsCard';
import {Card, Button, Typography, Badge} from './ui';
import {getBpmColor, getConfidenceColor} from '../styles/colors';
import {useResponsive} from '../styles/responsive';
import {useThemeColor} from '../hooks/useThemeColor';
import {SPACING} from '../theme/spacing';
import {BORDER_RADIUS, SHADOWS} from '../theme/layout';
import {FONT_SIZES} from '../theme/typography';

const isFiniteNumber = (value: unknown): value is number =>
  typeof value === 'number' && Number.isFinite(value);

// Bu component artık doğrudan C++'tan gelen senkronize edilmiş
// dalga formu snapshot'ını render eder.
type Props = {
  data: PPGAnalysisFrame; // Gelen veri artık tam bir analiz çerçevesi
  state: PPGState;
  onStart: () => void;
  onStop: () => void;
  layoutVariant?: 'single' | 'split';
};

const MAX_WAVEFORM_POINTS = 240;

// Minimalist Metric Card - sade ve sakin
type MinimalMetricCardProps = {
  label: string;
  value: string;
  valueColor?: string;
  fontSizeOverride?: number;
  containerWidth?: number | `${number}%`;
  containerMaxWidth?: number;
};

const MinimalMetricCard = React.memo(
  ({
    label,
    value,
    valueColor,
    fontSizeOverride,
    containerWidth,
    containerMaxWidth,
  }: MinimalMetricCardProps) => {
    const defaultValueColor = useThemeColor('textPrimary');
    const valueVariant = label === 'Confidence' ? 'headingM' : 'headingL';

    return (
      <Card
        padding="lg"
        radius="lg"
        style={[
          styles.minimalMetricCard,
          containerWidth ? {width: containerWidth} : null,
          containerMaxWidth ? {maxWidth: containerMaxWidth} : null,
        ]}>
        <Typography
          variant="caption"
          color="textSecondary"
          style={styles.minimalMetricLabel}>
          {label}
        </Typography>
        <Typography
          variant={valueVariant}
          weight={label === 'Confidence' ? 'medium' : 'semibold'}
          style={[
            styles.minimalMetricValue,
            fontSizeOverride ? {fontSize: fontSizeOverride} : null,
            valueColor ? {color: valueColor} : {color: defaultValueColor},
          ]}
          numberOfLines={1}
          ellipsizeMode="tail">
          {value}
        </Typography>
      </Card>
    );
  },
  (prev, next) =>
    prev.label === next.label &&
    prev.value === next.value &&
    prev.valueColor === next.valueColor &&
    prev.fontSizeOverride === next.fontSizeOverride &&
    prev.containerWidth === next.containerWidth &&
    prev.containerMaxWidth === next.containerMaxWidth,
);

const PPGDisplayComponent = ({
  data,
  state,
  onStart,
  onStop,
  layoutVariant = 'single',
}: Props): JSX.Element => {
  const r = useResponsive();
  const {metrics, waveform, warmupProgress} = data;
  const isIdle = state === 'idle';
  const isStarting = state === 'starting';
  const {bp, isLandscape, isTablet, ms, height} = r;

  const backgroundColor = useThemeColor('background');
  const surfaceColor = useThemeColor('surface');
  const surfaceMutedColor = useThemeColor('surfaceMuted');
  const textPrimary = useThemeColor('textPrimary');
  const borderColor = useThemeColor('border');
  const successColor = useThemeColor('success');
  const errorColor = useThemeColor('error');
  const inverseTextColor = useThemeColor('textInverse');

  const renderStatsRef = useRef({
    count: 0,
    lastLoggedCount: 0,
    lastLogTs: Date.now(),
  });
  renderStatsRef.current.count += 1;

  const isSplitLayout = layoutVariant === 'split';

  useEffect(() => {
    if (!PPG_CONFIG.debug.enabled) {
      return;
    }
    const now = Date.now();
    const elapsed = now - renderStatsRef.current.lastLogTs;
    if (elapsed >= 1_000) {
      const rendersSince =
        renderStatsRef.current.count - renderStatsRef.current.lastLoggedCount;
      console.log('[PPGDisplay] Render cadence', {
        rendersSince,
        elapsedMs: elapsed,
      });
      renderStatsRef.current.lastLoggedCount = renderStatsRef.current.count;
      renderStatsRef.current.lastLogTs = now;
    }
  });

  // --- GÜNCEL HAPTIC MANTIĞI ---
  const lastHapticPeakTsRef = useRef<number>(0);
  const lastHapticTimeRef = useRef<number>(0);

  const collapseThreshold = PPG_CONFIG.ui?.confidenceCollapseThreshold ?? 0.95;
  const collapseEnabled =
    Boolean(PPG_CONFIG.ui?.progressiveDisclosure) &&
    collapseThreshold > 0 &&
    collapseThreshold < 1;
  const collapseHysteresis = 0.02;
  const stabilityPolls = 3;
  const stabilityMs = 3_000;
  const reopenCooldownMs = 500;
  const [isConfidenceCollapsed, setIsConfidenceCollapsed] = useState(false);
  const stableSinceRef = useRef<number | null>(null);
  const consecutiveGoodRef = useRef(0);
  const lastDecisionRef = useRef(0);

  useEffect(() => {
    if (
      state !== 'running' ||
      !metrics?.peakTimestamps ||
      metrics.peakTimestamps.length === 0
    ) {
      return;
    }

    const pollId = metrics?.pollId ?? null;
    const pollTimestamp = metrics?.pollTimestamp ?? null;
    const verboseLogging = Boolean(PPG_CONFIG.debug.enableDetailedSnrLogging);
    const snrDebug = (metrics?.snrDebug ?? {}) as {
      originalSnrDb?: number | null;
      sanitizedSnrDb?: number;
      isFallbackUsed?: boolean;
      fallbackRatioPct?: number;
      fallbackReason?: string;
      f0Hz?: number | null;
      hardFallbackActive?: boolean;
      warmupActive?: boolean;
      sampleCount?: number | null;
    };
    const sanitizedSnrDb =
      typeof snrDebug?.sanitizedSnrDb === 'number'
        ? snrDebug.sanitizedSnrDb
        : metrics.snrDb ?? -10;
    const originalSnrDb =
      typeof snrDebug?.originalSnrDb === 'number'
        ? snrDebug.originalSnrDb
        : null;
    const snrFallbackUsed = !!snrDebug?.isFallbackUsed;

    const resolvedSignalQuality = (metrics?.signalQuality ??
      metrics?.quality?.signalQuality ??
      'unknown') as string;
    const confidenceOk =
      (metrics.confidence ?? 0) >= PPG_CONFIG.hapticMinConfidence;
    const snrOk = sanitizedSnrDb > PPG_CONFIG.snrDbThresholdHaptic;
    const qualityOk = resolvedSignalQuality === 'good';
    const isReliableForHaptic = confidenceOk && snrOk && qualityOk;

    if (!isReliableForHaptic) {
      if (PPG_CONFIG.debug.enabled && verboseLogging) {
        const latestPeakTs = Math.max(...metrics.peakTimestamps);
        const now = Date.now();
        console.log('[PPGDisplay] Haptic guard failed', {
          pollId,
          pollTimestamp,
          state,
          confidence: metrics.confidence,
          snrDb: sanitizedSnrDb,
          originalSnrDb,
          snrFallbackUsed,
          signalQuality: resolvedSignalQuality,
          snrFallbackRatioPct: snrDebug.fallbackRatioPct,
          snrFallbackReason: snrDebug.fallbackReason,
          f0Hz: snrDebug.f0Hz,
          hardFallbackActive: snrDebug.hardFallbackActive,
          warmupActive: snrDebug.warmupActive,
          snrSampleCount: snrDebug.sampleCount,
          thresholds: {
            minConfidence: PPG_CONFIG.hapticMinConfidence,
            snrThreshold: PPG_CONFIG.snrDbThresholdHaptic,
          },
          flags: {
            confidenceOk,
            snrOk,
            qualityOk,
          },
          latestPeakTs,
          now,
          deltaMs: now - latestPeakTs,
        });
      }
      if (PPG_CONFIG.debug.enabled && !verboseLogging) {
        console.log('[PPGDisplay] Haptic guard skipped', {
          pollId,
          confidence: metrics.confidence,
          snrDb: sanitizedSnrDb,
          signalQuality: resolvedSignalQuality,
        });
      }
      return;
    }

    const now = Date.now();
    const MIN_INTERVAL_MS = PPG_CONFIG.hapticDebounceMs; // Config'den al

    const latestPeakTs = Math.max(...metrics.peakTimestamps);
    const peakDeltaMs = now - latestPeakTs;
    const timeSinceLastTrigger = now - lastHapticTimeRef.current;
    const isNewPeak = latestPeakTs > lastHapticPeakTsRef.current;
    const passesDebounce = timeSinceLastTrigger > MIN_INTERVAL_MS;
    const pollToTriggerMs = pollTimestamp ? now - pollTimestamp : null;

    if (PPG_CONFIG.debug.enabled && verboseLogging) {
      console.log('[PPGDisplay] Haptic timing eval', {
        pollId,
        latestPeakTs,
        lastTriggeredPeak: lastHapticPeakTsRef.current,
        isNewPeak,
        timeSinceLastTrigger,
        debounceMs: MIN_INTERVAL_MS,
        peakDeltaMs,
        passesDebounce,
        deviceNow: now,
        pollTimestamp,
        pollToTriggerMs,
      });
    }

    if (!isNewPeak) {
      if (PPG_CONFIG.debug.enabled && verboseLogging) {
        console.log('[PPGDisplay] Haptic skipped (stale peak)', {
          pollId,
          latestPeakTs,
          lastTriggeredPeak: lastHapticPeakTsRef.current,
        });
      }
      return;
    }

    if (!passesDebounce) {
      if (PPG_CONFIG.debug.enabled && verboseLogging) {
        console.log('[PPGDisplay] Haptic skipped (debounce active)', {
          pollId,
          timeSinceLastTrigger,
          debounceMs: MIN_INTERVAL_MS,
          latestPeakTs,
        });
      }
      return;
    }

    console.log(
      '[PPGDisplay] HAPTIC TRIGGERED for peak timestamp:',
      latestPeakTs,
      {
        pollId,
        pollTimestamp,
        pollToTriggerMs,
        confidence: metrics.confidence,
        snrDb: sanitizedSnrDb,
        originalSnrDb,
        snrFallbackUsed,
        snrFallbackRatioPct: snrDebug.fallbackRatioPct,
        snrFallbackReason: snrDebug.fallbackReason,
        f0Hz: snrDebug.f0Hz,
        hardFallbackActive: snrDebug.hardFallbackActive,
        warmupActive: snrDebug.warmupActive,
        snrSampleCount: snrDebug.sampleCount,
        signalQuality: resolvedSignalQuality,
        timeSinceLast: timeSinceLastTrigger,
        debounceMs: MIN_INTERVAL_MS,
        peakDeltaMs,
        deviceNow: now,
      },
    );

    // Haptic feedback
    console.log('[PPGDisplay] ReactNativeHapticFeedback.trigger', {
      intensity: PPG_CONFIG.hapticIntensity,
      pollId,
    });
    ReactNativeHapticFeedback.trigger(PPG_CONFIG.hapticIntensity, {
      enableVibrateFallback: true,
      ignoreAndroidSystemSettings: true,
    });

    lastHapticPeakTsRef.current = latestPeakTs;
    lastHapticTimeRef.current = now;
  }, [metrics, state]);

  // --- GÜNCEL ve BASİTLEŞTİRİLMİŞ MARKER MANTIĞI ---
  const peakTimestampSet = useMemo(() => {
    // Gelen metriklerdeki tepe noktası zaman damgalarını bir Set'e koy
    return new Set<number>(metrics?.peakTimestamps || []);
  }, [metrics]);

  const displayWaveform = useMemo(() => {
    if (!waveform || waveform.length <= MAX_WAVEFORM_POINTS) {
      return waveform;
    }
    const stride = Math.ceil(waveform.length / MAX_WAVEFORM_POINTS);
    const sampled: Array<{value: number; timestamp: number}> = [];
    for (let i = 0; i < waveform.length; i += stride) {
      sampled.push(waveform[i]);
      if (sampled.length >= MAX_WAVEFORM_POINTS) {
        break;
      }
    }
    return sampled;
  }, [waveform]);

  const waveformPoints = displayWaveform ?? [];

  useEffect(() => {
    if (PPG_CONFIG.debug.enableSchedulerLogging) {
      console.log('[PPGDisplay] Waveform sample count', {
        points: waveformPoints.length,
      });
    }
  }, [waveformPoints.length]);

  const metricsViewModel = useMemo(() => {
    const bpmRaw = metrics?.bpm;
    const snrRaw = metrics?.snrDb ?? metrics?.quality?.snrDb;
    const confidenceRaw = metrics?.confidence ?? metrics?.quality?.confidence;
    const bpmNumber = isFiniteNumber(bpmRaw) ? bpmRaw : undefined;
    const snrNumber = isFiniteNumber(snrRaw) ? snrRaw : undefined;
    const confidenceNumber = isFiniteNumber(confidenceRaw)
      ? confidenceRaw
      : undefined;

    return {
      bpmNumber,
      snrNumber,
      confidenceNumber,
      bpmText: bpmNumber !== undefined ? bpmNumber.toFixed(1) : '--',
      snrText: snrNumber !== undefined ? snrNumber.toFixed(2) : '--',
      confidenceText:
        confidenceNumber !== undefined ? confidenceNumber.toFixed(2) : '--',
    };
  }, [
    metrics?.bpm,
    metrics?.confidence,
    metrics?.quality?.confidence,
    metrics?.quality?.snrDb,
    metrics?.snrDb,
  ]);

  useEffect(() => {
    if (!collapseEnabled) {
      if (isConfidenceCollapsed) {
        setIsConfidenceCollapsed(false);
      }
      stableSinceRef.current = null;
      consecutiveGoodRef.current = 0;
      return;
    }

    const confidence = metricsViewModel.confidenceNumber;
    const snr = metricsViewModel.snrNumber;
    const signalQuality =
      metrics?.signalQuality ?? metrics?.quality?.signalQuality ?? 'unknown';

    const goodQuality =
      state === 'running' &&
      signalQuality === 'good' &&
      isFiniteNumber(confidence) &&
      isFiniteNumber(snr) &&
      snr >= (PPG_CONFIG.snrDbThresholdUI ?? -Infinity);

    if (!goodQuality || !isFiniteNumber(confidence) || !isFiniteNumber(snr)) {
      consecutiveGoodRef.current = 0;
      stableSinceRef.current = null;
      if (isConfidenceCollapsed) {
        setIsConfidenceCollapsed(false);
        lastDecisionRef.current = Date.now();
      }
      return;
    }

    const now = Date.now();
    const upper = collapseThreshold;
    const lower = collapseThreshold - collapseHysteresis;
    const lastDecisionAgo = now - lastDecisionRef.current;

    if (confidence >= upper) {
      consecutiveGoodRef.current += 1;
      if (stableSinceRef.current == null) {
        stableSinceRef.current = now;
      }
      const stableDuration = now - stableSinceRef.current;
      if (
        !isConfidenceCollapsed &&
        (consecutiveGoodRef.current >= stabilityPolls ||
          stableDuration >= stabilityMs)
      ) {
        setIsConfidenceCollapsed(true);
        lastDecisionRef.current = now;
      }
    } else if (confidence <= lower) {
      consecutiveGoodRef.current = 0;
      stableSinceRef.current = null;
      if (isConfidenceCollapsed && lastDecisionAgo >= reopenCooldownMs) {
        setIsConfidenceCollapsed(false);
        lastDecisionRef.current = now;
      }
    }
  }, [
    collapseEnabled,
    collapseThreshold,
    metrics?.quality?.signalQuality,
    metrics?.signalQuality,
    metricsViewModel.confidenceNumber,
    metricsViewModel.snrNumber,
    isConfidenceCollapsed,
    state,
  ]);

  const confidencePercentText = useMemo(() => {
    if (!isFiniteNumber(metricsViewModel.confidenceNumber)) {
      return '--';
    }
    return `${(metricsViewModel.confidenceNumber * 100).toFixed(1)}%`;
  }, [metricsViewModel.confidenceNumber]);

  const showConfidenceCard = !collapseEnabled || !isConfidenceCollapsed;

  const primaryMetrics = useMemo(() => {
    const cards = [
      {
        key: 'bpm',
        label: 'Heart Rate',
        value:
          metricsViewModel.bpmNumber !== undefined
            ? `${metricsViewModel.bpmText} BPM`
            : '--',
        valueColor: getBpmColor(metricsViewModel.bpmNumber ?? 0),
      },
    ];

    if (showConfidenceCard) {
      cards.push({
        key: 'confidence',
        label: 'Confidence',
        value: confidencePercentText,
        valueColor: getConfidenceColor(metricsViewModel.confidenceNumber ?? 0),
      });
    }

    return cards;
  }, [
    confidencePercentText,
    metricsViewModel.bpmNumber,
    metricsViewModel.bpmText,
    metricsViewModel.confidenceNumber,
    showConfidenceCard,
  ]);

  const confidenceBadgeText = showConfidenceCard ? null : confidencePercentText;

  const detailMetrics = useMemo(() => {
    const formatValue = (
      value: number | null | undefined,
      formatter: (val: number) => string,
    ) => (isFiniteNumber(value) ? formatter(value) : '--');

    return [
      {key: 'snr', label: 'SNR (dB)', value: metricsViewModel.snrText},
      {
        key: 'sdnn',
        label: 'SDNN',
        value: formatValue(metrics?.sdnn, val => `${val.toFixed(0)} ms`),
      },
      {
        key: 'rmssd',
        label: 'RMSSD',
        value: formatValue(metrics?.rmssd, val => `${val.toFixed(0)} ms`),
      },
      {
        key: 'pnn50',
        label: 'pNN50',
        value: formatValue(metrics?.pnn50, val => `${(val * 100).toFixed(0)}%`),
      },
      {
        key: 'lfhf',
        label: 'LF/HF',
        value: formatValue(metrics?.lfhf, val => val.toFixed(2)),
      },
    ];
  }, [
    metrics?.lfhf,
    metrics?.pnn50,
    metrics?.rmssd,
    metrics?.sdnn,
    metricsViewModel.snrText,
  ]);

  const showBreathingGuide = useMemo(() => {
    if (!PPG_CONFIG.ui?.breathingGuide) {
      return false;
    }
    if (state !== 'running') {
      return false;
    }
    const confidence = metricsViewModel.confidenceNumber;
    if (!isFiniteNumber(confidence) || confidence < 0.7) {
      return false;
    }
    const snr = metricsViewModel.snrNumber;
    if (
      !isFiniteNumber(snr) ||
      snr <= (PPG_CONFIG.snrDbThresholdUI ?? -Infinity)
    ) {
      return false;
    }
    return true;
  }, [metricsViewModel.confidenceNumber, metricsViewModel.snrNumber, state]);

  // Responsive derived sizes
  const bpmFontSize = ms(FONT_SIZES.headingXL, isTablet ? 0.5 : 0.35);
  const confidenceFontSize = ms(FONT_SIZES.headingM, isTablet ? 0.45 : 0.35);
  const waveformHeight = isTablet
    ? isLandscape
      ? Math.max(220, Math.round(height * 0.35))
      : 220
    : isLandscape
    ? 180
    : 160;
  const cardMaxWidth = isTablet ? 420 : 320;
  const cardWidthPct: `${number}%` = isTablet ? '65%' : '80%';
  const detailCardBasis: `${number}%` =
    bp === 'xl' ? '22%' : bp === 'lg' ? '30%' : isLandscape ? '40%' : '48%';
  const strokeWidth = isTablet ? 3 : 2;
  const useUnifiedPrimaryCard = Boolean(
    PPG_CONFIG.ui?.unifiedPrimaryCard ?? true,
  );
  const useWaveformGradient = Boolean(PPG_CONFIG.ui?.waveformGradient ?? true);

  const waveformGradientSettings = useMemo(
    () =>
      useWaveformGradient
        ? {
            from: 'rgba(16, 185, 129, 0.16)',
            to: 'rgba(59, 130, 246, 0.06)',
            opacity: 1,
          }
        : undefined,
    [useWaveformGradient],
  );

  const primaryMetricsSpacingStyle = useMemo(
    () => ({marginBottom: ms(SPACING.xl)}),
    [ms],
  );

  const primaryCardWidthStyle = useUnifiedPrimaryCard
    ? {width: cardWidthPct, maxWidth: cardMaxWidth}
    : undefined;

  const detailMetricCardStyle = useMemo(
    () => ({
      flexBasis: detailCardBasis,
      maxWidth: isTablet ? 240 : 200,
      paddingHorizontal: ms(SPACING.sm),
      paddingVertical: ms(SPACING.sm),
      marginHorizontal: ms(SPACING.xs) / 2,
      marginVertical: ms(SPACING.xs) / 2,
    }),
    [detailCardBasis, isTablet, ms],
  );

  const detailGridGutterStyle = useMemo(
    () => ({marginHorizontal: -(ms(SPACING.xs) / 2)}),
    [ms],
  );

  const detailLabelSpacing = useMemo(
    () => ({marginBottom: ms(SPACING.xs) / 2}),
    [ms],
  );

  return (
    <View
      style={[
        styles.minimalContainer,
        {backgroundColor},
        isSplitLayout ? styles.minimalContainerSplit : null,
      ]}>
      {/* Minimalist Metrics - BPM & Confidence */}
      <View
        style={[
          styles.minimalMetricsContainer,
          primaryMetricsSpacingStyle,
          isSplitLayout ? styles.minimalMetricsContainerSplit : null,
        ]}>
        {useUnifiedPrimaryCard ? (
          <View
            style={[
              styles.primaryCardWrapper,
              primaryCardWidthStyle,
              isSplitLayout ? styles.primaryCardWrapperSplit : null,
            ]}>
            <PrimaryMetricsCard
              bpm={metricsViewModel.bpmNumber}
              bpmText={
                metricsViewModel.bpmNumber !== undefined
                  ? metricsViewModel.bpmText
                  : '--'
              }
              confidenceText={confidencePercentText}
              bpmColor={getBpmColor(metricsViewModel.bpmNumber ?? 0)}
              confidenceColor={getConfidenceColor(
                metricsViewModel.confidenceNumber ?? 0,
              )}
              showConfidence={showConfidenceCard}
              breakpoint={r.bp}
              isLandscape={r.isLandscape}
              ms={r.ms}
            />
          </View>
        ) : (
          <>
            {primaryMetrics.map(metric => (
              <MinimalMetricCard
                key={metric.key}
                label={metric.label}
                value={metric.value}
                valueColor={metric.valueColor}
                fontSizeOverride={
                  metric.key === 'bpm' ? bpmFontSize : confidenceFontSize
                }
                containerWidth={cardWidthPct}
                containerMaxWidth={cardMaxWidth}
              />
            ))}
            {confidenceBadgeText ? (
              <Badge
                label={`Confidence ${confidenceBadgeText}`}
                size="sm"
                textColorOverride={getConfidenceColor(
                  metricsViewModel.confidenceNumber ?? 0,
                )}
                backgroundOverride={surfaceMutedColor}
                style={[
                  styles.confidenceBadge,
                  isSplitLayout ? styles.confidenceBadgeSplit : null,
                ]}
              />
            ) : null}
          </>
        )}
      </View>

      {/* Warm-up Progress Bar */}
      {warmupProgress?.isWarmingUp && (
        <Card
          padding="md"
          radius="md"
          style={styles.warmupContainer}
          shadow="subtle">
          <Typography
            variant="bodyM"
            weight="medium"
            style={[
              styles.warmupText,
              styles.centeredText,
              {color: textPrimary},
            ]}>
            Initializing... {warmupProgress.progress.toFixed(0)}%
          </Typography>
          <View
            style={[styles.warmupProgressBar, {backgroundColor: borderColor}]}>
            {(() => {
              const boundedProgress = Math.min(
                100,
                Math.max(0, warmupProgress.progress ?? 0),
              );
              const widthPercent = `${boundedProgress}%` as `${number}%`;
              return (
                <View
                  style={[
                    styles.warmupProgressFill,
                    {
                      width: widthPercent,
                      backgroundColor: successColor,
                    },
                  ]}
                />
              );
            })()}
          </View>
          <Typography
            variant="caption"
            color="textSecondary"
            style={[styles.warmupSubtext, styles.centeredText]}>
            {warmupProgress.samplesPushed} / {warmupProgress.samplesRequired}{' '}
            samples
          </Typography>
        </Card>
      )}

      <View
        style={[
          styles.detailSection,
          isSplitLayout ? styles.detailSectionSplit : null,
        ]}>
        <Typography
          variant="bodyM"
          weight="semibold"
          style={[
            styles.detailTitle,
            styles.centeredText,
            {color: textPrimary},
          ]}>
          Advanced Metrics
        </Typography>
        <View style={[styles.detailMetricsGrid, detailGridGutterStyle]}>
          {detailMetrics.map(metric => (
            <Card
              key={metric.key}
              variant="outlined"
              padding="md"
              radius="md"
              shadow="none"
              style={[styles.detailMetricCard, detailMetricCardStyle]}>
              <Typography
                variant="caption"
                color="textSecondary"
                style={detailLabelSpacing}
                numberOfLines={1}
                ellipsizeMode="tail">
                {metric.label}
              </Typography>
              <Typography
                variant="bodyM"
                weight="medium"
                numberOfLines={1}
                ellipsizeMode="tail"
                style={{color: textPrimary}}>
                {metric.value}
              </Typography>
            </Card>
          ))}
        </View>
      </View>

      {/* Waveform - minimal ve sakin */}
      <View
        style={[
          styles.minimalWaveform,
          {height: waveformHeight, backgroundColor: surfaceColor},
        ]}>
        <SkiaWaveform
          points={waveformPoints}
          peaks={peakTimestampSet}
          strokeWidth={strokeWidth}
          backgroundGradient={waveformGradientSettings}
        />
      </View>

      {showBreathingGuide ? <_BreathingGuide /> : null}

      {/* Start/Stop Button - minimal */}
      <View
        style={[
          styles.minimalControls,
          isSplitLayout ? styles.minimalControlsSplit : null,
        ]}>
        <Button
          title={isIdle ? 'Start' : 'Stop'}
          onPress={isIdle ? onStart : onStop}
          loading={isStarting}
          disabled={isStarting}
          size="lg"
          textColorOverride={inverseTextColor}
          backgroundOverride={isIdle ? successColor : errorColor}
          borderColorOverride={isIdle ? successColor : errorColor}
          style={styles.minimalButton}
        />
      </View>
    </View>
  );
};

export const PPGDisplay = React.memo(PPGDisplayComponent);

// Simple, subtle breathing guide (inhale/exhale) for relaxation
const _BreathingGuide = () => {
  const anim = React.useRef(new Animated.Value(0)).current;
  const accentColor = useThemeColor('primary');
  const secondaryText = useThemeColor('textSecondary');

  React.useEffect(() => {
    const loop = () => {
      Animated.sequence([
        Animated.timing(anim, {
          toValue: 1,
          duration: 4000,
          easing: Easing.inOut(Easing.quad),
          useNativeDriver: true,
        }),
        Animated.timing(anim, {
          toValue: 0,
          duration: 4000,
          easing: Easing.inOut(Easing.quad),
          useNativeDriver: true,
        }),
      ]).start(({finished}) => {
        if (finished) {
          loop();
        }
      });
    };
    loop();
  }, [anim]);

  const scale = anim.interpolate({inputRange: [0, 1], outputRange: [0.9, 1.1]});
  const opacity = anim.interpolate({
    inputRange: [0, 1],
    outputRange: [0.3, 0.9],
  });

  return (
    <View style={breathingStyles.breathingWrapper}>
      <Animated.View
        style={[
          breathingStyles.breathingDot,
          {transform: [{scale}], opacity, backgroundColor: accentColor},
        ]}
      />
      <Typography variant="caption" style={{color: secondaryText}}>
        Breathe
      </Typography>
    </View>
  );
};

const styles = StyleSheet.create({
  minimalContainer: {
    flex: 1,
    padding: SPACING.md,
    paddingTop: SPACING.xxl,
    alignItems: 'center',
    justifyContent: 'flex-start',
  },
  minimalContainerSplit: {
    alignItems: 'stretch',
  },
  minimalMetricsContainer: {
    alignItems: 'center',
    marginBottom: SPACING.xl,
    width: '100%',
    justifyContent: 'center',
  },
  minimalMetricsContainerSplit: {
    alignItems: 'flex-start',
  },
  primaryCardWrapper: {
    alignSelf: 'center',
  },
  primaryCardWrapperSplit: {
    alignSelf: 'flex-start',
  },
  confidenceBadge: {
    marginTop: SPACING.sm,
    alignSelf: 'center',
  },
  confidenceBadgeSplit: {
    alignSelf: 'flex-start',
  },
  minimalMetricCard: {
    alignSelf: 'center',
    width: '80%',
    maxWidth: 360,
    marginBottom: SPACING.lg,
  },
  minimalMetricLabel: {
    textAlign: 'center',
    marginBottom: SPACING.sm,
  },
  minimalMetricValue: {
    textAlign: 'center',
  },
  centeredText: {
    textAlign: 'center',
  },
  minimalWaveform: {
    width: '100%',
    marginHorizontal: SPACING.sm,
    marginBottom: SPACING.xl,
    borderRadius: BORDER_RADIUS.md,
    overflow: 'hidden',
    ...SHADOWS.subtle,
  },
  minimalControls: {
    alignItems: 'center',
    marginTop: SPACING.xl,
    width: '100%',
  },
  minimalControlsSplit: {
    alignItems: 'flex-start',
  },
  minimalButton: {
    minWidth: 160,
  },
  warmupContainer: {
    marginHorizontal: SPACING.lg,
    marginVertical: SPACING.md,
  },
  warmupText: {
    marginBottom: SPACING.sm,
  },
  warmupProgressBar: {
    height: 8,
    borderRadius: 4,
    overflow: 'hidden',
    marginBottom: SPACING.sm,
  },
  warmupProgressFill: {
    height: '100%',
    borderRadius: 4,
  },
  warmupSubtext: {
    marginTop: SPACING.xs,
  },
  detailSection: {
    width: '100%',
    paddingHorizontal: SPACING.md,
    marginBottom: SPACING.lg,
  },
  detailSectionSplit: {
    alignSelf: 'flex-start',
    paddingHorizontal: SPACING.sm,
  },
  detailTitle: {
    marginBottom: SPACING.sm,
  },
  detailMetricsGrid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'center',
    alignItems: 'stretch',
  },
  detailMetricCard: {
    minWidth: 120,
    flexGrow: 1,
  },
});

const breathingStyles = StyleSheet.create({
  breathingWrapper: {
    alignSelf: 'center',
    alignItems: 'center',
    justifyContent: 'center',
    marginTop: 4,
    marginBottom: 4,
  },
  breathingDot: {
    width: 24,
    height: 24,
    borderRadius: 12,
    marginBottom: 4,
  },
});
