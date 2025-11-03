# /docs
Here are all the basic documentation items:
* diagrams
* images
* layouts
* configurations

These items should help with setup

## Image Naming conventions
The images will all use a standard naming convention, so as to not break links on the wiki or Toolauth site. If you need to generate new images for a new design, be sure to use the same exact names. Also, the `map-locations.svg` file may be helpful.

- `docs\img\map\location-*.png` are images that show where specific parts are located on the board.
    - NOTE: in the `docs\img\map\` repo, there is an SVG file: `map-locations.svg` with many layers to generate the "location" images using [Inkscape](https://inkscape.org/). 
- `docs\img\map\sq-*.png` are the square(ish) images that serve as the foundation for all the location images, and generally take up less screen-area. Cropped down to size from "ortho" images using [Gimp](https://www.gimp.org/).
- `docs\img\map\ortho-*.png` are the orthographic perspective "base" images, from which all the square and location images are derived. Exported from [FreeCAD](https://www.freecad.org/) extension of [KiCAD](https://www.kicad.org/) (accessible from the `alt + 3` command).
- `docs\img\offset-*.png` are the "hero" images that use perspective (which blocks some viewing) and look the most realistic.
