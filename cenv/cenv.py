import gymnasium as gym
from gymnasium import Env
import collections
import copy
import os
import shutil
import sys
import tempfile
import numpy as np
import ctypes
from ctypes import *
import struct

from typing import (
    Any,
    Dict,
    List,
    Optional,
    Tuple
)

# Types
CENV_VALUE_TYPE_INT = 0
CENV_VALUE_TYPE_FLOAT = 1
CENV_VALUE_TYPE_DOUBLE = 2
CENV_VALUE_TYPE_BYTE = 3

CENV_VALUE_TYPE_BOX = 4
CENV_VALUE_TYPE_MULTI_DISCRETE = 5

CENV_VALUE_TYPE_TO_CTYPE = [
    c_int32,
    c_float,
    c_double,
    c_byte,

    # Space types
    c_float,
    c_int32
]

CENV_PYTHON_TYPE_TO_VALUE_TYPE = {
    int: 0,
    float: 2
}

CENV_NUMPY_DTYPE_TO_VALUE_TYPE = {
    np.dtype('int32'): 0,
    np.dtype('float32'): 1,
    np.dtype('float64'): 2,
    np.dtype('uint8'): 3
}

CENV_VALUE_TYPE_TO_NUMPY_DTYPE = [
    np.int32,
    np.float32,
    np.float64,
    np.uint8,

    # Space
    np.float32,
    np.int32
]

class CEnv_Value(Union):
    _fields_ = [("i", c_int32),
                ("f", c_float),
                ("d", c_double),
                ("b", c_byte)]

class CEnv_Value_Buffer(Union):
    _fields_ = [("i", POINTER(c_int32)),
                ("f", POINTER(c_float)),
                ("d", POINTER(c_double)),
                ("b", POINTER(c_byte))]

class CEnv_Key_Value(Structure):
    _fields_ = [("key", c_char_p),
                ("value_type", c_int32),
                ("value_buffer_size", c_int32),
                ("value_buffer", CEnv_Value_Buffer)]

class CEnv_Option(Structure):
    _fields_ = [("name", c_char_p),
                ("value_type", c_int32),
                ("value", CEnv_Value)]

class CEnv_Make_Data(Structure):
    _fields_ = [("observation_spaces_size", c_int32),
                ("observation_spaces", POINTER(CEnv_Key_Value)),
                ("action_spaces_size", c_int32),
                ("action_spaces", POINTER(CEnv_Key_Value))]

class CEnv_Reset_Data(Structure):
    _fields_ = [("observations_size", c_int32),
                ("observations", POINTER(CEnv_Key_Value)),
                ("infos_size", c_int32),
                ("infos", POINTER(CEnv_Key_Value))] 

class CEnv_Step_Data(Structure):
    _fields_ = [("observations_size", c_int32),
                ("observations", POINTER(CEnv_Key_Value)),
                ("reward", CEnv_Value),
                ("terminated", c_bool),
                ("truncated", c_bool),
                ("infos_size", c_int32),
                ("infos", POINTER(CEnv_Key_Value))] 

class CEnv_Render_Data(Structure):
    _fields_ = [("value_type", c_int32),
                ("value_buffer_width", c_int32),
                ("value_buffer_height", c_int32),
                ("value_buffer_channels", c_int32),
                ("value_buffer", CEnv_Value_Buffer)]

# From https://stackoverflow.com/questions/4355524/getting-data-from-ctypes-array-into-numpy
def _make_nd_array(c_pointer, shape, dtype=np.float32, order='C', own_data=True):
    arr_size = np.prod(shape[:]) * np.dtype(dtype).itemsize 

    if sys.version_info.major >= 3:
        buf_from_mem = pythonapi.PyMemoryView_FromMemory
        buf_from_mem.restype = py_object
        buf_from_mem.argtypes = (c_void_p, c_int, c_int)
        buffer = buf_from_mem(c_pointer, arr_size, 0x100)
    else:
        buf_from_mem = pythonapi.PyBuffer_FromMemory
        buf_from_mem.restype = py_object
        buffer = buf_from_mem(c_pointer, arr_size)

    arr = np.ndarray(tuple(shape[:]), dtype, buffer, order=order)

    if own_data and not arr.flags.owndata:
        return arr.copy()

    return arr

