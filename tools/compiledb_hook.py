# Post-build hook: regenerate compile_commands.json after every build so
# clangd always has entries for newly added source files — and, more
# importantly, for newly used libraries. PlatformIO's LDF only adds a
# library's include dirs once some source file uses it, so without this
# hook clangd shows "<header> file not found" for every new dependency
# until someone remembers to run `pio run -t compiledb` manually.
Import("env")
import shutil
import subprocess
import sys

def generate_compilation_db(source, target, env):
    pio = shutil.which("pio")
    cmd = ([pio] if pio else [sys.executable, "-m", "platformio"]) + \
        ["run", "-e", env["PIOENV"], "-t", "compiledb"]
    subprocess.call(cmd, cwd=env["PROJECT_DIR"])

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", generate_compilation_db)
