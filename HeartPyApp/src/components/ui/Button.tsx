import React, {useMemo} from 'react';
import {
  ActivityIndicator,
  Pressable,
  Text,
  View,
  type PressableProps,
  type PressableStateCallbackType,
  type StyleProp,
  type TextStyle,
  type ViewStyle,
} from 'react-native';
import {SPACING} from '../../theme/spacing';
import {BORDER_RADIUS} from '../../theme/layout';
import {useThemeContext} from '../../contexts/ThemeContext';
import type {ThemePalette} from '../../theme';

export type ButtonVariant = 'primary' | 'secondary' | 'outline';
export type ButtonSize = 'sm' | 'md' | 'lg';

export type ButtonProps = PressableProps & {
  variant?: ButtonVariant;
  title: string;
  loading?: boolean;
  icon?: React.ReactNode;
  iconPosition?: 'left' | 'right';
  size?: ButtonSize;
  textColorOverride?: string;
  backgroundOverride?: string;
  borderColorOverride?: string;
};

const SIZE_PADDING: Record<ButtonSize, number> = {
  sm: SPACING.sm,
  md: SPACING.md,
  lg: SPACING.lg,
};

const SIZE_HEIGHT: Record<ButtonSize, number> = {
  sm: 40,
  md: 48,
  lg: 56,
};

const getVariantStyles = (variant: ButtonVariant, palette: ThemePalette) => {
  switch (variant) {
    case 'secondary':
      return {
        background: palette.surface,
        borderColor: palette.border,
        text: palette.textPrimary,
      };
    case 'outline':
      return {
        background: 'transparent',
        borderColor: palette.primary,
        text: palette.primary,
      };
    case 'primary':
    default:
      return {
        background: palette.primary,
        borderColor: 'transparent',
        text: palette.textInverse,
      };
  }
};

export const Button: React.FC<ButtonProps> = ({
  variant = 'primary',
  size = 'md',
  title,
  loading = false,
  disabled,
  icon,
  iconPosition = 'left',
  textColorOverride,
  backgroundOverride,
  borderColorOverride,
  style,
  ...rest
}) => {
  const {palette} = useThemeContext();
  const {background, borderColor, text} = useMemo(
    () => getVariantStyles(variant, palette),
    [palette, variant],
  );

  const containerStyle = useMemo<ViewStyle>(() => {
    return {
      alignItems: 'center',
      backgroundColor: backgroundOverride ?? background,
      borderColor: borderColorOverride ?? borderColor,
      borderRadius: BORDER_RADIUS.lg,
      borderWidth:
        (borderColorOverride ?? borderColor) === 'transparent' ? 0 : 1,
      flexDirection: 'row',
      gap: SPACING.sm,
      justifyContent: 'center',
      minHeight: SIZE_HEIGHT[size],
      opacity: disabled ? 0.4 : 1,
      paddingHorizontal: SIZE_PADDING[size],
    } satisfies ViewStyle;
  }, [
    background,
    backgroundOverride,
    borderColor,
    borderColorOverride,
    disabled,
    size,
  ]);

  const textStyle = useMemo<TextStyle>(() => {
    return {
      color: textColorOverride ?? text,
      fontFamily: 'Inter-SemiBold',
      fontSize: size === 'sm' ? 14 : size === 'lg' ? 18 : 16,
      fontWeight: '600',
    } satisfies TextStyle;
  }, [size, text, textColorOverride]);

  const indicatorColor = textColorOverride ?? text;

  const content = (
    <View style={styles.content}>
      {icon && iconPosition === 'left' ? icon : null}
      {!loading ? <Text style={textStyle}>{title}</Text> : null}
      {icon && iconPosition === 'right' ? icon : null}
      {loading ? (
        <ActivityIndicator size="small" color={indicatorColor} />
      ) : null}
    </View>
  );

  const pressableStyle = useMemo<
    | StyleProp<ViewStyle>
    | ((state: PressableStateCallbackType) => StyleProp<ViewStyle>)
  >(() => {
    if (typeof style === 'function') {
      return state => [containerStyle, style(state)];
    }
    return [containerStyle, style];
  }, [containerStyle, style]);

  const isDisabled = disabled || loading;

  return (
    <Pressable {...rest} disabled={isDisabled} style={pressableStyle}>
      {content}
    </Pressable>
  );
};

const styles = {
  content: {
    alignItems: 'center',
    flexDirection: 'row',
    gap: SPACING.xs,
  } as ViewStyle,
};
