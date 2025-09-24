import { RealtimeAnalyzer } from '../src/index';

describe('RealtimeAnalyzer guards and errors', () => {
  it('create invalid fs -> HEARTPY_E001', async () => {
    RealtimeAnalyzer.setConfig({ jsiEnabled: false });
    await expect(RealtimeAnalyzer.create(0 as any, {} as any)).rejects.toMatchObject({ code: 'HEARTPY_E001' });
  });

  it('push([]) on NM path -> HEARTPY_E102', async () => {
    RealtimeAnalyzer.setConfig({ jsiEnabled: false });
    const a = await RealtimeAnalyzer.create(50, {});
    await expect(a.push([] as any)).rejects.toMatchObject({ code: 'HEARTPY_E102' });
    await a.destroy();
  });

  it('JSI invalid handle surfaces HEARTPY_E101', async () => {
    const g: any = global as any;
    g.__hpRtCreate = jest.fn(() => 42);
    g.__hpRtPush = jest.fn(() => { throw new Error('HEARTPY_E101: invalid handle'); });
    g.__hpRtPoll = jest.fn(() => null);
    g.__hpRtDestroy = jest.fn();
    RealtimeAnalyzer.setConfig({ jsiEnabled: true });
    const a = await RealtimeAnalyzer.create(50, {});
    await expect(a.push(new Float32Array([1]))).rejects.toThrow(/HEARTPY_E101/);
    await a.destroy();
  });

  it('destroy is idempotent', async () => {
    RealtimeAnalyzer.setConfig({ jsiEnabled: false });
    const a = await RealtimeAnalyzer.create(50, {});
    await a.destroy();
    await expect(a.destroy()).resolves.toBeUndefined();
  });
});
