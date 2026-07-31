/**
 * Not a test: a screenshot driver for the docs.
 *
 * Run against a live engine when a release needs a fresh image, then delete
 * nothing -- it is cheap to keep and the alternative is hand-composing a
 * picture of a UI, which drifts from the real one the moment anything moves.
 *
 *   npx cypress run --spec cypress/e2e/shot.cy.ts
 */
describe('docs screenshots', () => {
  it('captures the empty board and the two-line router', () => {
    // An almost-empty board first: the palette on the right, a straight wire
    // where the rack would be. This is the state a new install now starts in
    // conceptually, even though an update keeps everyone's rig intact.
    cy.request('PUT', '/api/chain/board', { placed: [] });
    cy.visit('/');
    // Not "kabel lurus": the master stage is pinned, so even an empty board
    // still shows it in the strip. The rack is where emptiness is stated.
    cy.contains('Board kosong').should('be.visible');
    cy.wait(800);
    cy.screenshot('empty-board', { capture: 'viewport', overwrite: true });

    // Then a parallel section, so the router's two bus lines are on screen.
    cy.request('PUT', '/api/chain/board', {
      placed: ['noiseGate', 'split', 'overdrive', 'cabinet', 'reverb', 'mixer'],
    });
    cy.request('PUT', '/api/chain/buses', { busB: ['reverb'] });
    cy.request('POST', '/api/effects/split/enabled', { enabled: true });
    cy.request('PUT', '/api/parameters/split/mode', { value: 0 });

    cy.visit('/');
    cy.get('.chain__fork').should('be.visible');
    cy.wait(800);
    cy.screenshot('router', { capture: 'viewport', overwrite: true });
  });
});

// Deliberately outside cypress/e2e: this spec *mutates* the chain (it enables
// the split and picks a mode) and the E2E suite asserts against chain state, so
// leaving it where specPattern finds it would contaminate every run. Invoke it
// explicitly instead:
//
//   npx cypress run --config specPattern=cypress/shots/*.cy.ts
