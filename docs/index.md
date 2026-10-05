---
hide-toc: true
firstpage:
lastpage:
---

```{project-logo} _static/img/procgen2-text.png
:alt: Procgen2 Logo
:class: farama-black-logo-invert
```

```{project-heading}
Procedurally generated game environments for reinforcement learning
```

Procgen2 is a community rewrite of [OpenAI's Procgen](https://github.com/openai/procgen). The project began as a maintained fork of Procgen with support for the standard [Gymnasium](https://gymnasium.farama.org) API instead of Gym3, a documentation website, and environment bug fixes. It evolved into a rewrite because of concerns about code structure and maintainability, memory leaks, performance, and other large issues.

Each game is written in C++ and compiled to a shared library, which Python loads through [CEnv](development.md#cenv), a small interface for calling compiled environments from Python. Procgen2 also plans to release the evaluation environments from OpenAI's Procgen contest for the first time.

```{note}
Procgen2 is under active development and is not yet published on PyPI. To try the games, build them from source by following the [development guide](development.md).
```

## Citation

Procgen was originally introduced in the following work:

```
@article{cobbe2019procgen,
  title={Leveraging Procedural Generation to Benchmark Reinforcement Learning},
  author={Cobbe, Karl and Hesse, Christopher and Hilton, Jacob and Schulman, John},
  journal={arXiv preprint arXiv:1912.01588},
  year={2019}
}
```

```{toctree}
:hidden:
:caption: Introduction

development
```

```{toctree}
:hidden:
:caption: Development

Github <https://github.com/Farama-Foundation/Procgen2>
Contribute to the Docs <https://github.com/Farama-Foundation/Procgen2/blob/main/docs/README.md>
```
