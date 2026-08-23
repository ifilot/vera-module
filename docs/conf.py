from pathlib import Path

project = "VERA Module"
author = "Open VERA Module contributors"
release = (Path(__file__).resolve().parents[1] / "VERSION").read_text(encoding="ascii").strip()

extensions = ["sphinx_rtd_theme"]
templates_path = ["_templates"]
exclude_patterns = ["_build"]
html_theme = "sphinx_rtd_theme"
html_static_path = ["_static"]
html_title = "VERA Module Documentation"
html_show_sourcelink = False
