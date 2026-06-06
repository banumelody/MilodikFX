describe('Parameter Control E2E', () => {
  beforeEach(() => {
    cy.visit('http://localhost:5173');
  });

  it('displays knob control for gain', () => {
    cy.contains('Gain').should('be.visible');
  });

  it('displays input and output meters', () => {
    cy.contains('Input Level').should('be.visible');
    cy.contains('Output Level').should('be.visible');
  });

  it('displays meter values', () => {
    cy.contains(/\d+%/).should('be.visible');
  });

  it('shows settings card', () => {
    cy.contains('Settings').should('be.visible');
  });

  it('contains save and reset buttons', () => {
    cy.contains('button', 'Save Settings').should('be.visible');
    cy.contains('button', 'Reset').should('be.visible');
  });
});
