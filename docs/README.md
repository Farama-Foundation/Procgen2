# Procgen2 documentation

This directory contains the source for the Procgen2 documentation website, built with [Sphinx](https://www.sphinx-doc.org/), [MyST Markdown](https://myst-parser.readthedocs.io/), and the Farama [Celshast](https://github.com/Farama-Foundation/Celshast) theme.

For documentation changes, open an issue or pull request in the [Procgen2 repository](https://github.com/Farama-Foundation/Procgen2).
For more information about how to contribute to the documentation go to our [CONTRIBUTING.md](https://github.com/Farama-Foundation/Celshast/blob/main/CONTRIBUTING.md)

## Building the docs locally

From the repository root:

```bash
pip install -r docs/requirements.txt
sphinx-autobuild -b dirhtml docs _build
```

Then open <http://127.0.0.1:8000>. The site rebuilds automatically when you save a file.

## Layout

- `index.md`: landing page, and the table of contents (`toctree` blocks) for the sidebar
- `development.md`: building the games and implementing new ones
- `environments/<game>.md`: one page per game
- `_static/img/`: images; logos live here
- `conf.py`: Sphinx and theme configuration

## Adding a page

1. Create a Markdown file, e.g. `docs/environments/starpilot.md`, starting with a `# Title` heading.
2. Add its path (without `.md`) to the matching `toctree` block in `index.md`, so it appears in the sidebar.

To add a new sidebar section, add another `toctree` block with its own `:caption:`.

## Deployment

- Every push to `main` builds the site and deploys it to <https://procgen2.farama.org/main/>.
- Pushing a version tag (e.g. `v0.1.0`) deploys that version to `/v0.1.0/` and to the site root. Alpha tags (containing `a`) only deploy to their own folder.
- To build a version manually, run the **Manual Docs Versioning** workflow from the Actions tab.