def _put_value_buffer(arr):
    type_index = CENV_NUMPY_DTYPE_TO_VALUE_TYPE[arr.dtype]

    c_arr_p = ctypes.POINTER(CENV_VALUE_TYPE_TO_CTYPE[type_index])

    buffer = CEnv_Value_Buffer()

    if type_index == 0:
        buffer.i = arr.ctypes.data_as(c_arr_p)
    elif type_index == 1:
        buffer.f = arr.ctypes.data_as(c_arr_p)
    elif type_index == 2:
        buffer.d = arr.ctypes.data_as(c_arr_p)
    elif type_index == 3:
        buffer.b = arr.ctypes.data_as(c_arr_p)

    return buffer

class CEnv(Env):
    metadata = {"render_modes": ["rgb_array", "human"], "render_fps": 15}

    def __init__(self, lib_file_path: str, render_mode: Optional[str] = None, options: Optional[Dict[str, Any]] = None):
        # Each CEnv instance needs its own isolated copy of the shared library.
        #
        # On all major platforms dlopen/LoadLibrary de-duplicates by path: if
        # two Python objects call CDLL("same/path") they receive the same handle
        # and therefore share all C globals (game state, ECS, SDL surfaces, …).
        # Copying the library to a unique temp file forces the OS to create a
        # fresh address-space slot with independent globals, making SyncVectorEnv
        # with N envs behave correctly without any changes to the C++ code.
        lib_file_path = os.path.abspath(lib_file_path)
        suffix = os.path.splitext(lib_file_path)[1]   # .dylib / .so / .dll
        tmp_fd, self._tmp_lib_path = tempfile.mkstemp(suffix=suffix)
        os.close(tmp_fd)
        shutil.copy2(lib_file_path, self._tmp_lib_path)

        # Load the isolated copy
        self.lib = CDLL(self._tmp_lib_path)

        # Set up functions for Python
        self.lib.cenv_get_env_version.argtypes = []
        self.lib.cenv_get_env_version.restype = c_int32

        self.lib.cenv_make.argtypes = [c_char_p, POINTER(CEnv_Option), c_int32]
        self.lib.cenv_make.restype = c_int32

        self.lib.cenv_reset.argtypes = [POINTER(CEnv_Option), c_int32]
        self.lib.cenv_reset.restype = c_int32

        self.lib.cenv_step.argtypes = [POINTER(CEnv_Key_Value), c_int32]
        self.lib.cenv_step.restype = c_int32

        self.lib.cenv_render.argtypes = []
        self.lib.cenv_render.restype = c_int32

        self.lib.cenv_close.argtypes = []
        self.lib.cenv_close.restype = None

        # Get pointers to globals
        self.c_make_data = CEnv_Make_Data.in_dll(self.lib, "make_data")
        self.c_reset_data = CEnv_Reset_Data.in_dll(self.lib, "reset_data")
        self.c_step_data = CEnv_Step_Data.in_dll(self.lib, "step_data")
        self.c_render_data = CEnv_Render_Data.in_dll(self.lib, "render_data")

        ret = 0

        c_options = None
        num_options = 0

        if options != None:
            num_options = len(options)

            c_options = (CEnv_Option * num_options)()

            i = 0

            for k, v in options.items():
                c_options[i].name = bytes(k, encoding="ascii")

                value_type = CENV_PYTHON_TYPE_TO_VALUE_TYPE[type(v)]

                c_options[i].value_type = c_int32(value_type)
                c_options[i].value = CEnv_Value(CENV_VALUE_TYPE_TO_CTYPE[value_type](v))

                i += 1

        self.render_mode = render_mode

        ret = self.lib.cenv_make(bytes("" if render_mode == None else render_mode, "ascii"), c_options, c_int32(num_options))

        if ret != 0:
            raise(Exception("Non-zero error code!"))

        # ---- Observation spaces ----
        # make_data gives us the BOUNDS (low/high scalars or nvec).
        # The actual observation SHAPE and DTYPE come from reset_data, which
        # is fully initialised by cenv_make even before the first cenv_reset.
        obs_spaces = {}

        for i in range(self.c_make_data.observation_spaces_size):
            space_type   = int(self.c_make_data.observation_spaces[i].value_type)
            bounds_size  = int(self.c_make_data.observation_spaces[i].value_buffer_size)
            c_bounds_p   = self.c_make_data.observation_spaces[i].value_buffer.b

            bounds_arr = _make_nd_array(c_bounds_p, (bounds_size,),
                                        dtype=CENV_VALUE_TYPE_TO_NUMPY_DTYPE[space_type])

            key = self.c_make_data.observation_spaces[i].key.decode()

            if space_type == CENV_VALUE_TYPE_MULTI_DISCRETE:
                space = gym.spaces.MultiDiscrete(bounds_arr.astype(np.int64))
            else:
                # BOX: bounds_arr stores [low, high] scalars; real shape/dtype
                # come from the pre-allocated observation slot in reset_data.
                obs_dtype = CENV_VALUE_TYPE_TO_NUMPY_DTYPE[
                    int(self.c_reset_data.observations[i].value_type)]
                obs_size  = int(self.c_reset_data.observations[i].value_buffer_size)

                low  = np.full((obs_size,), bounds_arr[0],             dtype=obs_dtype)
                high = np.full((obs_size,), bounds_arr[bounds_size//2], dtype=obs_dtype)

                space = gym.spaces.Box(low=low, high=high, dtype=obs_dtype)

            obs_spaces[key] = space

        self.observation_space = gym.spaces.Dict(obs_spaces)

        # ---- Action spaces ----
        act_spaces = {}

        for i in range(self.c_make_data.action_spaces_size):
            space_type  = int(self.c_make_data.action_spaces[i].value_type)
            bounds_size = int(self.c_make_data.action_spaces[i].value_buffer_size)
            c_bounds_p  = self.c_make_data.action_spaces[i].value_buffer.b

            bounds_arr = _make_nd_array(c_bounds_p, (bounds_size,),
                                        dtype=CENV_VALUE_TYPE_TO_NUMPY_DTYPE[space_type])

            key = self.c_make_data.action_spaces[i].key.decode()

            if space_type == CENV_VALUE_TYPE_MULTI_DISCRETE:
                space = gym.spaces.MultiDiscrete(bounds_arr.astype(np.int64))
            else:
                low  = bounds_arr[:bounds_size // 2]
                high = bounds_arr[bounds_size // 2:]
                space = gym.spaces.Box(low=low, high=high)

            act_spaces[key] = space

        self.action_space = gym.spaces.Dict(act_spaces)

    def step(self, action: gym.core.ActType) -> Tuple[gym.core.ObsType, float, bool, bool, dict]:
        c_actions = None
        num_actions = 1

        if isinstance(action, (int, np.integer)):
            # Single integer action — wrap as a one-element CEnv_Key_Value array
            c_action = c_int32(int(action))

            c_value_buffer = CEnv_Value_Buffer()
            c_value_buffer.i = pointer(c_action)

            c_actions = (CEnv_Key_Value * 1)()
            c_actions[0].key = b"action"
            c_actions[0].value_type = c_int32(CENV_VALUE_TYPE_INT)
            c_actions[0].value_buffer_size = c_int32(1)
            c_actions[0].value_buffer = c_value_buffer
        elif isinstance(action, np.ndarray):
            action = np.ascontiguousarray(action)

            type_index = CENV_NUMPY_DTYPE_TO_VALUE_TYPE[action.dtype]
            c_arr_p = ctypes.POINTER(CENV_VALUE_TYPE_TO_CTYPE[type_index])

            c_value_buffer = CEnv_Value_Buffer()
            c_value_buffer.b = action.ctypes.data_as(ctypes.POINTER(c_byte))

            c_actions = (CEnv_Key_Value * 1)()
            c_actions[0].key = b"action"
            c_actions[0].value_type = c_int32(type_index)
            c_actions[0].value_buffer_size = c_int32(len(action))
            c_actions[0].value_buffer = c_value_buffer
        elif isinstance(action, dict):
            num_actions = len(action)

            c_actions = (CEnv_Key_Value * num_actions)()

            i = 0

            for k, v in action.items():
                v = np.ascontiguousarray(v)
                # int64 is not part of the cenv protocol; downcast to int32
                # (MultiDiscrete.sample() returns int64 on most platforms)
                if v.dtype == np.int64:
                    v = v.astype(np.int32)
                c_actions[i].key = bytes(k, encoding="ascii")
                c_actions[i].value_type = c_int32(CENV_NUMPY_DTYPE_TO_VALUE_TYPE[v.dtype])
                c_actions[i].value_buffer_size = c_int32(len(v))
                c_actions[i].value_buffer = _put_value_buffer(v)

                i += 1

        else:
            raise(Exception("Unrecognized action type! Supported are: int, np.integer, np.ndarray, Dict[np.ndarray]"))
            
        ret = self.lib.cenv_step(c_actions, c_int32(num_actions))

        if ret != 0:
            raise(Exception("Non-zero error code!"))

        # Create observation
        observation = {}

        for i in range(self.c_step_data.observations_size):
            value_type = int(self.c_step_data.observations[i].value_type)
            value_buffer_size = int(self.c_step_data.observations[i].value_buffer_size)
            c_buffer_p = self.c_step_data.observations[i].value_buffer.b

            arr = _make_nd_array(c_buffer_p, (value_buffer_size,), dtype=CENV_VALUE_TYPE_TO_NUMPY_DTYPE[value_type])

            observation[self.c_step_data.observations[i].key.decode()] = arr
        
        info = {}

        for i in range(self.c_step_data.infos_size):
            value_type = int(self.c_step_data.infos[i].value_type)
            value_buffer_size = int(self.c_step_data.infos[i].value_buffer_size)
            c_buffer_p = self.c_step_data.infos[i].value_buffer.b

            arr = _make_nd_array(c_buffer_p, (value_buffer_size,), dtype=CENV_VALUE_TYPE_TO_NUMPY_DTYPE[value_type])

            info[self.c_step_data.infos[i].key.decode()] = arr

        reward = float(self.c_step_data.reward.f)
        terminated = bool(self.c_step_data.terminated)
        truncated = bool(self.c_step_data.truncated)

        return (observation, reward, terminated, truncated, info)

    def reset(self, *, seed: Optional[int] = None, options: Optional[Dict[str, Any]] = None) -> Tuple[gym.core.ObsType, dict]:
        super().reset(seed=seed)

        c_options = None
        num_options = 0

        # Merge seed into options dict so the C side can consume it
        merged: Dict[str, Any] = {}
        if seed is not None:
            merged["seed"] = seed
        if options is not None:
            merged.update(options)

        if merged:
            num_options = len(merged)

            c_options = (CEnv_Option * num_options)()

            i = 0

            for k, v in merged.items():
                c_options[i].name = bytes(k, encoding="ascii")

                value_type = CENV_PYTHON_TYPE_TO_VALUE_TYPE[type(v)]

                c_options[i].value_type = c_int32(value_type)
                c_options[i].value = CEnv_Value(CENV_VALUE_TYPE_TO_CTYPE[value_type](v))

                i += 1

        ret = self.lib.cenv_reset(c_options, c_int32(num_options))
        
        if ret != 0:
            raise(Exception("Non-zero error code!"))

        # Create observation
        observation = {}

        for i in range(self.c_reset_data.observations_size):
            value_type = int(self.c_reset_data.observations[i].value_type)
            value_buffer_size = int(self.c_reset_data.observations[i].value_buffer_size)
            c_buffer_p = self.c_reset_data.observations[i].value_buffer.b

            arr = _make_nd_array(c_buffer_p, (value_buffer_size,), dtype=CENV_VALUE_TYPE_TO_NUMPY_DTYPE[value_type])

            observation[self.c_reset_data.observations[i].key.decode()] = arr
        
        info = {}

        for i in range(self.c_reset_data.infos_size):
            value_type = int(self.c_reset_data.infos[i].value_type)
            value_buffer_size = int(self.c_reset_data.infos[i].value_buffer_size)
            c_buffer_p = self.c_reset_data.infos[i].value_buffer.b

            arr = _make_nd_array(c_buffer_p, (value_buffer_size,), dtype=CENV_VALUE_TYPE_TO_NUMPY_DTYPE[value_type])

            info[self.c_reset_data.infos[i].key.decode()] = arr

        return (observation, info)

    def render(self) -> gym.core.RenderFrame:
        if self.render_mode is None:
            raise gym.error.Error(
                "render() called with render_mode=None. "
                "Pass render_mode='rgb_array' or render_mode='human' to the constructor."
            )

        self.lib.cenv_render()

        if self.render_mode == "human":
            # Display is handled entirely by the C++ SDL window; return None
            # per Gymnasium convention for human render mode.
            return None

        # rgb_array: extract the frame from the C-side render buffer
        value_type = self.c_render_data.value_type
        value_buffer_size = (self.c_render_data.value_buffer_height *
                             self.c_render_data.value_buffer_width *
                             self.c_render_data.value_buffer_channels)
        c_buffer_p = self.c_render_data.value_buffer.b

        arr = _make_nd_array(c_buffer_p, (value_buffer_size,), dtype=CENV_VALUE_TYPE_TO_NUMPY_DTYPE[value_type])

        return arr.reshape(self.c_render_data.value_buffer_height,
                           self.c_render_data.value_buffer_width,
                           self.c_render_data.value_buffer_channels)

    def close(self):
        self.lib.cenv_close()
        # Remove the per-instance temp copy of the shared library.
        try:
            os.unlink(self._tmp_lib_path)
        except OSError:
            pass
