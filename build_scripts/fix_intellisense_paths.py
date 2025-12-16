"""
PlatformIO post-initialization script for unit-test-desktop environment.

This script modifies the auto-generated .vscode/c_cpp_properties.json file to replace
the {project_dir}/src path with {project_dir}/test/desktop in both the includePath
and browse.path arrays. This ensures IntelliSense correctly resolves paths for the
desktop unit test environment instead of the embedded src directory.

This script runs automatically after the "Rebuild IntelliSense Index" task completes.
"""

import os
import json

Import("env")  # type: ignore # PlatformIO environment


# def replace_cpp_properties_paths(source=None, target=None, env=None, **kwargs):
#     """
#     Post-processing callback to fix IntelliSense paths for unit-test-desktop.
    
#     Replaces /src with /test/desktop in both includePath and browse.path arrays.
    
#     Args:
#         source: Build source (unused, required by PlatformIO callback signature)
#         target: Build target (unused, required by PlatformIO callback signature)
#         env: PlatformIO environment (passed by SCons as keyword argument)
#         **kwargs: Additional keyword arguments from SCons
#     """
#     # Use global env if not provided as argument
#     if env is None:
#         from SCons.Script import Import  # type: ignore
#         Import("env")  # type: ignore
#         env_obj = env  # type: ignore
#     else:
#         env_obj = env
    
#     project_dir = env_obj.get("PROJECT_DIR")
#     cpp_props_file = os.path.join(project_dir, ".vscode", "c_cpp_properties.json")
    
#     if not os.path.exists(cpp_props_file):
#         print(f"Warning: c_cpp_properties.json not found at {cpp_props_file}")
#         return
    
#     try:
#         # Read the JSON file (with comments support)
#         with open(cpp_props_file, 'r', encoding='utf-8') as f:
#             content = f.read()
        
#         # Normalize project directory path to use forward slashes
#         project_dir_normalized = project_dir.replace('\\', '/')
#         src_path = f"{project_dir_normalized}/src"
#         test_desktop_path = f"{project_dir_normalized}/test/desktop"
        
#         # Simple string replacement approach (handles JSON with comments)
#         if src_path in content:
#             modified_content = content.replace(src_path, test_desktop_path)
            
#             # Write back the modified content
#             with open(cpp_props_file, 'w', encoding='utf-8') as f:
#                 f.write(modified_content)
            
#             print(f"✓ Fixed IntelliSense paths: replaced '{src_path}' with '{test_desktop_path}'")
#         else:
#             print("ℹ No path replacement needed (src path not found)")
            
#     except Exception as e:
#         print(f"Error fixing c_cpp_properties.json: {e}")


def add_desktop_cpp_properties_paths(source=None, target=None, env=None, **kwargs): # NOSONAR
    """
    Ensure the VS Code c_cpp_properties.json includes the desktop unit test path
    in both includePath and browse.path arrays. This helps IntelliSense resolve
    headers for the desktop test environment.

    Runs as a lightweight post step in PlatformIO.
    """
    # Resolve PlatformIO env
    try:
        env_obj = env if env is not None else globals().get("env")
    except Exception:
        env_obj = None

    if not env_obj:
        # Fallback to current working directory if env is unavailable
        project_dir = os.getcwd()
    else:
        project_dir = env_obj.get("PROJECT_DIR", os.getcwd())

    cpp_props_file = os.path.join(project_dir, ".vscode", "c_cpp_properties.json")
    if not os.path.exists(cpp_props_file):
        print(f"Warning: c_cpp_properties.json not found at {cpp_props_file}")
        return

    # Normalize project directory with forward slashes (VS Code prefers this)
    project_dir_normalized = project_dir.replace("\\", "/")
    test_desktop_path = f"{project_dir_normalized}/test/desktop"

    try:
        with open(cpp_props_file, "r", encoding="utf-8") as f:
            original_lines = f.readlines()

        # Preserve header comment lines and strip them from JSON parsing
        header_comment_lines = []
        json_lines = []
        for line in original_lines:
            if line.strip().startswith("//"):
                header_comment_lines.append(line)
            else:
                json_lines.append(line)

        json_text = "".join(json_lines).strip()
        data = json.loads(json_text)

        changed = False
        # Iterate configurations (usually one named "PlatformIO")
        for cfg in data.get("configurations", []):
            # includePath update
            include_list = cfg.get("includePath", [])
            # Remove empty entries
            include_list = [p for p in include_list if isinstance(p, str) and p.strip()]
            include_list_norm = [p.replace("\\", "/") for p in include_list]
            if test_desktop_path not in include_list_norm:
                include_list.append(test_desktop_path)
                cfg["includePath"] = include_list
                changed = True

            # browse.path update
            browse = cfg.get("browse", {})
            browse_paths = browse.get("path", [])
            # Remove empty entries
            browse_paths = [p for p in browse_paths if isinstance(p, str) and p.strip()]
            browse_paths_norm = [p.replace("\\", "/") for p in browse_paths]
            if test_desktop_path not in browse_paths_norm:
                browse_paths.append(test_desktop_path)
                browse["path"] = browse_paths
                cfg["browse"] = browse
                changed = True

        if changed:
            # Write back, preserving header comments
            with open(cpp_props_file, "w", encoding="utf-8") as f:
                if header_comment_lines:
                    f.writelines(header_comment_lines)
                json.dump(data, f, indent=4)
                f.write("\n")
            print(f"✓ IntelliSense paths ensured: added '{test_desktop_path}' where missing")
        else:
            print("ℹ IntelliSense paths already include desktop test path; no changes made")

    except Exception as e:
        print(f"Error updating c_cpp_properties.json: {e}")
        

# Run once on any SCons invocation (build/test/clean) to keep IntelliSense paths fresh
try:
    add_desktop_cpp_properties_paths(env=env)  # type: ignore
except Exception as _e:
    pass

# Register broad post-actions so it also runs after typical build targets
# try:
#     # After main program build (covers `pio run`)
#     env.AddPostAction("buildprog", fix_cpp_properties_paths)  # type: ignore
#     # After compilation database generation (covers `pio run -t compiledb`)
#     env.AddPostAction("compiledb", fix_cpp_properties_paths)  # type: ignore
#     # Fallback: after specific output program, when applicable
#     env.AddPostAction("$BUILD_DIR/${PROGNAME}$PROGSUFFIX", fix_cpp_properties_paths)  # type: ignore
#     # Custom alias to run manually if needed
#     env.AlwaysBuild(env.Alias("fix_intellisense", None, fix_cpp_properties_paths))  # type: ignore
# except Exception:
#     pass

