import React, {useEffect, useMemo, useRef, useState} from 'react';
import {
  AccessibilityInfo,
  Animated,
  Easing,
  findNodeHandle,
  Modal,
  Pressable,
  ScrollView,
  StyleSheet,
  Switch,
  View,
} from 'react-native';
import {PPGParameterControls} from './PPGParameterControls';
import {Card, Typography, IconButton, SettingRow, Badge} from './ui';
import {useThemeColor} from '../hooks/useThemeColor';
import {useResponsive} from '../styles/responsive';
import {SPACING} from '../theme/spacing';
import {BORDER_RADIUS, SHADOWS} from '../theme/layout';
import type {AnalyzerTuningOptions} from '../core/PPGAnalyzer';
import {DEFAULT_ANALYZER_OPTIONS} from '../core/PPGAnalyzer';

const clamp = (value: number, min: number, max: number) =>
  Math.min(Math.max(value, min), max);

interface HiddenSettingsProps {
  isVisible: boolean;
  onClose: () => void;
  options: AnalyzerTuningOptions;
  onChange: (options: Partial<AnalyzerTuningOptions>) => void;
  onReset: () => void;
  disabled?: boolean;
}

export const HiddenSettings: React.FC<HiddenSettingsProps> = ({
  isVisible,
  onClose,
  options,
  onChange,
  onReset,
  disabled = false,
}) => {
  const {width, height, ms} = useResponsive();
  const panelWidth = useMemo(
    () => Math.min(width * 0.95, ms(460, 0.6)),
    [ms, width],
  );
  const sheetHeight = useMemo(
    () => Math.min(height * 0.9, ms(560, 0.6)),
    [height, ms],
  );

  const slideAnim = useRef(new Animated.Value(1)).current;
  const overlayOpacity = useRef(new Animated.Value(0)).current;
  const [showAdvanced, setShowAdvanced] = useState(false);
  const headerRef = useRef<View>(null);

  useEffect(() => {
    slideAnim.setValue(1);
  }, [sheetHeight, slideAnim]);

  const overlayColor = useThemeColor('overlay');
  const surfaceColor = useThemeColor('surface');
  const surfaceMutedColor = useThemeColor('surfaceMuted');
  const borderColor = useThemeColor('border');
  const primaryColor = useThemeColor('primary');
  const textPrimaryColor = useThemeColor('textPrimary');
  const textInverseColor = useThemeColor('textInverse');

  const rowActionGapStyle = useMemo(() => ({gap: ms(SPACING.xs)}), [ms]);

  const basicSettingContentStyle = useMemo(
    () => [
      styles.settingSurfaceBase,
      styles.settingSurfaceSm,
      {backgroundColor: surfaceMutedColor, borderColor},
    ],
    [borderColor, surfaceMutedColor],
  );

  const filterModeChipSpacing = useMemo(
    () => ({
      marginRight: ms(SPACING.xs),
      marginBottom: ms(SPACING.xs),
    }),
    [ms],
  );

  const advancedTriggerContentStyle = useMemo(
    () => [
      styles.advancedTriggerContent,
      {
        backgroundColor: surfaceMutedColor,
        borderColor,
        paddingHorizontal: ms(SPACING.md),
        paddingVertical: ms(SPACING.sm),
      },
    ],
    [borderColor, ms, surfaceMutedColor],
  );

  const basicCardStyle = useMemo(
    () => [styles.basicCard, {gap: ms(SPACING.sm), borderColor}],
    [borderColor, ms],
  );

  const footerSpacerStyle = useMemo(() => ({height: ms(SPACING.xl)}), [ms]);

  const dividerStyle = useMemo(
    () => ({
      height: 1,
      width: '100%' as const,
      backgroundColor: borderColor,
      marginVertical: ms(SPACING.sm),
    }),
    [borderColor, ms],
  );

  const thresholdScale = useMemo(() => {
    const value = options.thresholdScale;
    return typeof value === 'number'
      ? value
      : DEFAULT_ANALYZER_OPTIONS.thresholdScale ?? 0.5;
  }, [options.thresholdScale]);

  const refractory = useMemo(() => {
    const value = options.refractoryMs;
    return typeof value === 'number'
      ? value
      : DEFAULT_ANALYZER_OPTIONS.refractoryMs ?? 350;
  }, [options.refractoryMs]);

  const filterMode =
    options.filterMode ?? DEFAULT_ANALYZER_OPTIONS.filterMode ?? 'auto';
  const filterOrder =
    options.filterOrder ?? DEFAULT_ANALYZER_OPTIONS.filterOrder ?? 3;

  const advancedKeys: Array<keyof AnalyzerTuningOptions> = useMemo(
    () => [
      'pHalfOverFundThresholdSoft',
      'welchWsizeSec',
      'nfft',
      'highCutoffHz',
      'snrTauSec',
      'snrActiveTauSec',
    ],
    [],
  );

  useEffect(() => {
    if (isVisible) {
      Animated.parallel([
        Animated.timing(slideAnim, {
          toValue: 0,
          duration: 320,
          easing: Easing.out(Easing.quad),
          useNativeDriver: true,
        }),
        Animated.timing(overlayOpacity, {
          toValue: 1,
          duration: 280,
          easing: Easing.out(Easing.quad),
          useNativeDriver: true,
        }),
      ]).start();
      const announce = setTimeout(() => {
        AccessibilityInfo.announceForAccessibility?.('Settings sheet opened');
        const node = headerRef.current
          ? findNodeHandle(headerRef.current)
          : null;
        if (node) {
          AccessibilityInfo.setAccessibilityFocus?.(node);
        }
      }, 400);
      return () => clearTimeout(announce);
    } else {
      Animated.parallel([
        Animated.timing(slideAnim, {
          toValue: 1,
          duration: 220,
          easing: Easing.in(Easing.quad),
          useNativeDriver: true,
        }),
        Animated.timing(overlayOpacity, {
          toValue: 0,
          duration: 200,
          easing: Easing.in(Easing.quad),
          useNativeDriver: true,
        }),
      ]).start(() => {
        setShowAdvanced(false);
      });
    }
  }, [isVisible, overlayOpacity, slideAnim, width]);

  if (!isVisible) {
    return null;
  }

  const renderStepperRow = (
    label: string,
    value: number,
    key: keyof AnalyzerTuningOptions,
    config: {min: number; max: number; step: number; decimals?: number},
    format?: (val: number) => string,
    transform?: (val: number) => number,
  ) => {
    const displayValue = format
      ? format(value)
      : value.toFixed(config.decimals ?? 2);

    const adjust = (direction: -1 | 1) => {
      if (disabled) {
        return;
      }
      const rounded = Number(
        (value + direction * config.step).toFixed(config.decimals ?? 3),
      );
      const next = clamp(rounded, config.min, config.max);
      onChange({
        [key]: transform ? transform(next) : next,
      });
    };

    return (
      <SettingRow
        key={key as string}
        label={label}
        hint={displayValue}
        rightSlot={
          <View style={[styles.rowActions, rowActionGapStyle]}>
            <IconButton
              label="−"
              size="sm"
              variant="outline"
              onPress={() => adjust(-1)}
              disabled={disabled}
              accessibilityLabel={`Decrease ${label}`}
            />
            <IconButton
              label="+"
              size="sm"
              variant="outline"
              onPress={() => adjust(1)}
              disabled={disabled}
              accessibilityLabel={`Increase ${label}`}
            />
          </View>
        }
        contentStyle={basicSettingContentStyle}
      />
    );
  };

  const filterModes: Array<{
    key: AnalyzerTuningOptions['filterMode'];
    label: string;
  }> = [
    {key: 'auto', label: 'Auto'},
    {key: 'butter-filtfilt', label: 'Butter (Zero-phase)'},
  ];

  return (
    <Modal
      transparent
      visible={isVisible}
      animationType="none"
      onRequestClose={onClose}
      statusBarTranslucent
      presentationStyle="overFullScreen">
      <View style={styles.modalRoot} accessibilityViewIsModal>
        <Animated.View
          style={[
            styles.overlay,
            {
              backgroundColor: overlayColor,
              opacity: overlayOpacity.interpolate({
                inputRange: [0, 1],
                outputRange: [0, 0.6],
              }),
            },
          ]}
          pointerEvents={isVisible ? 'auto' : 'none'}>
          <Pressable
            style={styles.overlayTouchable}
            onPress={onClose}
            accessibilityRole="button"
            accessibilityLabel="Close settings"
          />
        </Animated.View>

        <Animated.View
          style={[
            styles.settingsPanel,
            {
              width: panelWidth,
              maxHeight: sheetHeight,
              backgroundColor: surfaceColor,
              borderTopLeftRadius: BORDER_RADIUS.lg,
              borderTopRightRadius: BORDER_RADIUS.lg,
              ...SHADOWS.medium,
              transform: [
                {
                  translateY: slideAnim.interpolate({
                    inputRange: [0, 1],
                    outputRange: [0, sheetHeight],
                  }),
                },
              ],
            },
          ]}>
          <View
            ref={headerRef}
            style={[
              styles.header,
              {
                padding: ms(SPACING.md),
                borderBottomColor: borderColor,
              },
            ]}
            accessible
            accessibilityRole="header">
            <Typography variant="headingS" weight="semibold">
              Settings
            </Typography>
            <IconButton
              label="✕"
              variant="ghost"
              size="sm"
              accessibilityLabel="Close settings"
              onPress={onClose}
            />
          </View>

          <ScrollView
            style={[styles.content, {padding: ms(SPACING.md)}]}
            showsVerticalScrollIndicator={false}>
            <Card padding="md" radius="md" style={basicCardStyle}>
              <Typography variant="headingS" weight="semibold">
                Basic Settings
              </Typography>
              <Typography variant="bodyS" color="textSecondary">
                Günlük kullanım için önerilen, güvenli ayarlar.
              </Typography>
              <View style={dividerStyle} />
              <SettingRow
                label="Frequency Domain (LF/HF)"
                hint="LF/HF analizi daha fazla CPU tüketir. Gerekli olduğunda açın."
                rightSlot={
                  <Switch
                    value={!!options.calcFreq}
                    onValueChange={value => onChange({calcFreq: value})}
                    disabled={disabled}
                    trackColor={{false: borderColor, true: primaryColor}}
                    thumbColor={disabled ? borderColor : textInverseColor}
                  />
                }
                contentStyle={basicSettingContentStyle}
              />
              <View style={dividerStyle} />
              {renderStepperRow(
                'Threshold Scale',
                thresholdScale,
                'thresholdScale',
                {
                  min: 0.3,
                  max: 0.9,
                  step: 0.05,
                  decimals: 2,
                },
              )}
              <View style={dividerStyle} />
              {renderStepperRow(
                'Refractory (ms)',
                refractory,
                'refractoryMs',
                {
                  min: 180,
                  max: 400,
                  step: 10,
                  decimals: 0,
                },
                value => `${Math.round(value)} ms`,
                val => Math.round(val),
              )}
              <View style={dividerStyle} />
              <View>
                <SettingRow
                  label="Filter Mode"
                  hint="Örnekleme koşullarına göre uygun filtreyi seç."
                  contentStyle={basicSettingContentStyle}
                />
                <View
                  style={[styles.selectChipRow, {marginTop: ms(SPACING.xs)}]}>
                  {filterModes.map(option => {
                    const isActive = option.key === filterMode;
                    return (
                      <Pressable
                        key={option.key ?? option.label}
                        onPress={() => onChange({filterMode: option.key})}
                        disabled={disabled}
                        style={[
                          filterModeChipSpacing,
                          disabled && styles.filterChipDisabled,
                        ]}>
                        <Badge
                          label={option.label}
                          size="sm"
                          backgroundOverride={
                            isActive ? primaryColor : surfaceMutedColor
                          }
                          textColorOverride={
                            isActive ? textInverseColor : textPrimaryColor
                          }
                        />
                      </Pressable>
                    );
                  })}
                </View>
              </View>
              <View style={dividerStyle} />
              {renderStepperRow(
                'Filter Order',
                filterOrder,
                'filterOrder',
                {
                  min: 1,
                  max: 4,
                  step: 1,
                  decimals: 0,
                },
                value => `Order ${Math.round(value)}`,
                val => Math.round(val),
              )}
            </Card>

            <SettingRow
              pressable
              onPress={() => setShowAdvanced(prev => !prev)}
              label="Advanced Settings"
              hint="Uzman ayarlar. CPU/pil kullanımını artırabilir."
              rightSlot={
                <Typography variant="bodyS" color="textSecondary">
                  {showAdvanced ? '▲' : '▼'}
                </Typography>
              }
              contentStyle={advancedTriggerContentStyle}
            />

            {showAdvanced ? (
              <View
                style={[styles.advancedContainer, {marginTop: ms(SPACING.md)}]}>
                <PPGParameterControls
                  title="Advanced Analyzer"
                  caption="Detaylı kontroller. Değişiklikler ölçüm sırasında yeniden uygulanır."
                  showCalcFreqToggle={false}
                  includeKeys={advancedKeys}
                  options={options}
                  onChange={onChange}
                  onReset={onReset}
                  disabled={disabled}
                />
              </View>
            ) : null}

            <View style={footerSpacerStyle} />
          </ScrollView>
        </Animated.View>
      </View>
    </Modal>
  );
};

const styles = StyleSheet.create({
  modalRoot: {
    flex: 1,
    justifyContent: 'flex-end',
  },
  overlay: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    zIndex: 100,
  },
  overlayTouchable: {
    flex: 1,
  },
  settingsPanel: {
    position: 'absolute',
    left: 0,
    right: 0,
    bottom: 0,
    zIndex: 101,
    overflow: 'hidden',
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
  },
  content: {
    flex: 1,
  },
  rowActions: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  selectChipRow: {
    flexDirection: 'row',
    flexWrap: 'wrap',
  },
  advancedContainer: {
    width: '100%',
  },
  settingSurfaceBase: {
    borderWidth: 1,
  },
  settingSurfaceSm: {
    borderRadius: BORDER_RADIUS.sm,
  },
  basicCard: {
    borderWidth: 1,
  },
  advancedTriggerContent: {
    borderRadius: BORDER_RADIUS.md,
    borderWidth: 1,
  },
  filterChipDisabled: {
    opacity: 0.5,
  },
});
