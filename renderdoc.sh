#!/usr/bin/env bash
# Opens RenderDoc's GUI with the engine preconfigured for capture.
# Usage: ./renderdoc.sh            (override the install dir with RENDERDOC_DIR=...)
# Press F12 in the engine window to capture a frame.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RENDERDOC_DIR="${RENDERDOC_DIR:-$HOME/tools/renderdoc_1.47}"
QRENDERDOC="$RENDERDOC_DIR/bin/qrenderdoc"
ENGINE="$PROJECT_DIR/VulkanEngine"

if [[ ! -x "$QRENDERDOC" ]]; then
    echo "qrenderdoc not found at $QRENDERDOC (set RENDERDOC_DIR)" >&2
    exit 1
fi
if [[ ! -x "$ENGINE" ]]; then
    echo "Engine binary not found at $ENGINE (build it first)" >&2
    exit 1
fi

# A .cap settings file makes qrenderdoc open its Launch Application dialog
# with the executable and working directory filled in, then start the app.
CAP_FILE="$(mktemp --suffix=.cap)"
trap 'rm -f "$CAP_FILE"' EXIT

cat > "$CAP_FILE" <<EOF
{
  "rdocCaptureSettings": 1,
  "settings": {
    "autoStart": true,
    "commandLine": "",
    "environment": [],
    "executable": "$ENGINE",
    "inject": false,
    "numQueuedFrames": 0,
    "options": {},
    "queuedFrameCap": 0,
    "workingDir": "$PROJECT_DIR"
  }
}
EOF

# The engine loads shaders/ and models/ by relative path, so it must start in the project dir.
cd "$PROJECT_DIR"
# Do NOT set VK_LAYER_PATH here: it replaces the loader's default layer search
# path, so VK_LAYER_KHRONOS_validation is no longer found and vkCreateInstance
# fails with VK_ERROR_LAYER_NOT_PRESENT. The RenderDoc layer is registered once
# via qrenderdoc: Tools -> Manage Vulkan Layer Registration.

# Not exec: the trap above has to remove the temp file when the GUI closes.
"$QRENDERDOC" "$CAP_FILE"
