import gym
import numpy as np
from cenv import CEnv
import time

env = CEnv("./test.so")

obs, info = env.reset()

for i in range(100):
    print(obs)

    obs, reward, term, trunc, info = env.step({ "blah123": np.ones(10, dtype=np.int32) * 123 })

    time.sleep(0.01)

    if term:
        print("Resetting...")
        obs, info = env.reset()

    print(reward)


