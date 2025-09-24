export type NumericArray = ReadonlyArray<number>;

export type HeartPyOptions = {
	bandpass?: { lowHz: number; highHz: number; order?: number };
	welch?: { nfft?: number; overlap?: number; wsizeSec?: number };
	peak?: { refractoryMs?: number; thresholdScale?: number; bpmMin?: number; bpmMax?: number };
	filter?: { mode?: 'auto' | 'rbj' | 'butter' | 'butter-filtfilt'; order?: number };
	calcFreq?: boolean;
	preprocessing?: {
		interpClipping?: boolean;
		clippingThreshold?: number;
		hampelCorrect?: boolean;
		hampelWindow?: number;
		hampelThreshold?: number;
		removeBaselineWander?: boolean;
		enhancePeaks?: boolean;
		scaleData?: boolean;
	};
	quality?: {
		rejectSegmentwise?: boolean;
		segmentRejectThreshold?: number;
		segmentRejectMaxRejects?: number;
		segmentRejectWindowBeats?: number;
		segmentRejectOverlap?: number;
		cleanRR?: boolean;
		cleanMethod?: 'quotient-filter' | 'iqr' | 'z-score';
		thresholdRR?: boolean;
	};
	timeDomain?: {
		sdsdMode?: 'signed' | 'abs';
		pnnAsPercent?: boolean;
	};
	poincare?: {
		mode?: 'formula' | 'masked';
	};
	highPrecision?: {
		enabled?: boolean;
		targetFs?: number;
	};
	rrSpline?: {
		s?: number;
		targetSse?: number;
		smooth?: number;
	};
	segmentwise?: {
		width?: number;
		overlap?: number;
		minSize?: number;
		replaceOutliers?: boolean;
	};
	breathingAsBpm?: boolean;
	windowSeconds?: number;
	snrTauSec?: number;
	snrActiveTauSec?: number;
	adaptivePsd?: boolean;
};

export type QualityInfo = {
	totalBeats: number;
	rejectedBeats: number;
	rejectionRate: number;
	goodQuality: boolean;
	qualityWarning?: string;
	confidence?: number;
	snrDb?: number;
	f0Hz?: number;
	maPercActive?: number;
	doublingFlag?: boolean;
	softDoublingFlag?: boolean;
	doublingHintFlag?: boolean;
	hardFallbackActive?: boolean;
	rrFallbackModeActive?: boolean;
	refractoryMsActive?: number;
	minRRBoundMs?: number;
	pairFrac?: number;
	rrShortFrac?: number;
	rrLongMs?: number;
	pHalfOverFund?: number;
	snrWarmupActive?: number;
	snrSampleCount?: number;
};

export type HeartPyResult = {
	bpm: number;
	ibiMs: number[];
	rrList: number[];
	peakList: number[];
	peakTimestamps?: number[];
	peakListRaw?: number[];
	binaryPeakMask?: number[];
	waveform_values?: number[];
	waveform_timestamps?: number[];
	sdnn: number;
	rmssd: number;
	sdsd: number;
	pnn20: number;
	pnn50: number;
	nn20: number;
	nn50: number;
	mad: number;
	sd1: number;
	sd2: number;
	sd1sd2Ratio: number;
	ellipseArea: number;
	vlf: number;
	lf: number;
	hf: number;
	lfhf: number;
	totalPower: number;
	lfNorm: number;
	hfNorm: number;
	breathingRate: number;
	quality: QualityInfo;
	segments?: HeartPyResult[];
};

export type RuntimeConfig = {
	jsiEnabled: boolean;
	zeroCopyEnabled: boolean;
	debug: boolean;
	maxSamplesPerPush: number;
};

export type RuntimeConfigPatch = Partial<RuntimeConfig>;

export type HeartPyMetrics = HeartPyResult;

export type RealtimeHandle = number;
