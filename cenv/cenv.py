import gymnasium as gym
from gymnasium import Env
import collections
import copy
import sys
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
    metadata = {"render_modes": ["human", "single_rgb_array"]}

    def __init__(self, lib_file_path: str, render_mode: Optional[str] = None, options: Optional[Dict[str, Any]] = None):
        # Load library
        self.lib = CDLL(lib_file_path)

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

        ret = self.lib.cenv_make(bytes("" if render_mode == None else render_mode, "ascii"), c_options, c_int32(num_options))

        if ret != 0:
            raise(Exception("Non-zero error code!"))

        self.observation_space = {}

        for i in range(self.c_make_data.observation_spaces_size):
            value_type = int(self.c_make_data.observation_spaces[i].value_type)
            value_buffer_size = int(self.c_make_data.observation_spaces[i].value_buffer_size)
            c_buffer_p = self.c_make_data.observation_spaces[i].value_buffer.b # Always reference as bytes for now

            arr = _make_nd_array(c_buffer_p, (value_buffer_size,), dtype=CENV_VALUE_TYPE_TO_NUMPY_DTYPE[value_type])

            space = None

            if value_type == CENV_VALUE_TYPE_MULTI_DISCRETE:
                space = gym.spaces.MultiDiscrete(arr)
            else:
                space = gym.spaces.Box(arr[:len(arr) // 2], arr[len(arr) // 2:])

            self.observation_space[self.c_make_data.observation_spaces[i].key.decode()] = space
        
        self.action_space = {}
        
        for i in range(self.c_make_data.action_spaces_size):
            value_type = int(self.c_make_data.action_spaces[i].value_type)
            value_buffer_size = int(self.c_make_data.action_spaces[i].value_buffer_size)
            c_buffer_p = self.c_make_data.action_spaces[i].value_buffer.b # Always reference as bytes for now

            arr = _make_nd_array(c_buffer_p, (value_buffer_size,), dtype=CENV_VALUE_TYPE_TO_NUMPY_DTYPE[value_type])

            space = None

            if value_type == CENV_VALUE_TYPE_MULTI_DISCRETE:
                space = gym.spaces.MultiDiscrete(arr)
            else:
                space = gym.spaces.Box(arr[:len(arr) // 2], arr[len(arr) // 2:])

            self.action_space[self.c_make_data.action_spaces[i].key.decode()] = space

    def step(self, action: gym.core.ActType) -> Tuple[gym.core.ObsType, float, bool, bool, dict]:
        c_actions = None
        num_actions = 1

        if type(action) is int:
            c_action = c_int32(action)

            c_value_buffer = CEnv_Value_Buffer()
            c_value_buffer.i = pointer(c_action)

            c_actions = CEnv_Key_Value(b"action", c_int32(CENV_VALUE_TYPE_INT), c_int32(1), c_value_buffer)
        elif type(action) is np.array:
            action = np.ascontiguousarray(action)

            c_value_buffer = CEnv_Value_Buffer()
            c_value_buffer.b = addressof(action.data)

            c_actions = CEnv_Key_Value(b"action", c_int32(CENV_NUMPY_DTYPE_TO_VALUE_TYPE[action.dtype]), c_int32(len(action)), c_value_buffer)
        elif type(action) is dict:
            num_actions = len(action)
            
            c_actions = (CEnv_Key_Value * num_actions)()

            i = 0

            for k, v in action.items():
                c_actions[i].key = bytes(k, encoding="ascii")
                c_actions[i].value_type = c_int32(CENV_NUMPY_DTYPE_TO_VALUE_TYPE[v.dtype])
                c_actions[i].value_buffer_size = c_int32(len(v))
                c_actions[i].value_buffer = _put_value_buffer(v)

                i += 1

        else:
            raise(Exception("Unrecognized action type! Supported are: int, np.array, Dict[np.array]"))
            
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

    def reset(self, options: Optional[List[Any]] = None) -> Tuple[gym.core.ObsType, dict]:
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
        self.lib.cenv_render()

        value_type = self.c_render_data.value_type
        value_buffer_size = self.c_render_data.value_buffer_height * self.c_render_data.value_buffer_width * self.c_render_data.value_buffer_channels
        c_buffer_p = self.c_render_data.value_buffer.b

        arr = _make_nd_array(c_buffer_p, (value_buffer_size,), dtype=CENV_VALUE_TYPE_TO_NUMPY_DTYPE[value_type])

        return arr.reshape(self.c_render_data.value_buffer_height, self.c_render_data.value_buffer_width, self.c_render_data.value_buffer_channels)

    def close(self):
        self.lib.cenv_close()
