Import("env")

def skip_m5gfx_fonts(node):
    # node.get_path() is the relative path of the file being compiled
    path = node.get_path()
    
    # Exclude specific font directories or files
    # M5GFX fonts are typically in lgfx/fonts/
    if "M5GFX" in path and "Fonts" in path:
        print(f"Skipping M5GFX font: {path}")
        return None

    if "M5GFX" in path and "utility" in path:
        print(f"Skipping M5GFX utility: {path}")
        return None

    return node

# Register the middleware to filter files
env.AddBuildMiddleware(skip_m5gfx_fonts, "*")
