import React from 'react';
import { LabeledValue } from './index';

export interface EditTabProps {
  selectedEffect?: any;
  onParameterChange?: (name: string, value: number) => void;
}

export const EditTab: React.FC<EditTabProps> = ({
  selectedEffect,
}) => {
  return (
    <div className="p-6 flex flex-col gap-4">
      {selectedEffect ? (
        <>
          <h2 className="text-2xl font-bold text-white">{selectedEffect.name}</h2>
          <p className="text-gray-400">{selectedEffect.type}</p>
          <div className="grid grid-cols-2 gap-4">
            {selectedEffect.parameters?.map((param: any) => (
              <div key={param.id} className="rounded-lg bg-gray-800 border border-gray-700 p-4">
                <LabeledValue
                  label={param.name}
                  value={param.value.toFixed(2)}
                  units={param.unit}
                  size="md"
                />
              </div>
            ))}
          </div>
        </>
      ) : (
        <div className="text-gray-500 text-center py-12">
          Select an effect to edit its parameters
        </div>
      )}
    </div>
  );
};
