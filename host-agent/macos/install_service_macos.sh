#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
VENV_DIR="$ROOT_DIR/.venv"
PY_EXE="$VENV_DIR/bin/python3"
REQ_FILE="$ROOT_DIR/requirements.txt"
LISTENER="$ROOT_DIR/host-agent/listener.py"
PLIST="$HOME/Library/LaunchAgents/com.wol.host.agent.plist"

echo "Repo root: $ROOT_DIR"
if [ ! -d "$VENV_DIR" ]; then
  python3 -m venv "$VENV_DIR"
fi
"$PY_EXE" -m pip install --upgrade pip
"$PY_EXE" -m pip install -r "$REQ_FILE"

cat > "$PLIST" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>com.wol.host.agent</string>
  <key>ProgramArguments</key>
  <array>
    <string>$VENV_DIR/bin/python3</string>
    <string>$ROOT_DIR/host-agent/listener.py</string>
  </array>
  <key>RunAtLoad</key>
  <true/>
  <key>KeepAlive</key>
  <true/>
  <key>WorkingDirectory</key>
  <string>$ROOT_DIR</string>
</dict>
</plist>
EOF

launchctl unload "$PLIST" 2>/dev/null || true
launchctl load "$PLIST"
echo "LaunchAgent installed. Check: launchctl list | grep com.wol.host.agent"


