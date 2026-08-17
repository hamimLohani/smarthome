#!/bin/zsh
# ─────────────────────────────────────────────────────────────────────────────
# Smart Home — Compile & Upload Script
# Usage:
#   ./flash.sh            → auto-detect port, compile & upload
#   ./flash.sh -p /dev/cu.usbserial-XXX  → specify port manually
#   ./flash.sh -c         → compile only, do not upload
#   ./flash.sh -m         → open Serial Monitor after upload
#   ./flash.sh -h         → show help
# ─────────────────────────────────────────────────────────────────────────────

set -e

# ── Config ────────────────────────────────────────────────────────────────────
FQBN="esp8266:esp8266:nodemcuv2"
BAUD="115200"
SKETCH_DIR="$(cd "$(dirname "$0")/smart-home-firmware" && pwd)"

# ── Colors ────────────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

# ── Flags ─────────────────────────────────────────────────────────────────────
PORT=""
COMPILE_ONLY=false
OPEN_MONITOR=false

# ── Help ──────────────────────────────────────────────────────────────────────
usage() {
  echo ""
  echo "${BOLD}Smart Home Flash Tool${RESET}"
  echo ""
  echo "  ${CYAN}./flash.sh${RESET}                    Auto-detect port, compile & upload"
  echo "  ${CYAN}./flash.sh -p /dev/cu.XXX${RESET}     Specify serial port manually"
  echo "  ${CYAN}./flash.sh -c${RESET}                  Compile only (no upload)"
  echo "  ${CYAN}./flash.sh -m${RESET}                  Open Serial Monitor after upload"
  echo "  ${CYAN}./flash.sh -h${RESET}                  Show this help"
  echo ""
}

# ── Parse arguments ───────────────────────────────────────────────────────────
while getopts "p:cmh" opt; do
  case $opt in
    p) PORT="$OPTARG" ;;
    c) COMPILE_ONLY=true ;;
    m) OPEN_MONITOR=true ;;
    h) usage; exit 0 ;;
    *) usage; exit 1 ;;
  esac
done

# ── Banner ────────────────────────────────────────────────────────────────────
echo ""
echo "${BOLD}${CYAN}╔════════════════════════════════╗${RESET}"
echo "${BOLD}${CYAN}║   Smart Home Firmware Flash    ║${RESET}"
echo "${BOLD}${CYAN}╚════════════════════════════════╝${RESET}"
echo ""

# ── Check arduino-cli ─────────────────────────────────────────────────────────
if ! command -v arduino-cli &> /dev/null; then
  echo "${RED}✗ arduino-cli not found.${RESET}"
  echo "  Install it with: brew install arduino-cli"
  exit 1
fi

echo "${GREEN}✓ arduino-cli found:${RESET} $(arduino-cli version | head -1)"

# ── Verify sketch directory ───────────────────────────────────────────────────
if [ ! -f "$SKETCH_DIR/smart-home-firmware.ino" ]; then
  echo "${RED}✗ Sketch not found at: $SKETCH_DIR${RESET}"
  exit 1
fi
echo "${GREEN}✓ Sketch:${RESET} $SKETCH_DIR"

# ── Compile ───────────────────────────────────────────────────────────────────
echo ""
echo "${BOLD}[1/2] Compiling...${RESET}"
echo "      Board: $FQBN"
echo ""

if arduino-cli compile \
    --fqbn "$FQBN" \
    "$SKETCH_DIR"; then
  echo ""
  echo "${GREEN}✓ Compile successful!${RESET}"
else
  echo ""
  echo "${RED}✗ Compile failed. Fix errors above and try again.${RESET}"
  exit 1
fi

# ── Exit here if compile-only ─────────────────────────────────────────────────
if [ "$COMPILE_ONLY" = true ]; then
  echo ""
  echo "${YELLOW}ℹ Compile-only mode. Skipping upload.${RESET}"
  echo ""
  exit 0
fi

# ── Auto-detect port ──────────────────────────────────────────────────────────
if [ -z "$PORT" ]; then
  echo ""
  echo "${BOLD}[2/2] Detecting serial port...${RESET}"

  # Look for common NodeMCU/CH340/CP2102 device names on macOS
  DETECTED=$(ls /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART* 2>/dev/null | head -1)

  if [ -z "$DETECTED" ]; then
    echo "${RED}✗ No USB serial device found.${RESET}"
    echo ""
    echo "  Make sure the NodeMCU is plugged in, then:"
    echo "  ${CYAN}ls /dev/cu.*${RESET}   <- find your port name"
    echo "  ${CYAN}./flash.sh -p /dev/cu.usbserial-XXXX${RESET}  <- use it"
    echo ""
    exit 1
  fi

  PORT="$DETECTED"
  echo "${GREEN}✓ Found port:${RESET} $PORT"
fi

# ── Upload ────────────────────────────────────────────────────────────────────
echo ""
echo "${BOLD}Uploading to $PORT...${RESET}"
echo ""

if arduino-cli upload \
    --fqbn "$FQBN" \
    --port "$PORT" \
    "$SKETCH_DIR"; then
  echo ""
  echo "${GREEN}✓ Upload complete! NodeMCU is running the new firmware.${RESET}"
else
  echo ""
  echo "${RED}✗ Upload failed.${RESET}"
  echo ""
  echo "  Troubleshooting tips:"
  echo "  - Hold the FLASH button on the NodeMCU while upload starts"
  echo "  - Try a different USB cable (some cables are charge-only)"
  echo "  - Check that no other app (Serial Monitor) has the port open"
  echo "  - Specify port manually:  ${CYAN}./flash.sh -p /dev/cu.XXX${RESET}"
  echo ""
  exit 1
fi

# ── Serial Monitor ────────────────────────────────────────────────────────────
if [ "$OPEN_MONITOR" = true ]; then
  echo ""
  echo "${BOLD}Opening Serial Monitor at ${BAUD} baud...${RESET}"
  echo "${YELLOW}Press Ctrl+C to exit.${RESET}"
  echo ""
  sleep 1
  arduino-cli monitor --port "$PORT" --config "baudrate=$BAUD"
fi

echo ""
