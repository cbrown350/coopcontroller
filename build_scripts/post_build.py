Import("env", "projenv")  # type: ignore
import subprocess

def after_build(source, target, env):
    result = subprocess.run(["pio", "run", "-t", "compiledb"], capture_output=True, text=True)
    print(result.stdout)
    if result.returncode != 0:
        print("Error:", result.stderr)

env.AddPostAction("buildprog", after_build)  # type: ignore
