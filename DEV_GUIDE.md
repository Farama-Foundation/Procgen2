# Development Guide

This guide serves to help describe how to implement a game for ProcGen2. For a guide on just CEnv, see its corresponding [usage guide](./cenv/USAGE_GUIDE.md).
Due to CEnv, ProcGen2 games can be developed mostly independently. However, for readability and maintainibility, we prefer if games are implemented in a similar manner.
The [coinrun](./games/coinrun/) environment should be roughly mimicked in terms of systems, programming language (C++), and programming style. The following describes some systems that exist in coinrun to aid implementation in other games (when appropriate).

## Compiling

Coinrun depends only on [SDL2](https://www.libsdl.org/) and the [SDL2_image](https://github.com/libsdl-org/SDL_image) extension. Make sure these are installed. If on Linux, these can likely be found in your package manager.

Coinrun can be compiled using [CMake](https://cmake.org/). From the directory containing the [CMakeLists.txt](./games/coinrun/CMakeLists.txt), create a directory called "build" and enter it:

```
mkdir build
cd build
```

Then call CMake:

```
cmake ..
```

CMake will then generate the build files for your operating system. Use these to build the game.

## ECS

Coinrun uses an Entity Component System (ECS) to help simplify game logic. The ECS in coinrun is a modified version of the system [from this article](https://austinmorlan.com/posts/entity_component_system/).
Please give it a read if you are unfamiliar with the basic concepts of ECS.

In coinrun, most components are implemented in [common_components.h](./games/coinrun/common_components.h). As is expected in ECS, components are data-only structures.
Most systems are implemented in [common_systems.h](./games/coinrun/common_systems.h) and [common_systems.cpp](./games/coinrun/common_systems.h). Included are systems for controlling mobs (enemies), the agent/player, particles, etc.
The tilemap system is implemented in its own files though, [tilemap.h](./games/coinrun/tilemap.h) and [tilemap.cpp](./games/coinrun/tilemap.cpp).

[coinrun.cpp](./games/coinrun/coinrun.cpp) contains the implementation of the CEnv interface but also registers ECS components and sets up ECS systems.

For example, the various components used by coinrun are registered in the cenv_make function in that file.

```c++
// Register components
c.register_component<Component_Transform>();
c.register_component<Component_Collision>();
c.register_component<Component_Dynamics>();
c.register_component<Component_Sprite>();
c.register_component<Component_Animation>();
c.register_component<Component_Hazard>();
c.register_component<Component_Goal>();
c.register_component<Component_Mob_AI>();
c.register_component<Component_Agent>(); // Player
c.register_component<Component_Particles>();
```

Note that throughout the coinrun code, the ECS is accessed through a global Coordinator variable "c". The Coordinator contains accessors for all entities, components, and systems.

Systems are created in the same file just below the component registration. For example, for the mob AI:

```c++
// Mob AI setup
mob_ai = c.register_system<System_Mob_AI>();
Signature mob_ai_signature;
mob_ai_signature.set(c.get_component_type<Component_Mob_AI>()); // Operate only on mobs
c.set_system_signature<System_Mob_AI>(mob_ai_signature);
```

It is important to set the signature appropriately. Signatures here are implemented as C++ bitsets, and define which components a system operates on. This is explained in the ECS article mentioned earlier.

Systems are run in the cenv_step function. For mob AI:

```c++
mob_ai->update(dt);
```

## Asset Manager

To avoid duplicate asset loading, coinrun uses an asset manager. This is implemented in [asset_manager.h](./games/coinrun/asset_manager.h).
Asset managers are per-asset-type, and asset managers for common asset types are defined as globals in [common_assets.h](./games/coinrun/common_assets.h) and [common_assets.cpp](./games/coinrun/common_assets.cpp).

## Renderer

Coinrun uses SDL2 software rendering, any implementation of ProcGen2 games should use SDL2's software rendering (GPU acceleration for such simple graphics is actually slower).
Due to some intricacies with how SDL2 works, it is recommended to use the sprite rendering system included in coinrun, as defined in [renderer.h](./games/coinrun/renderer.h) and [renderer.cpp](./games/coinrun/renderer.cpp).
The included renderer avoids overdraw, handles upscaling jitter, and switching between observation and viewer rendering.
It defines a global "gr" (global renderer) that should be used for rendering:

```c++
gr.render_texture(...)
```

## Helpers

Finally, [helpers.h](./games/coinrun/helpers.h) and [helpers.cpp](./games/coinrun/helpers.cpp) implement a few helpful structures and functions used throughout the code, such as collision detection.
