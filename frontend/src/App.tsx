import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

import { reorderBy, useChainReorder } from './hooks/useChainReorder';

import { AppFooter } from './components/AppFooter';
import { BoardPalette } from './components/BoardPalette';
import { ChainStrip } from './components/ChainStrip';
import { DeviceSettings } from './components/DeviceSettings';
import { EffectRack, EFFECT_ACCENTS } from './components/EffectRack';
import { Knob } from './components/Knob';
import { LevelMeter, ReductionMeter } from './components/LevelMeter';
import { LooperPanel } from './components/LooperPanel';
import { MidiMapping } from './components/MidiMapping';
import { ModulationPanel } from './components/ModulationPanel';
import { NamPanel } from './components/NamPanel';
import { PerformView } from './components/PerformView';
import { PresetControls } from './components/PresetControls';
import { SceneGrid } from './components/SceneGrid';
import { Sparkline } from './components/Sparkline';
import { TempoPanel } from './components/TempoPanel';
import { TunerDisplay } from './components/TunerDisplay';
import { UpdateBanner } from './components/UpdateBanner';
import {
  deletePreset,
  exportPreset,
  getDevices,
  getEffects,
  getHistory,
  getModifiers,
  getChainOrder,
  getPins,
  getPresets,
  getUpdate,
  importPreset,
  loadPreset,
  optimiseDevice,
  redoChange,
  revealIrFolder,
  savePreset,
  selectChannel,
  ApiError,
  setDevice,
  setEffectEnabled,
  setParameter,
  setPresetMetadata,
  setChainBoard,
  setChainBuses,
  setChainOrder,
  subscribeLevels,
  togglePin,
  undoChange,
} from './services/api';
import { isPluginHost } from './services/transport';
import type {
  DeviceRequest,
  DevicesResponse,
  EffectDescriptor,
  HistoryState,
  ChainOrderState,
  Levels,
  PinnedControl,
  PresetMetadata,
  UpdateInfo,
} from './services/api';

type Connection = 'connecting' | 'online' | 'offline';

/** Remembers the last release the user dismissed, so a newer one still shows. */
const DISMISSED_UPDATE_KEY = 'milodikfx.update.dismissed';

/** Which posture the app opens in: the dense Edit rack, or the big Perform screen. */
const VIEW_KEY = 'milodikfx.view';
type View = 'edit' | 'perform';

/** Coalescing window for parameter writes, in ms. */
const WRITE_INTERVAL_MS = 40;

/** How many CPU samples the history plot keeps (~60 s at 100 ms polling). */
const CPU_HISTORY_LENGTH = 600;

const IDLE_LEVELS: Levels = {
  inputLevel: -100,
  chainInputLevel: -100,
  outputLevel: -100,
  gateGain: 1,
  compressorReductionDb: 0,
  limiterReductionDb: 0,
  cpuPercent: 0,
  sampleRate: 0,
  bufferSize: 0,
  audioRunning: false,
  floorDb: -100,
};

function describeError(error: unknown) {
  if (error instanceof Error) return error.message;
  return String(error);
}

/**
 * True when this bundle is running inside the VST3's WebView rather than the
 * standalone app. Constant for the session, so it is read once here.
 *
 * What it hides is everything the *host* owns: the audio device, the MIDI
 * ports, the update check, and the looper the plugin's chain does not build.
 * Everything else is the same screen either way.
 */
const IN_PLUGIN = isPluginHost();

