# OTA only: default espota uses --progress with \r; Cursor/VS Code terminals
# often show each redraw as a new line. Dots per chunk still show activity.
Import("env")

if env.subst("$UPLOAD_PROTOCOL") == "espota":
    env.Replace(UPLOADERFLAGS=["-i", "$UPLOAD_PORT"])
