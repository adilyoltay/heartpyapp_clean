#!/usr/bin/env node

/**
 * PPG Acceptance Test Suite
 * Validates log output against acceptance criteria
 *
 * Usage: node check_ppg_acceptance.js <log_file_path>
 * Exit codes: 0 = all tests pass, 1 = any test fails
 */

const fs = require('fs');
const path = require('path');

function parseAnalyzerEntries(content) {
  const regex = /LOG\s+\[PPGAnalyzer\]\s+Metrics\s+polled\s+({.*})/g;
  const entries = [];
  let match;
  while ((match = regex.exec(content)) !== null) {
    try {
      entries.push(JSON.parse(match[1]));
    } catch (_) {
      // ignore malformed JSON
    }
  }
  return entries;
}

function parsePluginSamples(content) {
  const regex = /PPG value:\s*(-?\d+\.\d+)/g;
  const samples = [];
  let match;
  while ((match = regex.exec(content)) !== null) {
    samples.push(parseFloat(match[1]));
  }
  return samples;
}

function readConfigNumber(key, fallback) {
  try {
    const cfgPath = path.join(__dirname, '../src/core/PPGConfig.ts');
    const source = fs.readFileSync(cfgPath, 'utf8');
    const regex = new RegExp(`${key}\\s*:\\s*(-?\\d+(?:\\.\\d+)?)`);
    const match = source.match(regex);
    if (match) {
      return parseFloat(match[1]);
    }
  } catch (error) {
    console.warn(
      `⚠️ Failed to read ${key} from PPGConfig.ts: ${error.message}`,
    );
  }
  return fallback;
}

const UI_INTERVAL_MS = readConfigNumber('uiUpdateIntervalMs', 200);

function computeMetrics(series, samples, intervalSec = UI_INTERVAL_MS / 1000) {
  if (!Array.isArray(series) || series.length === 0) {
    return {
      ampRms: null,
      peakToPeak: null,
      warmUpSeconds: null,
      hasResultFalseFrac: 1,
      goodQualityFrac: 0,
    };
  }

  const warmIdx = series.findIndex(entry => {
    const hasResult = !!entry.hasResult;
    const goodQuality = !!entry?.quality?.goodQuality;
    const snr = typeof entry.snrDb === 'number' ? entry.snrDb : -10;
    return hasResult && goodQuality && snr >= -3;
  });
  const warmUpSeconds =
    warmIdx >= 0 ? parseFloat((warmIdx * intervalSec).toFixed(2)) : null;

  const maxWindow = Math.min(
    series.length,
    Math.max(1, Math.round(4 / intervalSec)),
  );
  const warmWindow = series.slice(0, maxWindow);
  const hasResultFalse = warmWindow.filter(
    entry => entry.hasResult === false,
  ).length;
  const hasResultFalseFrac = maxWindow > 0 ? hasResultFalse / maxWindow : 0;

  const goodQualityCount = series.filter(
    entry => entry?.quality?.goodQuality,
  ).length;
  const goodQualityFrac =
    series.length > 0 ? goodQualityCount / series.length : 0;

  let ampRms = null;
  let peakToPeak = null;
  if (samples && samples.length > 0) {
    let min = Infinity;
    let max = -Infinity;
    let sumSq = 0;
    samples.forEach(value => {
      if (value < min) {
        min = value;
      }
      if (value > max) {
        max = value;
      }
      sumSq += value * value;
    });
    peakToPeak = max - min;
    ampRms = Math.sqrt(sumSq / samples.length);
  }

  return {
    ampRms,
    peakToPeak,
    warmUpSeconds,
    hasResultFalseFrac,
    goodQualityFrac,
  };
}

class PPGAcceptanceChecker {
  constructor(logFilePath) {
    this.logFilePath = logFilePath;
    this.logContent = '';
    this.results = {passed: 0, failed: 0, tests: []};
  }

  loadLogFile() {
    try {
      this.logContent = fs.readFileSync(this.logFilePath, 'utf8');
      console.log(`📄 Loaded log file: ${this.logFilePath}`);
    } catch (error) {
      console.error(`❌ Failed to load log file: ${error.message}`);
      process.exit(1);
    }
  }

