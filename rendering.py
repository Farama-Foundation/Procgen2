import gymnasium as gym
import procgen2


env = gym.make("procgen2/Maze-v0", render_mode="human")
obs, _ = env.reset()
while True:
    obs, r, term, trunc, _ = env.step(env.action_space.sample())
    env.render()   # opens SDL window, caps at 15 FPS
    if term or trunc: break
