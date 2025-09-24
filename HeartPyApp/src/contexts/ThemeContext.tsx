import React, {createContext, useContext, useMemo, useState} from 'react';
import {useColorScheme} from 'react-native';
import {THEME_COLORS, type ThemePalette} from '../theme';
import {PPG_CONFIG} from '../core/PPGConfig';

export type ThemeMode = 'light' | 'dark' | 'system';

type ThemeContextValue = {
  mode: ThemeMode;
  setMode: (mode: ThemeMode) => void;
  palette: ThemePalette;
};

const DEFAULT_MODE: ThemeMode =
  (PPG_CONFIG.ui?.themeMode as ThemeMode | undefined) ?? 'system';

const DEFAULT_PALETTE = THEME_COLORS.light;

const noop = () => {};

const ThemeContext = createContext<ThemeContextValue>({
  mode: 'light',
  setMode: noop,
  palette: DEFAULT_PALETTE,
});

export const ThemeProvider: React.FC<{children: React.ReactNode}> = ({
  children,
}) => {
  const [mode, setMode] = useState<ThemeMode>(DEFAULT_MODE);
  const systemScheme = useColorScheme();

  const resolvedMode = useMemo<Exclude<ThemeMode, 'system'>>(() => {
    if (mode === 'system') {
      return (systemScheme ?? 'light') === 'dark' ? 'dark' : 'light';
    }
    return mode;
  }, [mode, systemScheme]);

  const palette = useMemo(() => THEME_COLORS[resolvedMode], [resolvedMode]);

  const value = useMemo(
    () => ({
      mode,
      setMode,
      palette,
    }),
    [mode, palette],
  );

  return (
    <ThemeContext.Provider value={value}>{children}</ThemeContext.Provider>
  );
};

export const useThemeContext = (): ThemeContextValue => {
  return useContext(ThemeContext);
};
