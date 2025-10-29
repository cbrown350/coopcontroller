Import("env") # type: ignore
import subprocess, platform, sys

def before_buildfs(source, target, env):
    print("Running pre-action before building file system image...")
    
    os_name = platform.system()
    npm_cmd = "npm"
    if os_name == "Windows":
        npm_cmd = "npm.cmd"
        
    result = subprocess.run([npm_cmd, "run", "build"], cwd="web", capture_output=True, text=True)
    print(result.stdout)
    if result.returncode != 0:
        print("Error:", result.stderr)
        print("npm build failed. Stopping build process.")
        env.Exit(1)  # Exit with error code to stop the build process

env.AddPreAction("$BUILD_DIR/littlefs.bin", before_buildfs) # type: ignore
