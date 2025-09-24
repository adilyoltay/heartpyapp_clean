export * from './colors';
export * from './typography';
export * from './spacing';
export * from './layout';

import {THEME_COLORS} from './colors';
import {TYPOGRAPHY} from './typography';
import {SPACING} from './spacing';
import {LAYOUT} from './layout';

export const THEME = {
  colors: THEME_COLORS,
  typography: TYPOGRAPHY,
  spacing: SPACING,
  layout: LAYOUT,
} as const;
