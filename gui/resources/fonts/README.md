# Material Symbols Rounded (variable font)

`MaterialSymbolsRounded.ttf` and `MaterialSymbolsRounded.codepoints` are
`variablefont/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].*` from google/material-design-icons
(Apache 2.0, see LICENSE). Axes: FILL 0–1, GRAD -25–200, opsz 20–48, wght 100–700.

`gui/M3Icons.cpp` loads the font at runtime (CMake copies this folder to `<build>/fonts` and
installs it to `share/onlyarchitect/fonts`) and looks glyphs up by name in the codepoints table,
so any of the ~4300 symbols can be used without adding files. The SVGs in
`gui/resources/icons/material-symbols-rounded` remain as fallback.
