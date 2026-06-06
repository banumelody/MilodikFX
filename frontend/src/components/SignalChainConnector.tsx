import React from 'react';

export interface SignalChainConnectorProps {
  fromX: number;
  fromY: number;
  toX: number;
  toY: number;
  color?: string;
}

export const SignalChainConnector: React.FC<SignalChainConnectorProps> = ({
  fromX,
  fromY,
  toX,
  toY,
  color = '#06b6d4',
}) => {
  const distance = Math.abs(toX - fromX);
  const controlOffset = Math.min(distance / 2, 50);

  return (
    <svg
      className="absolute top-0 left-0 w-full h-full pointer-events-none"
      style={{ zIndex: 0 }}
    >
      <defs>
        <marker
          id="arrowhead"
          markerWidth="10"
          markerHeight="10"
          refX="9"
          refY="3"
          orient="auto"
        >
          <polygon points="0 0, 10 3, 0 6" fill={color} />
        </marker>
        <linearGradient id="lineGradient" x1="0%" y1="0%" x2="100%" y2="0%">
          <stop offset="0%" stopColor={color} stopOpacity="0.5" />
          <stop offset="100%" stopColor={color} stopOpacity="1" />
        </linearGradient>
      </defs>
      <path
        d={`M ${fromX} ${fromY} C ${fromX + controlOffset} ${fromY}, ${toX - controlOffset} ${toY}, ${toX} ${toY}`}
        fill="none"
        stroke="url(#lineGradient)"
        strokeWidth="3"
        markerEnd="url(#arrowhead)"
      />
    </svg>
  );
};
