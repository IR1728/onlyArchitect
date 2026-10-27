# OpenEA — Recreación de Enterprise Architect en C++20

**OpenEA** es una implementación limpia y modular en C++20 de la suite de modelado y arquitectura **Enterprise Architect 17.2** (Sparx Systems), diseñada a partir de evidencia estática extraída con **REA** (Reverse Engineer Anything) sobre los binarios, tipos COM (`Interop.EA.dll`), ensamblados y bases de datos SQLite (`.qea`).

![OpenEA Prototype](docs/screenshots/prototype.jpeg)

---

## 1. Arquitectura y Mapeo de Ingeniería Inversa (REA)

A partir del análisis con REA se identificaron y recrearon los cuatro subsistemas fundamentales:

```
+-------------------------------------------------------------------------+
|                              OpenEA CLI / API                            |
+-------------------------------------------------------------------------+
                                     |
       +-----------------------------+-----------------------------+
       |                             |                             |
       v                             v                             v
+-------------------+      +-------------------+       +--------------------+
|  ea::repository   |      |    ea::diagram    |       |     ea::export     |
|  - Repository     |      |  - CoordinateSys  |       |  - PlantUML        |
|  - StorageEngine  |      |  - DiagramRenderer|       |  - JSON            |
|  - SqliteEngine   |      |  - Layout (Spring)|       |  - OMG UML 2.5/XMI |
|  - EapxImporter   |      |  - Connectors     |       |                    |
+-------------------+      +-------------------+       +--------------------+
       |                             |
       +--------------+--------------+
                      |
                      v
       +------------------------------------+
       |              ea::model             |
       |  - Package     (`t_package`)       |
       |  - Element     (`t_object`)        |
       |  - Connector   (`t_connector`)     |
       |  - Diagram     (`t_diagram`)       |
       |  - DiagramObj  (`t_diagramobjects`)|
       |  - DiagramLink (`t_diagramlinks`)  |
       |  - Attribute   (`t_attribute`)     |
       |  - Method      (`t_operation`)     |
       +------------------------------------+
                      |
                      v
       +------------------------------------+
       |          Bases de Datos            |
       |      SQLite (.qea / .feap)         |
       +------------------------------------+
```

### Hallazgos clave de REA incorporados:
1. **Esquema de Repositorio `.qea`**:
   - `t_package`: Estructura jerárquica con `Package_ID`, `Parent_ID`, `ea_guid`.
   - `t_object`: Entidades UML/SysML con columna singular `Note` (legado de MS Jet/Access), `Object_Type`, `Stereotype`, etc.
   - `t_diagramobjects`: Coordenadas en 4º cuadrante cartesiano (`RectTop < 0`, `RectBottom < RectTop`, `top > bottom`).
   - `t_diagramlinks`: Columnas `DiagramID` y `ConnectorID` (sin guión bajo), `Path` (puntos en formato `X:Y;`).
2. **Modelo de Objetos COM (`IDualRepository`)**:
   - `Collection<T>` polimórfico con métodos `Count()`, `GetAt()`, `GetByName()`, `AddNew()`, `Delete()`.
   - `Repository`: `OpenFile()`, `CloseFile()`, `GetPackageByID()`, `GetElementByID()`, `GetDiagramByID()`, `SQLQuery()`, `Execute()`.
3. **Renderizado de Diagramas Vectoriales**:
   - Mapeo de coordenadas cartesianas negativas a espacio positivo de pantalla/vector.
   - Generación SVG nativa de compartimentos UML (Cabecera, Estereotipos, Atributos con visibilidad `+/-/#/~`, Operaciones).
   - Conectores con marcadores estándar (Herencia/Generalización, Realización, Asociación, Composición, Agregación).
   - Renderizado PNG con **Cairo** (equivalente al motor `Cairo172-32.dll` del contenedor original).
