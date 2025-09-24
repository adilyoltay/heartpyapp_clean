export const GRID_UNIT = 8;

export const SPACING = {
  none: 0,
  xs: GRID_UNIT / 2, // 4px
  sm: GRID_UNIT, // 8px
  md: GRID_UNIT * 2, // 16px
  lg: GRID_UNIT * 3, // 24px
  xl: GRID_UNIT * 4, // 32px
  xxl: GRID_UNIT * 6, // 48px
} as const;

export type SpacingToken = keyof typeof SPACING;
