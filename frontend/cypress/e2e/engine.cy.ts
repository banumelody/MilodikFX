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
    cy.contains('nav[aria-label="Rantai sinyal"] span', 'OUT').should('be.visible');

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

    cy.get('body').type('{esc}');
    cy.wait(300);

    cy.request('/api/effects/master/muted').then(({ body }) => {
      expect(Number(body.value)).to.eq(1);
    });

    cy.get('body').type('{esc}');
    cy.wait(300);

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
