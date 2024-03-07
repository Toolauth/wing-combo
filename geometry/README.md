# /geometry
All special geometry for this project is done in [Inkscape](https://inkscape.org/) and is derived from geometry exported from the board design in [KiCAD](https://www.kicad.org/). 

## file naming conventions
Files that end in `*.multilayer.svg` all have multiple layers that represent the overlapping geometry of the board design. These files are useful for alignment and coordinating shapes with components on the board.

Files that end in `*.flattened.svg` are derived from their multilayer siblings, but have been reduced to just a single path that holds the information needed to import back into the KiCAD board design.

