# Copilot Cloud Agent E2E Testing Guide

This directory contains configuration and scripts for running MilodikFX frontend E2E tests using Cypress in the Copilot cloud agent environment.

## Overview

The Copilot cloud agent is configured to:
1. **Preinstall**: Node.js 18 and npm dependencies (via `npm ci`)
2. **Build**: React frontend with Vite (`npm run build` → `frontend/dist/`)
3. **Start**: Vite preview server on port 5173 with health checks
4. **Test**: Run Cypress E2E tests in headless mode (`npm run e2e:headless`)
5. **Cache**: node_modules and dist directories between runs for faster execution

## Files

### Configuration
- **`.github/copilot-setup-steps.yml`** — MCP server configuration for the cloud agent
  - Defines all steps: preinstall, build, server startup, E2E testing, caching, and artifacts
  - Includes environment variables and port mappings
  - Non-interactive with proper exit codes and retry policies

### Scripts
- **`.github/scripts/run-e2e-tests.sh`** — Bash script for macOS/Linux development
  - Reproduces the cloud agent workflow locally
  - Supports options: `--build`, `--no-server`, `--watch`, `--help`
  - Exit codes: 0 (success), 1 (build failed), 2 (server failed), 3 (tests failed), 4 (bad args)

- **`.github/scripts/run-e2e-tests.ps1`** — PowerShell script for Windows development
  - Native Windows implementation with colored output
  - Supports options: `-Build`, `-NoServer`, `-Watch`, `-Help`
  - Same exit codes as bash version for consistency

## Quick Start

### Cloud Agent (Copilot)
1. Reference the configuration file in your Copilot workspace:
   ```
   Configuration: .github/copilot-setup-steps.yml
   ```
2. The agent will automatically:
   - Install Node.js 18
   - Install dependencies with `npm ci` in `frontend/`
   - Build the frontend (`npm run build`)
   - Start the preview server (`npx vite preview --port 5173`)
   - Run Cypress tests (`npm run e2e:headless`)
   - Capture videos and screenshots

### Local Development (Linux/macOS)
```bash
# Run full E2E test suite
.github/scripts/run-e2e-tests.sh

# Force rebuild before testing
.github/scripts/run-e2e-tests.sh --build

# Skip server startup (if already running)
.github/scripts/run-e2e-tests.sh --no-server

# Run in watch/interactive mode
.github/scripts/run-e2e-tests.sh --watch

# View help
.github/scripts/run-e2e-tests.sh --help
```

### Local Development (Windows PowerShell)
```powershell
# Run full E2E test suite
.\run-e2e-tests.ps1

# Force rebuild before testing
.\run-e2e-tests.ps1 -Build

# Skip server startup (if already running)
.\run-e2e-tests.ps1 -NoServer

# Run in watch/interactive mode
.\run-e2e-tests.ps1 -Watch

# View help
.\run-e2e-tests.ps1 -Help
```

## Configuration Details

### copilot-setup-steps.yml Structure

#### Preinstall Section
```yaml
preinstall:
  - Node.js 18 (via nvm)
  - npm ci in frontend/ (exact versions from package-lock.json)
```
This ensures a clean, reproducible environment with pinned dependencies.

#### Build & Preparation Steps
1. **build_frontend** — `npm run build` (TypeScript + Vite build)
2. **verify_build** — Ensure `frontend/dist/index.html` exists
3. **start_preview_server** — `npx vite preview --port 5173 --strictPort` (background process)
4. **wait_for_server** — `npx wait-on http://localhost:5173` (polls server readiness)
5. **health_check_server** — Verify index.html is served with correct Content-Type

#### E2E Test Steps
1. **run_cypress_e2e** — `npm run e2e:headless` (all Cypress tests)
2. **upload_artifacts** — Report videos and screenshots collected during tests

#### Environment Variables
```env
NODE_ENV=production          # Optimize build
VITE_API_BASE=http://localhost:5173
CYPRESS_BASE_URL=http://localhost:5173
CI=true                       # Signal CI environment
CYPRESS_HEADLESS=true         # Enforce headless mode
```

#### Caching
- `frontend/node_modules` — 1 hour TTL
- `frontend/dist` — 30 minutes TTL (invalidated by source changes)

#### Artifacts
- `frontend/cypress/videos/` — Test execution videos (7-day retention)
- `frontend/cypress/screenshots/` — Failure screenshots (7-day retention)
- `frontend/dist/` — Build artifacts (1-day retention)

### Port Configuration
- **5173** — Vite preview server (frontend E2E target)
  - Protocol: HTTP
  - Service: vite-preview
  - Health check: GET /index.html (expect HTTP 200, Content-Type: text/html)

## Exit Codes

