#!/bin/bash
# AI Mozc IME Installer Script for Linux
# This script installs AI Mozc IME and sets up the required components

set -e

# ==================== Configuration ====================
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
CONFIG_DIR="${HOME}/.mozc"
DEFAULT_MODEL="mistral:7b"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# ==================== Helper Functions ====================

print_header() {
    echo ""
    echo -e "${CYAN}============================================================${NC}"
    echo -e "${CYAN} $1${NC}"
    echo -e "${CYAN}============================================================${NC}"
    echo ""
}

print_step() {
    echo -e "${YELLOW}[*] $1${NC}"
}

print_success() {
    echo -e "${GREEN}[OK] $1${NC}"
}

print_error() {
    echo -e "${RED}[ERROR] $1${NC}"
}

show_help() {
    cat << EOF
AI Mozc IME Installer

Usage: $0 [options]

Options:
    --prefix <path>       Installation prefix (default: /usr/local)
    --install-ollama      Also install Ollama (AI backend)
    --uninstall           Uninstall AI Mozc IME
    --help                Show this help message

Examples:
    $0                           # Standard installation
    $0 --install-ollama          # Install with Ollama
    $0 --uninstall               # Uninstall
    $0 --prefix /opt/mozc        # Custom install prefix

Environment Variables:
    INSTALL_PREFIX    Installation prefix (default: /usr/local)
EOF
}

check_root() {
    if [[ $EUID -ne 0 ]]; then
        print_error "This script must be run as root (use sudo)"
        exit 1
    fi
}

# ==================== Installation Functions ====================

install_prerequisites() {
    print_header "Checking Prerequisites"

    # Detect distribution
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
    else
        DISTRO="unknown"
    fi

    print_step "Detected distribution: $DISTRO"

    # Check for required packages
    print_step "Checking required packages..."

    case $DISTRO in
        ubuntu|debian)
            PACKAGES="build-essential git curl"
            for pkg in $PACKAGES; do
                if ! dpkg -l | grep -q "^ii  $pkg"; then
                    print_step "Installing $pkg..."
                    apt-get install -y $pkg
                fi
            done
            ;;
        fedora)
            PACKAGES="gcc-c++ git curl"
            for pkg in $PACKAGES; do
                if ! rpm -q $pkg &>/dev/null; then
                    print_step "Installing $pkg..."
                    dnf install -y $pkg
                fi
            done
            ;;
        arch)
            PACKAGES="base-devel git curl"
            for pkg in $PACKAGES; do
                if ! pacman -Qi $pkg &>/dev/null; then
                    print_step "Installing $pkg..."
                    pacman -S --noconfirm $pkg
                fi
            done
            ;;
        *)
            print_step "Unknown distribution. Please ensure build tools are installed."
            ;;
    esac

    print_success "Prerequisites check complete"
}

install_ollama() {
    print_header "Installing Ollama"

    # Check if Ollama is already installed
    if command -v ollama &>/dev/null; then
        print_success "Ollama is already installed: $(command -v ollama)"
        return
    fi

    print_step "Downloading and installing Ollama..."

    # Install using official script
    curl -fsSL https://ollama.ai/install.sh | sh

    if command -v ollama &>/dev/null; then
        print_success "Ollama installed successfully"

        # Download default model
        print_step "Downloading AI model ($DEFAULT_MODEL)..."
        ollama pull $DEFAULT_MODEL
        print_success "Model downloaded"
    else
        print_error "Ollama installation failed"
        echo "Please install Ollama manually from: https://ollama.ai"
    fi
}

