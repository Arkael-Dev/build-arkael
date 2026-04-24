#!/bin/bash
# FIX SUSFS v2.1.0 COMPATIBILITY FOR GKI 6.1
# Run this AFTER git clone but BEFORE make defconfig

echo "[FIX] Applying SUSFS v2.1.0 compatibility fixes for GKI 6.1..."

KSRC="${1:-$PWD/ksrc}"

if [ -f "$KSRC/fs/namespace.c" ]; then
    # Fix 1: Replace old variable names in namespace.c if exists
    if grep -q "susfs_set_sdcard_android_data_decrypted_key_false" "$KSRC/fs/namespace.c"; then
        sed -i 's/susfs_set_sdcard_android_data_decrypted_key_false/susfs_is_sdcard_android_data_not_decrypted/g' "$KSRC/fs/namespace.c"
        sed -i 's/static_key_false/static_key_true/g' "$KSRC/fs/namespace.c"
        echo "[✓] Fixed namespace.c variable names"
    fi
    
    # Fix 2: Add missing extern declaration if not present
    if ! grep -q "extern struct static_key_true susfs_is_sdcard_android_data_not_decrypted" "$KSRC/fs/namespace.c"; then
        if grep -q "CONFIG_KSU_SUSFS_SUS_MOUNT" "$KSRC/fs/namespace.c"; then
            sed -i '/#include "internal.h"/a\\n#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT\nextern struct static_key_true susfs_is_sdcard_android_data_not_decrypted;\n#endif' "$KSRC/fs/namespace.c"
            echo "[✓] Added missing extern declaration to namespace.c"
        fi
    fi
fi

if [ -f "$KSRC/drivers/kernelsu/extras.c" ]; then
    # Fix 3: Fix type mismatch in extras.c (AVC spoofing)
    if grep -q "bool susfs_is_avc_log_spoofing_enabled = false" "$KSRC/drivers/kernelsu/extras.c"; then
        sed -i 's/bool susfs_is_avc_log_spoofing_enabled = false;/DEFINE_STATIC_KEY_FALSE(susfs_is_avc_log_spoofing_key_true);/' "$KSRC/drivers/kernelsu/extras.c"
        echo "[✓] Fixed extras.c AVC spoofing type mismatch"
    fi
fi

echo "[DONE] All SUSFS v2.1.0 fixes applied!"
