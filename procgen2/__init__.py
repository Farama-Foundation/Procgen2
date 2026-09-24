"""
ProcGen2 — Gymnasium-compatible environments.

Registration happens automatically on import:

    import gymnasium as gym
    import procgen2

    env = gym.make("procgen2/CoinRun-v0")
    env = gym.make("procgen2/Climber-v0")
"""

import os
import gymnasium as gym
from gymnasium.utils.ezpickle import EzPickle

# Resolve the shared-library path relative to this file's location.
# The library is expected at games/<name>/build/lib<Name>.<ext>
_HERE = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.dirname(_HERE)


def _lib_path(game_dir: str, lib_name: str) -> str:
    """Return the platform-appropriate shared library path."""
    import sys

    if sys.platform == "darwin":
        ext = "dylib"
    elif sys.platform == "win32":
        ext = "dll"
    else:
        ext = "so"

    return os.path.join(_ROOT, "games", game_dir, "build", f"lib{lib_name}.{ext}")


def _make_env(game_dir, lib_name, render_mode, seed, options):
    """Shared factory: load library, build CEnv, raise readable error if missing."""
    from cenv.cenv import CEnv

    lib = _lib_path(game_dir, lib_name)
    if not os.path.isfile(lib):
        raise FileNotFoundError(
            f"{lib_name} shared library not found at:\n  {lib}\n"
            f"Build it with:\n"
            f"  cmake -S games/{game_dir} -B games/{game_dir}/build -DCMAKE_BUILD_TYPE=Release\n"
            f"  cmake --build games/{game_dir}/build --parallel"
        )

    init_options = dict(options)
    if seed is not None:
        init_options["seed"] = seed

    return CEnv(lib, render_mode=render_mode, options=init_options or None)


# ---------------------------------------------------------------------------
# Base mixin — shared Gymnasium boilerplate for every game wrapper
# ---------------------------------------------------------------------------

class _ProcGen2Env(gym.Env, EzPickle):
    """
    Thin gymnasium.Env + EzPickle wrapper around a CEnv shared-library instance.

    EzPickle makes the environment picklable so it can be used with
    multiprocessing-based vector environments (AsyncVectorEnv) without any
    additional serialisation logic.
    """

    metadata = {"render_modes": ["human", "single_rgb_array"], "render_fps": 15}

    # Subclasses must define:
    #   _GAME_DIR  : str   e.g. "coinrun"
    #   _LIB_NAME  : str   e.g. "CoinRun"

    def __init__(self, render_mode=None, seed=None, **options):
        # EzPickle stores these exact args so __reduce__ can reconstruct us.
        EzPickle.__init__(self, render_mode=render_mode, seed=seed, **options)

        self._env = _make_env(
            self._GAME_DIR, self._LIB_NAME, render_mode, seed, options
        )
        self.observation_space = self._env.observation_space
        self.action_space      = self._env.action_space
        self.render_mode       = render_mode

    # ------------------------------------------------------------------
    # Gymnasium interface
    # ------------------------------------------------------------------

    def reset(self, *, seed=None, options=None):
        super().reset(seed=seed)   # sets self._np_random (required by check_env)
        return self._env.reset(seed=seed, options=options)

    def step(self, action):
        return self._env.step(action)

    def render(self):
        return self._env.render()

    def close(self):
        self._env.close()


# ---------------------------------------------------------------------------
# Per-game concrete classes
# ---------------------------------------------------------------------------

class _CoinRunEnv(_ProcGen2Env):
    """
    CoinRun — classic platformer. Reach the coin, avoid hazards.

    Observation space : Dict{ "screen": Box(0, 255, (12288,), uint8) }  (64×64×3 flat)
    Action space      : Dict{ "action": MultiDiscrete([15]) }
    """
    _GAME_DIR = "coinrun"
    _LIB_NAME = "CoinRun"


class _ClimberEnv(_ProcGen2Env):
    """
    Climber — collect all stars on a vertical platformer map.

    Observation space : Dict{ "screen": Box(0, 255, (12288,), uint8) }  (64×64×3 flat)
    Action space      : Dict{ "action": MultiDiscrete([15]) }
    """
    _GAME_DIR = "climber"
    _LIB_NAME = "Climber"


# ---------------------------------------------------------------------------
# Register all environments with Gymnasium
# ---------------------------------------------------------------------------

gym.register(
    id="procgen2/CoinRun-v0",
    entry_point="procgen2:_CoinRunEnv",
    max_episode_steps=1000,
)

gym.register(
    id="procgen2/Climber-v0",
    entry_point="procgen2:_ClimberEnv",
    max_episode_steps=1000,
)
