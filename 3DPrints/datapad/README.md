# Datapad enclosure print files (ES3C28P)

[Español](README.es.md) · [Project overview](../../README.md)

Manufacturing files for the Star Wars datapad enclosure. See
[docs/enclosure.md](../../docs/enclosure.md) for the full printing guide,
calibrated dimensions and assembly notes.

## Files

| File | Type | Description |
| :--- | :--- | :--- |
| `ES3C28P_StarWars_Datapad.3mf` | Bambu Studio 3MF | **Complete 3-plate preconfigured project** (Front, Rear, Speaker Mount) — everything needed to print |
| `ES3C28P_StarWars_Front_Bezel.step` | STEP | Front bezel with Z = −0.80 mm boss extensions (True Zero-Support) |
| `ES3C28P_StarWars_Front_Bezel_Multicolor.step` | Multi-body STEP | Front assembly with separated solid bodies for multi-color slicer assignment |
| `ES3C28P_StarWars_Rear_Chassis.stl` / `.step` | STL / STEP | Rear chassis with widened upper speaker cavity |
| `ES3C28P_StarWars_Speaker_Mount.stl` / `.step` | STL / STEP | Adapter plate for a generic rectangular replacement speaker |
| `ES3C28P_StarWars_Speaker_Mount_b.stl` / `.step` | STL / STEP | Alternate speaker adapter (3.0 mm plate with stepped recess) for a different replacement speaker — the `.3mf` ships both on plate 3, keep only the one you need |
| `ES3C28P_StarWars_Pocket_Datapad_Assembly.step` | Assembly STEP | Full CAD assembly with internal component mockups |

## High-resolution front-bezel STLs

The multi-color front-bezel bodies also ship as high-resolution STLs, but
they are too large for the repository — they are published as **GitHub
Release assets** instead. Download them from
[Release v1.0.0](https://github.com/AINxStep/star-wars-pocket-datapad/releases/tag/v1.0.0):

- `ES3C28P_StarWars_Front_Bezel.stl` (~50 MB)
- `ES3C28P_StarWars_Front_White_Inlays.stl` (~55 MB)
- `ES3C28P_StarWars_Front_Accents.stl` (~43 MB)
- `ES3C28P_StarWars_Front_Rebel_Border.stl` (~12 MB)
- `ES3C28P_StarWars_Front_Rebel_Crest.stl` (~8 MB)

The release also carries the remaining print files for convenience
(`Rear_Chassis.stl`, `Speaker_Mount.stl` and the two `Stand` STLs) — the
complete STL set for every printed part lives in one place.

For printing you normally don't need them: the `.3mf` project already packs
every body with its color assignment. The loose STLs are meant for building
the multi-color project piece by piece in a different slicer.

Physical design is licensed under [CC BY-NC-SA 4.0](../../LICENSE-HARDWARE.md).
