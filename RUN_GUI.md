# GUI One-Click Run

From the project root, run:

```bat
run_gui.bat
```

The first run downloads EasyX for MinGW into `third_party/easyx4mingw`, then builds
and starts `roco_gui.exe`.

Default mode is local PvP:

```bat
set ROCO_GAME_MODE=0
run_gui.bat
```

For PvE, start the Python AI backend in another terminal first:

```bat
set ROCO_AI_POLICY=hard
py -3 -B src_ai\ai_backend\main.py
```

Then run:

```bat
set ROCO_GAME_MODE=1
run_gui.bat
```

To build without launching the GUI:

```bat
build_gui_mingw.bat --no-run
```
