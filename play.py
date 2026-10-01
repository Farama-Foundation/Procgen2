"""
Play any ProcGen2 game from the keyboard.

A pygame window is the display. The C++ env runs in rgb_array mode so we
do not mix SDL2 (pygame) and SDL3 (the game) windows — that pair silently
fails to show a window on macOS.

Usage (from the repo root):
    python play.py CoinRun
    python play.py starpilot
    python play.py --list

Shared keys:
  Arrow keys / WASD  — move
  R                  — reset
  Q / Escape         — quit

Per-game extras are printed when the game starts.
"""

import argparse
import sys
import os
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import pygame
import numpy as np
import procgen2  # noqa: F401  — registers env ids
import gymnasium as gym


# scheme:
#   platform  — 3×3 pad, up/space = jump, down = drop through
#   grid      — same 3×3 ids; only cardinals do anything in Chaser
#   shooter   — 3×3 pad + space = fire (action 9)
#   starpilot — 3×3 with up/down flipped + space/Z fire left/right
GAMES = {
    "coinrun":   ("procgen2/CoinRun-v0",   "platform"),
    "climber":   ("procgen2/Climber-v0",   "platform"),
    "jumper":    ("procgen2/Jumper-v0",    "platform"),
    "maze":      ("procgen2/Maze-v0",      "grid"),
    "chaser":    ("procgen2/Chaser-v0",    "grid"),
    "bossfight": ("procgen2/BossFight-v0", "shooter"),
    "caveflyer": ("procgen2/CaveFlyer-v0", "shooter"),
    "starpilot": ("procgen2/StarPilot-v0", "starpilot"),
}

SCHEME_HELP = {
    "platform": "Up / Space = jump    Down = drop through a platform",
    "grid":     "Arrow keys move one cell (diagonals ignored on Chaser)",
    "shooter":  "Space = fire",
    "starpilot": "Space = fire right    Z / Left-Shift = fire left",
}


def _standard_action(dx, dy, fire):
    """ProcGen 3×3 pad: 0–8 move, 9 fire.

    col = left/none/right, row = down/none/up
      0 3 6
      1 4 7
      2 5 8
    """
    if fire:
        return 9
    col = dx + 1
    row = 1 - dy  # keyboard up (dy=-1) → row 2 → action 5
    return col * 3 + row


def _starpilot_action(dx, dy, fire):
    """StarPilot: avy = (action % 3) - 1, so keyboard up must be row 0."""
    if fire:
        return 8 + fire  # 9 = right, 10 = left
    col = dx + 1
    row = dy + 1  # keyboard up (dy=-1) → row 0
    return col * 3 + row


def resolve_game(name):
    key = name.strip()
    if key.startswith("procgen2/"):
        key = key[len("procgen2/"):]
    if key.endswith("-v0"):
        key = key[:-3]
    key = key.lower().replace("_", "").replace("-", "")
    if key not in GAMES:
        known = ", ".join(sorted(GAMES))
        raise SystemExit(f"Unknown game {name!r}. Choose one of: {known}")
    return key, GAMES[key]


def frame_to_surface(frame, size):
    """Convert CEnv rgb_array (H, W, 3) into a pygame surface of `size`."""
    h, w = frame.shape[:2]
    surf = pygame.image.frombytes(np.ascontiguousarray(frame).tobytes(), (w, h), "RGB")
    if surf.get_size() != size:
        surf = pygame.transform.scale(surf, size)
    return surf


def parse_args():
    parser = argparse.ArgumentParser(description="Play a ProcGen2 game from the keyboard.")
    parser.add_argument(
        "game",
        nargs="?",
        default="coinrun",
        help="Game name (default: coinrun). Use --list to see options.",
    )
    parser.add_argument("--list", action="store_true", help="Print available games and exit.")
    parser.add_argument("--seed", type=int, default=None, help="Reset seed (default: time-based).")
    parser.add_argument("--scale", type=int, default=768, help="Window size in pixels (default: 768).")
    return parser.parse_args()


def main():
    args = parse_args()
    if args.list:
        print("Available games:")
        for name, (env_id, scheme) in GAMES.items():
            print(f"  {name:<12} {env_id:<28} {SCHEME_HELP[scheme]}")
        return

    key, (env_id, scheme) = resolve_game(args.game)
    build_action = _starpilot_action if scheme == "starpilot" else _standard_action

    env = gym.make(env_id, render_mode="rgb_array", disable_env_checker=True)
    seed = args.seed if args.seed is not None else (int(time.time()) & 0xFFFF)
    obs, _ = env.reset(seed=seed)

    pygame.init()
    window_size = (args.scale, args.scale)
    screen = pygame.display.set_mode(window_size)
    pygame.display.set_caption(key)

    clock = pygame.time.Clock()
    running = True
    total_reward = 0.0
    step_count = 0
    episode = 0

    print(f"{env_id}  (seed={seed})")
    print("  Arrow keys / WASD = move    R = reset    Q/Esc = quit")
    print(f"  {SCHEME_HELP[scheme]}")

    while running:
        dx, dy, fire = 0, 0, 0

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            if event.type == pygame.KEYDOWN:
                if event.key in (pygame.K_q, pygame.K_ESCAPE):
                    running = False
                if event.key == pygame.K_r:
                    obs, _ = env.reset()
                    total_reward = 0.0
                    step_count = 0
                    episode += 1
                    print(f"  Manual reset  (episode {episode})")

        keys = pygame.key.get_pressed()

        if keys[pygame.K_LEFT]  or keys[pygame.K_a]: dx -= 1
        if keys[pygame.K_RIGHT] or keys[pygame.K_d]: dx += 1
        if keys[pygame.K_UP]    or keys[pygame.K_w]: dy -= 1
        if keys[pygame.K_DOWN]  or keys[pygame.K_s]: dy += 1

        if scheme == "platform" and keys[pygame.K_SPACE]:
            dy = -1
        elif scheme == "shooter" and keys[pygame.K_SPACE]:
            fire = 1
        elif scheme == "starpilot":
            if keys[pygame.K_SPACE]:
                fire = 1
            if keys[pygame.K_z] or keys[pygame.K_LSHIFT]:
                fire = 2

        action = {"action": np.array([build_action(dx, dy, fire)], dtype=np.int32)}
        obs, reward, terminated, truncated, info = env.step(action)

        frame = env.render()
        if frame is not None:
            screen.blit(frame_to_surface(frame, window_size), (0, 0))
            pygame.display.flip()

        total_reward += reward
        step_count += 1

        if reward != 0:
            print(f"  step {step_count:4d}  reward={reward:+.1f}  total={total_reward:.1f}")

        if terminated or truncated:
            result = "WIN" if terminated and reward > 0 else ("DEAD" if terminated else "TIMEOUT")
            print(f"  Episode {episode} over — {result}  "
                  f"steps={step_count}  total_reward={total_reward:.1f}")
            obs, _ = env.reset()
            total_reward = 0.0
            step_count = 0
            episode += 1

        clock.tick(15)

    env.close()
    pygame.quit()
    print("Bye!")


if __name__ == "__main__":
    main()
