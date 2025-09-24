const {getDefaultConfig, mergeConfig} = require('@react-native/metro-config');
const path = require('path');

const defaultConfig = getDefaultConfig(__dirname);

/**
 * Metro configuration
 * https://facebook.github.io/metro/docs/configuration
 *
 * @type {import('metro-config').MetroConfig}
 */
const config = {
  watchFolders: [path.resolve(__dirname, '../react-native-heartpy')],
  watcher: {
    watchman: false,
    healthCheck: {
      enabled: false,
    },
  },
  resolver: {
    unstable_enableSymlinks: true,
    nodeModulesPaths: [
      path.resolve(__dirname, 'node_modules'),
      path.resolve(__dirname, '../react-native-heartpy'),
    ],
    sourceExts: Array.from(
      new Set([
        ...(defaultConfig.resolver?.sourceExts ?? []),
        'cjs',
        'ts',
        'tsx',
      ]),
    ),
    // Exclude directories to prevent EMFILE errors
    blockList: [
      // iOS specific
      /ios\/Pods\/.*/,
      /ios\/build\/.*/,
      /ios\/.*\.xcodeproj\/.*/,
      /ios\/.*\.xcworkspace\/.*/,
      
      // Android specific
      /android\/\.gradle\/.*/,
      /android\/build\/.*/,
      /android\/app\/build\/.*/,
      
      // Node modules in subdirectories
      /.*\/node_modules\/.*/,
      
      // Build and cache directories
      /.*\/\.next\/.*/,
      /.*\/\.cache\/.*/,
      /.*\/dist\/.*/,
      /.*\/coverage\/.*/,
      
      // Version control
      /.*\/\.git\/.*/,
      
      // React Native specific
      /.*\/\.bundle\/.*/,
    ],
  },
  transformer: {
    enableBabelRCLookup: false,
  },
};

module.exports = mergeConfig(defaultConfig, config);
