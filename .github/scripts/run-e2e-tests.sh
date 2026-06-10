#!/usr/bin/env bash
# ==============================================================================
# MilodikFX Frontend E2E Test Runner (macOS / Linux)
# ==============================================================================
# 
# This script reproduces the cloud agent E2E testing workflow locally.
# Designed to match the copilot-setup-steps.yml configuration.
#
# Usage:
#   ./run-e2e-tests.sh [--build] [--no-server] [--headless]
#
# Options:
#   --build       Force rebuild of frontend (default: only if dist/ missing)
#   --no-server   Skip preview server (assume it's already running)
#   --headless    Run Cypress in headless mode (default: true)
#   --help        Show this help message
#
# Exit Codes:
#   0 - All tests passed
#   1 - Build failed
#   2 - Server failed to start or health check failed
#   3 - Cypress tests failed
#   4 - Invalid arguments
#
# ==============================================================================

set -o pipefail  # Exit on pipe failures

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
FRONTEND_DIR="frontend"
PREVIEW_PORT=5173
PREVIEW_URL="http://localhost:${PREVIEW_PORT}"
PREVIEW_PID=""
MAX_WAIT_TIME=60
FORCE_BUILD=false
SKIP_SERVER=false
HEADLESS=true

# ==============================================================================
# Helper Functions
# ==============================================================================

log_info() {
    echo -e "${BLUE}ℹ${NC}  $1"
}

log_success() {
    echo -e "${GREEN}✓${NC}  $1"
}

log_warn() {
    echo -e "${YELLOW}⚠${NC}  $1"
}

log_error() {
    echo -e "${RED}✗${NC}  $1"
}

