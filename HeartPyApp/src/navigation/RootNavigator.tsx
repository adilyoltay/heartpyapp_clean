import React, {useMemo} from 'react';
import {createBottomTabNavigator} from '@react-navigation/bottom-tabs';
import {View, StyleSheet} from 'react-native';
import MeasureScreen from '../screens/MeasureScreen';
import {Typography} from '../components/ui';
import {useThemeColor} from '../hooks/useThemeColor';

const Tab = createBottomTabNavigator();

function PlaceholderScreen({title}: {title: string}) {
  const backgroundColor = useThemeColor('background');
  const textPrimary = useThemeColor('textPrimary');

  return (
    <View style={[styles.placeholderContainer, {backgroundColor}]}>
      <Typography
        variant="headingM"
        weight="semibold"
        style={{color: textPrimary}}>
        {title}
      </Typography>
      <Typography
        variant="bodyS"
        color="textSecondary"
        style={styles.placeholderSubtitle}>
        Yakında eklenecek.
      </Typography>
    </View>
  );
}

export default function RootNavigator(): React.JSX.Element {
  const primary = useThemeColor('primary');
  const inactive = useThemeColor('textSecondary');
  const tabBackground = useThemeColor('surface');
  const tabBorder = useThemeColor('border');

  const screenOptions = useMemo(
    () => ({
      headerShown: false,
      tabBarActiveTintColor: primary,
      tabBarInactiveTintColor: inactive,
      tabBarStyle: {
        backgroundColor: tabBackground,
        borderTopColor: tabBorder,
        borderTopWidth: StyleSheet.hairlineWidth,
      },
    }),
    [inactive, primary, tabBackground, tabBorder],
  );

  return (
    <Tab.Navigator screenOptions={screenOptions}>
      <Tab.Screen name="Measure" component={MeasureScreen} />
      <Tab.Screen
        name="History"
        children={() => <PlaceholderScreen title="History" />}
      />
      <Tab.Screen
        name="Insights"
        children={() => <PlaceholderScreen title="Insights" />}
      />
    </Tab.Navigator>
  );
}

const styles = StyleSheet.create({
  placeholderContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    padding: 24,
  },
  placeholderSubtitle: {
    marginTop: 8,
  },
});
