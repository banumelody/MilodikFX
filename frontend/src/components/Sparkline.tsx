export interface SparklineProps {
  /** Oldest first. Values are clamped into [0, max]. */
  values: number[];
  max?: number;
  label?: string;
  width?: number;
  height?: number;
}

/**
 * A tiny history plot. Purely client-side: the values are already arriving for
 * the readouts, so nothing extra is asked of the engine.
 */
export function Sparkline({ values, max = 100, label, width = 260, height = 40 }: SparklineProps) {
  if (values.length < 2) {
    return <div className="sparkline sparkline--empty" aria-label={label} />;
  }

  const safeMax = Math.max(1, max);
  const stepX = width / (values.length - 1);

  const points = values
    .map((value, index) => {
      const clamped = Math.min(safeMax, Math.max(0, value));
      const x = index * stepX;
      const y = height - (clamped / safeMax) * height;
      return `${x.toFixed(1)},${y.toFixed(1)}`;
    })
    .join(' ');

  const latest = values[values.length - 1];
  const peak = Math.max(...values);

  return (
    <svg
      className="sparkline"
      viewBox={`0 0 ${width} ${height}`}
      preserveAspectRatio="none"
      role="img"
      aria-label={
        label ? `${label}: sekarang ${latest.toFixed(1)}, puncak ${peak.toFixed(1)}` : undefined
      }
    >
      <polyline className="sparkline__line" points={points} />
    </svg>
  );
}

export default Sparkline;
