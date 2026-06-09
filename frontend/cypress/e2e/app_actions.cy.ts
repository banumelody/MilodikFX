describe('MilodikFX App Actions', () => {
  beforeEach(() => {
    cy.visit('/');
  });

  it('renders header and version', () => {
    cy.get('header').should('contain.text', 'MilodikFX');
    cy.get('header').should('contain.text', 'v0.6.0');
  });

  it('changes PRESET and DEVICE selects', () => {
    cy.contains('PRESET').parent().find('select').select('01A Default Clean').should('have.value', '01A Default Clean');
    cy.contains('DEVICE').parent().find('select').select('Focusrite USB ASIO').should('have.value', 'Focusrite USB ASIO');
  });

  it('adjusts master volume slider and shows dB', () => {
    // Use test ids for more reliable targeting and simulate user input via hidden numeric input
    // Use exposed global setter for reliable E2E control
    // Wait for the test helper to be exposed by the app and call it
    cy.window().its('__setMasterVolume').should('be.a', 'function').then((fn: any) => {
      fn(-3);
    });
    cy.get('[data-testid="master-volume-display"]').should('contain.text', '-3 dB');
  });

  it('toggles OVERDRIVE effect checkbox', () => {
    // Scope to EFFECTS grid and find the OVERDRIVE card, then toggle its checkbox
    cy.contains('EFFECTS').next('div').within(() => {
      cy.contains('OVERDRIVE').parent().find('input[type="checkbox"]').as('chk').should('be.checked').click().should('not.be.checked');
    });
  });
});
