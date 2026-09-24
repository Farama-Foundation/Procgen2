"""
Smoke-test: CoinRun inside the Gymnasium API.

Run from the repo root:
    python test_coinrun.py
"""

import sys
import os

# Make sure both procgen2 and cenv packages are importable from the repo root
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import numpy as np
import gymnasium as gym
import procgen2  # triggers registration

def main():
    print("=== Making env via gym.make ===")
    env = gym.make("procgen2/CoinRun-v0")

    print(f"observation_space : {env.observation_space}")
    print(f"action_space      : {env.action_space}")

    print("\n=== reset() ===")
    obs, info = env.reset(seed=42)
    print(f"obs keys  : {list(obs.keys())}")
    print(f"screen shape : {obs['screen'].shape}  dtype={obs['screen'].dtype}")
    print(f"info      : {info}")

    print("\n=== 200 random steps ===")
    total_reward = 0.0
    episodes = 0
    for i in range(200):
        # Sample from the 'action' MultiDiscrete space
        action_sample = env.action_space.sample()
        obs, reward, terminated, truncated, info = env.step(action_sample)
        total_reward += reward

        if terminated or truncated:
            episodes += 1
            obs, info = env.reset()

    print(f"Steps done : 200")
    print(f"Episodes   : {episodes}")
    print(f"Total reward: {total_reward}")
    print(f"Last screen shape: {obs['screen'].shape}")

    print("\n=== render() ===")
    frame = env.render()
    if frame is not None:
        print(f"Frame shape: {frame.shape}  dtype={frame.dtype}")
    else:
        print("No frame returned (render_mode not set).")

    env.close()
    print("\nAll checks passed!")

if __name__ == "__main__":
    main()
