import React, {useMemo} from 'react';
import {StyleSheet, Switch, View} from 'react-native';
import {
  DEFAULT_ANALYZER_OPTIONS,
  type AnalyzerTuningOptions,
} from '../core/PPGAnalyzer';
import {Card, Typography, IconButton, SettingRow, Button} from './ui';
import {useThemeColor} from '../hooks/useThemeColor';
import {useResponsive} from '../styles/responsive';
import {SPACING} from '../theme/spacing';
import {BORDER_RADIUS} from '../theme/layout';

type Props = {
  options: AnalyzerTuningOptions;
  onChange: (update: Partial<AnalyzerTuningOptions>) => Promise<void> | void;
  onReset?: () => Promise<void> | void;
  disabled?: boolean;
  includeKeys?: ReadonlyArray<keyof AnalyzerTuningOptions>;
  title?: string;
  caption?: string;
  showCalcFreqToggle?: boolean;
};

type ControlConfig = {
  key: keyof AnalyzerTuningOptions;
  label: string;
  step: number;
  min: number;
  max: number;
  decimals?: number;
  format?: (value: number) => string;
};

const CONTROL_CONFIG: ControlConfig[] = [
  {
    key: 'thresholdScale',
    label: 'Threshold Scale',
    step: 0.05,
    min: 0.3,
    max: 0.9,
    decimals: 2,
  },
  {
    key: 'pHalfOverFundThresholdSoft',
    label: 'pHalf/Fund Soft',
    step: 0.05,
    min: 0.8,
    max: 1.8,
    decimals: 2,
  },
  {
    key: 'refractoryMs',
    label: 'Refractory (ms)',
    step: 10,
    min: 180,
    max: 400,
    decimals: 0,
    format: value => `${Math.round(value)} ms`,
  },
  {
    key: 'highCutoffHz',
    label: 'High Cutoff (Hz)',
    step: 0.1,
    min: 2.0,
    max: 5.0,
    decimals: 2,
  },
  {
    key: 'welchWsizeSec',
    label: 'Welch Window (s)',
    step: 1,
    min: 4,
    max: 16,
    decimals: 0,
    format: value => `${Math.round(value)} s`,
  },
  {
    key: 'nfft',
    label: 'FFT Size',
    step: 256,
    min: 512,
    max: 4096,
    decimals: 0,
  },
  {
    key: 'snrTauSec',
    label: 'SNR Tau (s)',
    step: 0.5,
    min: 0.5,
    max: 10.0,
    decimals: 2,
    format: value => `${value.toFixed(2)} s`,
  },
  {
    key: 'snrActiveTauSec',
    label: 'SNR Active Tau (s)',
    step: 0.5,
    min: 0.5,
    max: 10.0,
    decimals: 2,
    format: value => `${value.toFixed(2)} s`,
  },
];

function clamp(value: number, min: number, max: number): number {
  return Math.min(Math.max(value, min), max);
}

export function PPGParameterControls({
  options,
  onChange,
  onReset,
  disabled,
  includeKeys,
  title,
  caption,
  showCalcFreqToggle = true,
}: Props): JSX.Element {
  const configs = useMemo(() => {
    if (!includeKeys || includeKeys.length === 0) {
      return CONTROL_CONFIG;
    }
    const allowed = new Set(includeKeys);
    return CONTROL_CONFIG.filter(config => allowed.has(config.key));
  }, [includeKeys]);

  const {ms} = useResponsive();
  const primaryColor = useThemeColor('primary');
  const borderColor = useThemeColor('border');
  const surfaceColor = useThemeColor('surface');
  const surfaceMutedColor = useThemeColor('surfaceMuted');
  const textInverseColor = useThemeColor('textInverse');

  const rowActionGapStyle = useMemo(() => ({gap: ms(SPACING.xs)}), [ms]);

  const settingRowContentStyle = useMemo(
    () => [
      styles.settingRowSurface,
      {backgroundColor: surfaceMutedColor, borderColor},
    ],
    [borderColor, surfaceMutedColor],
  );

  const cardContainerStyle = useMemo(
    () => ({
      gap: ms(SPACING.sm),
      backgroundColor: surfaceColor,
    }),
    [ms, surfaceColor],
  );

  const captionStyle = useMemo(
    () => [styles.captionText, {marginTop: ms(SPACING.xs)}],
    [ms],
  );

  const rows = useMemo(
    () =>
      configs.map(config => {
        const baseValue = options[config.key];
        const defaultValue = DEFAULT_ANALYZER_OPTIONS[config.key];
        const value =
          typeof baseValue === 'number' ? baseValue : (defaultValue as number);

        const displayValue = config.format
          ? config.format(value)
          : value.toFixed(config.decimals ?? 2);

        const adjust = (direction: -1 | 1) => {
          if (disabled) {
            return;
          }
          const next = clamp(
            Number(
              (value + direction * config.step).toFixed(config.decimals ?? 3),
            ),
            config.min,
            config.max,
          );
          onChange({[config.key]: next});
        };

        const rightSlot = (
          <View style={[styles.rowActions, rowActionGapStyle]}>
            <IconButton
              label="−"
              size="sm"
              variant="outline"
              onPress={() => adjust(-1)}
              disabled={disabled}
              accessibilityLabel={`Decrease ${config.label}`}
            />
            <IconButton
              label="+"
              size="sm"
              variant="outline"
              onPress={() => adjust(1)}
              disabled={disabled}
              accessibilityLabel={`Increase ${config.label}`}
            />
          </View>
        );

        return {
          key: config.key as string,
          node: (
            <SettingRow
              label={config.label}
              hint={displayValue}
              rightSlot={rightSlot}
              contentStyle={settingRowContentStyle}
            />
          ),
        };
      }),
    [
      configs,
      disabled,
      onChange,
      options,
      rowActionGapStyle,
      settingRowContentStyle,
    ],
  );

  const headingLabel = title ?? 'Anlık Ayarlar';
  const captionLabel =
    caption ??
    'Parametreleri değiştirirken dalga formu ve metriklere göz at. Ayarlar koşarken yeniden uygulanır.';

  const dividerStyle = useMemo(
    () => ({
      height: 1,
      width: '100%' as const,
      backgroundColor: borderColor,
      marginVertical: ms(SPACING.sm),
    }),
    [borderColor, ms],
  );
  return (
    <Card padding="md" radius="md" style={cardContainerStyle}>
      <Typography variant="headingS" weight="semibold">
        {headingLabel}
      </Typography>
      <Typography variant="bodyS" color="textSecondary" style={captionStyle}>
        {captionLabel}
      </Typography>
      {showCalcFreqToggle ? (
        <>
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
          />
        </>
      ) : null}
      <View style={dividerStyle} />
      {rows.map(({key, node}, index) => (
        <View key={key}>
          {node}
          {index < rows.length - 1 ? <View style={dividerStyle} /> : null}
        </View>
      ))}
      {typeof onReset === 'function' ? (
        <Button
          title="Varsayılanlara dön"
          variant="outline"
          onPress={onReset}
          disabled={disabled}
          style={styles.resetButton}
        />
      ) : null}
    </Card>
  );
}

const styles = StyleSheet.create({
  rowWrapper: {
    width: '100%',
  },
  rowActions: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  settingRowSurface: {
    borderRadius: BORDER_RADIUS.sm,
    borderWidth: 1,
  },
  captionText: {
    lineHeight: 20,
  },
  resetButton: {
    alignSelf: 'flex-start',
  },
});
