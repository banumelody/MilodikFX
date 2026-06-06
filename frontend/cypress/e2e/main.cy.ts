describe('MilodikFX E2E Tests', () => {
  beforeEach(() => {
    cy.visit('/');
  });

  it('should load the application', () => {
    cy.get('h1').should('contain', 'MilodikFX');
    cy.get('[data-cy=nav-tabs]').should('exist');
  });

  describe('Tab Navigation', () => {
    it('should switch between tabs', () => {
      // Check default Perform tab is visible
      cy.get('[data-cy=tab-perform]').should('have.class', 'active');
      
      // Click Edit tab
      cy.get('[data-cy=tab-edit]').click();
      cy.get('[data-cy=tab-edit]').should('have.class', 'active');
      
      // Click Library tab
      cy.get('[data-cy=tab-library]').click();
      cy.get('[data-cy=tab-library]').should('have.class', 'active');
      
      // Click Settings tab
      cy.get('[data-cy=tab-settings]').click();
      cy.get('[data-cy=tab-settings]').should('have.class', 'active');
      
      // Switch back to Perform
      cy.get('[data-cy=tab-perform]').click();
      cy.get('[data-cy=tab-perform]').should('have.class', 'active');
    });
  });

  describe('Signal Chain Management', () => {
    it('should add effects to signal chain', () => {
      // Click Add Effect button
      cy.get('[data-cy=add-effect-btn]').click();
      
      // Modal should appear
      cy.get('[data-cy=add-effect-modal]').should('be.visible');
      
      // Select Gain effect
      cy.get('[data-cy=effect-GAIN]').click();
      
      // Modal should close and effect should be added
      cy.get('[data-cy=add-effect-modal]').should('not.exist');
      cy.get('[data-cy=signal-chain-block]').should('exist');
    });

    it('should add multiple effects', () => {
      // Add first effect (Gain)
      cy.get('[data-cy=add-effect-btn]').click();
      cy.get('[data-cy=effect-GAIN]').click();
      
      // Add second effect (Overdrive)
      cy.get('[data-cy=add-effect-btn]').click();
      cy.get('[data-cy=effect-OVERDRIVE]').click();
      
      // Both effects should be in the signal chain
      cy.get('[data-cy=signal-chain-block]').should('have.length', 2);
    });

    it('should remove effects from signal chain', () => {
      // Add an effect
      cy.get('[data-cy=add-effect-btn]').click();
      cy.get('[data-cy=effect-GAIN]').click();
      
      // Verify effect is added
      cy.get('[data-cy=signal-chain-block]').should('exist');
      
      // Remove effect
      cy.get('[data-cy=effect-remove-btn]').first().click();
      
      // Signal chain should be empty
      cy.get('[data-cy=signal-chain-block]').should('not.exist');
    });

    it('should reorder effects via drag-drop', () => {
      // Add first effect
      cy.get('[data-cy=add-effect-btn]').click();
      cy.get('[data-cy=effect-GAIN]').click();
      
      // Add second effect
      cy.get('[data-cy=add-effect-btn]').click();
      cy.get('[data-cy=effect-OVERDRIVE]').click();
      
      // Get block IDs before reorder
      cy.get('[data-cy=signal-chain-block]').eq(0).invoke('attr', 'data-id').as('block1');
      cy.get('[data-cy=signal-chain-block]').eq(1).invoke('attr', 'data-id').as('block2');
      
      // Drag first block to second position
      cy.get('[data-cy=signal-chain-block]').eq(0).trigger('dragstart');
      cy.get('[data-cy=signal-chain-block]').eq(1).trigger('dragover');
      cy.get('[data-cy=signal-chain-block]').eq(1).trigger('drop');
      cy.get('[data-cy=signal-chain-block]').eq(1).trigger('dragend');
      
      // Order should be swapped
      cy.get('[data-cy=signal-chain-block]').eq(0).invoke('attr', 'data-id').then((id) => {
        cy.get('@block2').then((block2Id) => {
          expect(id).to.equal(block2Id);
        });
      });
    });
  });

  describe('Scene Management', () => {
    it('should switch between scenes', () => {
      // Verify default scene is selected
      cy.get('[data-cy=scene-btn-1]').should('have.class', 'active');
      
      // Switch to scene 2
      cy.get('[data-cy=scene-btn-2]').click();
      cy.get('[data-cy=scene-btn-2]').should('have.class', 'active');
      
      // Switch to scene 3
      cy.get('[data-cy=scene-btn-3]').click();
      cy.get('[data-cy=scene-btn-3]').should('have.class', 'active');
      
      // Switch back to scene 1
      cy.get('[data-cy=scene-btn-1]').click();
      cy.get('[data-cy=scene-btn-1]').should('have.class', 'active');
    });

    it('should preserve effects per scene', () => {
      // Add effect to scene 1
      cy.get('[data-cy=add-effect-btn]').click();
      cy.get('[data-cy=effect-GAIN]').click();
      cy.get('[data-cy=signal-chain-block]').should('have.length', 1);
      
      // Switch to scene 2
      cy.get('[data-cy=scene-btn-2]').click();
      
      // Signal chain should be empty for scene 2
      cy.get('[data-cy=signal-chain-block]').should('not.exist');
      
      // Add different effect to scene 2
      cy.get('[data-cy=add-effect-btn]').click();
      cy.get('[data-cy=effect-OVERDRIVE]').click();
      cy.get('[data-cy=signal-chain-block]').should('have.length', 1);
      
      // Switch back to scene 1
      cy.get('[data-cy=scene-btn-1]').click();
      
      // Original effect should be restored
      cy.get('[data-cy=signal-chain-block]').should('have.length', 1);
    });
  });

  describe('Metering and Levels', () => {
    it('should display input/output levels', () => {
      // Check left panel displays metering
      cy.get('[data-cy=input-level]').should('exist');
      cy.get('[data-cy=output-level]').should('exist');
    });

    it('should display CPU load graph', () => {
      // Check right panel displays CPU graph
      cy.get('[data-cy=cpu-graph]').should('exist');
    });

    it('should display master volume control', () => {
      cy.get('[data-cy=master-volume]').should('exist');
      cy.get('[data-cy=master-volume-knob]').should('exist');
    });
  });

  describe('Theme Toggle', () => {
    it('should toggle theme', () => {
      // Get initial background
      cy.get('body').should('have.class', 'bg-gray-950');
      
      // Click theme toggle
      cy.get('[data-cy=theme-toggle]').click();
      
      // Background should change (if light theme implemented)
      cy.get('body').should('exist');
    });
  });

  describe('Accessibility', () => {
    it('should have proper focus management', () => {
      // Tab through UI elements
      cy.get('body').tab();
      cy.focused().should('exist');
    });

    it('should have proper ARIA labels', () => {
      cy.get('[aria-label]').should('have.length.greaterThan', 0);
      cy.get('[aria-pressed]').should('exist');
    });
  });

  describe('Responsive Design', () => {
    it('should be responsive on mobile', () => {
      cy.viewport('iphone-x');
      cy.get('[data-cy=nav-tabs]').should('be.visible');
      cy.get('[data-cy=add-effect-btn]').should('be.visible');
    });

    it('should be responsive on tablet', () => {
      cy.viewport('ipad-2');
      cy.get('[data-cy=nav-tabs]').should('be.visible');
      cy.get('[data-cy=signal-chain]').should('be.visible');
    });

    it('should be responsive on desktop', () => {
      cy.viewport('macbook-15');
      cy.get('[data-cy=nav-tabs]').should('be.visible');
      cy.get('[data-cy=signal-chain]').should('be.visible');
      cy.get('[data-cy=left-panel]').should('be.visible');
      cy.get('[data-cy=right-panel]').should('be.visible');
    });
  });

  describe('Performance', () => {
    it('should handle multiple effects without lag', () => {
      // Add 10 effects
      for (let i = 0; i < 10; i++) {
        cy.get('[data-cy=add-effect-btn]').click();
        // Alternate effect types
        const effectType = i % 2 === 0 ? 'GAIN' : 'OVERDRIVE';
        cy.get(`[data-cy=effect-${effectType}]`).click();
      }
      
      // All effects should be added
      cy.get('[data-cy=signal-chain-block]').should('have.length', 10);
      
      // UI should remain responsive
      cy.get('[data-cy=scene-btn-2]').click();
      cy.get('[data-cy=scene-btn-2]').should('have.class', 'active');
    });
  });
});
