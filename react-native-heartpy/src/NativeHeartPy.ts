import {Platform} from 'react-native';

import type {Spec as HeartPyTurboModule} from './specs/NativeHeartPy';
import type {
	HeartPyMetrics,
	HeartPyOptions,
	HeartPyResult,
	NumericArray,
	RealtimeHandle,
	RuntimeConfig,
	RuntimeConfigPatch,
} from './types/heartpy';

export type HeartPyModule = HeartPyTurboModule;

let cachedTurboModule: HeartPyModule | null | undefined;
let cachedLegacyModule: HeartPyModule | null | undefined;

export function getTurboModule(): HeartPyModule | null {
	if (cachedTurboModule !== undefined) {
		return cachedTurboModule;
	}
	try {
		const {TurboModuleRegistry} = require('react-native') as {
			TurboModuleRegistry: { get<T>(name: string): T | null };
		};
		cachedTurboModule = TurboModuleRegistry.get<HeartPyModule>('HeartPyModule');
	} catch (e) {
		cachedTurboModule = null;
	}
	return cachedTurboModule ?? null;
}

function getLegacyModule(): HeartPyModule | null {
	const {NativeModules} = require('react-native');
	const native: unknown = NativeModules?.HeartPyModule;
	if (!native) {
		cachedLegacyModule = null;
		return cachedLegacyModule;
	}
	if (cachedLegacyModule !== native) {
		cachedLegacyModule = native as HeartPyModule;
	}
	return cachedLegacyModule;
}

export function getNativeModule(): HeartPyModule {
	const turbo = getTurboModule();
	if (turbo) {
		return turbo;
	}
	const legacy = getLegacyModule();
	if (legacy) {
		return legacy;
	}
	throw new Error('HeartPyModule: native implementation not found. Make sure it is linked.');
}

export function __resetHeartPyModuleCacheForTests(): void {
	cachedTurboModule = undefined;
	cachedLegacyModule = undefined;
}
