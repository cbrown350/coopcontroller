Import("env") # type: ignore
import subprocess, platform, os, shutil, gzip
from platformio.project.exception import InvalidProjectConfError # type: ignore

from dev_images import add_diagonal_text


extra_web_files = [
    "logo.webp",
    "favicon.ico"
]
watermark_dev_images = extra_web_files
gzip_extensions = ['.htm', '.css', '.js', '.ico']

def before_buildfs(source, target, env): # NOSONAR - complexity OK
    print("Running pre-action before building file system image...")
    
    data_dir = os.path.join(env.subst('$BUILD_DIR'), "data")
    print(f"Using data directory: {data_dir}")
    www_dir = os.path.join(data_dir, "www")
    build_data_dir = str(source[0]) 
    print(f"Build data directory: {build_data_dir}")
    try:
        web_dir = env.GetProjectOption("custom_WEB_SRC_DIR")
    except InvalidProjectConfError:
        web_dir = "web"
    print(f"Web directory: {web_dir}")
    
    # Create data directory
    print(f"Creating {data_dir}...")
    os.makedirs(data_dir, exist_ok=True)
    
    # Copy project data dir to data_dir
    static_data_src = os.path.join(env.subst('$PROJECT_DIR'), "data")
    LIVE_CONFIG_FILES = {}
    if env.subst('$PIOENV') != "esp32-dev":
        # Live-config files that are gitignored and hold a developer's local board
        # settings. Never bake these into a filesystem image — doing so silently
        # ships a developer's WiFi creds/hostname/BSSID into any board the image
        # gets flashed to (stranded a production board on 2026-07-20 when a local
        # image carrying a bad BSSID was ElegantOTA-flashed to the coop board).
        # CI builds are already clean (gitignored files absent in fresh checkout);
        # this matches local builds to that. Real settings survive flashes via the
        # NVS backup/restore in SettingsManager + the ElegantOTA onStart hook.
        LIVE_CONFIG_FILES = {"user_settings.json", "emulator_settings.json"}
    if os.path.exists(static_data_src):
        print(f"Copying {static_data_src} to {data_dir}...")
        # copy all files and folders except examples and live-config files
        for item in os.listdir(static_data_src):
            if "example" in item.lower():
                continue
            if item in LIVE_CONFIG_FILES:
                continue
            src = os.path.join(static_data_src, item)
            dst = os.path.join(data_dir, item)
            if os.path.isdir(src):
                shutil.copytree(src, dst, dirs_exist_ok=True)
            else:
                shutil.copy2(src, dst)
            print("Copied " + src + " to " + dst)
        
    # Build the web UI with TypeScript and Vite
    print("Building web UI with Vite...")
    os_name = platform.system()
    npm_cmd = "npm.cmd" if os_name == "Windows" else "npm"
    
    # Run npm install
    print("Running npm install...")
    result = subprocess.run([npm_cmd, "install"], cwd=web_dir, capture_output=True, text=True)
    if result.returncode != 0:
        print("npm install output:", result.stdout)
        print("npm install error:", result.stderr)
        print("npm install failed. Stopping build process.")
        env.Exit(1)
    
    # Run TypeScript compilation first
    result = subprocess.run([npm_cmd, "run", "build:ts"], cwd=web_dir, capture_output=True, text=True)
    if result.returncode != 0:
        print("TypeScript compilation output:", result.stdout)
        print("TypeScript compilation error:", result.stderr)
        print("npm build failed. Stopping build process.")
        env.Exit(1)
    
    # Run Vite build
    result = subprocess.run([npm_cmd, "run", "build"], cwd=web_dir, capture_output=True, text=True)
    print(result.stdout)
    if result.returncode != 0:
        print("Error:", result.stderr)
        print("Vite build failed. Stopping build process.")
        env.Exit(1)
    
    # Clean up old files
    print(f"Cleaning up old www files in {build_data_dir} and {www_dir}...")
    if os.path.exists(build_data_dir):
        shutil.rmtree(build_data_dir)
    if os.path.exists(www_dir):
        shutil.rmtree(www_dir)
    
    # Create www directory
    print(f"Creating {www_dir}...")
    os.makedirs(www_dir, exist_ok=True)
    
    # Copy built files from web_dir/dist to www
    print("Copying built files to www directory...")
    dist_dir = os.path.join(web_dir, "dist")
    if os.path.exists(dist_dir):
        for item in os.listdir(dist_dir):
            src = os.path.join(dist_dir, item)
            dst = os.path.join(www_dir, item)
            if os.path.isdir(src):
                shutil.copytree(src, dst, dirs_exist_ok=True)
            else:
                shutil.copy2(src, dst)
    
    # Copy extra web files to www directory
    print("Copying extra files...")    
    for filename in extra_web_files:
        src = os.path.join(web_dir, filename)
        if os.path.exists(src):
            dst = os.path.join(www_dir, filename)
            shutil.copy2(src, dst)
            print(f"Copied: {filename}")
        else:
            print(f"Warning: {filename} not found in web directory")
    
    # Rename index.html to index.htm
    print("Renaming index.html to index.htm...")
    index_html_path = os.path.join(www_dir, "index.html")
    index_htm_path = os.path.join(www_dir, "index.htm")
    if os.path.exists(index_html_path):
        if os.path.exists(index_htm_path):
            os.remove(index_htm_path)
        os.rename(index_html_path, index_htm_path)
        
    # Setp 6: If development build, watermark logos with "DEV"
    if "dev" in env.get("PIOENV").lower():
        for filename in watermark_dev_images:
            file_path =  os.path.join(www_dir, filename)
            if os.path.exists(file_path):
                add_diagonal_text(file_path, file_path, text="DEV")
                print(f"Watermarked for dev: {filename}")
            else:
                print(f"Warning: {filename} not found for watermarking")
    
    # Gzip files with specific extensions
    print("Gzipping files...")
    gzipped_count = 0
    
    for root, dirs, files in os.walk(www_dir):
        for file in files:
            file_path = os.path.join(root, file)
            _, ext = os.path.splitext(file)
            if ext.lower() in gzip_extensions:
                try:
                    gzip_path = file_path + ".gz"
                    with open(file_path, 'rb') as f_in:
                        with gzip.open(gzip_path, 'wb') as f_out:
                            f_out.writelines(f_in)
                    print(f"Gzipped: {file_path}")
                    os.remove(file_path) # remove original file after gzipping
                    gzipped_count += 1
                except Exception as e:
                    print(f"Error gzipping {file_path}: {e}")
    
    print(f"Gzipped {gzipped_count} files")
    
    # Copy data directory files to build data directory
    print(f"Coping data directory {data_dir} to build data directory {build_data_dir}...")
    os.makedirs(build_data_dir, exist_ok=True)
    shutil.copytree(data_dir, build_data_dir, dirs_exist_ok=True)
    print("Web UI build complete!")


