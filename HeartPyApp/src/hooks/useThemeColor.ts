import {useMemo} from 'react';
import {useThemeContext} from '../contexts/ThemeContext';
import type {ThemePalette} from '../theme';

export const useThemeColor = (
  token: keyof ThemePalette,
  fallback?: string,
): string => {
  const {palette} = useThemeContext();

  return useMemo(() => {
    const value = palette[token];
    if (value) {
      return value;
    }
    if (fallback) {
      return fallback;
    }
    return palette.textPrimary;
  }, [fallback, palette, token]);
};

export const getThemeColor = (
  palette: ThemePalette,
  token: keyof ThemePalette,
  fallback?: string,
): string => {
  return palette[token] ?? fallback ?? palette.textPrimary;
};
