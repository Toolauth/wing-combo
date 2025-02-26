# /odd-parts
Here are all the components that are needed for the board design, but are not in the standard KiCAD library.

Include as much information as possible about these parts, so that they can be easily used, updated, researched, etc.

*****

The best tool to get this information is [JLC2KiCad_lib](https://github.com/TousstNicolas/JLC2KiCad_lib): a python script that gets all the files for a part from JLCPCB, and can store them here. 

example usage (make `some_accurate_name` a meaningful thing for this part):
```
JLC2KiCadLib C24112 -dir some_accurate_name                \
                    -model_dir some_accurate_name\model    \
                    -footprint_lib some_accurate_name      \
                    -symbol_lib_dir some_accurate_name\lib \
                    -symbol_lib some_accurate_name
```
Just need the part number as a positional argument. 

For our purposes, it is probably best if you stick to these conventions: each odd-part gets its own folder inside this one, and its own self-titled KiCad library.