#!/usr/bin/env python3
"""
Run a cutechess-cli tournament to estimate ELO rating.
Requires: cutechess-cli in PATH, c64chess.exe built, and a reference engine.

Usage:
    python scripts/elo_test.py [--rounds 200] [--opponent tscp.exe]
"""

import argparse
import subprocess
import sys
import os

def main():
    parser = argparse.ArgumentParser(description="Run ELO test tournament")
    parser.add_argument("--rounds", type=int, default=200, help="Number of game pairs")
    parser.add_argument("--opponent", default="tscp.exe", help="Reference engine path")
    parser.add_argument("--tc", default="1+0.1", help="Time control (seconds+increment)")
    parser.add_argument("--openings", default="", help="Opening book file (EPD format)")
    args = parser.parse_args()

    engine_path = os.path.join("build", "c64chess.exe")
    if not os.path.exists(engine_path):
        print(f"Error: {engine_path} not found. Run 'make pc' first.")
        sys.exit(1)

    cmd = [
        "cutechess-cli",
        "-engine", f"cmd={engine_path}", "proto=uci", "name=C64Chess",
        "-engine", f"cmd={args.opponent}", "proto=uci", "name=Opponent",
        "-each", f"tc={args.tc}",
        "-rounds", str(args.rounds),
        "-games", "2",
        "-repeat",
        "-recover",
        "-pgnout", "tournament.pgn",
    ]

    if args.openings:
        cmd.extend(["-openings", f"file={args.openings}", "format=epd", "order=random"])

    print(f"Running: {' '.join(cmd)}")
    print(f"Playing {args.rounds} game pairs ({args.rounds * 2} total games)...")
    print()

    try:
        result = subprocess.run(cmd, capture_output=False)
        sys.exit(result.returncode)
    except FileNotFoundError:
        print("Error: cutechess-cli not found. Install it and add to PATH.")
        sys.exit(1)

if __name__ == "__main__":
    main()
