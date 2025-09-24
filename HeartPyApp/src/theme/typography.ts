export const FONT_FAMILY = {
  regular: 'Inter-Regular',
  medium: 'Inter-Medium',
  semibold: 'Inter-SemiBold',
  bold: 'Inter-Bold',
} as const;

export type FontWeightToken = keyof typeof FONT_FAMILY;

export const FONT_WEIGHTS = {
  regular: '400',
  medium: '500',
  semibold: '600',
  bold: '700',
} as const;

export const FONT_SIZES = {
  caption: 12,
  bodyS: 14,
  bodyM: 16,
  bodyL: 18,
  headingS: 20,
  headingM: 24,
  headingL: 28,
  headingXL: 32,
} as const;

export type FontSizeToken = keyof typeof FONT_SIZES;

export const LINE_HEIGHTS = {
  caption: 16,
  bodyS: 20,
  bodyM: 22,
  bodyL: 24,
  headingS: 28,
  headingM: 32,
  headingL: 36,
  headingXL: 40,
} as const;

export const TYPOGRAPHY = {
  fontFamily: 'Inter',
  fontFamilies: FONT_FAMILY,
  fontWeights: FONT_WEIGHTS,
  fontSizes: FONT_SIZES,
  lineHeights: LINE_HEIGHTS,
} as const;
