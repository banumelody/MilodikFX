import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { Dropdown } from '../Dropdown';

describe('Dropdown', () => {
  const options = [
    { value: 'option1', label: 'Option 1' },
    { value: 'option2', label: 'Option 2' },
    { value: 'option3', label: 'Option 3' },
  ];

  it('renders with placeholder', () => {
    render(
      <Dropdown
        options={options}
        value=""
        onChange={vi.fn()}
        placeholder="Select an option"
      />
    );
    expect(screen.getByPlaceholderText('Select an option')).toBeInTheDocument();
  });

  it('opens dropdown on click', async () => {
    render(
      <Dropdown
        options={options}
        value=""
        onChange={vi.fn()}
        placeholder="Select"
      />
    );
    
    const input = screen.getByPlaceholderText('Select');
    await userEvent.click(input);
    
    expect(screen.getByText('Option 1')).toBeInTheDocument();
  });

  it('calls onChange with selected value', async () => {
    const onChange = vi.fn();
    render(
      <Dropdown
        options={options}
        value=""
        onChange={onChange}
        placeholder="Select"
      />
    );
    
    const input = screen.getByPlaceholderText('Select');
    await userEvent.click(input);
    
    const option = screen.getByText('Option 2');
    await userEvent.click(option);
    
    expect(onChange).toHaveBeenCalledWith('option2');
  });

  it('displays selected value', () => {
    render(
      <Dropdown
        options={options}
        value="option1"
        onChange={vi.fn()}
      />
    );
    expect(screen.getByDisplayValue('Option 1')).toBeInTheDocument();
  });

  it('filters options by search', async () => {
    render(
      <Dropdown
        options={options}
        value=""
        onChange={vi.fn()}
        placeholder="Search"
      />
    );
    
    const input = screen.getByPlaceholderText('Search') as HTMLInputElement;
    await userEvent.click(input);
    await userEvent.type(input, 'Option 2');
    
    expect(screen.getByText('Option 2')).toBeInTheDocument();
    expect(screen.queryByText('Option 1')).not.toBeInTheDocument();
  });

  it('disables when disabled prop is true', async () => {
    const onChange = vi.fn();
    render(
      <Dropdown
        options={options}
        value=""
        onChange={onChange}
        disabled={true}
      />
    );
    
    const input = screen.getByRole('combobox');
    expect(input).toBeDisabled();
  });
});
