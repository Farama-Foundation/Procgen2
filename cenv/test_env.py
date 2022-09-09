import gym
import numpy as np
from cenv import CEnv

env = CEnv("./test.so")

print(env.observation_space)

obs, info = env.reset()

for i in range(100):
    print(obs)

    obs, reward, term, trunc, info = env.step(0)

    if term:
        print("Resetting...")
        obs, info = env.reset()

    print(reward)


