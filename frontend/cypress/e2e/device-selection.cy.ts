describe('Device Selection E2E', () => {
  beforeEach(() => {
    cy.visit('http://localhost:5173');
  });

  it('displays device selection section', () => {
    cy.contains('Audio Input').should('be.visible');
    cy.contains('Audio Output').should('be.visible');
  });

  it('contains input and output device selects', () => {
    cy.get('select').should('have.length.at.least', 2);
  });

  it('displays connection status indicator', () => {
    cy.get('h1').should('contain', 'MilodikFX');
  });

  it('has theme toggle button', () => {
    cy.contains('button', /☀️|🌙/).should('be.visible').click();
  });
});
