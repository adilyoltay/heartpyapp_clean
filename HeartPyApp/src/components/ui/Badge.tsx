import React, {useMemo} from 'react';
import {
  Text,
  View,
  type StyleProp,
  type TextStyle,
  type ViewStyle,
} from 'react-native';
import type {ThemePalette} from '../../theme';
import {SPACING} from '../../theme/spacing';
import {BORDER_RADIUS} from '../../theme/layout';
import {useThemeColor} from '../../hooks/useThemeColor';

export type BadgeSize = 'sm' | 'md';

export type BadgeProps = {
  label: string;
  color?: keyof ThemePalette;
  backgroundColor?: keyof ThemePalette;
  textColorOverride?: string;
  backgroundOverride?: string;
  size?: BadgeSize;
  style?: StyleProp<ViewStyle>;
  textStyle?: TextStyle;
  testID?: string;
};

export const Badge: React.FC<BadgeProps> = ({
  label,
  color = 'primary',
  backgroundColor = 'primaryMuted',
  textColorOverride,
  backgroundOverride,
  size = 'md',
  style,
  textStyle,
  testID,
}) => {
  const paletteTextColor = useThemeColor(color);
  const paletteBackgroundColor = useThemeColor(backgroundColor);

  const textColor = textColorOverride ?? paletteTextColor;
  const bgColor = backgroundOverride ?? paletteBackgroundColor;

  const containerStyle = useMemo<ViewStyle>(
    () => ({
      alignSelf: 'flex-start',
      backgroundColor: bgColor,
      borderRadius: BORDER_RADIUS.md,
      paddingHorizontal: size === 'sm' ? SPACING.sm : SPACING.md,
      paddingVertical: size === 'sm' ? SPACING.xs : SPACING.sm,
    }),
    [bgColor, size],
  );

  const computedTextStyle = useMemo<TextStyle>(
    () => ({
      color: textColor,
      fontFamily: 'Inter-SemiBold',
      fontSize: size === 'sm' ? 12 : 14,
      lineHeight: size === 'sm' ? 16 : 18,
    }),
    [size, textColor],
  );

  return (
    <View style={[containerStyle, style]} testID={testID}>
      <Text style={[computedTextStyle, textStyle]}>{label}</Text>
    </View>
  );
};
