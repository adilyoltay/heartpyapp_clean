import { RealtimeAnalyzer } from '../src/index';

describe('RealtimeAnalyzer path selection', () => {
  const g: any = global as any;
  let logSpy: jest.SpyInstance;
  beforeEach(() => {
    jest.resetModules();
    logSpy = jest.spyOn(console, 'log').mockImplementation(() => {});
    g.__hpRtCreate = jest.fn(() => 1);
    g.__hpRtPush = jest.fn();
    g.__hpRtPoll = jest.fn(() => null);
    g.__hpRtDestroy = jest.fn();
  });
  afterEach(() => {
    logSpy.mockRestore();
  });

  it('prefers JSI when enabled and globals present', async () => {
    RealtimeAnalyzer.setConfig({ jsiEnabled: true, debug: true });
    const a = await RealtimeAnalyzer.create(50, {});
    expect(g.__hpRtCreate).toHaveBeenCalled();
    expect(logSpy).toHaveBeenCalledWith('HeartPy: using JSI path');
    await a.destroy();
  });

  it('falls back to NativeModules when jsiEnabled=false', async () => {
    const { NativeModules } = require('react-native');
    (NativeModules.HeartPyModule.installJSI as jest.Mock).mockReturnValue(true);
    RealtimeAnalyzer.setConfig({ jsiEnabled: false, debug: true });
    const a = await RealtimeAnalyzer.create(50, {});
    expect(NativeModules.HeartPyModule.rtCreate).toHaveBeenCalled();
    expect(logSpy).toHaveBeenCalledWith('HeartPy: using NativeModules path');
    await a.destroy();
  });
});

