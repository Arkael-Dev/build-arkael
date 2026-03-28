#!/bin/bash

# Fix for btqca.c error: undeclared identifier 'QCA_WCN3988'
# This script adds the missing enum definition to btqca.h

TARGET_FILE="drivers/bluetooth/btqca.h"

if [ -f "$TARGET_FILE" ]; then
    # Check if already patched to avoid duplication
    if grep -q "QCA_WCN3988" "$TARGET_FILE"; then
        echo "[INFO] Patch sudah diterapkan: QCA_WCN3988 sudah ada di $TARGET_FILE"
    else
        echo "[FIX] Menambahkan definisi QCA_WCN3988 ke $TARGET_FILE..."
        
        # Menyisipkan 'QCA_WCN3988,' setelah baris 'QCA_WCN3998,'
        # Menggunakan tab untuk indentasi agar sesuai style kernel
        sed -i '/QCA_WCN3998,/a\	QCA_WCN3988,' "$TARGET_FILE"
        
        echo "[SUCCESS] Patch berhasil diterapkan."
    fi
else
    echo "[ERROR] File $TARGET_FILE tidak ditemukan!"
    exit 1
fi
