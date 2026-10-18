# Archivos de impresión de la carcasa del Datapad (ES3C28P)

[English](README.md) · [Resumen del proyecto](../../README.es.md)

Archivos de fabricación para la carcasa del datapad de Star Wars. Consulta
[docs/enclosure.es.md](../../docs/enclosure.es.md) para la guía de impresión
completa, dimensiones calibradas y notas de montaje.

## Archivos

| Archivo | Tipo | Descripción |
| :--- | :--- | :--- |
| `ES3C28P_StarWars_Datapad.3mf` | 3MF de Bambu Studio | **Proyecto completo preconfigurado de 3 placas** (Frontal, Trasera, Montura de altavoz) — todo lo necesario para imprimir |
| `ES3C28P_StarWars_Front_Bezel.step` | STEP | Bisel frontal con prolongaciones de torres Z = −0.80 mm (True Zero-Support) |
| `ES3C28P_StarWars_Front_Bezel_Multicolor.step` | STEP multicuerpo | Conjunto frontal con cuerpos sólidos separados para asignar colores en el slicer |
| `ES3C28P_StarWars_Rear_Chassis.stl` / `.step` | STL / STEP | Chasis trasero con cavidad superior de altavoz ampliada |
| `ES3C28P_StarWars_Speaker_Mount.stl` / `.step` | STL / STEP | Placa adaptadora para un altavoz rectangular genérico de repuesto |
| `ES3C28P_StarWars_Speaker_Mount_b.stl` / `.step` | STL / STEP | Adaptador alternativo para el **altavoz original de la ES3C28P** (placa de 3.0 mm con rebaje escalonado) — el `.3mf` trae ambas en la placa 3, conserva solo la que necesites |
| `ES3C28P_StarWars_Pocket_Datapad_Assembly.step` | STEP de ensamble | Conjunto CAD completo con componentes internos de referencia |

## STL del bisel frontal en alta resolución

Los cuerpos multicolor del bisel frontal también se publican como STL de alta
resolución, pero son demasiado grandes para el repositorio — se distribuyen
como **assets del Release de GitHub**. Descárgalos desde
[Release v1.0.0](https://github.com/AINxStep/star-wars-pocket-datapad/releases/tag/v1.0.0):

- `ES3C28P_StarWars_Front_Bezel.stl` (~50 MB)
- `ES3C28P_StarWars_Front_White_Inlays.stl` (~55 MB)
- `ES3C28P_StarWars_Front_Accents.stl` (~43 MB)
- `ES3C28P_StarWars_Front_Rebel_Border.stl` (~12 MB)
- `ES3C28P_StarWars_Front_Rebel_Crest.stl` (~8 MB)

El Release también incluye el resto de archivos de impresión por comodidad
(`Rear_Chassis.stl`, `Speaker_Mount.stl` y los dos STL del `Stand`) — el set
STL completo de cada pieza impresa queda reunido en un solo lugar.

Para imprimir normalmente no los necesitas: el proyecto `.3mf` ya empaqueta
todos los cuerpos con su asignación de color. Los STL sueltos están pensados
para armar el proyecto multicolor pieza por pieza en otro slicer.

El diseño físico se licencia bajo [CC BY-NC-SA 4.0](../../LICENSE-HARDWARE.md).
