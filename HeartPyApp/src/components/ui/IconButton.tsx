import React, {useMemo} from 'react';
import {
  Pressable,
  Text,
  type PressableProps,
  type PressableStateCallbackType,
  type StyleProp,
  type TextStyle,
  type ViewStyle,
} from 'react-native';
import {useThemeContext} from '../../contexts/ThemeContext';
import {SPACING} from '../../theme/spacing';
import {BORDER_RADIUS} from '../../theme/layout';

export type IconButtonVariant = 'ghost' | 'outline';
export type IconButtonSize = 'sm' | 'md';

export type IconButtonProps = PressableProps & {
  variant?: IconButtonVariant;
  size?: IconButtonSize;
  label?: string;
  children?: React.ReactNode;
};

const SIZE_MAP: Record<IconButtonSize, {padding: number; fontSize: number}> = {
  sm: {padding: SPACING.xs, fontSize: 14},
  md: {padding: SPACING.sm, fontSize: 16},
};

export const IconButton: React.FC<IconButtonProps> = ({
  variant = 'ghost',
  size = 'sm',
  label,
  children,
  disabled,
  style,
  ...rest
}) => {
  const {palette} = useThemeContext();

  const baseStyle = useMemo<ViewStyle>(() => {
    const sizeToken = SIZE_MAP[size];
    const backgroundColor =
      variant === 'ghost' ? 'transparent' : palette.background;
    const borderColor = variant === 'outline' ? palette.border : 'transparent';

    return {
      alignItems: 'center',
      justifyContent: 'center',
      padding: sizeToken.padding,
      minWidth: sizeToken.padding * 4,
      borderRadius: BORDER_RADIUS.sm,
      borderWidth: borderColor === 'transparent' ? 0 : 1,
      borderColor,
      backgroundColor,
      opacity: disabled ? 0.4 : 1,
    } satisfies ViewStyle;
  }, [disabled, palette.background, palette.border, size, variant]);

  const textStyle = useMemo<TextStyle>(() => {
    const sizeToken = SIZE_MAP[size];
    return {
      color: palette.textPrimary,
      fontFamily: 'Inter-SemiBold',
      fontSize: sizeToken.fontSize,
      fontWeight: '600',
      textAlign: 'center',
    } satisfies TextStyle;
  }, [palette.textPrimary, size]);

  const pressableStyle = useMemo<
    | StyleProp<ViewStyle>
    | ((state: PressableStateCallbackType) => StyleProp<ViewStyle>)
  >(() => {
    if (typeof style === 'function') {
      return state => [baseStyle, style(state)];
    }
    return [baseStyle, style];
  }, [baseStyle, style]);

  return (
    <Pressable disabled={disabled} style={pressableStyle} {...rest}>
      {children ? (
        children
      ) : label ? (
        <Text style={textStyle}>{label}</Text>
      ) : null}
    </Pressable>
  );
};
