#!/usr/bin/env python3
"""
MCTSlite Elo Evaluation Harness

Runs self-play tournaments between MCTSlite at different simulation counts
and the main C64Chess engine (alpha-beta) to estimate Elo ratings.

Uses UCI protocol over subprocess pipes. No external dependencies needed.

Usage:
    python3 scripts/mcts_elo_eval.py [--games 50] [--engine build/c64chess.exe]
"""

import argparse
import subprocess
import sys
import os
import math
import time
import re
from collections import defaultdict


class UCIEngine:
    """Manages a UCI engine subprocess."""

    def __init__(self, path, name=None, options=None):
        self.path = path
        self.name = name or os.path.basename(path)
        self.options = options or {}
        self.proc = None

    def start(self):
        self.proc = subprocess.Popen(
            [self.path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        self._send("uci")
        self._wait_for("uciok")
        for key, val in self.options.items():
            self._send(f"setoption name {key} value {val}")
        self._send("isready")
        self._wait_for("readyok")

    def stop(self):
        if self.proc and self.proc.poll() is None:
            try:
                self._send("quit")
                self.proc.wait(timeout=2)
            except Exception:
                self.proc.kill()

    def new_game(self):
        self._send("ucinewgame")
        self._send("isready")
        self._wait_for("readyok")

    def set_position(self, moves):
        if moves:
            self._send(f"position startpos moves {' '.join(moves)}")
        else:
            self._send("position startpos")

    def go(self, movetime=None, depth=None, nodes=None):
        cmd = "go"
        if movetime is not None:
            cmd += f" movetime {movetime}"
        if depth is not None:
            cmd += f" depth {depth}"
        if nodes is not None:
            cmd += f" nodes {nodes}"
        self._send(cmd)
        return self._wait_for_bestmove()

    def _send(self, cmd):
        if self.proc and self.proc.poll() is None:
            self.proc.stdin.write(cmd + "\n")
            self.proc.stdin.flush()

    def _wait_for(self, token, timeout=10):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.proc.poll() is not None:
                return None
            line = self.proc.stdout.readline().strip()
            if line == token:
                return line
        return None

    def _wait_for_bestmove(self, timeout=60):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.proc.poll() is not None:
                return None
            line = self.proc.stdout.readline().strip()
            if line.startswith("bestmove"):
                parts = line.split()
                if len(parts) >= 2:
                    return parts[1]
        return None


def play_game(white, black, max_moves=200, movetime=100, depth=None, nodes=None):
    """Play one game. Returns 1.0 for white win, 0.0 for black win, 0.5 for draw."""
    white.new_game()
    black.new_game()

    moves = []
    for ply in range(max_moves * 2):
        engine = white if ply % 2 == 0 else black

        engine.set_position(moves)
        move = engine.go(movetime=movetime, depth=depth, nodes=nodes)

        if not move or move == "0000" or move == "(none)":
            # Current side has no move = lost or stalemate
            # Treat as loss for current side
            return 0.0 if ply % 2 == 0 else 1.0

        moves.append(move)

        # Basic draw detection: if the move list gets very long
        if len(moves) > max_moves * 2:
            return 0.5

    return 0.5  # max moves reached = draw


def elo_diff(wins, losses, draws):
    """Compute Elo difference from score percentage."""
    total = wins + losses + draws
    if total == 0:
        return 0.0
    score = (wins + 0.5 * draws) / total
    if score <= 0.0:
        return -999.0
    if score >= 1.0:
        return 999.0
    return -400.0 * math.log10(1.0 / score - 1.0)


def elo_error_margin(wins, losses, draws, confidence=0.95):
    """Approximate Elo error margin (95% confidence)."""
    total = wins + losses + draws
    if total < 2:
        return float('inf')
    score = (wins + 0.5 * draws) / total
    if score <= 0.0 or score >= 1.0:
        return float('inf')
    # Variance of Bernoulli-like score
    var = (wins * (1 - score)**2 + losses * score**2 + draws * (0.5 - score)**2) / total
    se = math.sqrt(var / total)
    if se < 1e-9:
        return float('inf')
    # z-score for 95% CI
    z = 1.96
    lo = max(0.001, score - z * se)
    hi = min(0.999, score + z * se)
    elo_lo = -400.0 * math.log10(1.0 / lo - 1.0)
    elo_hi = -400.0 * math.log10(1.0 / hi - 1.0)
    return (elo_hi - elo_lo) / 2.0


def run_match(engine1_path, engine2_path, e1_name, e2_name,
              e1_options, e2_options, num_games, movetime, depth):
    """Run a match between two engines, alternating colors."""
    wins = [0, 0]  # [engine1_wins, engine2_wins]
    draws = 0

    for game_num in range(num_games):
        # Alternate who plays white
        if game_num % 2 == 0:
            w_path, w_name, w_opts = engine1_path, e1_name, e1_options
            b_path, b_name, b_opts = engine2_path, e2_name, e2_options
            e1_is_white = True
        else:
            w_path, w_name, w_opts = engine2_path, e2_name, e2_options
            b_path, b_name, b_opts = engine1_path, e1_name, e1_options
            e1_is_white = False

        white = UCIEngine(w_path, w_name, w_opts)
        black = UCIEngine(b_path, b_name, b_opts)

        try:
            white.start()
            black.start()
            result = play_game(white, black, movetime=movetime, depth=depth)
        except Exception as e:
            print(f"  Game {game_num+1}: ERROR - {e}")
            result = 0.5
        finally:
            white.stop()
            black.stop()

        # Map result to engine1's perspective
        if e1_is_white:
            e1_score = result
        else:
            e1_score = 1.0 - result

        if e1_score == 1.0:
            wins[0] += 1
            result_str = f"{e1_name} wins"
        elif e1_score == 0.0:
            wins[1] += 1
            result_str = f"{e2_name} wins"
        else:
            draws += 1
            result_str = "draw"

        total = game_num + 1
        score_pct = (wins[0] + 0.5 * draws) / total * 100
        print(f"  Game {total}/{num_games}: {result_str}  "
              f"[{e1_name} {wins[0]}W-{wins[1]}L-{draws}D = {score_pct:.0f}%]")

    return wins[0], wins[1], draws


def main():
    parser = argparse.ArgumentParser(
        description="MCTSlite Elo Evaluation - test MCTS at various sim counts")
    parser.add_argument("--games", type=int, default=30,
                        help="Games per matchup (default: 30)")
    parser.add_argument("--engine", default="build/c64chess.exe",
                        help="Path to main engine (alpha-beta)")
    parser.add_argument("--mcts", default="build/mctslite.exe",
                        help="Path to MCTS engine")
    parser.add_argument("--movetime", type=int, default=200,
                        help="Movetime in ms for alpha-beta engine (default: 200)")
    parser.add_argument("--depth", type=int, default=0,
                        help="Depth limit for alpha-beta (0=use movetime)")
    parser.add_argument("--sims", default="0,100,200,400,800",
                        help="Comma-separated sim counts to test (default: 0,100,200,400,800)")
    parser.add_argument("--mcts-vs-mcts", action="store_true",
                        help="Also run MCTS vs MCTS at adjacent sim counts")
    args = parser.parse_args()

    if not os.path.exists(args.engine):
        print(f"Error: {args.engine} not found. Run 'make pc' first.")
        sys.exit(1)
    if not os.path.exists(args.mcts):
        print(f"Error: {args.mcts} not found. Run 'make mcts' first.")
        sys.exit(1)

    sim_counts = [int(s) for s in args.sims.split(",")]
    depth = args.depth if args.depth > 0 else None

    print("=" * 65)
    print("MCTSlite Elo Evaluation")
    print("=" * 65)
    print(f"  Alpha-beta engine: {args.engine}")
    print(f"    movetime={args.movetime}ms" + (f" depth={depth}" if depth else ""))
    print(f"  MCTS engine: {args.mcts}")
    print(f"  Simulation counts: {sim_counts}")
    print(f"  Games per matchup: {args.games}")
    print("=" * 65)
    print()

    results = {}

    # MCTS vs Alpha-Beta at each sim count
    for sims in sim_counts:
        mcts_name = f"MCTS-{sims}"
        ab_name = "C64Chess-AB"

        print(f"--- {mcts_name} vs {ab_name} ({args.games} games) ---")

        mcts_opts = {"Simulations": str(sims)}

        w, l, d = run_match(
            args.mcts, args.engine,
            mcts_name, ab_name,
            mcts_opts, {},
            args.games, args.movetime, depth
        )

        elo = elo_diff(w, l, d)
        margin = elo_error_margin(w, l, d)
        results[sims] = (w, l, d, elo, margin)

        print(f"  Result: +{w} -{l} ={d}  Elo: {elo:+.0f} +/- {margin:.0f}")
        print()

    # MCTS vs MCTS (adjacent sim counts)
    mcts_vs_mcts_results = {}
    if args.mcts_vs_mcts and len(sim_counts) > 1:
        for i in range(len(sim_counts) - 1):
            s1, s2 = sim_counts[i], sim_counts[i + 1]
            name1, name2 = f"MCTS-{s1}", f"MCTS-{s2}"

            print(f"--- {name1} vs {name2} ({args.games} games) ---")

            w, l, d = run_match(
                args.mcts, args.mcts,
                name1, name2,
                {"Simulations": str(s1)}, {"Simulations": str(s2)},
                args.games, args.movetime, depth
            )

            elo = elo_diff(w, l, d)
            margin = elo_error_margin(w, l, d)
            mcts_vs_mcts_results[(s1, s2)] = (w, l, d, elo, margin)

            print(f"  Result: +{w} -{l} ={d}  Elo: {elo:+.0f} +/- {margin:.0f}")
            print()

    # Summary table
    print()
    print("=" * 65)
    print("SUMMARY: MCTSlite vs C64Chess Alpha-Beta")
    print("=" * 65)
    print(f"{'Sims':>6}  {'W':>4} {'L':>4} {'D':>4}  {'Score%':>7}  {'Elo':>8}  {'95% CI':>10}")
    print("-" * 65)
    for sims in sim_counts:
        w, l, d, elo, margin = results[sims]
        total = w + l + d
        score_pct = (w + 0.5 * d) / total * 100 if total > 0 else 0
        print(f"{sims:>6}  {w:>4} {l:>4} {d:>4}  {score_pct:>6.1f}%  {elo:>+7.0f}  +/- {margin:>5.0f}")
    print("-" * 65)

    if mcts_vs_mcts_results:
        print()
        print("MCTS vs MCTS (scaling with simulation count):")
        print(f"{'Matchup':>16}  {'W':>4} {'L':>4} {'D':>4}  {'Elo':>8}")
        print("-" * 50)
        for (s1, s2), (w, l, d, elo, margin) in mcts_vs_mcts_results.items():
            print(f"  {s1:>5} vs {s2:<5}  {w:>4} {l:>4} {d:>4}  {elo:>+7.0f}")

    print()
    print("Note: Elo is relative to C64Chess alpha-beta at "
          f"movetime={args.movetime}ms")
    print("Negative Elo = MCTS is weaker than alpha-beta.")


if __name__ == "__main__":
    main()
