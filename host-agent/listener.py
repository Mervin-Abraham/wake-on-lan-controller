#!/usr/bin/env python3
"""
Cross-platform Host Agent for Wake-on-LAN system

Endpoints:
  - GET /status
  - GET /sleep?token=...
  - GET /shutdown?token=...
  - GET /restart?token=...

Configuration (single file): config/device.yaml (copy from device.example.yaml)
Reads: esp.token, host.port, host.allowed_ips

Starts as a service via platform-specific installers.
"""

import os
import sys
import time
import json
import yaml
import platform
import logging
import subprocess
from pathlib import Path
from flask import Flask, request, jsonify


def project_root() -> Path:
    # listener.py is in <repo>/host-agent/, root is parent
    return Path(__file__).resolve().parent.parent


def load_config():
    cfg_path = project_root() / "config" / "device.yaml"
    print(f"Looking for config at: {cfg_path}")
    print(f"Project root: {project_root()}")
    
    if not cfg_path.exists():
        print(f"ERROR: Config not found: {cfg_path}")
        print(f"Please copy config/device.example.yaml to config/device.yaml and fill values.")
        sys.exit(1)
    
    print(f"Config found: {cfg_path}")
    with cfg_path.open("r", encoding="utf-8") as f:
        config = yaml.safe_load(f)
        print(f"Config loaded successfully")
        return config


CFG = load_config()

SECRET_TOKEN = str(CFG.get("esp", {}).get("token", ""))
HOST_CFG = CFG.get("host", {})
LISTEN_PORT = int(HOST_CFG.get("port", 8888))
ALLOWED_IPS = HOST_CFG.get("allowed_ips", []) or []

app = Flask(__name__)

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[logging.StreamHandler(sys.stdout)]
)
logger = logging.getLogger("wol-host-agent")


def _authorized_client(ip: str) -> bool:
    if not ALLOWED_IPS:
        return True
    return ip in ALLOWED_IPS


def _check_token(req_token: str) -> bool:
    return bool(SECRET_TOKEN) and req_token == SECRET_TOKEN


@app.route("/status")
def status():
    return jsonify({
        "status": "running",
        "hostname": platform.node(),
        "os": platform.system(),
        "os_version": platform.version(),
        "port": LISTEN_PORT,
        "uptime": int(time.time())
    })


@app.route("/sleep", methods=["GET"])
def route_sleep():
    client_ip = request.remote_addr
    if not _authorized_client(client_ip):
        return jsonify({"status": "error", "message": "Forbidden: IP not allowed"}), 403
    token = request.args.get("token", "")
    if not _check_token(token):
        return jsonify({"status": "error", "message": "Unauthorized"}), 401
    ok = sleep_computer()
    return (jsonify({"status": "success"}), 200) if ok else (jsonify({"status": "error"}), 500)


@app.route("/shutdown", methods=["GET"])
def route_shutdown():
    client_ip = request.remote_addr
    if not _authorized_client(client_ip):
        return jsonify({"status": "error", "message": "Forbidden: IP not allowed"}), 403
    token = request.args.get("token", "")
    if not _check_token(token):
        return jsonify({"status": "error", "message": "Unauthorized"}), 401
    ok = shutdown_computer()
    return (jsonify({"status": "success"}), 200) if ok else (jsonify({"status": "error"}), 500)


@app.route("/restart", methods=["GET"])
def route_restart():
    client_ip = request.remote_addr
    if not _authorized_client(client_ip):
        return jsonify({"status": "error", "message": "Forbidden: IP not allowed"}), 403
    token = request.args.get("token", "")
    if not _check_token(token):
        return jsonify({"status": "error", "message": "Unauthorized"}), 401
    ok = restart_computer()
    return (jsonify({"status": "success"}), 200) if ok else (jsonify({"status": "error"}), 500)


def shutdown_computer() -> bool:
    try:
        system = platform.system()
        if system == "Windows":
            subprocess.Popen(["shutdown", "/s", "/t", "0"])
            return True
        if system == "Linux":
            subprocess.Popen(["systemctl", "poweroff"])
            return True
        if system == "Darwin":
            subprocess.Popen(["sudo", "shutdown", "-h", "now"])
            return True
        return False
    except Exception as e:
        logger.error(f"shutdown failed: {e}")
        return False


def sleep_computer() -> bool:
    try:
        system = platform.system()
        if system == "Windows":
            ps_cmd = "(Add-Type -AssemblyName System.Windows.Forms); [System.Windows.Forms.Application]::SetSuspendState('Suspend',$false,$false)"
            subprocess.Popen(["powershell", "-NoProfile", "-Command", ps_cmd])
            return True
        if system == "Linux":
            subprocess.Popen(["systemctl", "suspend"])
            return True
        if system == "Darwin":
            subprocess.Popen(["pmset", "sleepnow"])
            return True
        return False
    except Exception as e:
        logger.error(f"sleep failed: {e}")
        return False


def restart_computer() -> bool:
    try:
        system = platform.system()
        if system == "Windows":
            subprocess.Popen(["shutdown", "/r", "/t", "0"])
            return True
        if system == "Linux":
            subprocess.Popen(["systemctl", "reboot"])
            return True
        if system == "Darwin":
            subprocess.Popen(["sudo", "shutdown", "-r", "now"])
            return True
        return False
    except Exception as e:
        logger.error(f"restart failed: {e}")
        return False


def main():
    print(f"Starting Wake-on-LAN Host Agent...")
    print(f"Config loaded from: {project_root() / 'config' / 'device.yaml'}")
    print(f"Listening on port: {LISTEN_PORT}")
    print(f"Allowed IPs: {ALLOWED_IPS if ALLOWED_IPS else 'Any'}")
    print(f"Service starting...")
    if not SECRET_TOKEN:
        print("ERROR: esp.token is empty in config/device.yaml")
        sys.exit(1)
    app.run(host="0.0.0.0", port=LISTEN_PORT)


if __name__ == "__main__":
    main()


