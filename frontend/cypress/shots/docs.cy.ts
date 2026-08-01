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
  it('captures two overdrives on one board', () => {
    // The headline of v0.31: a screamer into a fuzz, dialled differently,
    // which is the one thing a one-per-type board could never do.
    cy.request('PUT', '/api/chain/board', {
      placed: ['noiseGate', 'overdrive', 'overdrive2', 'nam', 'cabinet', 'reverb'],
    });
    cy.request('PUT', '/api/parameters/overdrive/type', { value: 1 });   // Tube Screamer
    cy.request('PUT', '/api/parameters/overdrive/drivePct', { value: 35 });
    cy.request('PUT', '/api/parameters/overdrive2/type', { value: 11 }); // Big Muff
    cy.request('PUT', '/api/parameters/overdrive2/drivePct', { value: 82 });

    cy.visit('/');
    // exist, not be.visible: the card may sit below the fold in a 1000px
    // viewport, and the shot is of the top of the rack either way.
    cy.contains('h2', 'Overdrive 2').should('exist');
    cy.wait(900);

    cy.screenshot('two-overdrives', { capture: 'viewport', overwrite: true });
  });
});

// Deliberately outside cypress/e2e: this spec *mutates* the chain (it enables
// the split and picks a mode) and the E2E suite asserts against chain state, so
// leaving it where specPattern finds it would contaminate every run. Invoke it
// explicitly instead:
//
//   npx cypress run --config specPattern=cypress/shots/*.cy.ts
