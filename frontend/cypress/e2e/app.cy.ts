describe('MilodikFX App E2E', () => {
  it('loads the homepage and shows app root', () => {
    cy.visit('/');
    // Check for a stable element that should render in the simplified perform view
    cy.get('[data-testid="app-root"]')
      .should('exist')
      .and('be.visible');
  });
});