def verify_littlefs_bin(source, target, env):
    """Verify the LittleFS binary was created successfully by checking file contents"""
    littlefs_bin = os.path.join(env.subst('$BUILD_DIR'), "littlefs.bin")
    
    if not os.path.exists(littlefs_bin):
        print(f"ERROR: LittleFS binary not found at {littlefs_bin}")
        env.Exit(1)
        return
    
    bin_size = os.path.getsize(littlefs_bin)
    
    # Check the LittleFS superblock magic bytes at expected offset
    try:
        with open(littlefs_bin, 'rb') as f:
            f.seek(8)
            magic = f.read(8)
            if magic != b'littlefs':
                print(f"ERROR: LittleFS binary at {littlefs_bin} has invalid magic bytes - filesystem image may be corrupt!")
                print(f"  Expected: b'littlefs', Got: {magic}")
                print("  This usually means the filesystem ran out of space.")
                print(f"  Binary size: {bin_size} bytes")
                env.Exit(1)
                return
    except Exception as e:
        print(f"ERROR: Failed to verify LittleFS binary: {e}")
        env.Exit(1)
        return
    
    # Verify all source files fit by comparing total data size vs image size
    # LittleFS has metadata overhead, so data must be LESS than image size
    data_dir = os.path.join(env.subst('$BUILD_DIR'), "data")
    if os.path.exists(data_dir):
        total_data_size = 0
        source_files = []
        for root, dirs, files in os.walk(data_dir):
            for file in files:
                file_path = os.path.join(root, file)
                file_size = os.path.getsize(file_path)
                total_data_size += file_size
                rel_path = os.path.relpath(file_path, data_dir)
                source_files.append((rel_path, file_size))
        
        print(f"LittleFS image size: {bin_size} bytes, data content: {total_data_size} bytes")
        
        if total_data_size > bin_size:
            print(f"ERROR: Data content ({total_data_size} bytes) exceeds LittleFS image size ({bin_size} bytes)!")
            print("  The filesystem image is full - not all files were written successfully.")
            print(f"  Overflow: {total_data_size - bin_size} bytes over capacity")
            print("  Source files:")
            for rel_path, file_size in sorted(source_files, key=lambda x: -x[1]):
                print(f"    {file_size:>10,} bytes  {rel_path}")
            env.Exit(1)
            return
        
        # Also verify using mklittlefs --list to count files in the image
        try:
            mklittlefs = env.subst(env.get("MKFSTOOL", "mklittlefs"))
            result = subprocess.run(
                [mklittlefs, "-l", littlefs_bin],
                capture_output=True, text=True
            )
            if result.returncode == 0:
                # Count non-empty lines that represent files (mklittlefs -l output)
                listed_files = [
                    line.strip() for line in result.stdout.strip().splitlines()
                    if line.strip() and not line.strip().startswith('<')
                ]
                image_file_count = len(listed_files)
                source_file_count = len(source_files)
                
                if image_file_count < source_file_count:
                    print(f"ERROR: LittleFS image contains {image_file_count} files but source has {source_file_count} files!")
                    print("  Some files were not written to the image (filesystem full).")
                    print(f"  mklittlefs listing:\n{result.stdout}")
                    env.Exit(1)
                    return
                
                print(f"LittleFS image verified: {image_file_count}/{source_file_count} files, "
                      f"{bin_size:,} bytes (data: {total_data_size:,} bytes, "
                      f"free: ~{bin_size - total_data_size:,} bytes)")
            else:
                # mklittlefs --list failed, fall back to size-only check
                print(f"Warning: Could not list LittleFS image contents (mklittlefs returned {result.returncode})")
                print(f"LittleFS image size check passed: {bin_size:,} bytes (data: {total_data_size:,} bytes)")
        except FileNotFoundError:
            print(f"Warning: mklittlefs not found, skipping file count verification")
            print(f"LittleFS image size check passed: {bin_size:,} bytes (data: {total_data_size:,} bytes)")
    else:
        print(f"LittleFS image verified: {bin_size} bytes")


env.AddPreAction("$BUILD_DIR/littlefs.bin", before_buildfs) # type: ignore
env.AddPostAction("$BUILD_DIR/littlefs.bin", verify_littlefs_bin) # type: ignore
# clean up files
env.AddPostAction("$BUILD_DIR/littlefs.bin", lambda target, source, env: shutil.rmtree(str(source[0]))) # type: ignore
