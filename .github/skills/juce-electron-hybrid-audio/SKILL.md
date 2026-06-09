---
name: juce-electron-hybrid-audio
description: "Build and validate a hybrid desktop app using C++, JUCE, audio engine DSP, HTML, JavaScript, and Electron. Use when planning architecture, wiring IPC, implementing features, and verifying low-latency audio behavior."
argument-hint: "Feature or workflow to implement, for example: add compressor parameter sync from JUCE DSP to Electron UI"
user-invocable: true
disable-model-invocation: false
---

# JUCE Electron Hybrid Audio Workflow

## What This Skill Produces

This skill produces an implementation-ready workflow for a hybrid app where:

- C++ and JUCE handle real-time audio engine and DSP
- Electron hosts desktop shell and IPC bridge
- HTML and JavaScript provide UI and interaction logic

## When to Use

- You are adding or changing DSP features in a JUCE audio engine.
- You need reliable parameter synchronization between native audio code and Electron UI.
- You want repeatable checks for audio correctness, latency safety, and UI-state consistency.

## Procedure

1. Define the feature contract.

- Specify parameters, ranges, defaults, units, and automation expectations.
- Define ownership for each state value (audio engine, main process, renderer).

2. Design data flow and thread boundaries.

- Keep real-time audio thread free of locks, allocations, and blocking IO.
- Route UI updates through lock-safe state snapshots or message queues.

3. Implement JUCE engine changes.

- Add parameter definitions and validation.
- Integrate DSP block processing changes with deterministic behavior.
- Expose read/write points for IPC-safe parameter sync.

4. Implement Electron bridge updates.

- Main process: define secure IPC channels and payload schemas.
- Preload: expose minimal, explicit API to renderer.
- Renderer HTML/JS: bind UI controls to validated parameter messages.

5. Add synchronization safeguards.

- Debounce or batch high-frequency UI messages.
- Clamp values on both renderer and native sides.
- Ensure one source of truth for parameter state reconciliation.

6. Verify end-to-end behavior.

- Parameter change in UI updates DSP audibly and reflects in state.
- Preset/load flows keep UI and engine in sync.
- App start and reconnect flows restore expected values.

7. Run quality checks and document outcomes.

- Confirm no audio glitches during stress interactions.
- Confirm IPC errors are handled without engine instability.
- Record feature notes and test evidence.

## Decision Points

- If a parameter updates faster than UI needs:
  - Prefer throttled UI refresh while preserving full-rate DSP updates.
- If IPC traffic causes jitter:
  - Move to event coalescing or pull-based state snapshots.
- If state conflicts appear between engine and UI:
  - Keep engine authoritative and perform explicit UI reconciliation.
- If cross-process schema drift appears:
  - Introduce versioned payload contracts.

## Completion Checks

- Audio thread safety: no blocking calls in real-time path.
- Parameter integrity: values are clamped and validated at all boundaries.
- Sync correctness: UI state, IPC state, and engine state converge after updates.
- Resilience: invalid payloads and disconnected renderer do not break audio processing.
- Test coverage: unit/integration checks added for changed feature paths.

## Example Prompts

- /juce-electron-hybrid-audio Add a tone control with JUCE DSP processing and Electron slider binding.
- /juce-electron-hybrid-audio Implement preset save and recall across JUCE engine and HTML UI.
- /juce-electron-hybrid-audio Diagnose parameter desync between renderer and audio engine under rapid automation.

## Related Next Customizations

- Add `.github/instructions/audio-realtime.instructions.md` for always-on real-time safety constraints.
- Add `.github/prompts/add-dsp-parameter.prompt.md` for one-shot parameter scaffolding.
- Add `.github/agents/hybrid-audio-implementer.agent.md` for staged implementation and validation.