export function App() {
  const inPlugin = IN_PLUGIN;
  const [effects, setEffects] = useState<EffectDescriptor[]>([]);
  const [levels, setLevels] = useState<Levels>(IDLE_LEVELS);
  const [cpuHistory, setCpuHistory] = useState<number[]>([]);
  const [devices, setDevices] = useState<DevicesResponse | null>(null);
  const [presets, setPresets] = useState<string[]>([]);
  const [presetDetails, setPresetDetails] = useState<PresetMetadata[]>([]);
  const [selectedPreset, setSelectedPreset] = useState('');
  const [connection, setConnection] = useState<Connection>('connecting');
  const [message, setMessage] = useState<string | null>(null);
  const [update, setUpdate] = useState<UpdateInfo | null>(null);
  const [dismissedUpdate, setDismissedUpdate] = useState<string>(() => {
    try {
      return window.localStorage.getItem(DISMISSED_UPDATE_KEY) ?? '';
    } catch {
      return '';
    }
  });
  const [view, setView] = useState<View>(() => {
    try {
      return window.localStorage.getItem(VIEW_KEY) === 'perform' ? 'perform' : 'edit';
    } catch {
      return 'edit';
    }
  });

  const chooseView = useCallback((next: View) => {
    setView(next);
    try {
      window.localStorage.setItem(VIEW_KEY, next);
    } catch {
      /* private mode; it just opens in Edit next time */
    }
  }, []);
  const [deviceError, setDeviceError] = useState<string | null>(null);
  const [deviceBusy, setDeviceBusy] = useState(false);
  const [history, setHistory] = useState<HistoryState>({
    canUndo: false,
    canRedo: false,
    undoDepth: 0,
    redoDepth: 0,
  });

  // Writes are coalesced per parameter: dragging a knob fires a pointermove per
  // frame, and the engine runs one thread per connection.
  const pendingWrites = useRef(
    new Map<string, { effect: string; parameter: string; value: number | string }>(),
  );
  const flushTimer = useRef<number | null>(null);

  // Watches the engine's chain version (see the levels subscription). syncToken
  // increments whenever a MIDI-driven change lands, so the scene views know to
  // refetch even though they hold their own scene state.
  const chainVersionRef = useRef<number | undefined>(undefined);
  const syncTimer = useRef<number | null>(null);
  const [syncToken, setSyncToken] = useState(0);

  const [modulatedParams, setModulatedParams] = useState<Set<string>>(() => new Set());
  const [pins, setPins] = useState<PinnedControl[]>([]);
  const [chainOrder, setChain] = useState<ChainOrderState | null>(null);

  const refreshEffects = useCallback(async () => {
    const response = await getEffects();
    setEffects(response.effects);
  }, []);

  // "<effect>.<parameter>" keys, so the rack can light the pin button without
  // scanning the pin list once per knob.
  const pinnedParams = useMemo(
    () => new Set(pins.map((pin) => `${pin.effect}.${pin.parameter}`)),
    [pins],
  );

  // Which parameters a modifier currently owns, as "<effect>.<parameter>" keys.
  // The rack shows these knobs inert, since the modifier writes them each block.
  const refreshModifiers = useCallback(async () => {
    try {
      const state = await getModifiers();
      setModulatedParams(
        new Set(state.modifiers.filter((m) => m.active).map((m) => `${m.effect}.${m.parameter}`)),
      );
    } catch {
      /* the connection banner already reports an unreachable engine */
    }
  }, []);

  // The knobs this preset wants on the stage screen. They live in the preset, so
  // changing preset changes what Perform shows.
  const refreshPins = useCallback(async () => {
    try {
      setPins((await getPins()).pins);
    } catch {
      /* an engine without /api/pins simply shows no pinned controls */
    }
  }, []);

  // The chain's processing order. The effects listing already arrives in this
  // order, so the rack and the chain strip need no help drawing it -- this is
  // only for knowing which stages may move and which are pinned.
  const refreshChainOrder = useCallback(async () => {
    try {
      setChain(await getChainOrder());
    } catch {
      /* an engine without /api/chain simply offers no reordering */
    }
  }, []);

  const handleMoveStage = useCallback((effectId: string, delta: number) => {
    void (async () => {
      try {
        const current = await getChainOrder();
        const from = current.order.indexOf(effectId);
        const to = from + delta;

        if (from < 0 || to < 0 || to >= current.order.length) return;

        const next = [...current.order];
        next.splice(to, 0, ...next.splice(from, 1));

        setChain(await setChainOrder(next));

        // The rack draws itself from the effects listing, which the engine emits
        // in chain order -- so refetching it is what makes the cards move.
        await refreshEffects();
      } catch {
        /* the engine refused it -- a pinned stage -- so nothing changes */
      }
    })();
  }, [refreshEffects]);

  // Applies a whole new order at once -- what a drag or a keyboard move
  // produces -- rather than one step at a time.
  const applyChainOrder = useCallback(
    (next: string[]) => {
      void (async () => {
        try {
          setChain(await setChainOrder(next));
          await refreshEffects();
        } catch {
          /* refused (a pinned stage): the rack keeps what the engine still has */
        }
      })();
    },
    [refreshEffects],
  );

  // Which stages sit between the split and the mixer. Those are the only ones
  // where "path A or path B" means anything, so they are the only ones offered
  // the choice -- a selector on a stage outside the section would do nothing.
  const parallelSection = useMemo(() => {
    const order = chainOrder?.order ?? [];
    const from = order.indexOf('split');
    const to = order.indexOf('mixer');

    if (from < 0 || to < 0 || to <= from + 1) return new Set<string>();

    return new Set(order.slice(from + 1, to));
  }, [chainOrder]);

  const busB = useMemo(() => new Set(chainOrder?.busB ?? []), [chainOrder]);

  const handleBusChange = useCallback(
    (effectId: string, bus: 'A' | 'B') => {
      void (async () => {
        try {
          const current = new Set(chainOrder?.busB ?? []);

          if (bus === 'B') current.add(effectId);
          else current.delete(effectId);

          setChain(await setChainBuses([...current]));
        } catch {
          /* the engine keeps whatever it had */
        }
      })();
    },
    [chainOrder],
  );

  // Which stages are on the board. An engine that does not report the list at
  // all is one from before v0.30, and there everything was placed -- so an
  // absent list must read as "all", never as "none".
  const placed = useMemo(() => {
    if (chainOrder?.placed == null) return null;
    return new Set(chainOrder.placed);
  }, [chainOrder]);

  /**
   * Applies a new board.
   *
   * The Splitter and the Mixer travel together: Apple adds the Mixer the moment
   * a Splitter is dropped, and removing the Splitter has to take it away again
   * or the board keeps a mix point with nothing to mix.
   */
  const applyBoard = useCallback(
    (next: Set<string>) => {
      if (next.has('split')) next.add('mixer');
      else next.delete('mixer');

      void (async () => {
        try {
          setChain(await setChainBoard([...next]));
          await refreshEffects();
        } catch {
          /* the engine keeps whatever it had */
        }
      })();
    },
    [refreshEffects],
  );

  const handleRemoveStage = useCallback(
    (effectId: string) => {
      const next = new Set(placed ?? []);
      next.delete(effectId);
      applyBoard(next);
    },
    [placed, applyBoard],
  );

  const handlePlaceStage = useCallback(
    (effectId: string, beforeId: string | null) => {
      const next = new Set(placed ?? []);
      next.add(effectId);
      applyBoard(next);

      // Position is a separate value from placement, so a drop onto a card is
      // two changes: put the block on the board, then move it to where it
      // landed. A drop on empty space leaves it at its build position.
      if (beforeId != null && beforeId !== effectId) {
        const order = chainOrder?.order ?? [];
        const moved = reorderBy(order, effectId, beforeId);
        if (moved !== order) applyChainOrder(moved);
      }
    },
    [placed, applyBoard, chainOrder, applyChainOrder],
  );

  const {
    state: dragState,
    handleProps: dragHandleProps,
    paletteProps,
  } = useChainReorder({
    order: chainOrder?.order ?? [],
    fixed: chainOrder?.fixed ?? [],
    onReorder: applyChainOrder,
    onPlace: handlePlaceStage,
  });

  const handleTogglePin = useCallback((effectId: string, parameterId: string) => {
    void (async () => {
      try {
        setPins((await togglePin(effectId, parameterId)).pins);
      } catch {
        /* full, or not pinnable; the current list stands */
      }
    })();
  }, []);

  const refreshDevices = useCallback(async () => {
    // The host owns the device inside a plugin; there is no endpoint to ask.
    if (IN_PLUGIN) return;

    try {
      setDevices(await getDevices());
      setDeviceError(null);
    } catch (error) {
      setDeviceError(describeError(error));
    }
  }, []);

  const refreshPresets = useCallback(async () => {
    const response = await getPresets();
    setPresets(response.presets);
    setPresetDetails(response.details ?? []);
    setSelectedPreset(response.selected);
  }, []);

  useEffect(() => {
    let cancelled = false;

    const bootstrap = async () => {
      try {
        await Promise.all([
          refreshEffects(),
          refreshDevices(),
          refreshPresets(),
          refreshModifiers(),
          refreshPins(),
          refreshChainOrder(),
        ]);
        if (!cancelled) setConnection('online');
      } catch (error) {
        if (!cancelled) {
          setConnection('offline');
          setMessage(describeError(error));
        }
      }
    };

    void bootstrap();
    return () => {
      cancelled = true;
    };
  }, [refreshEffects, refreshDevices, refreshPresets, refreshModifiers, refreshPins, refreshChainOrder]);

  const handleModifiersChanged = useCallback(() => {
    void refreshModifiers();
    void refreshEffects();
  }, [refreshModifiers, refreshEffects]);

  // The update check is deliberately separate from bootstrap: it reaches out to
  // GitHub, which may be slow or blocked, and its failure must never colour the
  // "connected to engine" status or hold up the rack.
  useEffect(() => {
    // A plugin has no business reaching out to GitHub from inside someone's DAW.
    if (IN_PLUGIN) return undefined;

    let cancelled = false;

    getUpdate()
      .then((info) => {
        if (!cancelled) setUpdate(info);
      })
      .catch(() => {
        /* offline or blocked; the banner simply stays hidden */
      });

    return () => {
      cancelled = true;
    };
  }, []);

  const dismissUpdate = useCallback(() => {
    setUpdate((current) => {
      if (current) {
        setDismissedUpdate(current.latest);
        try {
          window.localStorage.setItem(DISMISSED_UPDATE_KEY, current.latest);
        } catch {
          /* private mode; it will just show again next launch */
        }
      }
      return current;
    });
  }, []);

  useEffect(() => {
    const unsubscribe = subscribeLevels(
      (next) => {
        setLevels(next);
        setConnection('online');
        setCpuHistory((history) => {
          const appended = [...history, next.cpuPercent];
          return appended.length > CPU_HISTORY_LENGTH
            ? appended.slice(appended.length - CPU_HISTORY_LENGTH)
            : appended;
        });

        // The chain changed from somewhere the UI did not drive (a footswitch, a
        // MIDI CC). Refetch once, debounced, so a burst of CC moves is one round
        // trip -- and bump a token the scene views watch so they refetch too.
        const version = next.chainVersion;
        if (
          version !== undefined &&
          chainVersionRef.current !== undefined &&
          version !== chainVersionRef.current &&
          syncTimer.current === null
        ) {
          syncTimer.current = window.setTimeout(() => {
            syncTimer.current = null;
            void refreshEffects();
            void refreshModifiers();
            void refreshPins();
            setSyncToken((token) => token + 1);
          }, 200);
        }
        chainVersionRef.current = version;
      },
      () => setConnection('offline'),
    );

    return unsubscribe;
  }, [refreshEffects, refreshModifiers, refreshPins]);

  const flushWrites = useCallback(async () => {
    flushTimer.current = null;

    const batch = Array.from(pendingWrites.current.values());
    pendingWrites.current.clear();

    for (const write of batch) {
      try {
        await setParameter(write.effect, write.parameter, write.value);
      } catch (error) {
        setConnection('offline');
        setMessage(describeError(error));
        return;
      }
    }

    if (pendingWrites.current.size > 0 && flushTimer.current === null)
      flushTimer.current = window.setTimeout(() => void flushWrites(), WRITE_INTERVAL_MS);
  }, []);

  const handleParameterChange = useCallback(
    (effectId: string, parameterId: string, value: number | string) => {
      // Update locally first so the knob tracks the pointer even if the engine
      // is momentarily slow; the value is authoritative on the next refresh.
      setEffects((current) =>
        current.map((effect) =>
          effect.id === effectId
            ? {
                ...effect,
                parameters: effect.parameters.map((parameter) =>
                  parameter.id === parameterId ? { ...parameter, value } : parameter,
                ),
              }
            : effect,
        ),
      );

      pendingWrites.current.set(`${effectId}.${parameterId}`, {
        effect: effectId,
        parameter: parameterId,
        value,
      });

      if (flushTimer.current === null)
        flushTimer.current = window.setTimeout(() => void flushWrites(), WRITE_INTERVAL_MS);
    },
    [flushWrites],
  );

  const handleEnabledChange = useCallback(async (effectId: string, enabled: boolean) => {
    setEffects((current) =>
      current.map((effect) => (effect.id === effectId ? { ...effect, enabled } : effect)),
    );

    try {
      await setEffectEnabled(effectId, enabled);
    } catch (error) {
      setConnection('offline');
      setMessage(describeError(error));
    }
  }, []);

  // Stable void-returning wrappers. The meter stream re-renders App ~22 times a
  // second; the memoised children (EffectRack, ChainStrip, the panels) only stay
  // memoised if the callbacks they receive keep the same identity across those
  // renders, so the inline arrows they used to get are hoisted into useCallback.
  const toggleEffect = useCallback(
    (effectId: string, enabled: boolean) => {
      void handleEnabledChange(effectId, enabled);
    },
    [handleEnabledChange],
  );

  // Selecting a channel jumps the whole block to a saved sound. The engine
  // returns the effect with its new parameter values, so the one card is
  // replaced from the response rather than refetching the whole chain.
  const handleChannelSelect = useCallback((effectId: string, index: number) => {
    void (async () => {
      try {
        const updated = await selectChannel(effectId, index);
        setEffects((current) => current.map((effect) => (effect.id === effectId ? updated : effect)));
      } catch (error) {
        setConnection('offline');
        setMessage(describeError(error));
      }
    })();
  }, []);

  const handleDeviceApply = useCallback(
    async (request: DeviceRequest) => {
      setDeviceBusy(true);
      setDeviceError(null);

      try {
        await setDevice(request);
        await refreshDevices();
      } catch (error) {
        setDeviceError(describeError(error));
        await refreshDevices();
      } finally {
        setDeviceBusy(false);
      }
    },
    [refreshDevices],
  );

  const handleOptimise = useCallback(async () => {
    setDeviceBusy(true);
    setDeviceError(null);

    try {
      const result = await optimiseDevice();
      await refreshDevices();

      const ms = result.current.roundTripLatencyMs;
      setMessage(`Latensi sekarang ${ms.toFixed(1)} ms (${result.current.bufferSize} sampel)`);
      window.setTimeout(() => setMessage(null), 4000);
    } catch (error) {
      setDeviceError(describeError(error));
      await refreshDevices();
    } finally {
      setDeviceBusy(false);
    }
  }, [refreshDevices]);

  // The rest of the stable wrappers (see toggleEffect above for why).
  const applyDevice = useCallback(
    (request: DeviceRequest) => {
      void handleDeviceApply(request);
    },
    [handleDeviceApply],
  );
  const refreshDevicesVoid = useCallback(() => {
    void refreshDevices();
  }, [refreshDevices]);
  const optimiseVoid = useCallback(() => {
    void handleOptimise();
  }, [handleOptimise]);
  const refreshEffectsVoid = useCallback(() => {
    void refreshEffects();
  }, [refreshEffects]);

  const withMessage = useCallback(async (action: () => Promise<void>, success: string) => {
    try {
      await action();
      setMessage(success);
      window.setTimeout(() => setMessage(null), 2500);
    } catch (error) {
      setMessage(describeError(error));
    }
  }, []);

  const handlePresetLoad = useCallback(
    (name: string) =>
      void withMessage(async () => {
        await loadPreset(name);
        await refreshEffects();
        // The pins live inside the preset, so this preset's stage screen is a
        // different set of knobs from the last one's.
        await refreshPins();
        // The order travels inside the preset, so a different preset can be a
        // different chain.
        await refreshChainOrder();
        setSelectedPreset(name);
      }, `Preset "${name}" dimuat`),
    [refreshEffects, refreshPins, refreshChainOrder, withMessage],
  );

  const handlePresetSave = useCallback(
    (name: string) =>
      void withMessage(async () => {
        await savePreset(name);
        await refreshPresets();
      }, `Preset "${name}" disimpan`),
    [refreshPresets, withMessage],
  );

  const handlePresetDelete = useCallback(
    (name: string) =>
      void withMessage(async () => {
        await deletePreset(name);
        await refreshPresets();
      }, `Preset "${name}" dihapus`),
    [refreshPresets, withMessage],
  );

  const handleMetadataChange = useCallback(
    (name: string, changes: Parameters<typeof setPresetMetadata>[1]) =>
      void withMessage(async () => {
        await setPresetMetadata(name, changes);
        await refreshPresets();
      }, 'Info preset diperbarui'),
    [refreshPresets, withMessage],
  );

  const handlePresetExport = useCallback(
    (name: string) =>
      void withMessage(async () => {
        const exported = await exportPreset(name);

        // A Blob download rather than writing a file from the engine: the
        // browser already knows where this machine puts downloads.
        const url = URL.createObjectURL(
          new Blob([exported.data], { type: 'application/json' }),
        );

        const link = document.createElement('a');
        link.href = url;
        link.download = exported.filename;
        document.body.appendChild(link);
        link.click();
        link.remove();

        // Revoked on the next tick; doing it immediately can cancel the
        // download before it starts.
        window.setTimeout(() => URL.revokeObjectURL(url), 1000);
      }, 'Preset diekspor'),
    [withMessage],
  );

  const handlePresetImport = useCallback(
    (name: string, data: string) =>
      void withMessage(async () => {
        await importPreset(name, data);
        await refreshPresets();
      }, `Preset "${name}" diimpor`),
    [refreshPresets, withMessage],
  );

  const handleRevealIr = useCallback(
    () =>
      void withMessage(async () => {
        await revealIrFolder('cabinet');
        // The folder is open; a refresh picks up whatever was dropped into it.
        window.setTimeout(() => void refreshEffects(), 1500);
      }, 'Folder impulse response dibuka'),
    [refreshEffects, withMessage],
  );

  const master = useMemo(() => effects.find((effect) => effect.id === 'master'), [effects]);
  const masterVolume = master?.parameters.find((parameter) => parameter.id === 'volumeDb');
  const masterMuted = master?.parameters.find((parameter) => parameter.id === 'muted');

  const global = useMemo(() => effects.find((effect) => effect.id === 'global'), [effects]);
  const bypass = global?.parameters.find((parameter) => parameter.id === 'bypass');
  const bpm = global?.parameters.find((parameter) => parameter.id === 'bpm');

  const metronome = useMemo(
    () => effects.find((effect) => effect.id === 'metronome'),
    [effects],
  );

  const offline = connection === 'offline';
  const isMuted = Number(masterMuted?.value ?? 0) >= 0.5;
  const isBypassed = Number(bypass?.value ?? 0) >= 0.5;

  const toggleMute = useCallback(() => {
    if (offline || !masterMuted) return;
    handleParameterChange('master', 'muted', isMuted ? 0 : 1);
  }, [offline, masterMuted, isMuted, handleParameterChange]);

  const toggleBypass = useCallback(() => {
    if (offline || !bypass) return;
    handleParameterChange('global', 'bypass', isBypassed ? 0 : 1);
  }, [offline, bypass, isBypassed, handleParameterChange]);

  const refreshHistory = useCallback(async () => {
    // The DAW has its own undo stack, and a second one inside the plugin
    // fighting it would be worse than none.
    if (IN_PLUGIN) return;

    try {
      setHistory(await getHistory());
    } catch {
      /* the connection banner already says the engine is unreachable */
    }
  }, []);

  // The engine commits a step once the chain has been still for a moment, so
  // the buttons cannot know they have become available without asking.
  useEffect(() => {
    if (IN_PLUGIN) return undefined;

    const timer = window.setInterval(() => void refreshHistory(), 1500);
    void refreshHistory();
    return () => window.clearInterval(timer);
  }, [refreshHistory]);

  const applyHistory = useCallback(
    (action: () => Promise<HistoryState>) =>
      void (async () => {
        try {
          const next = await action();
          setHistory(next);

          // The response carries the effects, so there is no second round trip
          // and no window where the rack still shows the pre-undo values.
          if (next.effects) setEffects(next.effects);
        } catch (error) {
          // 409 just means there was nothing to undo, which a keyboard
          // shortcut does all the time. Not worth a banner.
          if (!(error instanceof ApiError) || error.status !== 409)
            setMessage(describeError(error));
        }
      })(),
    [],
  );

  // Panic controls have to be reachable without hunting for a card. Escape mutes
  // and B compares against the dry signal; both are ignored while typing.
  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      const target = event.target as HTMLElement | null;
      const typing =
        target != null &&
        (target.tagName === 'INPUT' || target.tagName === 'SELECT' || target.isContentEditable);

      // Undo/redo are the exception: they are modified shortcuts by convention,
      // and they should work while a name field has focus too.
      if ((event.ctrlKey || event.metaKey) && !event.altKey) {
        const key = event.key.toLowerCase();

        if (key === 'z') {
          event.preventDefault();
          applyHistory(event.shiftKey ? redoChange : undoChange);
        } else if (key === 'y') {
          event.preventDefault();
          applyHistory(redoChange);
        }

        return;
      }

      if (typing || event.ctrlKey || event.altKey || event.metaKey) return;

      if (event.key === 'Escape') {
        event.preventDefault();
        toggleMute();
      } else if (event.key === 'b' || event.key === 'B') {
        event.preventDefault();
        toggleBypass();
      }
    };

    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [toggleMute, toggleBypass, applyHistory]);

  const scrollToEffect = useCallback((effectId: string) => {
    document.getElementById(`rack-${effectId}`)?.scrollIntoView({
      behavior: 'smooth',
      block: 'nearest',
    });
  }, []);

  // Global has its own controls in the top bar and the tempo panel; the
  // metronome gets the tempo panel too. Neither belongs in the rack of stages.
  const rackEffects = useMemo(
    () =>
      effects.filter(
        (effect) =>
          effect.id !== 'global' && effect.id !== 'metronome' && effect.placed !== false,
      ),
    [effects],
  );

  /** Every placeable stage, whether on the board or not, for the palette. */
  const boardStages = useMemo(
    () => effects.filter((effect) => effect.placed != null),
    [effects],
  );

  return (
    <div className="app">
      <header className="topbar">
        <div className="topbar__brand">
          <span className="topbar__logo" aria-hidden="true" />
          <div>
            <h1>MilodikFX</h1>
            <p>{levels.audioRunning ? 'Audio berjalan' : 'Audio berhenti'}</p>
          </div>
        </div>

        <div className="topbar__view" role="tablist" aria-label="Tampilan">
          <button
            type="button"
            role="tab"
            aria-selected={view === 'perform'}
            className={`viewtab${view === 'perform' ? ' viewtab--active' : ''}`}
            onClick={() => chooseView('perform')}
          >
            Perform
          </button>
          <button
            type="button"
            role="tab"
            aria-selected={view === 'edit'}
            className={`viewtab${view === 'edit' ? ' viewtab--active' : ''}`}
            onClick={() => chooseView('edit')}
          >
            Edit
          </button>
        </div>

        <div className="topbar__meters">
          {/* Post-trim, because that is what the Input knob is dialled against
              and what the chain actually receives. The pre-trim figure is still
              passed in so a clipping interface is reported rather than hidden
              behind a trim that pulled the reading back down. */}
          <LevelMeter
            label="Input"
            db={levels.chainInputLevel}
            sourceDb={levels.inputLevel}
          />
          <LevelMeter label="Output" db={levels.outputLevel} />
          <ReductionMeter label="Comp" db={levels.compressorReductionDb} />
          <ReductionMeter label="Limiter" db={levels.limiterReductionDb} />
        </div>

        <div className="topbar__actions">
          <button
            type="button"
            className="pill-btn"
            disabled={offline || !history.canUndo}
            onClick={() => applyHistory(undoChange)}
            title="Batalkan perubahan terakhir (Ctrl+Z)"
            aria-label="Batalkan"
          >
            ↶
          </button>
          <button
            type="button"
            className="pill-btn"
            disabled={offline || !history.canRedo}
            onClick={() => applyHistory(redoChange)}
            title="Ulangi perubahan (Ctrl+Shift+Z)"
            aria-label="Ulangi"
          >
            ↷
          </button>
          {bypass ? (
            <button
              type="button"
              className={`pill-btn${isBypassed ? ' pill-btn--active' : ''}`}
              disabled={offline}
              onClick={toggleBypass}
              title="Bandingkan dengan sinyal kering (B)"
              aria-pressed={isBypassed}
            >
              Bypass
            </button>
          ) : null}
          {masterMuted ? (
            <button
              type="button"
              className={`pill-btn${isMuted ? ' pill-btn--danger' : ''}`}
              disabled={offline}
              onClick={toggleMute}
              title="Bisukan keluaran (Esc)"
              aria-pressed={isMuted}
            >
              {isMuted ? 'Bisu' : 'Mute'}
            </button>
          ) : null}
        </div>

        <div className="topbar__master">
          {masterVolume ? (
            <Knob
              value={Number(masterVolume.value)}
              min={masterVolume.min}
              max={masterVolume.max}
              step={masterVolume.step}
              defaultValue={masterVolume.default}
              label="Master"
              unit="dB"
              size={88}
              accent={EFFECT_ACCENTS.master}
              disabled={offline}
              format={(value) => (value <= masterVolume.min ? 'MUTE' : value.toFixed(1))}
              onChange={(value) => handleParameterChange('master', 'volumeDb', value)}
            />
          ) : null}
        </div>

        <div className={`status status--${connection}`}>
          <span className="status__dot" aria-hidden="true" />
          <span>
            {connection === 'online'
              ? 'Terhubung'
              : connection === 'connecting'
                ? 'Menghubungkan...'
                : 'Terputus'}
          </span>
        </div>
      </header>

      {offline ? (
        <div className="banner banner--error" role="alert">
          Tidak dapat menghubungi engine audio. Pastikan MilodikFX masih berjalan.
        </div>
      ) : null}

      {isBypassed ? (
        <div className="banner banner--warn" role="status">
          Global bypass aktif — kamu mendengar sinyal kering tanpa efek.
        </div>
      ) : null}

      {message ? (
        <div className="banner" role="status">
          {message}
        </div>
      ) : null}

      {/* No update banner inside a plugin: it has no business reaching out to
          GitHub from someone's DAW, and the host updates it with the installer. */}
      {inPlugin ? null : (
        <UpdateBanner
          info={update && update.latest !== dismissedUpdate ? update : null}
          onDismiss={dismissUpdate}
        />
      )}

      {view === 'perform' ? (
        <PerformView
          levels={levels}
          effects={rackEffects}
          presets={presets}
          selectedPreset={selectedPreset}
          onLoadPreset={handlePresetLoad}
          bpm={bpm}
          pins={pins}
          onParameterChange={handleParameterChange}
          isBypassed={isBypassed}
          isMuted={isMuted}
          onToggleBypass={toggleBypass}
          onToggleMute={toggleMute}
          offline={offline}
          onScenesRecalled={refreshEffectsVoid}
          refreshToken={syncToken}
        />
      ) : (
        <>
      <ChainStrip
        effects={effects}
        disabled={offline}
        onSelect={scrollToEffect}
        onToggle={toggleEffect}
        dragHandleProps={chainOrder ? dragHandleProps : undefined}
        draggingId={dragState.activeId}
        dropTargetId={dragState.overId}
        busB={busB}
      />

      <main className="layout">
        <div className="layout__chain">
          {effects.length === 0 && !offline ? (
            <p className="layout__empty">Memuat rantai efek...</p>
          ) : null}

          {/* "Empty" means nothing of your own is on the board. The input trim
              and the master limiter are pinned, so the rack is never literally
              empty and a length check would never fire. */}
          {chainOrder != null &&
          !offline &&
          rackEffects.every((effect) => effect.removable === false) ? (
            <p className="layout__empty">
              Board kosong &mdash; sinyalnya lewat lurus. Ambil blok dari daftar di kanan.
            </p>
          ) : null}

          {rackEffects.map((effect, index) => (
            <EffectRack
              key={effect.id}
              effect={effect}
              index={index + 1}
              total={rackEffects.length}
              disabled={offline}
              sampleRate={levels.sampleRate || undefined}
              onParameterChange={handleParameterChange}
              onEnabledChange={toggleEffect}
              onChannelSelect={handleChannelSelect}
              modulatedParams={modulatedParams}
              pinnedParams={pinnedParams}
              onTogglePin={handleTogglePin}
              onMove={chainOrder ? handleMoveStage : undefined}
              dragHandleProps={chainOrder ? dragHandleProps(effect.id) : undefined}
              isDragging={dragState.activeId === effect.id}
              isDropTarget={
                dragState.activeId !== null &&
                dragState.overId === effect.id &&
                dragState.overId !== dragState.activeId
              }
              bus={parallelSection.has(effect.id) ? (busB.has(effect.id) ? 'B' : 'A') : undefined}
              onBusChange={handleBusChange}
              movable={!(chainOrder?.fixed ?? []).includes(effect.id)}
              onRemove={chainOrder && effect.removable !== false ? handleRemoveStage : undefined}
              canMoveUp={index > 0 && !(chainOrder?.fixed ?? []).includes(rackEffects[index - 1].id)}
              canMoveDown={
                index < rackEffects.length - 1 &&
                !(chainOrder?.fixed ?? []).includes(rackEffects[index + 1].id)
              }
            />
          ))}
        </div>

        <aside className="layout__side">
          {/* The host owns the audio device and the MIDI ports inside a plugin.
              Offering a device picker there would, at best, be a lie. */}
          {inPlugin ? null : (
            <DeviceSettings
              devices={devices}
              busy={deviceBusy}
              error={deviceError}
              onApply={applyDevice}
              onRefresh={refreshDevicesVoid}
              onOptimise={optimiseVoid}
            />
          )}

          {/* Below the device panel, not above it. The palette is the only panel
              here whose height depends on what you have done -- put it first and
              every fixed panel underneath moves by an amount nobody chose, which
              is how the device readout ended up scrolled out of view. */}
          {chainOrder != null ? (
            <BoardPalette
              stages={boardStages}
              disabled={offline}
              paletteProps={paletteProps}
              activeId={dragState.activeId}
            />
          ) : null}

          <TunerDisplay disabled={offline} />

          <TempoPanel
            bpm={bpm}
            metronome={metronome}
            disabled={offline}
            onParameterChange={handleParameterChange}
            onEnabledChange={toggleEffect}
          />

          <SceneGrid
            effects={rackEffects}
            disabled={offline}
            onRecalled={refreshEffectsVoid}
            refreshToken={syncToken}
          />

          <PresetControls
            presets={presets}
            details={presetDetails}
            selected={selectedPreset}
            busy={offline}
            onLoad={handlePresetLoad}
            onSave={handlePresetSave}
            onDelete={handlePresetDelete}
            onMetadataChange={handleMetadataChange}
            onExport={handlePresetExport}
            onImport={handlePresetImport}
          />

          {inPlugin ? null : <MidiMapping effects={effects} disabled={offline} />}

          <NamPanel disabled={offline} onLibraryChanged={refreshEffectsVoid} />

          {/* The plugin's chain has no looper -- a DAW has its own, and the
              record buffer is tens of megabytes per instance. */}
          {levels.looper === undefined && inPlugin ? null : (
            <LooperPanel disabled={offline} streamed={levels.looper} />
          )}

          <ModulationPanel
            effects={rackEffects}
            disabled={offline}
            onModifiersChanged={handleModifiersChanged}
          />

          <section className="panel" aria-label="Impulse response">
            <header className="panel__head">
              <h2 className="panel__title">Impulse Response</h2>
              <button type="button" className="btn btn--ghost" disabled={offline} onClick={handleRevealIr}>
                Buka folder
              </button>
            </header>
            <p className="panel__hint">
              Letakkan berkas WAV di folder <code>Cabinets</code> atau <code>Reverbs</code>, lalu
              pilih pada kartu Cabinet / Reverb. Tanpa berkas, keduanya memakai algoritma bawaan.
            </p>
          </section>

          <section className="panel" aria-label="Performa">
            <header className="panel__head">
              <h2 className="panel__title">Performa</h2>
            </header>

            <Sparkline values={cpuHistory} max={100} label="Riwayat beban DSP" />

            <dl className="stats">
              <div>
                <dt>Beban DSP</dt>
                <dd className={levels.cpuPercent > 70 ? 'stats--warn' : undefined}>
                  {levels.cpuPercent.toFixed(1)} %
                </dd>
              </div>
              <div>
                <dt>Sample rate</dt>
                <dd>{levels.sampleRate ? `${(levels.sampleRate / 1000).toFixed(1)} kHz` : '--'}</dd>
              </div>
              <div>
                <dt>Buffer</dt>
                <dd>
                  {levels.bufferSize
                    ? `${levels.bufferSize} (${((levels.bufferSize / (levels.sampleRate || 1)) * 1000).toFixed(1)} ms)`
                    : '--'}
                </dd>
              </div>
              <div>
                <dt>Gate</dt>
                <dd>
                  {levels.gateGain > 0.99
                    ? 'Terbuka'
                    : levels.gateGain < 0.01
                      ? 'Tertutup'
                      : 'Menutup'}
                </dd>
              </div>
            </dl>
          </section>
        </aside>
      </main>
        </>
      )}

      <AppFooter version={update?.current} />
    </div>
  );
}

export default App;
