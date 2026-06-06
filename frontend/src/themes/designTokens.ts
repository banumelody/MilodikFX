export const designTokens = {
  colors: {
    primary: {
      light: '#3b82f6',
      main: '#2563eb',
      dark: '#1e40af',
    },
    secondary: {
      light: '#10b981',
      main: '#059669',
      dark: '#047857',
    },
    warning: {
      light: '#f59e0b',
      main: '#d97706',
      dark: '#b45309',
    },
    error: {
      light: '#ef4444',
      main: '#dc2626',
      dark: '#b91c1c',
    },
    success: {
      light: '#10b981',
      main: '#059669',
      dark: '#047857',
    },
    neutral: {
      light: '#f3f4f6',
      main: '#e5e7eb',
      dark: '#1f2937',
    },
  },
  spacing: {
    xs: '4px',
    sm: '8px',
    md: '12px',
    lg: '16px',
    xl: '24px',
    '2xl': '32px',
  },
  typography: {
    heading: { size: '18px', weight: 600 },
    subheading: { size: '16px', weight: 500 },
    body: { size: '14px', weight: 400 },
    small: { size: '12px', weight: 400 },
    label: { size: '13px', weight: 500 },
  },
  borderRadius: {
    sm: '4px',
    md: '8px',
    lg: '12px',
    full: '9999px',
  },
  shadows: {
    sm: '0 1px 2px 0 rgba(0, 0, 0, 0.05)',
    md: '0 4px 6px -1px rgba(0, 0, 0, 0.1)',
    lg: '0 10px 15px -3px rgba(0, 0, 0, 0.1)',
    xl: '0 20px 25px -5px rgba(0, 0, 0, 0.1)',
  },
} as const;

export type DesignTokens = typeof designTokens;
