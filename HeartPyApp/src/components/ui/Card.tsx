import React, {useMemo} from 'react';
import {View, type ViewProps, type ViewStyle} from 'react-native';
import type {ThemePalette} from '../../theme';
import {SPACING} from '../../theme/spacing';
import {BORDER_RADIUS, SHADOWS} from '../../theme/layout';
import {useThemeColor} from '../../hooks/useThemeColor';

export type CardVariant = 'default' | 'outlined' | 'elevated';

export type CardProps = ViewProps & {
  variant?: CardVariant;
  padding?: keyof typeof SPACING;
  radius?: keyof typeof BORDER_RADIUS;
  backgroundColor?: keyof ThemePalette;
  shadow?: keyof typeof SHADOWS;
};

const resolveShadow = (
  variant: CardVariant,
  shadowProp: keyof typeof SHADOWS,
) => {
  if (variant === 'elevated') {
    return SHADOWS.medium;
  }
  return SHADOWS[shadowProp] ?? SHADOWS.subtle;
};

export const Card: React.FC<CardProps> = ({
  variant = 'default',
  padding = 'lg',
  radius = 'lg',
  backgroundColor = 'surface',
  shadow = 'subtle',
  style,
  children,
  ...rest
}) => {
  const resolvedBackground = useThemeColor(backgroundColor);
  const borderColor = useThemeColor('border');

  const containerStyle = useMemo<ViewStyle>(() => {
    const base: ViewStyle = {
      backgroundColor: resolvedBackground,
      borderRadius: BORDER_RADIUS[radius] ?? BORDER_RADIUS.lg,
      padding: SPACING[padding] ?? SPACING.lg,
      ...resolveShadow(variant, shadow),
    };

    if (variant === 'outlined') {
      base.borderWidth = 1;
      base.borderColor = borderColor;
    }

    if (variant === 'elevated') {
      base.borderWidth = 0;
    }

    return base;
  }, [borderColor, padding, radius, resolvedBackground, shadow, variant]);

  return (
    <View style={[containerStyle, style]} {...rest}>
      {children}
    </View>
  );
};
