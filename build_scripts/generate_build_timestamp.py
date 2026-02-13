"""
Pre-build script to generate build timestamp header.
This captures the actual project build time and generates a header file with the timestamp.
"""
Import("env", "projenv")  # type: ignore
from datetime import datetime
import os

def generate_timestamp_header(source, target, env):
    """Generate build_timestamp.h with current build date/time"""
    # Get current time in UTC in the same format as __DATE__ and __TIME__
    now = datetime.utcnow()
    
    # __DATE__ format: "Feb 10 2026"
    build_date = now.strftime("%b %d %Y")
    
    # __TIME__ format: "20:23:17"
    build_time = now.strftime("%H:%M:%S")
    
    # Create header file content
    header_content = f"""// Auto-generated build timestamp header
// Generated at build time
#ifndef BUILD_TIMESTAMP_H
#define BUILD_TIMESTAMP_H

#define BUILD_TIMESTAMP_DATE "{build_date}"
#define BUILD_TIMESTAMP_TIME "{build_time} UTC"
#define BUILD_TIMESTAMP "{now.strftime("%Y-%m-%dT%H%M%SZ")}"

#endif // BUILD_TIMESTAMP_H
"""
    
    # Ensure include directory exists
    include_dir = "include"
    if not os.path.exists(include_dir):
        os.makedirs(include_dir)
    
    # Write header file
    header_path = os.path.join(include_dir, "build_timestamp.h")
    with open(header_path, 'w') as f:
        f.write(header_content)
    
    print(f"[BUILD] Generated build timestamp: {build_date} {build_time}")

# Generate the timestamp header immediately when this script is loaded
generate_timestamp_header(None, None, env) # type: ignore