4. **Migración de Repositorios Heredados (.EAP / .EAPX a .QEA)**:
   - Análisis de binarios originales (`EA.exe`, `EAPExport.exe`, `MDBTools172-32.dll`): Enterprise Architect 16+ implementa el diálogo nativo `CEAPToQEATransferDlg` que invoca `EAPExport.exe` y `MDBTools` para transferir bases de datos Microsoft Access Jet a repositorios SQLite `.qea`.
   - Implementado en OpenEA mediante [`EapxImporter`](file:///home/iran/EA/ea_cpp/include/ea/repository/EapxImporter.hpp) y [`EapToQeaTransferDialog`](file:///home/iran/EA/ea_cpp/gui/EapToQeaTransferDialog.hpp), permitiendo migración directa en 0.35s y apertura transparente tanto en GUI como en CLI.

---

## 2. Compilación

```bash
cd /home/iran/EA/ea_cpp
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

Dependencias del sistema:
- GCC con soporte C++20
- `sqlite3` (librería de desarrollo)
- `cairo` (librería de desarrollo para gráficos vectoriales)

---

## 3. Ejecutar la Aplicación Gráfica (`openea-gui`)

La aplicación cuenta con una interfaz gráfica completa construida con **Qt6**:

```bash
/home/iran/EA/ea_cpp/build/openea-gui
```
O pasando directamente la ruta de un repositorio `.qea` o `.eapx` / `.eap`:
```bash
/home/iran/EA/ea_cpp/build/openea-gui /home/iran/CicloAgostoDiciembre2026/Diseño/DiagramaCasosUsoExamen.eapx
```

### Características de la GUI:
- **Project Browser (Panel Izquierdo)**: Navegación jerárquica de modelos, paquetes, clases, interfaces, componentes y diagramas con barra de búsqueda/filtro en tiempo real.
- **Diagram Editor Manipulable (Panel Central)**: Lienzo interactivo basado en `QGraphicsScene` con manipulación directa:
  - **Arrastre y movimiento libre**: Cada clase/interfaz/nota se puede mover libremente con el ratón.
  - **Auto-enrutamiento y renderizado de conectores UML de alta fidelidad**:
    - **Descubrimiento automático de conectores implícitos**: Resuelve la convención de EA donde los diagramas muestran conectores entre sus elementos aunque no existan filas explícitas en `t_diagramlinks` (ej. diagramas de secuencia como "Close Account").
    - **Soporte de puntos de quiebre (waypoints ortogonales)**: Interpreta y sigue la geometría de `t_diagramlinks.Path` (`X:Y;`).
    - **Marcadores UML completos**: Triángulos cerrados para Generalización y Realización, diamantes rellenos para Composición, diamantes huecos para Agregación, flechas abiertas para Dependencia y Mensajes de Secuencia, y líneas directas para Asociaciones.
    - **Etiquetas de roles, cardinalidades y nombres**: Muestra nombres con pastilla de fondo anti-solapamiento y multiplicidades/roles (`+role [1..*]`) en extremos de origen y destino.
    - **Z-Order y geometría limpia**: Capa por encima de las sombras de los nodos (`zValue = 1`), intersección exacta al borde de los elementos y zona de clic de 10px para fácil selección y borrado con la tecla `Supr`.
  - **Redimensión con tirador inferior derecho**: Tirador de resize para ajustar el tamaño de cada elemento directamente en el diagrama.
  - **Edición in-place**: Doble clic o menú contextual abre el diálogo de edición (`EditElementDialog`) para modificar nombre, estereotipo, notas, agregar o quitar atributos y métodos.
  - **Creación interactiva**: Botones de barra de herramientas y menú contextual para crear clases ("➕ Class"), interfaces ("➕ Interface"), notas ("➕ Note") y relaciones entre elementos ("🔗 Link").
  - **Persistencia en base de datos**: Atajo `Ctrl+S` o botón "💾 Save Diagram Layout" que persiste las nuevas coordenadas en la tabla `t_diagramobjects` del archivo `.qea`.
  - **Navegación fluida**: Zoom con Ctrl+Rueda o botones de zoom, ajuste a pantalla (Fit), y modo paneo/mano con barra espaciadora o botón del medio del ratón.
- **Inspector de Propiedades (Panel Derecho)**: Edición y visualización de propiedades generales (GUID, Nombre, Tipo, Estereotipo, Estado, Notas), tablas de atributos, métodos con visibilidad y conectores.
- **Consola SQL (Panel Inferior)**: Scratchpad interactivo para ejecutar sentencias SQL directas sobre el archivo `.qea` y ver los datos tabulados al instante.
- **Barra de Herramientas**: Botones de acceso rápido para auto-layout por resortes (Spring/Force-directed), exportación a SVG, PNG y PlantUML.

---

## 4. Comandos de la CLI (`openea`)

### Ver información y estadísticas del repositorio:
```bash
./openea info /home/iran/EA/re/bin/EA_Trial/EAExample.qea
```

### Visualizar el árbol de paquetes y elementos:
```bash
./openea tree /home/iran/EA/re/bin/EA_Trial/EAExample.qea 2
```

### Renderizar diagramas a SVG y PNG:
```bash
# Renderizar a SVG
./openea render /home/iran/EA/re/bin/EA_Trial/EAExample.qea --diagram 11 --out diagram11.svg

# Renderizar a PNG usando Cairo
./openea render /home/iran/EA/re/bin/EA_Trial/EAExample.qea --diagram 11 --out diagram11.png
```

### Exportar modelos a PlantUML, JSON o XMI:
```bash
# Exportar diagrama a PlantUML
./openea export /home/iran/EA/re/bin/EA_Trial/EAExample.qea --diagram 11 --format plantuml --out diagram11.puml

# Exportar repositorio completo a JSON
./openea export /home/iran/EA/re/bin/EA_Trial/EAExample.qea --format json --out repo.json
```

### Consultas SQL directas (salida compatible con `Repository.SQLQuery` de EA):
```bash
./openea query /home/iran/EA/re/bin/EA_Trial/EAExample.qea "SELECT Diagram_ID, Name, Diagram_Type FROM t_diagram LIMIT 5"
```

### Crear un nuevo repositorio `.qea` compatible desde cero:
```bash
./openea create nuevo_modelo.qea
```

---

## 5. Ejecución de Tests Automatizados

### Test de Repositorio y Persistencia (`test_openea`):
```bash
cd /home/iran/EA/ea_cpp/build
./test_openea
```
El test valida:
1. Carga completa de la base de datos de producción `EAExample.qea` (1.050 paquetes, 11.035 elementos, 1.129 diagramas, 4.793 enlaces).
2. Renderizado de SVG y exportación PlantUML del diagrama `11` ("Abstract Class Model").
3. Creación, guardado y lectura de un nuevo repositorio SQLite `.qea`.
4. Manipulación de objetos de diagrama, persistencia de coordenadas y verificación en frío tras recargar.

### Test de Manipulación Gráfica de la GUI (`test_gui`):
```bash
./test_gui
```
El test valida:
1. Carga del lienzo gráfico de diagrama interactivo con `QGraphicsScene`.
2. Selección y arrastre de elementos modificando sus coordenadas de escena.
3. Redimensión geométrica del elemento.
4. Auto-enrutamiento dinámico de conectores vinculados.
5. Zoom in/out, fit to view y conmutación de modo paneo.
6. Persistencia de la disposición del diagrama hacia la base de datos SQLite `.qea`.
