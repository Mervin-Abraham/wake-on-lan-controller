#!/usr/bin/env bash
set -euo pipefail

PLIST="$HOME/Library/LaunchAgents/com.wol.host.agent.plist"
launchctl unload "$PLIST" 2>/dev/null || true
rm -f "$PLIST"
echo "LaunchAgent removed (files and venv kept)."


