export type ThemeMode = 'light' | 'dark';

export type ThemePalette = {
  primary: string;
  primaryMuted: string;
  background: string;
  surface: string;
  surfaceMuted: string;
  textPrimary: string;
  textSecondary: string;
  textInverse: string;
  border: string;
  overlay: string;
  success: string;
  warning: string;
  error: string;
};

export const LIGHT_COLORS: ThemePalette = {
  primary: '#10B981',
  primaryMuted: '#6EE7B7',
  background: '#F3F4F6',
  surface: '#FFFFFF',
  surfaceMuted: '#F9FAFB',
  textPrimary: '#374151',
  textSecondary: '#6B7280',
  textInverse: '#F9FAFB',
  border: '#E5E7EB',
  overlay: 'rgba(15, 23, 42, 0.12)',
  success: '#10B981',
  warning: '#F59E0B',
  error: '#EF4444',
};

export const DARK_COLORS: ThemePalette = {
  primary: '#34D399',
  primaryMuted: '#115E59',
  background: '#0F172A',
  surface: '#111827',
  surfaceMuted: '#1F2937',
  textPrimary: '#F3F4F6',
  textSecondary: '#CBD5F5',
  textInverse: '#0F172A',
  border: '#1F2937',
  overlay: 'rgba(148, 163, 184, 0.18)',
  success: '#34D399',
  warning: '#FBBF24',
  error: '#F87171',
};

export const BRAMAN_COLORS = {
  happy: '#F7C59F',
  calm: '#C3F0CA',
  focus: '#C1D9FF',
  sad: '#B8C5D6',
} as const;

export const THEME_COLORS = {
  light: LIGHT_COLORS,
  dark: DARK_COLORS,
  braman: BRAMAN_COLORS,
} as const;
