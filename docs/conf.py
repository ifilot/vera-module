from pathlib import Path

project = "VERA Module"
author = "Open VERA Module contributors"
release = (Path(__file__).resolve().parents[1] / "VERSION").read_text(encoding="ascii").strip()
copyright = "2025-2026, Open VERA Module contributors"

extensions = ["sphinx_rtd_theme"]
templates_path = ["_templates"]
exclude_patterns = ["_build"]
html_theme = "sphinx_rtd_theme"
html_static_path = ["_static"]
html_css_files = ["custom.css"]
html_title = "VERA Module Documentation"
html_show_sourcelink = False
html_theme_options = {
    "collapse_navigation": False,
    "navigation_depth": 3,
    "sticky_navigation": True,
}
