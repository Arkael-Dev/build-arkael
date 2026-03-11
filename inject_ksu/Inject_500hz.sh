#!/usr/bin/env bash

# Define Kconfig Hz file location
KCONFIG_HZ="kernel/Kconfig.hz"

echo " Applying 500Hz patch to $KCONFIG_HZ..."

# Check if target file exists
if [ ! -f "$KCONFIG_HZ" ]; then
    echo "Error: File $KCONFIG_HZ not found."
    echo "Ensure the script is run from the kernel source root directory."
    exit 1
fi

# Perform replacement configuration to ManualHz
sed -i 's/config HZ_500/config HZ_500/g' "$KCONFIG_HZ"
sed -i 's/bool "500 HZ"/bool "500 HZ"/g' "$KCONFIG_HZ"
sed -i 's/default 500 if HZ_500/default 500 if HZ_500/g' "$KCONFIG_HZ"

echo " Successfully patched kernel for 500Hz support."
