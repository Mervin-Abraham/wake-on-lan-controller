#!/usr/bin/env bash
set -euo pipefail

# Always run from this script's folder
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
VENV_DIR="$ROOT_DIR/.venv"
PY_EXE="$VENV_DIR/bin/python3"
REQ_FILE="$ROOT_DIR/requirements.txt"
LISTENER="$ROOT_DIR/host-agent/listener.py"

UNIT_DIR_USER="$HOME/.config/systemd/user"
UNIT_NAME="wol-host-agent.service"

echo "Repo root: $ROOT_DIR"
echo "[1/3] Creating Python virtualenv (if missing)"
if [ ! -d "$VENV_DIR" ]; then
  python3 -m venv "$VENV_DIR"
fi

echo "[2/3] Installing requirements"
"$PY_EXE" -m pip install --upgrade pip
"$PY_EXE" -m pip install -r "$REQ_FILE"

echo "[3/3] Creating systemd user service"
mkdir -p "$UNIT_DIR_USER"
cat > "$UNIT_DIR_USER/$UNIT_NAME" <<EOF
[Unit]
Description=PC Shutdown & Sleep Listener (WoL Companion)
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=%h/.venv/bin/python3 %h/wake-on-lan/host-agent/listener.py
Restart=always
RestartSec=5
WorkingDirectory=%h/wake-on-lan

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload
systemctl --user enable "$UNIT_NAME"
systemctl --user start "$UNIT_NAME"

echo "Service installed (user). To check: systemctl --user status $UNIT_NAME"
echo "Note: ensure linger is enabled for user if service should run without login: loginctl enable-linger $(whoami)"


