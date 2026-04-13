/*!
 * © 2024-2026 Kingfinik98 (VorteX_E-Sport). All Rights Reserved.
 * Original Author: Kingfinik98
 * Original Repository: https://github.com/Kingfinik98/build-vortex
 * Modifying or claiming this as your own (e.g., Generic Zixine) is prohibited.
 */

#!/usr/bin/env bash
set -x
__DIR="$(dirname "$(realpath "$0")")"
if ! command -v shfmt &> /dev/null; then
  echo "Installing shfmt..."
  sleep 1
  sudo apt-get update -qq && sudo apt-get install -qq shfmt
fi
find "$__DIR" -name "*.sh" -exec shfmt -w -i 2 -ci -sr -bn {} +
