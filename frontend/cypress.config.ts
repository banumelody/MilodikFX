import { defineConfig } from 'cypress'

export default defineConfig({
  e2e: {
    // Points at the running engine, not at `vite preview`. Driving the UI with
    // no backend behind it validated nothing: every control is bound to a real
    // DSP parameter, so the engine has to be up for the suite to mean anything.
    baseUrl: process.env.MILODIKFX_URL ?? 'http://127.0.0.1:3000',
    specPattern: 'cypress/e2e/**/*.cy.ts',
    supportFile: 'cypress/support/e2e.ts',
    video: false,
    screenshotOnRunFailure: false,
  },
})
