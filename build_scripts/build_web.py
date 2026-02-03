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
    
    # If user_settins.json exists in data dir, copy it to data_dir
    static_data_src = os.path.join(env.subst('$PROJECT_DIR'), "data")
    if os.path.exists(static_data_src):
        # copy all files and folders except those with "example" in the name
        for item in os.listdir(static_data_src):
            if "example" in item.lower():
                continue
            src = os.path.join(static_data_src, item)
            dst = os.path.join(data_dir, item)
            if os.path.isdir(src):
                shutil.copytree(src, dst, dirs_exist_ok=True)
            else:
                shutil.copy2(src, dst)
        
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
    print(f"Cleaning up old www files in {build_data_dir}...")
    if os.path.exists(build_data_dir):
        shutil.rmtree(build_data_dir)
    
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
                    gzipped_count += 1
                except Exception as e:
                    print(f"Error gzipping {file_path}: {e}")
    
    print(f"Gzipped {gzipped_count} files")
    
    # Copy data directory files to build data directory
    print(f"Coping data directory {data_dir} to build data directory {build_data_dir}...")
    os.makedirs(build_data_dir, exist_ok=True)
    shutil.copytree(data_dir, build_data_dir, dirs_exist_ok=True)
    print("Web UI build complete!")


env.AddPreAction("$BUILD_DIR/littlefs.bin", before_buildfs) # type: ignore
# clean up files
env.AddPostAction("$BUILD_DIR/littlefs.bin", lambda target, source, env: shutil.rmtree(str(source[0]))) # type: ignore
