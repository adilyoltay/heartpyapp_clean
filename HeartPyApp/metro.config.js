const {getDefaultConfig, mergeConfig} = require('@react-native/metro-config');
const path = require('path');

const loadMetroExclusionList = () => {
  const candidates = [
    path.join(path.dirname(require.resolve('metro-config/package.json')), 'src/defaults/exclusionList'),
    path.join(path.dirname(require.resolve('@react-native/metro-config/package.json')), '../metro-config/src/defaults/exclusionList'),
  ];

  for (const candidate of candidates) {
    try {
      const mod = require(candidate);
      const fn = mod?.default ?? mod;
      if (typeof fn === 'function') {
        return fn;
      }
    } catch (error) {
      if (process.env.METRO_CONFIG_DEBUG) {
        console.warn(`metro.config.js: exclusionList candidate failed -> ${candidate}: ${error.message}`);
      }
    }
  }

  throw new Error('metro.config.js: Unable to resolve exclusionList helper from metro-config.');
};

const exclusionList = loadMetroExclusionList();
const blockList = (patternArray) => {
  if (typeof exclusionList === 'function') {
    const maybeFunction = exclusionList(patternArray);
    if (typeof maybeFunction === 'function') {
      return maybeFunction;
    }
    if (maybeFunction && typeof maybeFunction.blockList === 'function') {
      return maybeFunction.blockList(patternArray);
    }
    throw new Error('metro.config.js: exclusionList returned unexpected shape.');
  }

  if (exclusionList && typeof exclusionList.blockList === 'function') {
    return exclusionList.blockList(patternArray);
  }

  throw new Error('metro.config.js: exclusionList helper missing.');
};
const defaultConfig = getDefaultConfig(__dirname);

const libraryRoot = path.resolve(__dirname, '../react-native-heartpy');

const config = {
  watchFolders: [libraryRoot],
  resolver: {
    sourceExts: [...defaultConfig.resolver.sourceExts, 'cjs'],
    resolveRequest: (context, moduleName, platform) => {
      if (moduleName === 'react-native-heartpy') {
        return {
          type: 'sourceFile',
          filePath: path.join(libraryRoot, 'dist', 'index.js'),
        };
      }
      return context.resolveRequest(context, moduleName, platform);
    },
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