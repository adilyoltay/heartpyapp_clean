import {NativeModules} from 'react-native';

import type {HeartPyModule, Spec} from '../src/NativeHeartPy';

let useTurboModule = false;

export const __setUseTurboModule = (next: boolean) => {
	useTurboModule = next;
};

export const __resetHeartPyModuleCacheForTests = () => {
	useTurboModule = false;
};

export const getTurboModule = (): Spec | null => {
	return useTurboModule ? (NativeModules.HeartPyModule as HeartPyModule) : null;
};

export const getNativeModule = (): HeartPyModule => {
	return NativeModules.HeartPyModule as HeartPyModule;
};
