module.exports = {
  dependencies: {
    'react-native-reanimated': {
      platforms: {
        ios: null, // Reanimated için New Architecture'ı devre dışı bırak
        android: null, // Android için de devre dışı
      },
    },
    'react-native-vision-camera': {
      platforms: {
        ios: null, // VisionCamera için New Architecture'ı devre dışı bırak
        android: null, // Android için de devre dışı
      },
    },
    'react-native-heartpy': {
      platforms: {
        ios: null, // HeartPy için New Architecture'ı devre dışı bırak
        android: null, // Android için de devre dışı
      },
    },
  },
};
