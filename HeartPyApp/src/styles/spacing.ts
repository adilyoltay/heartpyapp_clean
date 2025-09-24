// Minimal Spacing System for HeartPy
// Minimal spacing - sade ve tutarlı

import {Platform} from 'react-native';

export const SPACING = {
  xs: 8, // 8px
  sm: 16, // 16px
  md: 24, // 24px
  lg: 32, // 32px
  xl: 48, // 48px
  xxl: 64, // 64px
} as const;

// Layout constants
export const LAYOUT = {
  container: {
    padding: SPACING.md, // 24px all around
    maxWidth: 400, // Max width for readability
  },

  borderRadius: {
    small: 8,
    medium: 12,
    large: 16,
  },

  shadows: {
    subtle: Platform.select({
      ios: {
        shadowColor: '#000',
        shadowOffset: {width: 0, height: 2},
        shadowOpacity: 0.05,
        shadowRadius: 4,
      },
      android: {
        elevation: 2,
      },
      default: {
        shadowColor: '#000',
        shadowOffset: {width: 0, height: 2},
        shadowOpacity: 0.05,
        shadowRadius: 4,
        elevation: 2,
      },
    }),

    medium: Platform.select({
      ios: {
        shadowColor: '#000',
        shadowOffset: {width: 0, height: 4},
        shadowOpacity: 0.1,
        shadowRadius: 8,
      },
      android: {
        elevation: 4,
      },
      default: {
        shadowColor: '#000',
        shadowOffset: {width: 0, height: 4},
        shadowOpacity: 0.1,
        shadowRadius: 8,
        elevation: 4,
      },
    }),
  },
} as const;

// Spacing utility functions
export const getSpacing = (size: keyof typeof SPACING): number => {
  return SPACING[size];
};

export const getBorderRadius = (
  size: keyof typeof LAYOUT.borderRadius,
): number => {
  return LAYOUT.borderRadius[size];
};

export const getShadow = (size: keyof typeof LAYOUT.shadows) => {
  return LAYOUT.shadows[size];
};
