import React, {useMemo} from 'react';
import {StyleSheet, View} from 'react-native';
import type {ViewStyle} from 'react-native';
import {Card, Typography, Badge} from './ui';
import {SPACING} from '../theme/spacing';
import {FONT_SIZES, LINE_HEIGHTS} from '../theme/typography';
import {useThemeColor} from '../hooks/useThemeColor';
import type {Breakpoint} from '../styles/responsive';

export type PrimaryMetricsCardProps = {
  bpm?: number;
  bpmText: string;
  confidenceText: string;
  bpmColor: string;
  confidenceColor: string;
  showConfidence: boolean;
  breakpoint: Breakpoint;
  isLandscape: boolean;
  ms: (size: number, factor?: number) => number;
};

export const PrimaryMetricsCard: React.FC<PrimaryMetricsCardProps> = ({
  bpmText,
  confidenceText,
  bpmColor,
  confidenceColor,
  showConfidence,
  breakpoint,
  isLandscape,
  ms,
}) => {
  const isTabletLayout = breakpoint === 'lg' || breakpoint === 'xl';
  const useRowLayout = isTabletLayout || isLandscape;
  const isBpmProvided = bpmText !== '--';

  const defaultBpmColor = useThemeColor('textPrimary');
  const defaultBadgeBackground = useThemeColor('surfaceMuted');
  const labelColor = useThemeColor('textSecondary');
  const defaultConfidenceColor = useThemeColor('primary');

  const resolvedBpmColor = bpmColor ?? defaultBpmColor;
  const resolvedConfidenceColor = confidenceColor ?? defaultConfidenceColor;

  const contentDirectionStyle = useMemo<ViewStyle>(
    () => ({
      flexDirection: useRowLayout ? 'row' : 'column',
      alignItems: useRowLayout ? 'center' : 'flex-start',
      justifyContent: useRowLayout ? 'space-between' : 'flex-start',
    }),
    [useRowLayout],
  );

  const bpmFontSize = useMemo(
    () => ms(FONT_SIZES.headingXL, isTabletLayout ? 0.6 : 0.4),
    [isTabletLayout, ms],
  );

  const bpmLineHeight = useMemo(
    () => ms(LINE_HEIGHTS.headingXL, isTabletLayout ? 0.6 : 0.4),
    [isTabletLayout, ms],
  );

  const bpmSpacingStyle = useMemo<ViewStyle>(
    () =>
      useRowLayout
        ? {marginRight: ms(SPACING.md)}
        : {marginBottom: ms(SPACING.md)},
    [ms, useRowLayout],
  );

  const badgeAlignment = useMemo<ViewStyle>(
    () => ({alignSelf: useRowLayout ? 'center' : 'flex-start'}),
    [useRowLayout],
  );

  const confidenceBadgeSize = useRowLayout ? 'md' : 'sm';

  return (
    <Card padding="lg" radius="lg" testID="primary-metrics-card">
      <View style={[styles.content, contentDirectionStyle]}>
        <View style={[styles.bpmWrapper, bpmSpacingStyle]}>
          <Typography
            testID="primary-metrics-bpm"
            variant="headingXL"
            weight="semibold"
            style={{
              color: resolvedBpmColor,
              fontSize: bpmFontSize,
              lineHeight: bpmLineHeight,
            }}
            accessibilityLabel="Heart rate">
            {bpmText}
          </Typography>
          {isBpmProvided ? (
            <Typography
              variant="caption"
              weight="medium"
              style={{
                color: labelColor,
                marginTop: ms(SPACING.xs),
              }}>
              BPM
            </Typography>
          ) : null}
        </View>

        {showConfidence ? (
          <Badge
            label={`Confidence ${confidenceText}`}
            size={confidenceBadgeSize}
            textColorOverride={resolvedConfidenceColor}
            backgroundOverride={defaultBadgeBackground}
            style={badgeAlignment}
            testID="primary-metrics-confidence"
          />
        ) : null}
      </View>
    </Card>
  );
};
const styles = StyleSheet.create({
  content: {
    width: '100%',
  },
  bpmWrapper: {
    flexDirection: 'column',
  },
});
