module.exports = {
  dependency: {
    platforms: {
      android: {
        sourceDir: './android',
      },
      ios: {}, // Auto-detect podspec
    },
  },
  codegenConfig: {
    name: 'HeartPyModule',
    type: 'modules',
    jsSrcsDir: './src/specs',
    android: {
      javaPackageName: 'com.heartpy',
    },
    ios: {
      libraryName: 'HeartPy',
      podspecPath: 'ios/HeartPy.podspec',
    },
  },
};
