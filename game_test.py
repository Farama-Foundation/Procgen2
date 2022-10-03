import gym
import numpy as np
from cenv.cenv import CEnv

env = CEnv("games/coinrun/build/libCoinRun.so")

print(env.observation_space)

obs, info = env.reset()

while True:
    obs, reward, term, trunc, info = env.step(0)

    img = env.render()

    if term:
        print("Resetting...")
        obs, info = env.reset()

