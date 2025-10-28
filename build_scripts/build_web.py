Import("env") # type: ignore
import subprocess, platform

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

env.AddPreAction("$BUILD_DIR/littlefs.bin", before_buildfs) # type: ignore
