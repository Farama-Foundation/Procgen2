import gym
import numpy as np
from cenv.cenv import CEnv
import time

env = CEnv("games/coinrun/build/libCoinRun.so")

print(env.observation_space)

obs, info = env.reset()

for i in range(10000):
    start = time.perf_counter()
    obs, reward, term, trunc, info = env.step(np.random.randint(0, 16))
    end = time.perf_counter()

    if i % 100 == 0:
        fps = 1.0 / (end - start)
        print(fps)

    obs_screen = obs["screen"].copy().reshape((64, 64, 3))

    #img = env.render()

    if term:
        print("Resetting...")
        obs, info = env.reset()

