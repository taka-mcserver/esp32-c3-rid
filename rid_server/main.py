"""
RID Management System Launcher
Usage: python main.py   (from rid_server directory)
   or: python rid_server/main.py  (from parent directory)
"""
import os
import sys

# Determine the server directory
script_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(script_dir)

# Add server dir to path if not already there
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

from server import main

if __name__ == "__main__":
    print("Starting RID Management System...")
    print("Web dashboard: http://127.0.0.1:5000")
    print()
    print("Notes:")
    print("  - Wi-Fi sniffing requires monitor mode (run as Administrator)")
    print("  - Wi-Fi simulation requires Administrator privileges")
    print("  - BLE scanning requires: pip install bleak")
    print("  - Press Ctrl+C to stop")
    print()
    main()
