const {getDefaultConfig, mergeConfig} = require('@react-native/metro-config');
const path = require('path');
const exclusionList = require('metro-config/src/defaults/exclusionList');

const defaultConfig = getDefaultConfig(__dirname);

const config = {
  watchFolders: [path.resolve(__dirname, '../react-native-heartpy')],
  resolver: {
    sourceExts: [...defaultConfig.resolver.sourceExts, 'cjs'],
    blockList: exclusionList([
      // Block large iOS directories entirely
      /.*ios\/Pods\/.*/,
      /.*ios\/build\/.*/,
      /.*\.xcodeproj\/.*/,
      /.*\.xcworkspace\/.*/,

      // Block Android build directories
      /.*android\/build\/.*/,
      /.*android\/\.gradle\/.*/,
      /.*android\/app\/build\/.*/,

      // Block common problematic directories
      /.*node_modules\/.*\/node_modules\/.*/, // Nested node_modules
      /.*\.git\/.*/,
      /.*coverage\/.*/,
      /.*\.cache\/.*/,
    ]),
  },
  watcher: {
    healthCheck: {
      enabled: false,
    },
  },
};

module.exports = mergeConfig(defaultConfig, config);