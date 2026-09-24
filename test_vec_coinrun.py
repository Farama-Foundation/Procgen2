"""
Vector-environment smoke test — 16 parallel CoinRun instances.

Run from the repo root:
    python test_vec_coinrun.py
"""

import sys
import os
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import numpy as np
import gymnasium as gym
import procgen2  # registers procgen2/CoinRun-v0

NUM_ENVS  = 16
NUM_STEPS = 200


def main():
    print(f"=== Creating {NUM_ENVS} parallel CoinRun envs (SyncVectorEnv) ===")
    t0 = time.perf_counter()
    vec = gym.make_vec(
        "procgen2/CoinRun-v0",
        num_envs=NUM_ENVS,
        vectorization_mode="sync",
    )
    print(f"  make_vec took {time.perf_counter() - t0:.2f}s")

    print(f"\nobservation_space : {vec.observation_space}")
    print(f"action_space      : {vec.action_space}")

    # ------------------------------------------------------------------ reset
    print(f"\n=== reset(seed=0) ===")
    obs, infos = vec.reset(seed=0)
    screen = obs["screen"]          # shape (N, 12288)
    print(f"  screen batch shape : {screen.shape}  dtype={screen.dtype}")

    # Each env gets a different RNG seed → screens must NOT all be identical
    all_same = np.all(screen == screen[0:1])
    print(f"  All {NUM_ENVS} screens identical? {all_same}  (should be False)")
    assert not all_same, "All screens are identical — envs are sharing global state!"

    num_unique = len({screen[i].tobytes() for i in range(NUM_ENVS)})
    print(f"  Unique screens: {num_unique} / {NUM_ENVS}")

    # ------------------------------------------------------------------ step
    print(f"\n=== {NUM_STEPS} vectorised steps ===")
    total_reward = np.zeros(NUM_ENVS)
    episodes_done = np.zeros(NUM_ENVS, dtype=int)

    t1 = time.perf_counter()
    for step in range(NUM_STEPS):
        actions = vec.action_space.sample()
        obs, rewards, terminated, truncated, infos = vec.step(actions)
        total_reward += rewards
        episodes_done += (terminated | truncated).astype(int)

    elapsed = time.perf_counter() - t1
    sps = NUM_ENVS * NUM_STEPS / elapsed

    print(f"  Elapsed        : {elapsed:.2f}s")
    print(f"  Steps/s (total): {sps:,.0f}  ({sps / NUM_ENVS:,.0f} per env)")
    print(f"  Episodes done  : {episodes_done.tolist()}")
    print(f"  Total reward   : {total_reward.tolist()}")

    # ------------------------------------------------------------------ render
    # render() is not supported on SyncVectorEnv at the vector level,
    # but we can verify the inner envs still render individually.
    inner_env = vec.envs[0]
    frame = inner_env.render()
    if frame is not None:
        print(f"\n  Inner env[0] render shape: {frame.shape}")

    # ------------------------------------------------------------------ close
    vec.close()
    print("\nAll checks passed!")


if __name__ == "__main__":
    main()
