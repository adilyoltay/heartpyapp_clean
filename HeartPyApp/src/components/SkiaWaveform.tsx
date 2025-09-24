import React, {useCallback, useMemo, useState} from 'react';
import {LayoutChangeEvent, StyleSheet, View} from 'react-native';
import type {StyleProp, ViewStyle} from 'react-native';
import {Canvas, Path, Rect, Skia, TileMode} from '@shopify/react-native-skia';
import {useThemeColor} from '../hooks/useThemeColor';
import {BORDER_RADIUS} from '../theme/layout';

type WaveformPoint = {
  readonly value: number;
  readonly timestamp: number;
};

type WaveformGradient = {
  readonly from: string;
  readonly to: string;
  readonly opacity?: number;
};

type Props = {
  readonly points: ReadonlyArray<WaveformPoint>;
  readonly peaks?: ReadonlySet<number>;
  readonly strokeColor?: string;
  readonly peakColor?: string;
  readonly strokeWidth?: number;
  readonly backgroundGradient?: WaveformGradient;
  readonly containerStyle?: StyleProp<ViewStyle>;
};

const PADDING_Y = 6;
const PEAK_RADIUS = 3;

const createWavePath = (
  points: ReadonlyArray<WaveformPoint>,
  width: number,
  height: number,
) => {
  const path = Skia.Path.Make();
  if (points.length === 0 || width <= 0 || height <= 0) {
    return path;
  }

  let min = Number.POSITIVE_INFINITY;
  let max = Number.NEGATIVE_INFINITY;
  for (let i = 0; i < points.length; i++) {
    const v = points[i].value;
    if (v < min) {
      min = v;
    }
    if (v > max) {
      max = v;
    }
  }
  if (!Number.isFinite(min) || !Number.isFinite(max)) {
    return path;
  }
  const span = max - min || 1;
  const availableHeight = Math.max(1, height - PADDING_Y * 2);
  const stepX = points.length === 1 ? 0 : width / (points.length - 1);

  for (let i = 0; i < points.length; i++) {
    const {value} = points[i];
    const norm = (value - min) / span;
    const x = stepX * i;
    const y = height - PADDING_Y - norm * availableHeight;
    if (i === 0) {
      path.moveTo(x, y);
    } else {
      path.lineTo(x, y);
    }
  }
  return path;
};

const createPeakPath = (
  points: ReadonlyArray<WaveformPoint>,
  peaks: ReadonlySet<number> | undefined,
  width: number,
  height: number,
) => {
  if (!peaks || peaks.size === 0 || points.length === 0) {
    return null;
  }
  const path = Skia.Path.Make();
  let min = Number.POSITIVE_INFINITY;
  let max = Number.NEGATIVE_INFINITY;
  for (let i = 0; i < points.length; i++) {
    const v = points[i].value;
    if (v < min) {
      min = v;
    }
    if (v > max) {
      max = v;
    }
  }
  if (!Number.isFinite(min) || !Number.isFinite(max)) {
    return null;
  }
  const span = max - min || 1;
  const availableHeight = Math.max(1, height - PADDING_Y * 2);
  const stepX = points.length === 1 ? 0 : width / (points.length - 1);

  for (let i = 0; i < points.length; i++) {
    const point = points[i];
    if (!peaks.has(point.timestamp)) {
      continue;
    }
    const norm = (point.value - min) / span;
    const x = stepX * i;
    const y = height - PADDING_Y - norm * availableHeight;
    path.addCircle(x, y, PEAK_RADIUS);
  }
  return path;
};

export function SkiaWaveform({
  points,
  peaks,
  strokeColor,
  peakColor,
  strokeWidth = 2,
  backgroundGradient,
  containerStyle,
}: Props): JSX.Element {
  const [layout, setLayout] = useState({width: 0, height: 0});
  const themeStrokeColor = useThemeColor('primary');
  const themePeakColor = useThemeColor('error');

  const resolvedStrokeColor = strokeColor ?? themeStrokeColor;
  const resolvedPeakColor = peakColor ?? themePeakColor;

  const onLayout = useCallback((event: LayoutChangeEvent) => {
    const {width, height} = event.nativeEvent.layout;
    setLayout({width, height});
  }, []);

  const waveformPath = useMemo(
    () => createWavePath(points, layout.width, layout.height),
    [points, layout.height, layout.width],
  );

  const peakPath = useMemo(
    () => createPeakPath(points, peaks, layout.width, layout.height),
    [points, peaks, layout.height, layout.width],
  );

  const gradientPaint = useMemo(() => {
    if (!backgroundGradient || layout.width <= 0 || layout.height <= 0) {
      return null;
    }
    const {from, to, opacity = 1} = backgroundGradient;
    const colors = [Skia.Color(from), Skia.Color(to)];
    const positions = [0, 1];
    const shader = Skia.Shader.MakeLinearGradient(
      {x: 0, y: 0},
      {x: 0, y: layout.height},
      colors,
      positions,
      TileMode.Clamp,
    );
    const paint = Skia.Paint();
    paint.setShader(shader);
    paint.setAlphaf(opacity);
    return paint;
  }, [backgroundGradient, layout.height, layout.width]);

  const shouldRender = layout.width > 0 && layout.height > 0;

  return (
    <View style={[styles.container, containerStyle]} onLayout={onLayout}>
      {shouldRender ? (
        <Canvas style={StyleSheet.absoluteFill}>
          {gradientPaint ? (
            <Rect
              x={0}
              y={0}
              width={layout.width}
              height={layout.height}
              paint={gradientPaint}
            />
          ) : null}
          <Path
            path={waveformPath}
            style="stroke"
            color={resolvedStrokeColor}
            strokeWidth={strokeWidth}
          />
          {peakPath ? (
            <Path path={peakPath} color={resolvedPeakColor} style="fill" />
          ) : null}
        </Canvas>
      ) : null}
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignSelf: 'stretch',
    width: '100%',
    borderRadius: BORDER_RADIUS.md,
    overflow: 'hidden',
  },
});

export default SkiaWaveform;
