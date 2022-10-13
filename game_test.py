import gym
import numpy as np
from cenv.cenv import CEnv
import cv2
import time

env = CEnv("games/coinrun/build/libCoinRun.so")

print(env.observation_space)

obs, info = env.reset()

average = 0.0

cv2.namedWindow("Debug", cv2.WINDOW_NORMAL)

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

    obs_screen = obs["screen"].copy().reshape((64, 64, 3))
    
    cv2.imshow("Debug", cv2.cvtColor(obs_screen, cv2.COLOR_RGB2BGR))

    if cv2.waitKey(1) & 0x0f == ord('q'):
        break

    if term:
        print("Resetting...")
        obs, info = env.reset()

