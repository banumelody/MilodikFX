import React from 'react';
import { Dropdown, DropdownOption } from './Dropdown';

export interface ExpressionAssignmentProps {
  expNum: number;
  selectedParam?: string;
  onChange: (paramId: string) => void;
  availableParams?: DropdownOption[];
}

export const ExpressionAssignment: React.FC<ExpressionAssignmentProps> = ({
  expNum,
  selectedParam,
  onChange,
  availableParams = [
    { value: 'gain', label: 'Gain' },
    { value: 'level', label: 'Level' },
    { value: 'mix', label: 'Mix' },
    { value: 'tone', label: 'Tone' },
  ],
}) => {
  return (
    <div className="flex flex-col gap-2">
      <label className="text-sm font-medium text-gray-300">{`EXP ${expNum}`}</label>
      <Dropdown
        options={availableParams}
        value={selectedParam || ''}
        onChange={(value) => onChange(String(value))}
        placeholder="Assign parameter..."
      />
    </div>
  );
};
