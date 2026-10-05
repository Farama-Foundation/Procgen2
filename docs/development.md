# Development

This page covers building Procgen2 from source and where to find the guides for implementing games.

## Building the games

The games depend on [SDL3](https://www.libsdl.org/) and the [SDL3_image](https://github.com/libsdl-org/SDL_image) extension. On Linux, both are likely available from your package manager. The games are built with [CMake](https://cmake.org/).

To build every game, run the following from the repository root:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

Each game is compiled to a shared library under `build/games/<game>/` (`.so` on Linux, `.dylib` on macOS, `.dll` on Windows).

## Loading a game from Python

Compiled games are loaded from Python with `CEnv`, which exposes them as Gymnasium environments:

```python
from cenv.cenv import CEnv

env = CEnv("build/games/caveflyer/libCaveFlyer.so", options={"width": 512, "height": 512})
obs, info = env.reset()
```

See `interactive_viewer.py` in the repository root for a complete example that plays a game with the keyboard.

## CEnv

CEnv is the interface that lets environments be implemented in a compiled language and called from Python. Any language that can produce a shared library using the C calling convention will work. See the [CEnv usage guide](https://github.com/Farama-Foundation/Procgen2/blob/main/cenv/USAGE_GUIDE.md) for the full interface.

## Implementing a game

New games should roughly mimic the structure and style of the Coinrun implementation, which uses an Entity Component System (ECS). See the [development guide](https://github.com/Farama-Foundation/Procgen2/blob/main/DEV_GUIDE.md) for a walkthrough.