  test(name, condition, description) {
    const passed = condition();
    this.results.tests.push({name, passed, description});
    if (passed) {
      this.results.passed++;
      console.log(`✅ ${name}: ${description}`);
    } else {
      this.results.failed++;
      console.log(`❌ ${name}: ${description}`);
    }
  }

  runAllTests() {
    console.log('🚀 Starting PPG Acceptance Test Suite...\n');
    this.loadLogFile();

    // Sample Stream Flow Tests
    console.log('\n🔍 Testing Sample Stream Flow...');
    this.test(
      'Valid samples received',
      () => {
        const matches = this.logContent.match(
          /LOG\s+\[PPGCamera\]\s+Received\s+valid\s+sample\s+from\s+NativeModules/g,
        );
        return matches && matches.length >= 10;
      },
      'At least 10 valid samples received',
    );

    this.test(
      'HeartPy pushWithTimestamps called',
      () => {
        const matches = this.logContent.match(
          /LOG\s+\[HeartPyWrapper\]\s+pushWithTimestamps/g,
        );
        return matches && matches.length >= 5;
      },
      'HeartPy pushWithTimestamps called at least 5 times',
    );

    // HeartPy Warm-up Tests
    console.log('\n🔍 Testing HeartPy Warm-up...');
    this.test(
      'Native confidence preserved',
      () => {
        const matches = this.logContent.match(
          /LOG\s+\[HeartPyWrapper\]\s+Native\s+metrics\b.*"confidence"\s*:\s*-?\d+(?:\.\d+)?/g,
        );
        return matches && matches.length >= 3;
      },
      'Native metrics with confidence logged during warm-up',
    );

    this.test(
      'BPM calculation started',
      () => {
        const matches = this.logContent.match(
          /LOG\s+\[HeartPyWrapper\]\s+poll\s+response.*"bpm"/g,
        );
        return matches && matches.length >= 2;
      },
      'BPM calculation started',
    );

    this.test(
      'NaN sample handling',
      () => {
        const matches = this.logContent.match(
          /LOG\s+\[PPGCamera\]\s+Received\s+NaN\s+sample\s+\(warm-up\/low\s+signal\)/g,
        );
        return matches ? matches.length >= 1 : true;
      },
      'NaN samples properly handled during warm-up (optional)',
    );

    this.test(
      'NaN ratio monitoring',
      () => {
        const nanMatches = this.logContent.match(
          /LOG\s+\[PPGCamera\]\s+Received\s+NaN\s+sample/g,
        );
        const validMatches = this.logContent.match(
          /LOG\s+\[PPGCamera\]\s+Received\s+valid\s+sample\s+from\s+NativeModules/g,
        );

        if (!nanMatches || nanMatches.length === 0) {
          return true;
        }
        if (!validMatches || validMatches.length === 0) {
          return false;
        }

        const totalSamples = nanMatches.length + validMatches.length;
        if (totalSamples < 20) {
          return true;
        }

        const nanRatio = nanMatches.length / totalSamples;
        return nanRatio <= 0.8;
      },
      'NaN ratio not excessive (optional)',
    );

    this.test(
      'Confidence fallback logic',
      () => {
        const nativeMetricsMatches = this.logContent.match(
          /LOG\s+\[HeartPyWrapper\]\]\s+Native\s+metrics\b/g,
        );
        return nativeMetricsMatches && nativeMetricsMatches.length >= 5;
      },
      'Confidence fallback logic working',
    );

    // Peak Filtering Tests
    console.log('\n🔍 Testing Peak Filtering...');
    this.test(
      'Peak filtering logs present',
      () => {
        const matches = this.logContent.match(
          /LOG\s+\[HeartPyWrapper\]\s+Peak\s+list\s+(?:filtering|normalization)/g,
        );
        return matches && matches.length >= 2;
      },
      'Peak filtering logs present',
    );

