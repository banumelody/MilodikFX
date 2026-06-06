describe('Preset Management E2E', () => {
  beforeEach(() => {
    cy.visit('http://localhost:5173');
  });

  it('displays settings section with input', () => {
    cy.contains('Settings').should('be.visible');
    cy.get('input[value="MilodikFX"]').should('be.visible');
  });

  it('contains save button for presets', () => {
    cy.contains('button', 'Save Settings').should('be.visible');
  });

  it('displays the footer with copyright', () => {
    cy.contains('MilodikFX © 2024').should('be.visible');
  });

  it('maintains layout responsiveness', () => {
    cy.get('main').should('be.visible');
    cy.get('.grid').should('have.length.at.least', 1);
  });
});
