import { memo, useState } from 'react';
import type { DeviceRequest, DevicesResponse } from '../services/api';

export interface DeviceSettingsProps {
  devices: DevicesResponse | null;
  busy: boolean;
  error: string | null;
  onApply: (request: DeviceRequest) => void;
  onRefresh: () => void;
  onOptimise: () => void;
}

function latencyClass(ms: number) {
  if (ms <= 0) return '';
  if (ms <= 12) return ' pill--good';
  if (ms <= 25) return ' pill--warn';
  return ' pill--bad';
}

function DeviceSettingsBase({
  devices,
  busy,
  error,
  onApply,
  onRefresh,
  onOptimise,
}: DeviceSettingsProps) {
  const [open, setOpen] = useState(false);

  const current = devices?.current;
  const available = devices?.available;
  const routing = devices?.inputRouting;

  const currentType = available?.types.find((type) => type.name === available.currentType);

  return (
    <section className="panel" aria-label="Audio device">
      <header className="panel__head">
        <h2 className="panel__title">Audio Device</h2>
        <div className="panel__actions">
          <button
            type="button"
            className="btn btn--ghost"
            disabled={busy}
            onClick={onOptimise}
            title="Cari ulang driver dengan latensi terendah"
          >
            Optimalkan latensi
          </button>
          <button type="button" className="btn btn--ghost" onClick={() => setOpen((v) => !v)}>
            {open ? 'Tutup' : 'Ubah'}
          </button>
        </div>
      </header>

      {current ? (
        <div className="device-summary">
          <div className="device-summary__name" title={current.outputDevice}>
            {current.inputDevice || 'Tidak ada input'}
          </div>
          <div className="device-summary__pills">
            <span className={`pill${current.lowLatency ? ' pill--good' : ' pill--warn'}`}>
              {current.type || 'Belum terbuka'}
            </span>
            <span className="pill">{(current.sampleRate / 1000).toFixed(1)} kHz</span>
            <span className="pill">{current.bufferSize} smp</span>
            <span className={`pill${latencyClass(current.roundTripLatencyMs)}`}>
              {current.roundTripLatencyMs.toFixed(1)} ms bolak-balik
            </span>
            <span className="pill">
              {current.inputChannels} in / {current.outputChannels} out
            </span>
          </div>
        </div>
      ) : (
        <p className="panel__empty">Menunggu perangkat audio...</p>
      )}

      {error ? <p className="panel__error">{error}</p> : null}

      {open && available ? (
        <div className="device-form">
          <label>
            <span>Driver</span>
            <select
              value={available.currentType}
              disabled={busy}
              onChange={(event) => onApply({ type: event.target.value })}
            >
              {available.types.map((type) => (
                <option key={type.name} value={type.name}>
                  {type.name}
                  {type.lowLatency ? ' - latensi rendah' : ''}
                </option>
              ))}
            </select>
          </label>

          <label>
            <span>Input</span>
            <select
              value={current?.inputDevice ?? ''}
              disabled={busy || !currentType?.inputs.length}
              onChange={(event) => onApply({ inputDevice: event.target.value })}
            >
              {(currentType?.inputs ?? []).map((name) => (
                <option key={name} value={name}>
                  {name}
                </option>
              ))}
            </select>
          </label>

          <label>
            <span>Output</span>
            <select
              value={current?.outputDevice ?? ''}
              disabled={busy || !currentType?.outputs.length}
              onChange={(event) => onApply({ outputDevice: event.target.value })}
            >
              {(currentType?.outputs ?? []).map((name) => (
                <option key={name} value={name}>
                  {name}
                </option>
              ))}
            </select>
          </label>

          <label>
            <span>Sample rate</span>
            <select
              value={current?.sampleRate ?? 0}
              disabled={busy || !available.availableSampleRates.length}
              onChange={(event) => onApply({ sampleRate: Number(event.target.value) })}
            >
              {available.availableSampleRates.map((rate) => (
                <option key={rate} value={rate}>
                  {rate.toLocaleString()} Hz
                </option>
              ))}
            </select>
          </label>

          <label>
            <span>Buffer</span>
            <select
              value={current?.bufferSize ?? 0}
              disabled={busy || !available.availableBufferSizes.length}
              onChange={(event) => onApply({ bufferSize: Number(event.target.value) })}
            >
              {available.availableBufferSizes.map((size) => (
                <option key={size} value={size}>
                  {size} sampel
                  {current?.sampleRate
                    ? ` (${((size / current.sampleRate) * 1000).toFixed(1)} ms)`
                    : ''}
                </option>
              ))}
            </select>
          </label>

          {routing && routing.ports.length > 1 ? (
            <>
              <label>
                <span>Kanal L dari</span>
                <select
                  value={routing.left}
                  disabled={busy}
                  onChange={(event) => onApply({ inputPortLeft: event.target.value })}
                >
                  <option value="">Otomatis (port pertama)</option>
                  {routing.ports.map((port) => (
                    <option key={port.name} value={port.name} disabled={!port.available}>
                      {port.name}
                      {port.available ? '' : ' - tidak aktif'}
                    </option>
                  ))}
                </select>
              </label>

              <label>
                <span>Kanal R dari</span>
                <select
                  value={routing.right}
                  disabled={busy}
                  onChange={(event) => onApply({ inputPortRight: event.target.value })}
                >
                  <option value="">Otomatis (port kedua)</option>
                  {routing.ports.map((port) => (
                    <option key={port.name} value={port.name} disabled={!port.available}>
                      {port.name}
                      {port.available ? '' : ' - tidak aktif'}
                    </option>
                  ))}
                </select>
              </label>
            </>
          ) : null}

          <button type="button" className="btn btn--ghost" onClick={onRefresh} disabled={busy}>
            Pindai ulang
          </button>

          <p className="device-form__hint">
            Buffer terkecil memberi latensi terendah. Kalau suara mulai putus-putus, naikkan satu
            langkah. ASIO muncul di daftar ini setelah aplikasi dibangun dengan Steinberg ASIO SDK.
          </p>

          {routing && routing.ports.length > 1 ? (
            <p className="device-form__hint">
              Pilihan port melekat pada perangkat, bukan pada preset - ia menggambarkan kabel di
              rig-mu. Untuk gitar dua jack (magnetik + piezo), colok ke dua port lalu pakai mode
              split <strong>L/R</strong> supaya tiap pickup dapat chain sendiri.
            </p>
          ) : null}
        </div>
      ) : null}
    </section>
  );
}

export const DeviceSettings = memo(DeviceSettingsBase);

export default DeviceSettings;
