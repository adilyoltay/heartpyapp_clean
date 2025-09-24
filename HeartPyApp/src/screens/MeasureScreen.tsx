import React, {useEffect, useRef, useState} from 'react';
import {
  SafeAreaView,
  ScrollView,
  StatusBar,
  StyleSheet,
  View,
} from 'react-native';
import {PPGDisplay} from '../components/PPGDisplay';
import {PPGCamera} from '../components/PPGCamera';
import {HiddenSettings} from '../components/HiddenSettings';
import {PPGParameterControls} from '../components/PPGParameterControls';
import {useThemeColor} from '../hooks/useThemeColor';
import {PPG_CONFIG} from '../core/PPGConfig';
import {useAnalyzer} from '../hooks/useAnalyzer';
import {SPACING} from '../theme/spacing';
import {Button} from '../components/ui';

export function MeasureScreen(): React.JSX.Element {
  const {
    analysisData,
    state,
    start,
    stop,
    addSample,
    updateSampleRate,
    options,
    updateOptions,
    resetOptions,
  } = useAnalyzer();

  const autoStartInvokedRef = useRef(false);
  const [showSettings, setShowSettings] = useState(false);
  const backgroundColor = useThemeColor('background');

  useEffect(() => {
    if (!PPG_CONFIG.ui?.autoStart) {
      return;
    }
    if (autoStartInvokedRef.current) {
      return;
    }
    if (state === 'idle') {
      autoStartInvokedRef.current = true;
      start();
    }
  }, [start, state]);

  const showParameterControls =
    !PPG_CONFIG.ui?.minimalMode && PPG_CONFIG.debug.enabled;

  return (
    <SafeAreaView style={[styles.safeArea, {backgroundColor}]}>
      <StatusBar barStyle="dark-content" />
      <View style={[styles.container, {backgroundColor}]}>
        <View style={styles.topBar}>
          <Button
            title="Settings"
            variant="outline"
            onPress={() => setShowSettings(true)}
            accessibilityLabel="Open analyzer settings"
          />
        </View>
        <PPGDisplay
          data={analysisData}
          state={state}
          onStart={start}
          onStop={stop}
        />
        {showParameterControls && (
          <ScrollView
            style={styles.panelScroll}
            contentContainerStyle={styles.panelScrollContent}
            keyboardShouldPersistTaps="handled">
            <PPGParameterControls
              options={options}
              onChange={updateOptions}
              onReset={resetOptions}
              disabled={state === 'starting'}
            />
          </ScrollView>
        )}
      </View>

      <HiddenSettings
        isVisible={showSettings}
        onClose={() => setShowSettings(false)}
        options={options}
        onChange={updateOptions}
        onReset={resetOptions}
        disabled={state === 'starting'}
      />

      <View style={styles.hiddenCameraWrapper} pointerEvents="none">
        <PPGCamera
          hidden
          onSample={addSample}
          isActive={state !== 'idle'}
          onFpsUpdate={updateSampleRate}
        />
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safeArea: {
    flex: 1,
  },
  container: {
    flex: 1,
    padding: SPACING.lg,
    gap: SPACING.lg,
  },
  topBar: {
    flexDirection: 'row',
    justifyContent: 'flex-end',
  },
  panelScroll: {
    flexGrow: 0,
    maxHeight: 320,
  },
  panelScrollContent: {
    paddingBottom: SPACING.md,
  },
  hiddenCameraWrapper: {
    position: 'absolute',
    width: 1,
    height: 1,
    opacity: 0,
    top: -100,
    left: -100,
  },
});

export default MeasureScreen;
