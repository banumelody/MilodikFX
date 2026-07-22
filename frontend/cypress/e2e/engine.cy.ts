/**
 * End-to-end against the real engine.
 *
 * Requires MilodikFX.exe to be running; `.github/scripts/run-local-e2e.ps1`
 * starts it, waits for the UI to answer, and runs this suite.
 */

describe('MilodikFX UI against a live engine', () => {
  beforeEach(() => {
    cy.visit('/');
  });

  it('renders the signal chain the engine reports', () => {
    cy.request('/api/effects').then(({ body }) => {
      // "global" holds chain-wide controls and "metronome" is mixed in after
      // the master stage; both get their own panels rather than a rack card.
      const labels: string[] = body.effects
        .filter((effect: { id: string }) => !['global', 'metronome'].includes(effect.id))
        .map((effect: { label: string }) => effect.label);

      expect(labels.length).to.be.greaterThan(5);

      labels.forEach((label) => {
        cy.contains('h2', label).should('exist');
      });
    });
  });

  it('shows a connected status and a running audio device', () => {
    cy.contains('Terhubung', { timeout: 10000 }).should('be.visible');
    cy.get('.status--online').should('exist');
  });

  it('reports meter levels that update', () => {
    cy.get('[role="meter"]').should('have.length.greaterThan', 1);

    cy.request('/api/levels').then(({ body }) => {
      expect(body).to.have.property('inputLevel');
      expect(body).to.have.property('audioRunning');
      expect(body.sampleRate).to.be.greaterThan(0);
    });
  });

  it('writes a knob change through to the engine', () => {
    // Set up the state this needs rather than inheriting it: a knob on a
    // switched-off effect is deliberately inert, so a test that assumed Reverb
    // happened to be on failed for a reason that had nothing to do with knobs.
    cy.request('POST', '/api/effects/reverb/enabled', { enabled: true });
    cy.request('PUT', '/api/effects/reverb/dryWetMix', { value: 0.25 });
    cy.reload();

    // Scope to the Reverb rack: several effects have a knob labelled "Mix".
    cy.get('section[aria-label="Reverb"] [role="slider"][aria-label="Mix"]').as('mix');
    cy.get('@mix').should('have.attr', 'aria-valuenow', '0.25');

    // eventConstructor matters here: trigger() builds a plain Event by default,
    // and React never sees `key` on one of those.
    const arrowUp = { key: 'ArrowUp', eventConstructor: 'KeyboardEvent' as const };

    cy.get('@mix').focus();
    cy.get('@mix').trigger('keydown', arrowUp);
    cy.get('@mix').trigger('keydown', arrowUp);
    cy.get('@mix').trigger('keydown', arrowUp);

    // First that the control moved at all...
    cy.get('@mix').should('have.attr', 'aria-valuenow', '0.28');

    // ...then that the engine, not just the optimistic UI, took the value.
    cy.wait(400);
    cy.request('/api/parameters/reverb/dryWetMix').then(({ body }) => {
      expect(body.value).to.be.closeTo(0.28, 0.005);
    });

    cy.request('PUT', '/api/effects/reverb/dryWetMix', { value: 0.25 });
  });

  it('leaves the knobs of a switched-off effect inert', () => {
    cy.request('POST', '/api/effects/delay/enabled', { enabled: false });
    cy.reload();

    cy.request('/api/effects/delay').then(({ body }) => {
      expect(body.enabled).to.eq(false);
    });

    cy.get('section[aria-label="Delay"] [role="slider"][aria-label="Time"]').as('time');
    cy.get('@time').should('have.attr', 'aria-disabled', 'true');

    cy.get('@time')
      .invoke('attr', 'aria-valuenow')
      .then((before) => {
        cy.get('@time').trigger('keydown', { key: 'ArrowUp', eventConstructor: 'KeyboardEvent' });
        cy.get('@time').should('have.attr', 'aria-valuenow', String(before));
      });
  });

  it('toggles an effect and the engine agrees', () => {
    cy.get('[role="switch"][aria-label="Delay on/off"]').as('toggle');

    cy.get('@toggle')
      .invoke('attr', 'aria-checked')
      .then((before) => {
        cy.get('@toggle').click();
        cy.wait(300);

        cy.request('/api/effects/delay').then(({ body }) => {
          expect(String(body.enabled)).to.not.equal(before);
        });

        // Put it back so the suite leaves no trace.
        cy.get('@toggle').click();
      });
  });

  it('saves and deletes a preset', () => {
    const name = 'Cypress Temp';

    cy.get('input[aria-label="Nama preset"]').clear().type(name);
    cy.contains('button', 'Simpan').click();

    cy.request('/api/presets').then(({ body }) => {
      expect(body.presets).to.include(name);
    });

    cy.request('POST', '/api/presets/delete', { name });
  });

  it('draws the signal chain in order, from input to output', () => {
    cy.get('nav[aria-label="Rantai sinyal"]').should('exist');
    cy.contains('nav[aria-label="Rantai sinyal"] span', 'IN').should('be.visible');
    // The strip scrolls horizontally, and with the amp head added it is now
    // wider than the panel, so OUT sits off the right edge until scrolled to.
    cy.contains('nav[aria-label="Rantai sinyal"] span', 'OUT').scrollIntoView().should('be.visible');

    cy.request('/api/effects').then(({ body }) => {
      const stages = body.effects
        .filter((e: { id: string }) => !['input', 'global', 'metronome'].includes(e.id))
        .map((e: { label: string }) => e.label);

      cy.get('nav[aria-label="Rantai sinyal"] button').should('have.length', stages.length);

      cy.get('nav[aria-label="Rantai sinyal"] button').then(($buttons) => {
        const rendered = [...$buttons].map((b) => b.textContent);
        expect(rendered).to.deep.equal(stages);
      });
    });
  });

  it('toggles global bypass from the top bar', () => {
    cy.request('PUT', '/api/effects/global/bypass', { value: 0 });
    cy.reload();

    cy.contains('button', 'Bypass').click();
    cy.wait(300);

    cy.request('/api/effects/global/bypass').then(({ body }) => {
      expect(Number(body.value)).to.eq(1);
    });

    cy.contains('Global bypass aktif').should('be.visible');

    cy.contains('button', 'Bypass').click();
    cy.wait(300);

    cy.request('/api/effects/global/bypass').then(({ body }) => {
      expect(Number(body.value)).to.eq(0);
    });
  });

  it('mutes with the Escape key', () => {
    cy.request('PUT', '/api/effects/master/muted', { value: 0 });
    cy.reload();

    // The shortcut does nothing until the effect list has arrived, so wait for
    // the control it drives rather than racing the first fetch.
    cy.contains('button', 'Mute').should('be.visible');

    // Waiting for the label to change rather than a fixed 300 ms: writes are
    // coalesced on a timer, so a flat wait raced the flush and this failed
    // about one run in three for reasons that had nothing to do with muting.
    cy.get('body').type('{esc}');
    cy.contains('button', 'Bisu').should('be.visible');

    cy.request('/api/effects/master/muted').then(({ body }) => {
      expect(Number(body.value)).to.eq(1);
    });

    cy.get('body').type('{esc}');
    cy.contains('button', 'Mute').should('be.visible');

    cy.request('/api/effects/master/muted').then(({ body }) => {
      expect(Number(body.value)).to.eq(0);
    });
  });

  it('offers the impulse responses the engine reports', () => {
    cy.request('/api/ir').then(({ body }) => {
      expect(body).to.have.property('cabinetDirectory');
      expect(body).to.have.property('cabinets');

      cy.request('/api/effects/cabinet').then((response) => {
        const irFile = response.body.parameters.find(
          (p: { id: string }) => p.id === 'irFile',
        );

        expect(irFile, 'cabinet exposes an irFile parameter').to.not.be.undefined;
        expect(irFile.type).to.eq('text');
        // Whatever is on disk is exactly what the UI must offer.
        expect(irFile.options).to.deep.equal(body.cabinets);
      });
    });

    cy.get('section[aria-label="Cabinet"] select').should('exist');
  });

  it('exposes the device panel with a latency readout', () => {
    cy.contains('Audio Device').should('be.visible');
    cy.contains(/ms bolak-balik/).should('be.visible');
  });

  it('gives the master stage a labelled mute instead of a bypass switch', () => {
    // A header switch on Master looks like every other effect's bypass but
    // silences the whole app, which is exactly how the output once went dead
    // with nothing on screen to explain it.
    cy.get('section[aria-label="Master"]').within(() => {
      cy.get('[role="switch"][aria-label="Master on/off"]').should('not.exist');
      cy.get('[role="switch"][aria-label="Mute"]').should('exist');
    });

    cy.request('/api/effects/master').then(({ body }) => {
      expect(body.toggleable).to.eq(false);
      expect(body.parameters.map((p: { id: string }) => p.id)).to.include('muted');
    });
  });

  it('refuses to bypass a stage the engine marks as not toggleable', () => {
    cy.request({
      method: 'POST',
      url: '/api/effects/master/enabled',
      body: { enabled: false },
      failOnStatusCode: false,
    }).then((response) => {
      expect(response.status).to.be.oneOf([404, 503]);
    });

    cy.request('/api/effects/master').then(({ body }) => {
      expect(body.enabled).to.eq(true);
    });
  });

  it('switches the tuner on only while its panel is open', () => {
    // Pitch detection costs a background thread in the engine, so the panel is
    // the switch: leaving it analysing behind a closed card is the bug here.
    cy.request('POST', '/api/tuner/enable', { enabled: false });
    cy.reload();

    cy.request('/api/tuner').then(({ body }) => {
      expect(body.enabled).to.eq(false);
    });

    cy.get('section[aria-label="Tuner"]').within(() => {
      cy.contains('button', 'Mulai').click();
    });

    cy.wait(300);
    cy.request('/api/tuner').then(({ body }) => {
      expect(body.enabled).to.eq(true);
      expect(body).to.have.property('note');
      expect(body).to.have.property('cents');
      expect(body).to.have.property('frequency');
      expect(body).to.have.property('confidence');
    });

    cy.get('section[aria-label="Tuner"]').within(() => {
      cy.contains('button', 'Berhenti').click();
    });

    cy.wait(300);
    cy.request('/api/tuner').then(({ body }) => {
      expect(body.enabled).to.eq(false);
    });
  });

  it('refuses a tuner request it does not understand', () => {
    cy.request({
      method: 'POST',
      url: '/api/tuner/enable',
      body: { nonsense: true },
      failOnStatusCode: false,
    }).then((response) => {
      expect(response.status).to.eq(400);
    });
  });

  it('shares one tempo between the metronome and a synced delay', () => {
    // Two separately-edited BPMs would drift against each other, which is the
    // whole reason the tempo lives on "global" rather than on the metronome.
    cy.request('PUT', '/api/effects/global/bpm', { value: 100 });
    cy.reload();

    cy.get('[aria-label="Tempo dalam BPM"]').should('contain.text', '100');

    cy.request('PUT', '/api/effects/delay/syncMode', { value: 1 }); // 1/4
    cy.request('PUT', '/api/effects/global/bpm', { value: 120 });
    cy.wait(300);

    cy.request('/api/effects/global/bpm').then(({ body }) => {
      expect(Number(body.value)).to.eq(120);
    });

    cy.request('PUT', '/api/effects/delay/syncMode', { value: 0 });
  });

  it('disables the delay Time knob while it is locked to the tempo', () => {
    cy.request('POST', '/api/effects/delay/enabled', { enabled: true });
    cy.request('PUT', '/api/effects/delay/syncMode', { value: 0 });
    cy.reload();

    cy.get('section[aria-label="Delay"] [role="slider"][aria-label="Time"]')
      .should('have.attr', 'aria-disabled', 'false');

    cy.get('section[aria-label="Delay"]').within(() => {
      cy.contains('label', 'Sync').find('select').select('1/8');
    });

    // The delay is taking its time from the tempo now, so a Time knob still
    // showing a number it is not using would be a lie.
    cy.get('section[aria-label="Delay"] [role="slider"][aria-label="Time"]')
      .should('have.attr', 'aria-disabled', 'true');

    cy.wait(300);
    cy.request('/api/effects/delay/syncMode').then(({ body }) => {
      expect(Number(body.value)).to.eq(3);
    });

    cy.request('PUT', '/api/effects/delay/syncMode', { value: 0 });
  });

  it('starts and stops the metronome from the tempo panel', () => {
    cy.request('POST', '/api/effects/metronome/enabled', { enabled: false });
    cy.reload();

    cy.get('[aria-label="Metronom"]').should('have.attr', 'aria-pressed', 'false');
    cy.get('[aria-label="Metronom"]').click();
    cy.wait(300);

    cy.request('/api/effects/metronome').then(({ body }) => {
      expect(body.enabled).to.eq(true);
    });

    cy.get('[aria-label="Metronom"]').click();
    cy.wait(300);

    cy.request('/api/effects/metronome').then(({ body }) => {
      expect(body.enabled).to.eq(false);
    });
  });

  it('keeps the metronome out of the signal chain strip', () => {
    // It is mixed in after the master stage, so drawing it as a stage the
    // guitar passes through would misrepresent what it does.
    cy.get('nav[aria-label="Rantai sinyal"]').should('exist');
    cy.get('nav[aria-label="Rantai sinyal"]').should('not.contain.text', 'Metronome');
    cy.get('section[aria-label="Metronome"]').should('not.exist');
    cy.get('section[aria-label="Tempo"]').should('exist');
  });

  it('sets the tempo by tapping', () => {
    cy.request('PUT', '/api/effects/global/bpm', { value: 120 });
    cy.reload();

    cy.get('section[aria-label="Tempo"]').within(() => {
      // Four taps a second apart is 60 BPM.
      cy.contains('button', 'Tap').click();
      cy.wait(1000);
      cy.contains('button', 'Tap').click();
      cy.wait(1000);
      cy.contains('button', 'Tap').click();
    });

    cy.wait(300);
    cy.request('/api/effects/global/bpm').then(({ body }) => {
      const tapped = Number(body.value);

      // Bounded rather than compared to 60: cy.wait guarantees *at least* a
      // second between taps, and command overhead adds a hundred-odd ms on top,
      // so the real tempo is always somewhat under 60. The upper bound is the
      // hard one -- a tap tempo reading faster than the taps is measuring
      // something other than the gaps between them.
      expect(tapped).to.be.at.most(60);
      expect(tapped).to.be.at.least(40);
    });

    cy.request('PUT', '/api/effects/global/bpm', { value: 120 });
  });

  it('pushes meter updates over a held-open event stream', () => {
    cy.window().then(
      (win) =>
        new Cypress.Promise<void>((resolve, reject) => {
          const source = new win.EventSource('/api/levels/stream');
          const seen: unknown[] = [];

          const timer = win.setTimeout(() => {
            source.close();
            reject(new Error(`only ${seen.length} event(s) in 1.5 s`));
          }, 1500);

          source.onmessage = (event: MessageEvent) => {
            // Parsed, not just counted: pretty-printed JSON split across lines
            // once arrived as a payload of exactly "{", which counts as an
            // event but is useless.
            const parsed = JSON.parse(event.data);
            expect(parsed).to.have.property('inputLevel');
            expect(parsed).to.have.property('audioRunning');

            seen.push(parsed);

            // Several, so this proves a stream rather than one response.
            if (seen.length >= 5) {
              win.clearTimeout(timer);
              source.close();
              resolve();
            }
          };

          source.onerror = () => {
            win.clearTimeout(timer);
            source.close();
            reject(new Error('the level stream errored'));
          };
        }),
    );
  });

  it('draws a response curve for each tone stage', () => {
    // All three bands, not just the one under test: "flat" means the whole
    // stage is doing nothing, and the persisted settings carry whatever the
    // last session left behind.
    cy.request('POST', '/api/effects/eq/enabled', { enabled: true });
    cy.request('PUT', '/api/effects/eq/bassDb', { value: 0 });
    cy.request('PUT', '/api/effects/eq/midDb', { value: 0 });
    cy.request('PUT', '/api/effects/eq/trebleDb', { value: 0 });
    cy.reload();

    cy.get('section[aria-label="EQ"] svg[aria-label*="Kurva respons"]')
      .should('exist')
      .and('have.attr', 'aria-label')
      .and('match', /rata/);

    cy.get('section[aria-label="EQ"] path.tone-curve__line')
      .invoke('attr', 'd')
      .then((flat) => {
        cy.request('PUT', '/api/effects/eq/midDb', { value: 12 });
        cy.reload();

        cy.get('section[aria-label="EQ"] path.tone-curve__line')
          .invoke('attr', 'd')
          .should((boosted) => {
            expect(boosted).to.not.equal(flat);
          });
      });

    cy.get('section[aria-label="Contour"] path.tone-curve__line').should('exist');

    // Not every stage has a frequency response to draw.
    cy.get('section[aria-label="Overdrive"] path.tone-curve__line').should('not.exist');

    cy.request('PUT', '/api/effects/eq/midDb', { value: 0 });
  });

  it('switches the chain with a scene, without moving a knob', () => {
    // The design decision scenes rest on: a scene change mid-song must not jump
    // a parameter to a value you cannot see on a control you were not touching.
    cy.request('POST', '/api/effects/overdrive/enabled', { enabled: true });
    cy.request('PUT', '/api/effects/overdrive/drivePct', { value: 40 });
    cy.request('POST', '/api/scenes/1/capture', {});

    cy.request('POST', '/api/effects/overdrive/enabled', { enabled: false });
    cy.request('PUT', '/api/effects/overdrive/drivePct', { value: 80 });
    cy.request('POST', '/api/scenes/0/capture', {});

    cy.request('POST', '/api/scenes/1/recall', {}).then(({ body }) => {
      expect(body.active).to.eq(1);
    });

    cy.request('/api/effects/overdrive').then(({ body }) => {
      expect(body.enabled, 'the scene restored the switch').to.eq(true);

      const drive = body.parameters.find((p: { id: string }) => p.id === 'drivePct');
      expect(Number(drive.value), 'the scene moved a knob').to.be.closeTo(80, 0.01);
    });
  });

  it('shows the scene grid and recalls from it', () => {
    cy.request('POST', '/api/scenes/0/capture', {});
    cy.reload();

    cy.get('section[aria-label="Scene"]').should('exist');
    cy.get('section[aria-label="Scene"]').scrollIntoView();
    cy.get('section[aria-label="Scene"]').within(() => {
      cy.contains('[role="rowheader"]', 'Crunch').click();
    });

    cy.wait(300);
    cy.request('/api/scenes').then(({ body }) => {
      expect(body.active).to.eq(1);
    });
  });

  it('refuses a scene slot that does not exist', () => {
    cy.request({ method: 'POST', url: '/api/scenes/9/recall', body: {}, failOnStatusCode: false })
      .then((response) => expect(response.status).to.eq(404));

    cy.request({
      method: 'PUT',
      url: '/api/scenes/0/effects/master',
      body: { enabled: false },
      failOnStatusCode: false,
    }).then((response) => {
      // Master is always in the path; a scene must not be able to silence it.
      expect(response.status).to.eq(404);
    });
  });

  it('carries metadata and scenes through a preset round trip', () => {
    const name = 'Cypress Metadata';

    cy.request('POST', '/api/presets/save', { name });
    cy.request('POST', '/api/presets/metadata', {
      name,
      description: 'Dibuat oleh Cypress',
      tags: ['test', 'e2e'],
      favourite: true,
      notes: 'catatan',
    }).then(({ body }) => {
      expect(body.tags).to.deep.equal(['test', 'e2e']);
      expect(body.favourite).to.eq(true);
    });

    // Re-saving the sound must not discard the notes.
    cy.request('POST', '/api/presets/save', { name });

    cy.request('/api/presets').then(({ body }) => {
      const entry = body.details.find((d: { name: string }) => d.name === name);
      expect(entry.notes).to.eq('catatan');
      expect(entry.description).to.eq('Dibuat oleh Cypress');
    });

    cy.request('POST', '/api/presets/export', { name }).then(({ body }) => {
      expect(body.filename).to.eq(`${name}.milodikfx.json`);
      expect(body.data).to.contain('Dibuat oleh Cypress');

      cy.request('POST', '/api/presets/import', { name: 'Cypress Imported', data: body.data })
        .then((response) => {
          expect(response.body.presets).to.include('Cypress Imported');
        });
    });

    cy.request('POST', '/api/presets/delete', { name });
    cy.request('POST', '/api/presets/delete', { name: 'Cypress Imported' });
  });

  it('refuses to import a file that is not a preset', () => {
    cy.request({
      method: 'POST',
      url: '/api/presets/import',
      body: { name: 'Rubbish', data: '{"hello":"world"}' },
      failOnStatusCode: false,
    }).then((response) => {
      expect(response.status).to.eq(400);
    });

    cy.request('/api/presets').then(({ body }) => {
      expect(body.presets).to.not.include('Rubbish');
    });
  });

  it('undoes and redoes a parameter change', () => {
    cy.request('POST', '/api/effects/overdrive/enabled', { enabled: true });
    cy.request('PUT', '/api/effects/overdrive/drivePct', { value: 20 });

    // The engine commits a step only once the chain has been still, so a burst
    // of knob writes is one undo rather than one per frame. Wait it out.
    cy.wait(2000);

    cy.request('PUT', '/api/effects/overdrive/drivePct', { value: 70 });
    cy.wait(2000);

    cy.request('/api/history').then(({ body }) => {
      expect(body.canUndo).to.eq(true);
    });

    cy.request('POST', '/api/history/undo', {}).then(({ body }) => {
      expect(body.canRedo).to.eq(true);
    });

    cy.request('/api/effects/overdrive/drivePct').then(({ body }) => {
      expect(Number(body.value)).to.be.closeTo(20, 0.5);
    });

    cy.request('POST', '/api/history/redo', {});

    cy.request('/api/effects/overdrive/drivePct').then(({ body }) => {
      expect(Number(body.value)).to.be.closeTo(70, 0.5);
    });
  });

  it('offers undo in the top bar and answers 409 when there is nothing to undo', () => {
    cy.contains('button', '↶').should('exist');
    cy.contains('button', '↷').should('exist');

    // Each undo has to consume a step. Measured rather than drained to
    // exhaustion: a loop that keeps going until it gets a 409 hangs the whole
    // run if the stack ever stops emptying, and a page is open here writing to
    // the same engine, so the depth is not ours alone to predict.
    cy.request('/api/history').then(({ body }) => {
      const before = body.undoDepth;

      if (before === 0) {
        cy.request({ method: 'POST', url: '/api/history/undo', body: {}, failOnStatusCode: false })
          .then((response) => expect(response.status).to.eq(409));
        return;
      }

      cy.request('POST', '/api/history/undo', {}).then((response) => {
        expect(response.body.undoDepth).to.eq(before - 1);
        expect(response.body.redoDepth).to.be.greaterThan(0);
      });
    });
  });

  it('offers an input trim ahead of the whole chain', () => {
    cy.request('/api/effects/input').then(({ body }) => {
      const gain = body.parameters.find((p: { id: string }) => p.id === 'gainDb');

      expect(gain, 'input exposes a gain').to.not.be.undefined;
      expect(gain.min).to.eq(-24);
      expect(gain.max).to.eq(24);
      // Always in the path: there is nothing to bypass on an input trim.
      expect(body.toggleable).to.eq(false);
    });

    cy.request('PUT', '/api/effects/input/gainDb', { value: -6 });
    cy.reload();

    cy.get('section[aria-label="Input"] [role="slider"][aria-label="Gain"]')
      .should('have.attr', 'aria-valuenow', '-6');

    cy.request('PUT', '/api/effects/input/gainDb', { value: 0 });
  });

  it('reports what the chain receives, not just what the interface sent', () => {
    // Without this the Input knob has no feedback at all: the meter is measured
    // before the chain runs, so a chain-stage trim would be invisible on it.
    cy.request('PUT', '/api/effects/input/gainDb', { value: 0 });
    cy.wait(300);

    cy.request('/api/levels').then(({ body }) => {
      expect(body).to.have.property('chainInputLevel');
      expect(body.chainInputLevel).to.be.closeTo(body.inputLevel, 0.01);

      cy.request('PUT', '/api/effects/input/gainDb', { value: 12 });
      cy.wait(300);

      cy.request('/api/levels').then((next) => {
        // Trim is a pure gain, so the trimmed figure is the raw one plus 12 dB
        // -- unless the raw signal is at the meter floor, where it stays.
        const expected = Math.max(next.body.floorDb, next.body.inputLevel + 12);
        expect(next.body.chainInputLevel).to.be.closeTo(expected, 0.01);
      });
    });

    cy.request('PUT', '/api/effects/input/gainDb', { value: 0 });
  });

  it('lets the delay and reverb tails ring on by default', () => {
    // Spillover is what makes a scene change mid-song sound smooth instead of
    // chopping the repeats dead.
    for (const effect of ['delay', 'reverb']) {
      cy.request(`/api/effects/${effect}`).then(({ body }) => {
        const spill = body.parameters.find((p: { id: string }) => p.id === 'spillover');

        expect(spill, `${effect} exposes spillover`).to.not.be.undefined;
        expect(spill.type).to.eq('bool');
        expect(spill.default).to.eq(1);
      });
    }

    cy.request('PUT', '/api/effects/delay/spillover', { value: 0 });
    cy.request('/api/effects/delay/spillover').then(({ body }) => {
      expect(Number(body.value)).to.eq(0);
    });

    cy.request('PUT', '/api/effects/delay/spillover', { value: 1 });
  });

  it('changes the overdrive controls with the voicing', () => {
    cy.request('POST', '/api/effects/overdrive/enabled', { enabled: true });
    cy.request('PUT', '/api/effects/overdrive/type', { value: 0 });
    cy.reload();

    // Custom keeps the pre-voicing controls.
    cy.get('section[aria-label="Overdrive"]').within(() => {
      cy.get('[role="slider"][aria-label="Asymmetry"]').should('exist');
      cy.get('[role="slider"][aria-label="Tone"]').should('not.exist');

      cy.contains('label', 'Tipe').find('select').select('Tube Screamer');
    });

    // A Tube Screamer has a Tone knob and no Asymmetry.
    cy.get('section[aria-label="Overdrive"] [role="slider"][aria-label="Tone"]').should('exist');
    cy.get('section[aria-label="Overdrive"] [role="slider"][aria-label="Asymmetry"]')
      .should('not.exist');

    cy.wait(300);
    cy.request('/api/effects/overdrive/type').then(({ body }) => {
      expect(Number(body.value)).to.eq(1);
    });

    // A Bluesbreaker names its gain control Gain, not Drive.
    cy.get('section[aria-label="Overdrive"]').within(() => {
      cy.contains('label', 'Tipe').find('select').select('Bluesbreaker');
    });

    cy.get('section[aria-label="Overdrive"] [role="slider"][aria-label="Gain"]').should('exist');
    cy.get('section[aria-label="Overdrive"] [role="slider"][aria-label="Volume"]').should('exist');

    cy.request('PUT', '/api/effects/overdrive/type', { value: 0 });
  });

  it('registers every voicing control once, whichever type is selected', () => {
    // The registry is a flat, stable set: hiding a control is a presentation
    // decision, so a preset saved under one voicing still round-trips the rest.
    cy.request('/api/effects/overdrive').then(({ body }) => {
      const ids = body.parameters.map((p: { id: string }) => p.id);

      for (const id of ['type', 'drivePct', 'levelPct', 'tonePct', 'voicePct',
                        'bassDb', 'midDb', 'trebleDb', 'hpMode', 'bright',
                        'asymmetry', 'oversampling']) {
        expect(ids, `overdrive exposes ${id}`).to.include(id);
      }

      const type = body.parameters.find((p: { id: string }) => p.id === 'type');
      expect(type.min).to.eq(0);
      expect(type.max).to.eq(11);
    });
  });

  it('reports its own version and an update verdict', () => {
    // The shape is what matters: current is the build's version and the verdict
    // is a boolean. The value depends on what is published on GitHub and whether
    // the check reached it, so the test does not pin it -- the engine still
    // answers (with current filled in) even when GitHub is unreachable.
    cy.request('/api/update').then(({ body }) => {
      expect(body.current, 'reports a current version').to.match(/^\d+\.\d+\.\d+$/);
      expect(body.updateAvailable, 'gives a boolean verdict').to.be.a('boolean');
    });
  });

  it('offers two cabinet impulse responses and a blend between them', () => {
    cy.request('/api/effects/cabinet').then(({ body }) => {
      const ids = body.parameters.map((p: { id: string }) => p.id);

      expect(ids).to.include('irFile');
      expect(ids).to.include('irFileB');
      expect(ids).to.include('irBlend');

      const blend = body.parameters.find((p: { id: string }) => p.id === 'irBlend');
      expect(blend.min).to.eq(0);
      expect(blend.max).to.eq(1);
      // Defaults to the first IR alone, so adding a second slot changes nothing
      // until it is deliberately blended in.
      expect(blend.default).to.eq(0);

      const second = body.parameters.find((p: { id: string }) => p.id === 'irFileB');
      expect(second.type).to.eq('text');
    });

    cy.request('PUT', '/api/effects/cabinet/irBlend', { value: 0.5 });
    cy.request('/api/effects/cabinet/irBlend').then(({ body }) => {
      expect(Number(body.value)).to.be.closeTo(0.5, 0.01);
    });

    cy.request('PUT', '/api/effects/cabinet/irBlend', { value: 0 });
  });

  it('puts the amp head between the tone shaping and the cabinet', () => {
    cy.request('/api/effects').then(({ body }) => {
      const ids = body.effects.map((e: { id: string }) => e.id);
      const nam = ids.indexOf('nam');
      const contour = ids.indexOf('toneStack');
      const cabinet = ids.indexOf('cabinet');

      expect(nam, 'nam is registered').to.be.greaterThan(-1);
      expect(nam).to.be.greaterThan(contour);
      expect(nam).to.be.lessThan(cabinet);
    });

    cy.request('/api/effects/nam').then(({ body }) => {
      const ids = body.parameters.map((p: { id: string }) => p.id);
      expect(ids).to.include('inputDb');
      expect(ids).to.include('outputDb');
      expect(ids).to.include('namFile');
      expect(body.toggleable).to.eq(true);
    });

    // The card is drawn in the rack like any other stage.
    cy.get('section[aria-label="Amp (NAM)"]').should('exist');
  });

  it('reports the NAM model library and whether models can run', () => {
    cy.request('/api/nam').then(({ body }) => {
      expect(body.models).to.be.an('array');
      expect(body).to.have.property('directory');
      expect(body).to.have.property('available');
      // available and the reason must agree.
      if (body.available) {
        expect(body.unavailableReason).to.eq('');
      } else {
        expect(body.unavailableReason).to.not.eq('');
      }
    });

    cy.get('section[aria-label="Model NAM"]').should('exist');
  });

  it('refuses a NAM import that is not a real request', () => {
    cy.request({
      method: 'POST',
      url: '/api/nam/import',
      body: { name: '' },
      failOnStatusCode: false,
    }).then((response) => {
      expect(response.status).to.eq(400);
    });
  });

  it('round-trips the amp head input and output gains', () => {
    cy.request('PUT', '/api/effects/nam/inputDb', { value: 4.5 });
    cy.reload();

    cy.get('section[aria-label="Amp (NAM)"] [role="slider"][aria-label="Input"]')
      .should('have.attr', 'aria-valuenow', '4.5');

    cy.request('/api/effects/nam/inputDb').then(({ body }) => {
      expect(Number(body.value)).to.be.closeTo(4.5, 0.01);
    });

    cy.request('PUT', '/api/effects/nam/inputDb', { value: 0 });
  });

  it('reports MIDI state even with no controller attached', () => {
    // A build machine has no MIDI hardware, so the contract that has to hold is
    // that the endpoint answers with an empty device list rather than failing.
    cy.request('/api/midi').then(({ body }) => {
      expect(body.devices).to.be.an('array');
      expect(body.mappings).to.be.an('array');
      expect(body).to.have.property('open');
      expect(body).to.have.property('lastCc');
    });

    cy.get('section[aria-label="MIDI"]').should('exist');
    cy.get('section[aria-label="MIDI"]').find('select[aria-label="Perangkat"]').should('exist');
  });

  it('stores and removes a controller mapping', () => {
    cy.request('PUT', '/api/midi/mappings/7', {
      effect: 'overdrive',
      parameter: 'drivePct',
      mode: 'continuous',
    }).then(({ body }) => {
      const mapping = body.mappings.find((m: { cc: number }) => m.cc === 7);
      expect(mapping).to.deep.include({
        cc: 7,
        effect: 'overdrive',
        parameter: 'drivePct',
        mode: 'continuous',
      });
    });

    cy.reload();
    // The panel sits low in a scrolling sidebar, so scroll to it rather than
    // asserting visibility on something below the fold.
    cy.get('section[aria-label="MIDI"]').scrollIntoView();
    cy.get('section[aria-label="MIDI"]').within(() => {
      cy.contains('.midi__item', 'CC 7').should('contain.text', 'Overdrive — Drive');
      cy.contains('button', 'Hapus').click();
    });

    cy.wait(300);
    cy.request('/api/midi').then(({ body }) => {
      expect(body.mappings.find((m: { cc: number }) => m.cc === 7)).to.be.undefined;
    });
  });

  it('refuses a controller number that does not exist', () => {
    // 128 wrapping to 0 would silently rebind an unrelated controller.
    cy.request({
      method: 'PUT',
      url: '/api/midi/mappings/128',
      body: { effect: 'overdrive', parameter: 'drivePct' },
      failOnStatusCode: false,
    }).then((response) => {
      expect(response.status).to.eq(404);
    });

    cy.request({
      method: 'PUT',
      url: '/api/midi/mappings/3',
      body: { effect: 'overdrive' },
      failOnStatusCode: false,
    }).then((response) => {
      expect(response.status).to.eq(400);
    });
  });

  it('arms and cancels MIDI learn', () => {
    cy.request('POST', '/api/midi/learn', {
      effect: 'delay',
      parameter: 'mixPct',
      mode: 'continuous',
    }).then(({ body }) => {
      expect(body.learning).to.not.be.null;
      expect(body.learning.parameter).to.eq('mixPct');
    });

    cy.request('POST', '/api/midi/learn', {}).then(({ body }) => {
      expect(body.learning).to.be.null;
    });
  });

  it('offers a way back to low latency after the device has been changed', () => {
    cy.contains('button', 'Optimalkan latensi').should('be.visible');

    cy.request('/api/devices').then(({ body }) => {
      const before = body.current.roundTripLatencyMs;

      // Re-opening the hardware several times is genuinely slow.
      cy.request({
        method: 'POST',
        url: '/api/devices/optimise',
        body: {},
        timeout: 120000,
      }).then((response) => {
        // Never worse than what we started from, and always a real device.
        expect(response.body.current.roundTripLatencyMs).to.be.at.most(before + 0.01);
        expect(response.body.current.bufferSize).to.be.greaterThan(0);
        expect(response.body.current.inputChannels).to.be.greaterThan(0);
      });
    });
  });
});
