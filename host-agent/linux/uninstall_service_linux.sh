#!/usr/bin/env bash
set -euo pipefail

UNIT_NAME="wol-host-agent.service"

systemctl --user stop "$UNIT_NAME" || true
systemctl --user disable "$UNIT_NAME" || true
UNIT_FILE="$HOME/.config/systemd/user/$UNIT_NAME"
if [ -f "$UNIT_FILE" ]; then
  rm -f "$UNIT_FILE"
fi
systemctl --user daemon-reload
echo "Service removed (files and venv kept)."


