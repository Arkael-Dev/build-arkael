#!/bin/bash

# Fix for btqca.c error: undeclared identifier 'QCA_WCN3988'
# This script adds the missing enum definition to btqca.h

TARGET_FILE="drivers/bluetooth/btqca.h"

if [ -f "$TARGET_FILE" ]; then
    # Check if already patched to avoid duplication
    if grep -q "QCA_WCN3988" "$TARGET_FILE"; then
        echo "[INFO] Patch has already been applied: QCA_WCN3988 already exists in $TARGET_FILE"
    else
        echo "[FIX] Adding QCA_WCN3988 definition to $TARGET_FILE..."
        
        # Insert 'QCA_WCN3988,' after the line 'QCA_WCN3998,'
        # Using a tab for indentation to match kernel style
        sed -i '/QCA_WCN3998,/a\	QCA_WCN3988,' "$TARGET_FILE"
        
        echo "[SUCCESS] Patch successfully applied."
    fi
else
    echo "[ERROR] File $TARGET_FILE not found!"
    exit 1
fi
