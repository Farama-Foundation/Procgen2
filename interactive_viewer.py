import gym
import numpy as np
from cenv.cenv import CEnv
import pygame
import pygame.surfarray
import time

width = 512
height = 512

env = CEnv("games/jumper/build/libJumper.so", options={ "width": width, "height": height })

print(env.observation_space)

obs, info = env.reset()

screen = pygame.display.set_mode((width, height))

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

    obs, reward, term, trunc, info = env.step(action)

    if term or force_reset:
        obs, info = env.reset()

    image = env.render()

    surf = pygame.surfarray.make_surface(np.swapaxes(image, 0, 1))

    screen.blit(surf, (0, 0))

    ks_prev = ks

    pygame.display.flip()

    pygame.time.delay(max(0, 1000 // 20 - dt))
