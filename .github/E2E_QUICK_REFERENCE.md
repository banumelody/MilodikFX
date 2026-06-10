# MilodikFX E2E Testing — Quick Reference

## Cloud Agent Configuration (Copilot MCP)

### File Location
```
.github/copilot-setup-steps.yml
```

### How to Invoke

#### Option 1: Direct Configuration Reference
In your Copilot workspace settings or agent config:
```yaml
# Copilot Agent Config
mcp_servers:
  frontend_e2e:
    config_file: ".github/copilot-setup-steps.yml"
    name: "MilodikFX Frontend E2E Testing"
```

#### Option 2: GitHub Actions Integration
```yaml
# .github/workflows/e2e-cloud-agent.yml (example)
name: E2E Tests (Cloud Agent)
on: [pull_request]
jobs:
  e2e:
    uses: github/copilot-cloud-agent@v1
    with:
      config: .github/copilot-setup-steps.yml
```

#### Option 3: Manual Cloud Agent Invocation
```bash
# From IDE/CLI using Copilot
copilot run --config .github/copilot-setup-steps.yml
```

## Local Reproduction

### Linux / macOS
```bash
cd /path/to/MilodikFX

# Full run (install deps, build, start server, run tests)
.github/scripts/run-e2e-tests.sh

# With rebuild forced
.github/scripts/run-e2e-tests.sh --build

# Skip server (assume it's running on :5173)
.github/scripts/run-e2e-tests.sh --no-server

# Interactive mode (open Cypress UI)
.github/scripts/run-e2e-tests.sh --watch
```

### Windows PowerShell
```powershell
cd C:\path\to\MilodikFX

# Full run (install deps, build, start server, run tests)
.\run-e2e-tests.ps1

# With rebuild forced
.\run-e2e-tests.ps1 -Build

# Skip server (assume it's running on :5173)
.\run-e2e-tests.ps1 -NoServer

# Interactive mode (open Cypress UI)
.\run-e2e-tests.ps1 -Watch
```

### Manual Commands (if you prefer direct execution)

#### 1. Install Dependencies
```bash
cd frontend
npm ci
```

#### 2. Build Frontend
```bash
cd frontend
npm run build
# Output: frontend/dist/
```

#### 3. Start Preview Server
```bash
cd frontend
npx vite preview --port 5173 --strictPort
# Server starts on http://localhost:5173
```

#### 4. Wait for Server (in another terminal)
```bash
# Option A: Using npm package
npx wait-on http://localhost:5173 --timeout 60000

# Option B: Using curl in loop (macOS/Linux)
for i in {1..60}; do curl -s http://localhost:5173/ && break || sleep 1; done

# Option C: Using curl in loop (Windows PowerShell)
for($i=0; $i -lt 60; $i++) { 
  if(curl.exe -s http://localhost:5173/) { break } else { Start-Sleep -Seconds 1 }
}
```

#### 5. Run Cypress Tests
```bash
cd frontend

# Headless (CI mode)
npm run e2e:headless
# or
cypress run

# Interactive (UI mode)
npm run e2e
# or
cypress open
```

## Configuration Structure

### Preinstall Phase
```yaml
preinstall:
  - Node.js 18 (nvm)
  - npm ci in frontend/
```

### Setup Steps
| Step | Command | Timeout | Purpose |
|------|---------|---------|---------|
| build_frontend | npm run build | 300s | Compile TypeScript + bundle with Vite |
| verify_build | Check frontend/dist exists | 30s | Ensure build artifacts present |
| start_preview_server | npx vite preview --port 5173 | 60s | Start background server |
| wait_for_server | npx wait-on http://localhost:5173 | 75s | Poll for server readiness |
| health_check_server | curl http://localhost:5173/index.html | 30s | Verify HTTP 200 + text/html |

### Test Steps
| Step | Command | Timeout | Purpose |
|------|---------|---------|---------|
| run_cypress_e2e | npm run e2e:headless | 600s | Execute all E2E tests |
| upload_artifacts | Report videos/screenshots | 30s | Collect test output files |

## Environment Variables

Automatically set by cloud agent:
```env
NODE_ENV=production
VITE_API_BASE=http://localhost:5173
CYPRESS_BASE_URL=http://localhost:5173
CI=true
CYPRESS_HEADLESS=true
```

Override locally if needed:
```bash
# Linux/macOS
export CYPRESS_BASE_URL=http://localhost:5173
npm run e2e:headless

# Windows PowerShell
$env:CYPRESS_BASE_URL="http://localhost:5173"
npm run e2e:headless
```

## Caching

The cloud agent automatically caches:

1. **node_modules/** (1-hour TTL)
   - Avoids re-downloading 1000+ packages
   - Invalidated on package-lock.json changes

2. **dist/** (30-minute TTL)
   - Avoids rebuild if source unchanged
   - Invalidated on frontend source file changes

Cache is per-agent-environment (Linux runner), so warm builds typically complete in 30-40% of cold build time.

## Artifacts Preserved

- **cypress/videos/** — Test execution videos (7 days)
- **cypress/screenshots/** — Failure screenshots (7 days)
- **dist/** — Build artifacts (1 day)

Download artifacts from cloud agent logs/storage after run completes.

## Exit Codes

```
0 = Success (all tests passed)
1 = Build failed
2 = Server failed to start / health check failed
3 = Cypress tests failed
4 = Invalid arguments / missing requirements
```

## Health Checks

The cloud agent performs the following health checks:

### 1. Build Verification
✓ frontend/dist/ directory exists
✓ frontend/dist/index.html present

### 2. Server Readiness
✓ HTTP 200 response on GET /index.html
✓ Content-Type header includes "text/html"
✓ Server responds within 60 seconds

### 3. Test Execution
✓ All cypress/e2e/**/*.cy.ts tests run
✓ No test failures
✓ Completion within 10 minutes

## Troubleshooting Checklist

- [ ] Node.js 18+ installed: `node -v` (should be v18.x or higher)
- [ ] npm available: `npm -v`
- [ ] frontend/ directory exists
- [ ] Port 5173 not in use: `lsof -i :5173` (macOS) or `Get-Process -Id (Get-NetTCPConnection -LocalPort 5173).OwningProcess` (Windows)
- [ ] package-lock.json is checked in (ensures exact dependencies)
- [ ] cypress/e2e/ tests exist and are valid TypeScript
- [ ] dist/ removed if stale: `rm -rf frontend/dist && npm run build`
- [ ] Check firewall: localhost:5173 accessible
- [ ] Inspect Cypress logs: `frontend/cypress/videos/` and `frontend/cypress/screenshots/`

## Retry Policy

The cloud agent will automatically retry on failures:
- **wait_for_server** — up to 2 retries (5-second backoff)
- **run_cypress_e2e** — up to 2 retries (5-second backoff)

Other steps fail immediately without retry.

## Examples

### Scenario 1: Run Full E2E Suite
**Command:**
```bash
.github/scripts/run-e2e-tests.sh
```

**What Happens:**
1. Check Node.js 18+
2. Install dependencies (npm ci)
3. Build frontend (npm run build)
4. Verify dist/ exists
5. Start preview server (port 5173)
6. Wait for server to be ready
7. Health check (/index.html exists, HTTP 200)
8. Run Cypress headless (npm run e2e:headless)
9. Report artifacts (video/screenshot counts)
10. Exit 0 on success

---

### Scenario 2: Force Rebuild
**Command:**
```bash
.github/scripts/run-e2e-tests.sh --build
```

**What Happens:**
- Always rebuilds, even if dist/ exists
- Useful for: TypeScript changes, Vite config changes, dependency updates

---

### Scenario 3: Skip Server (Already Running)
**Command:**
```bash
.github/scripts/run-e2e-tests.sh --no-server
# (server on :5173 must be running in another terminal)
```

**What Happens:**
- Skips server startup and health check
- Useful for: Development iteration, multiple test runs against same server

---

### Scenario 4: Interactive Mode
**Command:**
```bash
.github/scripts/run-e2e-tests.sh --watch
```

**What Happens:**
- Opens Cypress browser UI (not headless)
- Allows manual inspection of elements, breakpoints, etc.
- Exit with Ctrl+C

## Summary

| Aspect | Detail |
|--------|--------|
| **Config File** | `.github/copilot-setup-steps.yml` |
| **Local Script (Linux/macOS)** | `.github/scripts/run-e2e-tests.sh` |
| **Local Script (Windows)** | `.github/scripts/run-e2e-tests.ps1` |
| **Frontend Dir** | `frontend/` |
| **Build Output** | `frontend/dist/` |
| **Preview Server** | `http://localhost:5173` |
| **Test Framework** | Cypress 13.x (headless) |
| **Node Version** | 18.x (pinned in CI matrix) |
| **Test Specs** | `cypress/e2e/**/*.cy.ts` |
| **Caching** | node_modules (1h), dist (30m) |
| **Artifacts** | videos (7d), screenshots (7d), dist (1d) |
| **Exit Codes** | 0=success, 1=build, 2=server, 3=tests, 4=args |

## Documentation Files

- **`.github/copilot-setup-steps.yml`** — Full MCP configuration (this is what the agent uses)
- **`.github/E2E_TESTING.md`** — Detailed troubleshooting and reference guide
- **`.github/scripts/run-e2e-tests.sh`** — Bash script for local runs (Linux/macOS)
- **`.github/scripts/run-e2e-tests.ps1`** — PowerShell script for local runs (Windows)

---

**Last Updated:** 2024  
**Maintained By:** MilodikFX Team  
**Status:** Active