### Local Scripts
| Code | Meaning | Example |
|------|---------|---------|
| 0    | Success | All tests passed |
| 1    | Build failed | `npm run build` returned non-zero |
| 2    | Server failed | Preview server didn't start or health check failed |
| 3    | Tests failed | Cypress tests had failures |
| 4    | Invalid args | Bad script arguments or missing requirements |

### Cloud Agent
The agent will:
- **Succeed** if all steps complete with `exit_code_check: true` returning 0
- **Fail** if any critical step fails (on_error: "fail")
- **Warn** if health check fails but continue (on_error: "warn")
- **Retry** up to 2 times on steps: `wait_for_server`, `run_cypress_e2e`

## Troubleshooting

### Preview Server Won't Start
```bash
# Check if port 5173 is already in use
lsof -i :5173  # macOS/Linux
Get-Process -Id (Get-NetTCPConnection -LocalPort 5173).OwningProcess  # Windows
```

**Solution**: Kill the process or use `--no-server` to skip startup.

### Cypress Tests Timeout
- Check if preview server is responding: `curl http://localhost:5173/`
- Increase `wait-on-timeout` in cypress.config.ts if needed
- Check `frontend/cypress/videos/` and `frontend/cypress/screenshots/` for test failures

### Dependencies Not Installing
```bash
# Clear npm cache and reinstall
npm cache clean --force
rm -rf frontend/node_modules frontend/package-lock.json
npm ci --prefix frontend/
```

### Build Artifacts Missing
```bash
# Force rebuild
.github/scripts/run-e2e-tests.sh --build  # Linux/macOS
.\run-e2e-tests.ps1 -Build                # Windows
```

## How the Cloud Agent Runs This

### Step-by-Step Execution

1. **Agent Startup**
   - Cloud agent provisions a Linux runner
   - Installs Node.js 18 via nvm
   
2. **Dependency Installation**
   - `npm ci` in `frontend/` (locks exact versions)
   
3. **Build Phase**
   - `npm run build` compiles TypeScript and bundles with Vite
   - Verifies `frontend/dist/index.html` exists
   
4. **Server Startup**
   - Background process: `npx vite preview --port 5173 --strictPort`
   - Waits up to 60 seconds for server readiness
   - Health check: verifies HTTP 200 + text/html Content-Type
   
5. **E2E Testing**
   - `npm run e2e:headless` runs all tests in `cypress/e2e/**/*.cy.ts`
   - Collects videos and screenshots automatically
   - Timeout: 10 minutes per test suite
   
6. **Artifact Collection**
   - Preserves test videos (7 days)
   - Preserves screenshots on failure (7 days)
   - Reports artifact counts to logs

### Caching Behavior

The agent caches two directories:

1. **node_modules (1-hour TTL)**
   - Avoids re-downloading dependencies
   - Reused across multiple runs
   - Invalidated if package-lock.json changes

2. **dist (30-minute TTL)**
   - Avoids rebuilding if source unchanged
   - Faster test execution
   - Invalidated by source file changes

### Failure Handling

- **Build fails** → Entire run fails, full output logged
- **Server startup fails** → Retry once (5-second backoff), then fail
- **Health check fails** → Warning logged, tests continue (API may not exist)
- **Tests fail** → Retry once (5-second backoff), then fail with full Cypress output
- **Artifacts missing** → Warning logged, testing continues

## Integration with GitHub Actions

The cloud agent workflow complements the existing GitHub Actions CI:

**GitHub Actions** (`.github/workflows/ci.yml`):
- Runs on ubuntu-latest
- Handles build + unit tests
- Optional E2E via Cypress GitHub Action

**Copilot Cloud Agent** (`.github/copilot-setup-steps.yml`):
- On-demand E2E testing from IDE
- Same environment as CI (Node 18)
- Caching for fast iteration
- Detailed logs + artifacts in cloud

Both use identical Node/npm versions and build steps for consistency.

## API Endpoints (Future)

The health check currently verifies:
- GET `/index.html` → HTTP 200, text/html

Future expansion (if needed):
- GET `/api/parameters/master-volume` — Plugin parameter
- GET `/api/levels` — Current audio levels
- Health check framework ready in `health_check_server` step

## Summary

- **copilot-setup-steps.yml** — Complete MCP configuration for cloud agent
- **run-e2e-tests.sh** — Cross-platform bash script for local Linux/macOS
- **run-e2e-tests.ps1** — Native PowerShell script for Windows developers
- **Port**: 5173 (Vite preview, not configurable in current setup)
- **Build**: `npm run build` → `frontend/dist/`
- **Tests**: `npm run e2e:headless` (Cypress headless)
- **Caching**: node_modules + dist between runs
- **Artifacts**: Videos + screenshots (7-day retention)
- **Exit Codes**: 0 (success), 1 (build), 2 (server), 3 (tests), 4 (args)
