# Configuration file for the Sphinx documentation builder.
#
# For the full list of options see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------

project = "Procgen2"
copyright = "2026 Farama Foundation"
author = "Farama Foundation"

# -- General configuration ---------------------------------------------------

extensions = [
    "sphinx.ext.githubpages",
    "myst_parser",
]

templates_path = ["_templates"]
exclude_patterns = ["README.md", "_build"]

# Allow `## Heading` anchors to be linked from other pages, e.g. [](page.md#heading)
myst_heading_anchors = 3

# The landing page uses the project-logo directive in place of an H1 heading
suppress_warnings = ["myst.header"]

# -- Options for HTML output -------------------------------------------------

html_theme = "celshast"
html_title = "Procgen2 Documentation"
html_baseurl = "https://procgen2.farama.org"
html_copy_source = False
html_theme_options = {
    "light_logo": "img/procgen2-black.png",
    "dark_logo": "img/procgen2-white.png",
    "gtag": "G-6H9C8TWXZ8",
    "description": "Procgen2 is a community rewrite of OpenAI's Procgen benchmark: procedurally generated game environments for reinforcement learning.",
    "image": "img/procgen2-text.png",
    "versioning": True,
    "source_repository": "https://github.com/Farama-Foundation/Procgen2/",
    "source_branch": "main",
    "source_directory": "docs/",
}

html_static_path = ["_static"]
html_css_files = []
