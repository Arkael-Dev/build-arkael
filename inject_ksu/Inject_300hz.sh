/*!
 * © 2024-2026 Kingfinik98 (VorteX_E-Sport). All Rights Reserved.
 * Original Author: Kingfinik98
 * Original Repository: https://github.com/Kingfinik98/build-vortex
 * Modifying or claiming this as your own (e.g., Generic Zixine) is prohibited.
 */

#!/usr/bin/env bash

# Define Kconfig Hz file location
KCONFIG_HZ="kernel/Kconfig.hz"

echo " Applying 300Hz patch to $KCONFIG_HZ..."

# Check if target file exists
if [ ! -f "$KCONFIG_HZ" ]; then
    echo "Error: File $KCONFIG_HZ not found."
    echo "Ensure the script is run from the kernel source root directory."
    exit 1
fi

# Perform replacement configuration to ManualHz
sed -i 's/config HZ_300/config HZ_300/g' "$KCONFIG_HZ"
sed -i 's/bool "300 HZ"/bool "300 HZ"/g' "$KCONFIG_HZ"
sed -i 's/default 300 if HZ_300/default 300 if HZ_300/g' "$KCONFIG_HZ"

echo " Successfully patched kernel for 300Hz support."
