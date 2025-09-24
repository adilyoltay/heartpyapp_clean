import React, {useMemo} from 'react';
import {
  Pressable,
  View,
  type PressableProps,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import {Typography} from './Typography';
import {useThemeContext} from '../../contexts/ThemeContext';
import {SPACING} from '../../theme/spacing';
import {useResponsive} from '../../styles/responsive';

type BaseProps = {
  label: string;
  hint?: string;
  rightSlot?: React.ReactNode;
  style?: StyleProp<ViewStyle>;
  contentStyle?: StyleProp<ViewStyle>;
};

export type SettingRowProps = BaseProps &
  (
    | ({pressable?: false} & Omit<PressableProps, 'style'>)
    | ({pressable: true} & PressableProps)
  );

export const SettingRow: React.FC<SettingRowProps> = ({
  label,
  hint,
  rightSlot,
  style,
  contentStyle,
  pressable,
  ...rest
}) => {
  const {palette} = useThemeContext();
  const {ms} = useResponsive();

  const containerStyle = useMemo<ViewStyle>(
    () => ({
      paddingVertical: ms(SPACING.sm / 1.5),
      paddingHorizontal: ms(SPACING.sm),
      flexDirection: 'row',
      alignItems: 'center',
      justifyContent: 'space-between',
      backgroundColor: palette.surface,
    }),
    [ms, palette.surface],
  );

  const leftColumnStyle = useMemo<ViewStyle>(
    () => ({
      flex: 1,
      marginRight: ms(SPACING.sm),
    }),
    [ms],
  );

  const rightColumnStyle = useMemo<ViewStyle>(
    () => ({
      flexShrink: 0,
      alignItems: 'flex-end',
      justifyContent: 'center',
    }),
    [],
  );

  const content = (
    <View style={[containerStyle, contentStyle]}>
      <View style={leftColumnStyle}>
        <Typography variant="bodyM" weight="medium" numberOfLines={2}>
          {label}
        </Typography>
        {hint ? (
          <Typography
            variant="caption"
            color="textSecondary"
            numberOfLines={2}
            style={{marginTop: ms(SPACING.xs / 2)}}>
            {hint}
          </Typography>
        ) : null}
      </View>
      {rightSlot ? <View style={rightColumnStyle}>{rightSlot}</View> : null}
    </View>
  );

  if (pressable) {
    return (
      <Pressable style={style} {...(rest as PressableProps)}>
        {content}
      </Pressable>
    );
  }

  return <View style={style}>{content}</View>;
};
