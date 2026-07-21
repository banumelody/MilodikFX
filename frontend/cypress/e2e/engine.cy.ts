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
      const labels: string[] = body.effects.map((effect: { label: string }) => effect.label);

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
