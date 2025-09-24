import {Dimensions, PixelRatio, useWindowDimensions} from 'react-native';
import {useMemo} from 'react';

export type Breakpoint = 'sm' | 'md' | 'lg' | 'xl';

const BREAKPOINT_VALUES: Record<Breakpoint, number> = {
  sm: 0,
  md: 360,
  lg: 600,
  xl: 840,
} as const;

const BASE_WIDTH = 375;

export const getBreakpoint = (width: number): Breakpoint => {
  if (width >= BREAKPOINT_VALUES.xl) {
    return 'xl';
  }
  if (width >= BREAKPOINT_VALUES.lg) {
    return 'lg';
  }
  if (width >= BREAKPOINT_VALUES.md) {
    return 'md';
  }
  return 'sm';
};

export const scaleSize = (
  size: number,
  width: number = Dimensions.get('window').width,
): number => {
  const scaled = (width / BASE_WIDTH) * size;
  return PixelRatio.roundToNearestPixel(scaled);
};

export const moderateScale = (
  size: number,
  factor: number = 0.25,
  width: number = Dimensions.get('window').width,
): number => {
  const scaled = scaleSize(size, width);
  const moderated = size + (scaled - size) * factor;
  return PixelRatio.roundToNearestPixel(moderated);
};

export const useResponsive = () => {
  const {width, height} = useWindowDimensions();
  const breakpoint = getBreakpoint(width);
  const isLandscape = width > height;
  const isTablet = breakpoint === 'lg' || breakpoint === 'xl';

  return useMemo(
    () => ({
      width,
      height,
      bp: breakpoint,
      isLandscape,
      isTablet,
      scale: (size: number) => scaleSize(size, width),
      ms: (size: number, factor = 0.25) => moderateScale(size, factor, width),
    }),
    [width, height, breakpoint, isLandscape, isTablet],
  );
};