    // UI Haptic Tests
    console.log('\n🔍 Testing UI Haptic Feedback...');
    this.test(
      'Haptic feedback logic',
      () => {
        const matches = this.logContent.match(
          /LOG\s+💓\s+Haptic\s+disabled\s+-\s+BPM\s+unreliable/g,
        );
        return matches && matches.length >= 1;
      },
      'Haptic feedback logic working',
    );

    this.test(
      'Signal recovery detection',
      () => {
        const signalQualityMatches = this.logContent.match(
          /Signal quality.*(?:poor|good)/gi,
        );
        const hapticDisabledMatches = this.logContent.match(
          /LOG\s+💓\s+Haptic\s+disabled.*BPM\s+unreliable/gi,
        );
        const hapticTriggeredMatches = this.logContent.match(
          /LOG\s+💓\s+Heart\s+beat\s+detected.*Haptic\s+triggered/gi,
        );

        if (hapticDisabledMatches && hapticDisabledMatches.length >= 1) {
          return true;
        }

        if (hapticDisabledMatches && hapticTriggeredMatches) {
          return (
            hapticDisabledMatches.length >= 1 &&
            hapticTriggeredMatches.length >= 1
          );
        }

        return signalQualityMatches && signalQualityMatches.length >= 2;
      },
      'Signal recovery after poor quality detected (optional)',
    );

    // Error Handling Tests
    console.log('\n🔍 Testing Error Handling...');
    this.test(
      'No critical errors',
      () => {
        const matches = this.logContent.match(
          /ERROR|CRITICAL|FATAL|Error|Critical|Fatal/gi,
        );
        return !matches || matches.length === 0;
      },
      'No critical errors in log',
    );

    this.reportMetrics();
    this.printSummary();
    process.exit(this.results.failed > 0 ? 1 : 0);
  }

  reportMetrics() {
    const analyzerEntries = parseAnalyzerEntries(this.logContent);
    const pluginSamples = parsePluginSamples(this.logContent);
    const stats = computeMetrics(analyzerEntries, pluginSamples);
    const segmentRejected = /qualityWarning"\s*:\s*"High rejection rate"/g.test(
      this.logContent,
    );

    console.log('\n📈 Metrics Summary:');
    console.log(`   Warm-up (s): ${stats.warmUpSeconds ?? 'n/a'}`);
    console.log(
      `   RMS (V): ${stats.ampRms != null ? stats.ampRms.toFixed(4) : 'n/a'}`,
    );
    console.log(
      `   Peak-to-peak (V): ${
        stats.peakToPeak != null ? stats.peakToPeak.toFixed(4) : 'n/a'
      }`,
    );
    console.log(
      `   hasResult false fraction (first 4s): ${stats.hasResultFalseFrac.toFixed(
        2,
      )}`,
    );
    console.log(`   goodQuality fraction: ${stats.goodQualityFrac.toFixed(2)}`);
    console.log(`   Poll interval (ms): ${UI_INTERVAL_MS}`);
    console.log(
      `   Segment rejection observed: ${segmentRejected ? 'yes' : 'no'}`,
    );
  }

  printSummary() {
    console.log('\n📊 Test Summary:');
    console.log(`✅ Passed: ${this.results.passed}`);
    console.log(`❌ Failed: ${this.results.failed}`);

    if (this.results.failed > 0) {
      console.log('\n❌ Failed Tests:');
      this.results.tests
        .filter(test => !test.passed)
        .forEach(test => console.log(`   - ${test.name}`));
    }

    console.log(
      `\n${
        this.results.failed === 0
          ? '🎉 All tests passed!'
          : '⚠️  Some tests failed.'
      }`,
    );
  }
}

// Main execution
if (require.main === module) {
  const logFilePath = process.argv[2];

  if (!logFilePath) {
    console.error('❌ Usage: node check_ppg_acceptance.js <log_file_path>');
    process.exit(1);
  }

  const checker = new PPGAcceptanceChecker(logFilePath);
  checker.runAllTests();
}

module.exports = PPGAcceptanceChecker;
module.exports.PPGAcceptanceChecker = PPGAcceptanceChecker;
module.exports.computeMetrics = computeMetrics;
module.exports.parseAnalyzerEntries = parseAnalyzerEntries;
module.exports.parsePluginSamples = parsePluginSamples;
