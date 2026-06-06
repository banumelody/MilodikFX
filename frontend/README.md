# MilodikFX Frontend

Modern React frontend for MilodikFX DSP audio plugin.

## Development

```bash
npm run dev
```

Runs on http://localhost:5173

## Building

```bash
npm run build
```

Produces optimized bundle in `dist/`

## Testing

```bash
npm run test              # Run unit tests
npm run test:watch       # Watch mode
npm run test:ui          # Interactive UI
npm run coverage          # Coverage report
npm run e2e              # Cypress interactive
npm run e2e:headless     # Cypress headless
```

## Linting

```bash
npm run lint             # Check
npm run lint:fix         # Fix issues
npm run format           # Format code
```

## Type Checking

```bash
npm run type-check
```

## Project Structure

```
src/
  components/       # UI components library
  services/         # API and communication services
  hooks/           # Custom React hooks
  themes/          # Design tokens and theme
  utils/           # Utility functions
  __tests__/       # Unit tests
  App.tsx          # Root component
  main.tsx         # Entry point
cypress/
  e2e/             # End-to-end tests
  component/       # Component tests
  support/         # Test support files
```
