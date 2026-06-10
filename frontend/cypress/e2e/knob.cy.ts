describe('RotaryKnob E2E', () => {
  beforeEach(() => {
    cy.visit('/');
    cy.get('[data-cy=master-volume-knob]').as('knob');
    cy.get('[data-cy=master-volume-display]').as('display');
  });

  it('should focus knob and change value with keyboard', () => {
    cy.get('@display').invoke('text').then((initial) => {
      cy.get('@knob').find('div[role="slider"]').focus().type('{uparrow}');
      cy.get('@display').should('not.eq', initial);
    });
  });
});
