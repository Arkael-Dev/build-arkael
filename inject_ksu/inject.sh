#!/usr/bin/env bash

# Define Kconfig Hz file location
KCONFIG_HZ="kernel/Kconfig.hz"

echo " Applying 1000Hz patch to $KCONFIG_HZ..."

# Check if target file exists
if [ ! -f "$KCONFIG_HZ" ]; then
    echo "Error: File $KCONFIG_HZ not found."
    echo "Ensure the script is run from the kernel source root directory."
    exit 1
fi

# Perform replacement configuration to ManualHz
sed -i 's/config HZ_1000/config HZ_1000/g' "$KCONFIG_HZ"
sed -i 's/bool "1000 HZ"/bool "1000 HZ"/g' "$KCONFIG_HZ"
sed -i 's/default 1000 if HZ_1000/default 1000 if HZ_1000/g' "$KCONFIG_HZ"

echo " Successfully patched kernel for 1000Hz support."
