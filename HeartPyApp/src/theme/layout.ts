import {GRID_UNIT} from './spacing';

export const BORDER_RADIUS = {
  sm: GRID_UNIT,
  md: GRID_UNIT + 4, // 12px
  lg: GRID_UNIT * 2, // 16px
  xl: GRID_UNIT * 3, // 24px
} as const;

export type BorderRadiusToken = keyof typeof BORDER_RADIUS;

export const SHADOWS = {
  none: {
    shadowColor: 'transparent',
    shadowOffset: {width: 0, height: 0},
    shadowOpacity: 0,
    shadowRadius: 0,
    elevation: 0,
  },
  subtle: {
    shadowColor: 'rgba(15, 23, 42, 0.12)',
    shadowOffset: {width: 0, height: 2},
    shadowOpacity: 0.12,
    shadowRadius: 8,
    elevation: 2,
  },
  medium: {
    shadowColor: 'rgba(15, 23, 42, 0.16)',
    shadowOffset: {width: 0, height: 4},
    shadowOpacity: 0.16,
    shadowRadius: 12,
    elevation: 4,
  },
  strong: {
    shadowColor: 'rgba(15, 23, 42, 0.24)',
    shadowOffset: {width: 0, height: 8},
    shadowOpacity: 0.24,
    shadowRadius: 24,
    elevation: 8,
  },
} as const;

export type ShadowToken = keyof typeof SHADOWS;

export const LAYOUT = {
  maxContentWidth: GRID_UNIT * 60,
  corner: BORDER_RADIUS,
  shadow: SHADOWS,
} as const;
