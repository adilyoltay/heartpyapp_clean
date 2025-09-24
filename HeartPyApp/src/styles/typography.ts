// Minimal Typography System for HeartPy
// Sade font sistemi - minimal ve okunabilir

export const TYPOGRAPHY = {
  fontFamily: 'System', // Native system font

  fontSizes: {
    large: 48, // BPM değeri
    medium: 24, // Confidence
    small: 16, // Labels
    tiny: 12, // Secondary info
  },

  fontWeights: {
    regular: '400', // Normal text
    medium: '500', // Emphasized text
    semibold: '600', // Strong emphasis
  },

  lineHeights: {
    tight: 1.2,
    normal: 1.4,
    relaxed: 1.6,
  },
} as const;

// Typography utility functions
export const getFontSize = (
  size: keyof typeof TYPOGRAPHY.fontSizes,
): number => {
  return TYPOGRAPHY.fontSizes[size];
};

export const getFontWeight = (
  weight: keyof typeof TYPOGRAPHY.fontWeights,
): string => {
  return TYPOGRAPHY.fontWeights[weight];
};

export const getLineHeight = (
  height: keyof typeof TYPOGRAPHY.lineHeights,
): number => {
  return TYPOGRAPHY.lineHeights[height];
};

// Predefined text styles
export const TEXT_STYLES = {
  bpmValue: {
    fontSize: TYPOGRAPHY.fontSizes.large,
    fontWeight: TYPOGRAPHY.fontWeights.medium,
    // Convert relative lineHeight ratios to pixel values
    lineHeight: TYPOGRAPHY.fontSizes.large * TYPOGRAPHY.lineHeights.tight,
  },

  confidenceValue: {
    fontSize: TYPOGRAPHY.fontSizes.medium,
    fontWeight: TYPOGRAPHY.fontWeights.regular,
    lineHeight: TYPOGRAPHY.fontSizes.medium * TYPOGRAPHY.lineHeights.normal,
  },

  label: {
    fontSize: TYPOGRAPHY.fontSizes.small,
    fontWeight: TYPOGRAPHY.fontWeights.regular,
    lineHeight: TYPOGRAPHY.fontSizes.small * TYPOGRAPHY.lineHeights.normal,
  },

  secondary: {
    fontSize: TYPOGRAPHY.fontSizes.tiny,
    fontWeight: TYPOGRAPHY.fontWeights.regular,
    lineHeight: TYPOGRAPHY.fontSizes.tiny * TYPOGRAPHY.lineHeights.normal,
  },
} as const;