usage() {
    cat << 'EOF'
MilodikFX Frontend E2E Test Runner

Usage:
  ./run-e2e-tests.sh [OPTIONS]

Options:
  --build         Force rebuild of frontend (default: only if dist/ missing)
  --no-server     Skip preview server (assume it's already running)
  --watch         Run Cypress in watch mode (interactive)
  --help          Show this help message

Examples:
  ./run-e2e-tests.sh                 # Run full E2E test suite
  ./run-e2e-tests.sh --build         # Rebuild frontend then run E2E
  ./run-e2e-tests.sh --no-server     # Skip server startup (it's running elsewhere)
  ./run-e2e-tests.sh --watch         # Run in watch/interactive mode

EOF
}

cleanup() {
    if [ -n "$PREVIEW_PID" ]; then
        log_info "Stopping preview server (PID: $PREVIEW_PID)..."
        kill $PREVIEW_PID 2>/dev/null || true
        wait $PREVIEW_PID 2>/dev/null || true
    fi
}

# Trap SIGINT (Ctrl+C) and SIGTERM to cleanup
trap cleanup EXIT

check_requirements() {
    log_info "Checking requirements..."
    
    # Check Node.js version
    if ! command -v node &> /dev/null; then
        log_error "Node.js is not installed"
        exit 4
    fi
    
    NODE_VERSION=$(node -v | cut -d'v' -f2 | cut -d'.' -f1)
    if [ "$NODE_VERSION" -lt 18 ]; then
        log_error "Node.js 18+ is required, but found: $(node -v)"
        exit 4
    fi
    log_success "Node.js $(node -v) ✓"
    
    # Check npm
    if ! command -v npm &> /dev/null; then
        log_error "npm is not installed"
        exit 4
    fi
    log_success "npm $(npm -v) ✓"
    
    # Check frontend directory
    if [ ! -d "$FRONTEND_DIR" ]; then
        log_error "Frontend directory not found: $FRONTEND_DIR"
        exit 4
    fi
    log_success "Frontend directory found ✓"
}

install_dependencies() {
    log_info "Installing frontend dependencies..."
    
    cd "$FRONTEND_DIR"
    
    if npm ci; then
        log_success "Dependencies installed ✓"
    else
        log_error "Failed to install dependencies"
        exit 1
    fi
    
    cd ..
}

build_frontend() {
    log_info "Building frontend..."
    
    cd "$FRONTEND_DIR"
    
    if npm run build; then
        log_success "Frontend built ✓"
    else
        log_error "Frontend build failed"
        exit 1
    fi
    
    cd ..
}

verify_build() {
    log_info "Verifying build artifacts..."
    
    if [ ! -d "$FRONTEND_DIR/dist" ]; then
        log_error "Build directory not found: $FRONTEND_DIR/dist"
        exit 1
    fi
    
    if [ ! -f "$FRONTEND_DIR/dist/index.html" ]; then
        log_error "index.html not found in dist"
        exit 1
    fi
    
    log_success "Build artifacts verified ✓"
}

start_preview_server() {
    if [ "$SKIP_SERVER" = true ]; then
        log_info "Skipping preview server (--no-server flag set)"
        return 0
    fi
    
    log_info "Starting preview server on port $PREVIEW_PORT..."
    
    cd "$FRONTEND_DIR"
    
    # Start preview server in background
    npx vite preview --port $PREVIEW_PORT --strictPort > /tmp/vite-preview.log 2>&1 &
    PREVIEW_PID=$!
    
    cd ..
    
    # Wait for server to be ready
    wait_for_server
}

wait_for_server() {
    log_info "Waiting for preview server to be ready (max ${MAX_WAIT_TIME}s)..."
    
    local elapsed=0
    local interval=2
    
    while [ $elapsed -lt $MAX_WAIT_TIME ]; do
        if curl -s -f "$PREVIEW_URL" > /dev/null 2>&1; then
            log_success "Preview server is ready ✓"
            health_check_server
            return 0
        fi
        
        sleep $interval
        elapsed=$((elapsed + interval))
        echo -ne "\r  Waiting... ${elapsed}s"
    done
    
    echo ""
    log_error "Preview server failed to start within ${MAX_WAIT_TIME}s"
    
    if [ -f /tmp/vite-preview.log ]; then
        log_error "Server logs:"
        cat /tmp/vite-preview.log | sed 's/^/    /'
    fi
    
    exit 2
}

health_check_server() {
    log_info "Performing health check on preview server..."
    
    # Check if index.html is served
    local response=$(curl -s -w "\n%{http_code}" "$PREVIEW_URL/index.html")
    local http_code=$(echo "$response" | tail -n 1)
    local content_type=$(curl -s -I "$PREVIEW_URL/index.html" 2>/dev/null | grep -i "content-type" || true)
    
    if [ "$http_code" != "200" ]; then
        log_error "Server returned HTTP $http_code for /index.html"
        exit 2
    fi
    
    if echo "$content_type" | grep -qi "text/html"; then
        log_success "Server health check passed (HTTP $http_code, Content-Type: text/html) ✓"
    else
        log_warn "Unexpected Content-Type: $content_type"
    fi
}

run_cypress_tests() {
    log_info "Running Cypress E2E tests..."
    
    cd "$FRONTEND_DIR"
    
    if [ "$HEADLESS" = true ]; then
        npm run e2e:headless
    else
        npm run e2e
    fi
    
    local exit_code=$?
    cd ..
    
    if [ $exit_code -eq 0 ]; then
        log_success "Cypress tests passed ✓"
    else
        log_error "Cypress tests failed (exit code: $exit_code)"
        exit 3
    fi
}

report_artifacts() {
    log_info "Test artifacts:"
    
    if [ -d "$FRONTEND_DIR/cypress/videos" ]; then
        local video_count=$(find "$FRONTEND_DIR/cypress/videos" -type f | wc -l)
        if [ $video_count -gt 0 ]; then
            log_success "Videos: $video_count file(s)"
        fi
    fi
    
    if [ -d "$FRONTEND_DIR/cypress/screenshots" ]; then
        local screenshot_count=$(find "$FRONTEND_DIR/cypress/screenshots" -type f | wc -l)
        if [ $screenshot_count -gt 0 ]; then
            log_success "Screenshots: $screenshot_count file(s)"
        fi
    fi
}

# ==============================================================================
# Parse Arguments
# ==============================================================================

while [ $# -gt 0 ]; do
    case "$1" in
        --build)
            FORCE_BUILD=true
            shift
            ;;
        --no-server)
            SKIP_SERVER=true
            shift
            ;;
        --watch)
            HEADLESS=false
            shift
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            exit 4
            ;;
    esac
done

# ==============================================================================
# Main Execution
# ==============================================================================

echo ""
log_info "=========================================="
log_info "MilodikFX Frontend E2E Test Runner"
log_info "=========================================="
echo ""

check_requirements
install_dependencies

# Check if we need to build
if [ "$FORCE_BUILD" = true ] || [ ! -d "$FRONTEND_DIR/dist" ]; then
    build_frontend
    verify_build
else
    log_info "Build artifacts found, skipping build (use --build to force rebuild)"
fi

start_preview_server
run_cypress_tests
report_artifacts

echo ""
log_success "=========================================="
log_success "All E2E tests completed successfully! ✓"
log_success "=========================================="
echo ""

exit 0
