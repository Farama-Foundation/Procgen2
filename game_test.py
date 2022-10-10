import gym
import numpy as np
from cenv.cenv import CEnv
import cv2
import time

env = CEnv("games/coinrun/build/libCoinRun.so")

print(env.observation_space)

obs, info = env.reset()

average = 0.0

for i in range(1000000):
    start = time.perf_counter()
    obs, reward, term, trunc, info = env.step(np.random.randint(0, 16))
    end = time.perf_counter()
    
    fps = 1.0 / (end - start)

    if average == 0.0:
        average = fps
    else:
        average += 0.01 * (fps - average)

    if i % 1000 == 0:
        print(average)

    if term:
        print("Resetting...")
        obs, info = env.reset()

