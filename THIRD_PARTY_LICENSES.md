# Licenses & Third-Party Notices

## onlyArchitect Studio

Copyright (C) 2026 onlyArchitect Contributors

**onlyArchitect Studio** is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version (`GPL-3.0-or-later`)**.

This program is distributed in the hope that it will be useful, **but WITHOUT ANY WARRANTY**; without even the implied warranty of **MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE**. See the [`LICENSE`](LICENSE) file in this repository or <https://www.gnu.org/licenses/gpl-3.0.html> for full details.

---

## Third-Party Components

onlyArchitect dynamically links against or bundles the following open-source libraries and assets. All third-party components remain the property of their respective copyright holders and are governed by their own licenses.

### 1. Qt 6 (`Qt6Widgets`, `Qt6Gui`, `Qt6Core`, `Qt6Svg`, `Qt6SvgWidgets`, `Qt6Network`)
* **License:** GNU Lesser General Public License v3.0 (LGPLv3) / GNU General Public License v3.0 (GPLv3)
* **Copyright:** The Qt Company Ltd. and other contributors
* **Source Code:** <https://download.qt.io/official_releases/qt/> and <https://code.qt.io/>
* **Compliance Notice:** onlyArchitect dynamically links against Qt 6 shared libraries (`.so`, `.dylib`, `.dll`). Users are free to relink or replace the bundled Qt 6 shared libraries with compatible modified versions in accordance with the LGPLv3 / GPLv3.

### 2. Cairo 2D Graphics Library
* **License:** GNU Lesser General Public License v2.1 (LGPLv2.1) or Mozilla Public License v1.1 (MPL-1.1)
* **Website & Source Code:** <https://cairographics.org/>

### 3. SQLite 3
* **License:** Public Domain (dedicated to the public domain by its authors)
* **Website & Source Code:** <https://www.sqlite.org/copyright.html>

### 4. Google Material Symbols Rounded (Font & SVG Icons)
* **Files:** `gui/resources/fonts/MaterialSymbolsRounded.ttf`, `MaterialSymbolsRounded.codepoints`, and `gui/resources/icons/`
* **License:** Apache License, Version 2.0 (`Apache-2.0`)
* **Copyright:** Google LLC
* **Source & License Text:** <https://github.com/google/material-design-icons> and <https://www.apache.org/licenses/LICENSE-2.0>

### 5. MDB Tools (`mdb-schema`, `mdb-export`, `mdb-tables`, optional external system utility)
* **License:** GNU General Public License v2.0 or later (`GPL-2.0-or-later`) for CLI tools; GNU LGPL v2.1 or later (`LGPL-2.1-or-later`) for `libmdb`
* **Source Code:** <https://github.com/mdbtools/mdbtools>
* **Notice:** Not bundled in this repository. Invoked optionally as an external system command-line utility (`apt install mdbtools` / `brew install mdbtools`) to migrate legacy Microsoft Access repositories (`.eap` / `.eapx`) into SQLite `.qea`.

---

## Trademark & Independence Disclaimer

* **Enterprise Architect®** is a registered trademark of **Sparx Systems Pty Ltd**.
* **UML®** and **XMI®** are registered trademarks of the **Object Management Group, Inc. (OMG)**.
* **Windows®** is a registered trademark of Microsoft Corporation; **macOS®** is a registered trademark of Apple Inc.; **Linux®** is the registered trademark of Linus Torvalds.

**onlyArchitect Studio** is an independent, clean-room open-source project created for UML 2.5.1 modeling and file-format interoperability. It is **not affiliated with, endorsed by, sponsored by, or officially connected to Sparx Systems Pty Ltd** or any other commercial vendor.
