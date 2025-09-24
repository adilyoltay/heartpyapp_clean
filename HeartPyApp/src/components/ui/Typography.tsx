import React, {useMemo} from 'react';
import {Text, type TextProps, type TextStyle} from 'react-native';
import {
  FONT_FAMILY,
  FONT_SIZES,
  FONT_WEIGHTS,
  LINE_HEIGHTS,
  type FontWeightToken,
} from '../../theme/typography';
import type {ThemePalette} from '../../theme';
import {useThemeColor} from '../../hooks/useThemeColor';

export type TypographyVariant =
  | 'caption'
  | 'bodyS'
  | 'bodyM'
  | 'bodyL'
  | 'headingS'
  | 'headingM'
  | 'headingL'
  | 'headingXL';

export type TypographyProps = TextProps & {
  variant?: TypographyVariant;
  weight?: FontWeightToken;
  color?: keyof ThemePalette;
};

export const Typography: React.FC<TypographyProps> = ({
  variant = 'bodyM',
  weight = 'regular',
  color = 'textPrimary',
  style,
  allowFontScaling = true,
  children,
  ...rest
}) => {
  const resolvedColor = useThemeColor(color);

  const textStyle = useMemo<TextStyle>(() => {
    const fontFamily = FONT_FAMILY[weight] ?? FONT_FAMILY.regular;
    const fontWeight = (FONT_WEIGHTS[weight] ?? FONT_WEIGHTS.regular) as
      | TextStyle['fontWeight']
      | undefined;

    return {
      color: resolvedColor,
      fontFamily,
      fontWeight,
      fontSize: FONT_SIZES[variant] ?? FONT_SIZES.bodyM,
      lineHeight: LINE_HEIGHTS[variant] ?? LINE_HEIGHTS.bodyM,
    } satisfies TextStyle;
  }, [resolvedColor, variant, weight]);

  return (
    <Text
      {...rest}
      allowFontScaling={allowFontScaling}
      style={[textStyle, style]}>
      {children}
    </Text>
  );
};