install_mozc_ai() {
    print_header "Installing AI Mozc IME"

    # Create installation directories
    print_step "Creating installation directories..."
    mkdir -p "$INSTALL_PREFIX/lib/mozc-ai"
    mkdir -p "$INSTALL_PREFIX/share/mozc-ai"
    mkdir -p "$CONFIG_DIR"

    print_success "Created directories"

    # Copy built binaries (if they exist)
    print_step "Copying files..."
    BAZEL_BIN="$PROJECT_ROOT/bazel-bin"

    if [ -d "$BAZEL_BIN" ]; then
        cp -r "$BAZEL_BIN"/* "$INSTALL_PREFIX/lib/mozc-ai/" 2>/dev/null || true
        print_success "Copied built binaries"
    else
        echo "  Note: No built binaries found. Run build.sh first."
    fi

    # Copy documentation
    if [ -d "$PROJECT_ROOT/docs" ]; then
        cp -r "$PROJECT_ROOT/docs"/* "$INSTALL_PREFIX/share/mozc-ai/"
        print_success "Copied documentation"
    fi

    # Create default config file
    print_step "Creating default configuration..."
    CONFIG_FILE="$CONFIG_DIR/ai_config.json"

    if [ ! -f "$CONFIG_FILE" ]; then
        cat > "$CONFIG_FILE" << 'EOFCONFIG'
{
  "enabled": true,
  "backend_type": "ollama",
  "ollama_endpoint": "http://localhost:11434",
  "ollama_model": "mistral:7b",
  "connect_timeout_ms": 50,
  "request_timeout_ms": 500,
  "max_wait_ms": 600,
  "cache_ttl_seconds": 60,
  "cache_max_entries": 100,
  "history_size": 5,
  "log_level": "info",
  "log_ai_communication": false,
  "disable_ai": false,
  "use_mock": false
}
EOFCONFIG
        # Set proper ownership for user config
        SUDO_USER_HOME=$(eval echo ~${SUDO_USER:-$USER})
        if [ -n "$SUDO_USER" ]; then
            chown -R "$SUDO_USER:$(id -gn $SUDO_USER)" "$CONFIG_DIR"
        fi
        print_success "Created config at: $CONFIG_FILE"
    else
        echo "  Config already exists, skipping..."
    fi
}

setup_ibus() {
    print_header "Setting up IBus"

    print_step "Checking IBus installation..."

    if ! command -v ibus &>/dev/null; then
        print_step "IBus not found. Installing..."
        case $DISTRO in
            ubuntu|debian)
                apt-get install -y ibus ibus-mozc
                ;;
            fedora)
                dnf install -y ibus ibus-mozc
                ;;
            arch)
                pacman -S --noconfirm ibus ibus-mozc
                ;;
            *)
                echo "Please install IBus manually"
                ;;
        esac
    fi

    print_success "IBus is available"

    echo ""
    echo "To enable AI Mozc IME:"
    echo "1. Run: ibus-setup"
    echo "2. Go to Input Method tab"
    echo "3. Add Japanese > Mozc"
    echo "4. Use Super+Space to switch input methods"
}

uninstall_mozc_ai() {
    print_header "Uninstalling AI Mozc IME"

    # Remove installation directory
    if [ -d "$INSTALL_PREFIX/lib/mozc-ai" ]; then
        print_step "Removing installation directory..."
        rm -rf "$INSTALL_PREFIX/lib/mozc-ai"
        print_success "Removed: $INSTALL_PREFIX/lib/mozc-ai"
    fi

    if [ -d "$INSTALL_PREFIX/share/mozc-ai" ]; then
        rm -rf "$INSTALL_PREFIX/share/mozc-ai"
        print_success "Removed: $INSTALL_PREFIX/share/mozc-ai"
    fi

    # Ask about config
    if [ -d "$CONFIG_DIR" ]; then
        read -p "Remove configuration directory ($CONFIG_DIR)? [y/N] " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            rm -rf "$CONFIG_DIR"
            print_success "Removed: $CONFIG_DIR"
        fi
    fi

    print_success "Uninstallation complete"
}

show_summary() {
    print_header "Installation Summary"

    echo "AI Mozc IME has been installed!"
    echo ""
    echo "Installation Path: $INSTALL_PREFIX/lib/mozc-ai"
    echo "Documentation:     $INSTALL_PREFIX/share/mozc-ai"
    echo "Config Directory:  $CONFIG_DIR"
    echo "Config File:       $CONFIG_DIR/ai_config.json"
    echo "Log File:          $CONFIG_DIR/ai_log.txt"
    echo ""

    # Check Ollama status
    if curl -s http://localhost:11434/api/tags &>/dev/null; then
        echo -e "Ollama Status: ${GREEN}Running${NC}"
    else
        echo -e "Ollama Status: ${YELLOW}Not Running${NC}"
        echo ""
        echo "To start Ollama, run: ollama serve"
    fi

    echo ""
    echo "Next Steps:"
    echo "1. Ensure Ollama is running: ollama serve"
    echo "2. Download a model: ollama pull $DEFAULT_MODEL"
    echo "3. Set up IBus: ibus-setup"
    echo "4. Add Japanese > Mozc input method"
    echo ""
}

# ==================== Main ====================

# Parse arguments
INSTALL_OLLAMA=false
UNINSTALL=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        --install-ollama)
            INSTALL_OLLAMA=true
            shift
            ;;
        --uninstall)
            UNINSTALL=true
            shift
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Show banner
cat << 'EOF'

    _    ___   __  __                   ___ __  __ _____
   / \  |_ _| |  \/  | ___ ______ __   |_ _|  \/  | ____|
  / _ \  | |  | |\/| |/ _ \_  /  ___|   | || |\/| |  _|
 / ___ \ | |  | |  | | (_) / /| (___    | || |  | | |___
/_/   \_\___| |_|  |_|\___/___|\___|   |___|_|  |_|_____|

EOF

# Check root for installation
if [ "$UNINSTALL" = true ]; then
    check_root
    uninstall_mozc_ai
    exit 0
fi

# Normal installation requires root
check_root

install_prerequisites

if [ "$INSTALL_OLLAMA" = true ]; then
    install_ollama
fi

install_mozc_ai
setup_ibus
show_summary

print_success "Installation complete!"
