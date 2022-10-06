import gym
import numpy as np
from cenv.cenv import CEnv
import cv2

env = CEnv("games/coinrun/build/libCoinRun.so")

print(env.observation_space)

obs, info = env.reset()

cv2.namedWindow("Debug", cv2.WINDOW_NORMAL)

while True:
    obs, reward, term, trunc, info = env.step(0)

    obs_screen = obs[b"screen"].copy().reshape((64, 64, 3))

    img = env.render()

    cv2.imshow("Debug", cv2.cvtColor(obs_screen, cv2.COLOR_RGB2BGR))

    if cv2.waitKey(17) & 0xFF == ord('q'):
        break

    if term:
        print("Resetting...")
        obs, info = env.reset()

