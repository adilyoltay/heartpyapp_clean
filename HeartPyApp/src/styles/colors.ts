// Calm Color Palette for HeartPy Minimalist Design
// Sakin, yumuşak renkler - göz yormayan, huzurlu

export const COLORS = {
  // Primary colors
  primary: '#6B7280', // Soft gray
  secondary: '#9CA3AF', // Light gray
  accent: '#10B981', // Gentle green

  // Backgrounds
  background: '#FAFAFA', // Very light gray
  surface: '#FFFFFF', // Pure white
  card: '#FFFFFF', // Card backgrounds

  // Text
  text: '#374151', // Soft dark gray
  textSecondary: '#6B7280', // Muted gray
  textInverse: '#FFFFFF', // White text

  // Minimal contrast
  border: '#E5E7EB', // Very light border
  shadow: 'rgba(0,0,0,0.05)', // Subtle shadow

  // Status colors
  success: '#10B981', // Green for good values
  warning: '#F59E0B', // Amber for attention
  error: '#EF4444', // Red for critical

  // BPM adaptive colors
  bpmNormal: '#10B981', // Green for normal BPM (60-100)
  bpmHigh: '#F59E0B', // Amber for high BPM (100+)
  bpmLow: '#3B82F6', // Blue for low BPM (<60)

  // Confidence colors
  confidenceHigh: '#10B981', // Green for high confidence (>90%)
  confidenceMedium: '#F59E0B', // Amber for medium confidence (70-90%)
  confidenceLow: '#EF4444', // Red for low confidence (<70%)
} as const;

// Color utility functions
export const getBpmColor = (bpm: number): string => {
  if (bpm < 60) {
    return COLORS.bpmLow;
  }
  if (bpm > 100) {
    return COLORS.bpmHigh;
  }
  return COLORS.bpmNormal;
};

export const getConfidenceColor = (confidence: number): string => {
  if (confidence >= 0.9) {
    return COLORS.confidenceHigh;
  }
  if (confidence >= 0.7) {
    return COLORS.confidenceMedium;
  }
  return COLORS.confidenceLow;
};

export const getStatusBarStyle = (
  isDark: boolean = false,
): 'light-content' | 'dark-content' => {
  return isDark ? 'light-content' : 'dark-content';
};
