"""
Check that every ProcGen2 env can be created with gym.make(..., distribution_mode=...).

Run from the repo root (shared libraries must already be built):

    pytest test_distribution_modes.py -q
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import pytest
import gymnasium as gym
import procgen2

ALL_MODES = ("easy", "hard", "extreme", "memory")
ENV_IDS = sorted(
    env_id for env_id in gym.envs.registry.keys() if env_id.startswith("procgen2/")
)


def _make(env_id, **kwargs):
    try:
        return gym.make(env_id, disable_env_checker=True, **kwargs)
    except FileNotFoundError as exc:
        pytest.skip(str(exc))


@pytest.fixture(params=ENV_IDS, ids=lambda env_id: env_id.split("/")[-1])
def env_id(request):
    return request.param


def test_all_procgen2_games_are_registered():
    assert len(ENV_IDS) == 16, ENV_IDS


@pytest.mark.parametrize("mode", ALL_MODES)
def test_gym_make_supports_distribution_mode(env_id, mode):
    env = _make(env_id, distribution_mode=mode)
    try:
        assert env.unwrapped._distribution_mode == procgen2.DISTRIBUTION_MODE[mode]
        obs, info = env.reset(seed=0)
        assert "screen" in obs
        action = env.action_space.sample()
        obs, reward, terminated, truncated, info = env.step(action)
        assert obs["screen"].shape == (12288,)
        assert isinstance(reward, float) or reward.shape == ()
        assert terminated in (True, False)
        assert truncated in (True, False)
    finally:
        env.close()


def test_gym_make_default_is_hard(env_id):
    env = _make(env_id)
    try:
        assert env.unwrapped._distribution_mode == procgen2.DISTRIBUTION_MODE["hard"]
        env.reset(seed=1)
    finally:
        env.close()


def test_exploration_mode(env_id):
    cls_name = gym.spec(env_id).entry_point.split(":")[-1]
    game_dir = getattr(procgen2, cls_name)._GAME_DIR
    supports = game_dir in procgen2.EXPLORATION_LEVEL_SEEDS

    if supports:
        env = _make(env_id, distribution_mode="exploration")
        try:
            assert env.unwrapped._distribution_mode == procgen2.DISTRIBUTION_MODE["hard"]
            env.reset()
        finally:
            env.close()
    else:
        with pytest.raises(ValueError, match="does not support exploration mode"):
            gym.make(env_id, distribution_mode="exploration", disable_env_checker=True)


def test_invalid_distribution_mode_is_rejected(env_id):
    with pytest.raises(ValueError, match="not a valid distribution mode"):
        gym.make(env_id, distribution_mode="impossible", disable_env_checker=True)


def test_gym_make_works_from_a_different_cwd(tmp_path):
    """Textures are `assets/...` paths; they must resolve from the package, not cwd."""
    prev = os.getcwd()
    try:
        os.chdir(tmp_path)
        env = _make("procgen2/CoinRun-v0")
        try:
            obs, _ = env.reset(seed=0)
            assert obs["screen"].shape == (12288,)
            env.step(env.action_space.sample())
        finally:
            env.close()
    finally:
        os.chdir(prev)
