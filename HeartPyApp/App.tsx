import React, {useMemo} from 'react';
import {NavigationContainer, DefaultTheme} from '@react-navigation/native';
import RootNavigator from './src/navigation/RootNavigator';
import {ThemeProvider} from './src/contexts/ThemeContext';
import {useThemeColor} from './src/hooks/useThemeColor';

function AppNavigation(): React.JSX.Element {
  const background = useThemeColor('background');
  const surface = useThemeColor('surface');
  const text = useThemeColor('textPrimary');
  const border = useThemeColor('border');

  const navigationTheme = useMemo(
    () => ({
      ...DefaultTheme,
      colors: {
        ...DefaultTheme.colors,
        background,
        card: surface,
        text,
        border,
      },
    }),
    [background, border, surface, text],
  );

  return (
    <NavigationContainer theme={navigationTheme}>
      <RootNavigator />
    </NavigationContainer>
  );
}

export default function App(): React.JSX.Element {
  return (
    <ThemeProvider>
      <AppNavigation />
    </ThemeProvider>
  );
}
