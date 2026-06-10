Design tokens and visual spec for Rotary knob used across MilodikFX UI

Purpose
- Provide precise, reproducible visual tokens so frontend implementers and QA can create a knob that visually matches the reference image (E0B4AFA6-C41E-41B9-AFF3-4AAE87785844.png).

Tokens
- Diameter: 92px (default master knob)
- Outer ring thickness: 6px
- Outer ring color (idle track): #2b3340
- Accent (active arc): #22c55e (green) or #3b82f6 (blue) for other knobs
- Face gradient: radial-gradient(circle at 35% 25%, #2a2f36 0%, #0b0f14 45%, #050609 100%)
- Inner cap diameter: 34% of knob diameter (≈31px for 92px knob)
- Indicator: rectangular marker, width 8% of knob diameter (≈7px), length 28% of diameter (≈26px), centered at top when angle=0
- Indicator gradient: linear-gradient(180deg, rgba(255,255,255,0.95), rgba(255,255,255,0.65), rgba(0,0,0,0.6))
- Shadow: outer subtle drop shadow: 0 10px 30px rgba(0,0,0,0.75);
- Inner inset: inset 0 6px 14px rgba(0,0,0,0.75)
- Highlight strip: top elliptical highlight, 12% of knob height
- Ticks: optional 12 ticks around ring, faint #ffffff10 (10% opacity), 1px length
- Label font: Inter / system sans, size 12px, color #9aa4b2
- Value font: 12px, color #e6eef6

Behavior
- Rotation range: -135deg .. +135deg (270deg sweep)
- Interaction: pointer drag (pointerdown + pointermove) and keyboard (Arrow keys +/- step)
- Step: configurable per instance (default 1 or 0.5 depending on use)
- Accessibility: role="slider", aria-valuemin, aria-valuemax, aria-valuenow

Testing guidance
- Unit tests should verify keyboard adjustments and role/aria attributes.
- Visual regression: capture screenshots of default, mid, and max positions at 92px size.
- Cypress E2E: ensure master-volume-knob exists and pointer interactions change displayed master volume.

Notes
- Provide both a canvas fallback (Knob.tsx) and DOM-based implementation (RotaryKnob.tsx) so specific apps can prefer whichever is easier for visuals vs accessibility.
