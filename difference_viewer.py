import gym
import numpy as np
from cenv.cenv import CEnv
import pygame
import pygame.surfarray
import procgen
import time

env_original = gym.make("procgen:procgen-coinrun-v0", render_mode="rgb_array")

env = CEnv("games/coinrun/build/libCoinRun.so")

obs_original = env_original.reset()
obs, _ = env.reset(options={ "width": 512, "height": 512 })

screen = pygame.display.set_mode((512, 512))

running = True

start_time = pygame.time.get_ticks()

ks_prev = pygame.key.get_pressed()

while running:
    end_time = pygame.time.get_ticks()
    dt = end_time - start_time
    start_time = end_time

    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

    ks = pygame.key.get_pressed()

    if ks[pygame.K_q]:
        running = False

    force_reset = False

    if ks[pygame.K_r] and not ks_prev[pygame.K_r]:
        force_reset = True

    action = 0

    if ks[pygame.K_a]:
        action = 0
    elif ks[pygame.K_d]:
        action = 6
    else:
        action = 4

    if action != 4:
        if ks[pygame.K_w]:
            action += 2
        elif ks[pygame.K_s]:
            action += 0
        else:
            action += 1
    else:
        if ks[pygame.K_w]:
            action = 5
        elif ks[pygame.K_s]:
            action = 3
        else:
            action = 4

    frame_original = env_original.render(mode="rgb_array")
    frame = env.render()

    obs_original, _, _, _ = env_original.step(action)
    obs, reward, term, trunc, _ = env.step(action)

    if term or force_reset:
        obs_original = env_original.reset()
        obs, _ = env.reset()

    surf = None

    if ks[pygame.K_0]:
        surf = pygame.surfarray.make_surface(np.swapaxes(frame_original, 0, 1))
    elif ks[pygame.K_1]:
        surf = pygame.surfarray.make_surface(np.swapaxes(frame.reshape((512, 512, 3)), 0, 1))
    else:
        difference_image = (((frame / 255.0 - frame_original / 255.0) * 0.5 + 0.5) * 255.0).astype(np.uint8)
        surf = pygame.surfarray.make_surface(np.swapaxes(difference_image, 0, 1))

    surf = pygame.transform.scale(surf, (512, 512))

    screen.blit(surf, (0, 0))

    ks_prev = ks

    pygame.display.flip()

    pygame.time.delay(max(0, 1000 // 20 - dt))
