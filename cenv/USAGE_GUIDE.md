# CEnv Usage Guide

CEnv is an interface that allows one to implement environments for reinforcement learning in a compiled language and call it from Python.

To do this, environments are compiled into .so/.dylib/.dll files and loaded from Python using cenv.py, where they can be used like a regular Python environment.

This guide will show you how to use the CEnv interface to write your own environments.

## What Languages Will Work?

Any language capable of producing shared object files. So generally, any compiled language that can produce:

For Linux, .so (shared object)
For MacOS, .dylib (dynamic library)
For Windows, .dll (dynamic link library)

The language must define functions use the C calling convention for that operating system.

## The CEnv Interface

The CEnv interface can be seen in the included [cenv.h](./cenv.h) file. An example test environment (in C) is provided as [test_env.c](./test_env.c). To use CEnv, your library must:

- Define the following (global) structures:
    * make_data (data for your library for making/initializing the environment)
    * reset_data (data for your library upon environment reset)
    * step_data (data for your library on every environment step)
    * render_data (data for your library for transfer to Python containing an image frame)

    These must be defined globally with the given names. These are used to pass information between Python and your library.

- Define the following functions:
    * cenv_get_env_version: Returns a version number.
    * cenv_make: Makes the environment (initialization). Should initialize make_data, reset_data, step_data, render_data. Returns an error code (0 = no error).
    * cenv_reset: Resets the environment and updates reset_data. Returns an error code (0 = no error).
    * cenv_step: Steps the environment, and updates step_data. Returns an error code (0 = no error).
    * cenv_render: Updates render_data with the current rendered frame.
    * cenv_close: Closes the environment, allowing your library to perform any cleanup needed.

    These must match the signatures in [cenv.h](./cenv.h).

The cenv functions also may take several arguments, which are typically additional structures containing things such as options.
If you are not using the C-header (from C/C++), make sure to declare all structures somewhere in your library. In C/C++, the included cenv.h header will define these structures for you, but in other languages you will need to declare them yourself.

Note that the structures in cenv.h often support multiple datatypes, this is handled through a tagged union. Supported types:

- i: int32
- f: float32
- d: float64 (double)
- b: byte (unsigned char)

Which one to use is identified by the tag (an enum, cenv_value_type). See [test_env.c](./test_env.c) for an example of how to access these.

### Key-Value

Several things in the various data structures are represented with arrays of key-values (through cenv_key_value). The keys are C-strings and the values are a union of arrays of the previously mentioned types (i, f, d, b).

### Options

Options (through cenv_option) are similar to key-value but only contain a single value (not an array) as their value. It therefore contains a union of the supported primitive types previously mentioned (i, f, d, b).

## Example Usage from C/C++:

Create a .c/.cpp file and include cenv.h. Define the above global structures and functions.

- In cenv_get_env_version, just return an integer describing the version number (example format: 132 = major version 1, minor version 3, patch version 2).
- In cenv_make, initialize all the global structures to suite your environment.
    * Parse the render_mode C-string, and array of options (passed as a pointer and a size). An option contains a "key" (a name) and a "value" (value of the option). These can have different types, so use the unions as described above.
- In cenv_reset, reset the environment and update the reset_data to contain the starting observation as will as additional optional info.
- In cenv_step, step the environment and fill out the appropriate fields in step_data (observation, reward, termination, truncation, info).
- In cenv_render, render the environment for display. Fill out render_data to do this.
- In cenv_close, deallocate anything allocated in cenv_make and anything additional. **All allocations and deallocations are your own!**

Compile your code to produce a .so/.dylib/.dll (depending on your OS), using whatever build system your prefer (we used CMake). Your environment can then be used as shown in [test_env.py](./test_env.py).

## Full Game Example

A full procgen2 game has been implemented using cenv in [coinrun](../games/coinrun/). This can serve as an example of how to use cenv with a real environment.
