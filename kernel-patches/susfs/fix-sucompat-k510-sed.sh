#!/bin/bash
# Fix sucompat.c conflicting types for GKI 5.10
# Change KERNEL_VERSION(5, 10, 0) to KERNEL_VERSION(6, 1, 0) to match sucompat.h declaration

TARGET="drivers/kernelsu/feature/sucompat.c"

if [ -f "$TARGET" ]; then
    # Fix: Change 5.10.0 to 6.1.0 in the #if conditional for ksu_handle_stat
    if grep -q "LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) && defined(CONFIG_KSU_SUSFS)" "$TARGET"; then
        sed -i 's/LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) \&\& defined(CONFIG_KSU_SUSFS)/LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0) \&\& defined(CONFIG_KSU_SUSFS)/' "$TARGET"
        echo "[✓] sucompat.c fixed: ksu_handle_stat now uses KERNEL_VERSION(6,1,0)"
    else
        echo "[!] Pattern not found in sucompat.c"
    fi
else
    echo "[!] sucompat.c not found!"
fi
