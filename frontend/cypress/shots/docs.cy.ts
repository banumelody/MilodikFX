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
  it('captures the input panel and the L/R split', () => {
    // Wide enough that the responsive layout keeps the sidebar beside the rack,
    // which is how the app actually looks in its own window.
    cy.viewport(1600, 1000);

    // Put the chain into the state the shot is meant to show, through the API
    // rather than by clicking: fewer moving parts, and it cannot half-apply.
    cy.request('POST', '/api/effects/split/enabled', { enabled: true });
    cy.request('PUT', '/api/parameters/split/mode', { value: 2 });
    cy.request('POST', '/api/devices', {
      inputPortLeft: 'Input 1',
      inputPortRight: 'Input 2',
    });

    cy.visit('/');
    cy.contains('.panel__title', 'Audio Device').should('be.visible');

    // The port selectors live behind the panel's own toggle, so the shot has to
    // open it the way a person would.
    cy.contains('.panel', 'Audio Device').within(() => {
      cy.contains('button', 'Ubah').click();
    });

    cy.contains('span', 'Kanal L dari').should('be.visible');
    cy.contains('span', 'Kanal R dari').should('be.visible');

    // Let the meter stream settle so the capture is not mid-repaint.
    cy.wait(1200);

    // Two figures rather than one. Headless Electron pins its window to
    // 1280x720 whatever cy.viewport says, and at that width the sidebar sits
    // off the right edge -- so a single shot would either clip the ports or
    // shrink the rack past legibility.
    cy.contains('.rack', 'Split').screenshot('split-lr', { overwrite: true });

    // The sidebar starts past the right edge at this width, so scroll the page
    // sideways until it is inside the capture area.
    cy.window().then((win) => win.scrollTo(win.document.body.scrollWidth, 0));
    cy.wait(600);
    cy.screenshot('input-ports', { capture: 'viewport', overwrite: true });
  });
});

// Deliberately outside cypress/e2e: this spec *mutates* the chain (it enables
// the split and picks a mode) and the E2E suite asserts against chain state, so
// leaving it where specPattern finds it would contaminate every run. Invoke it
// explicitly instead:
//
//   npx cypress run --config specPattern=cypress/shots/*.cy.ts
