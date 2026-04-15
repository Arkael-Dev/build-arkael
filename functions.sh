#!/usr/bin/env bash

#/*!
# * © 2024-2026 Kingfinik98 (VorteX_E-Sport). All Rights Reserved.
# * Original Author: Kingfinik98
# * Original Repository: https://github.com/Kingfinik98/build-vortex
# * Signed-off-by: kingfinix98@gmail.com
# */

# ==============
#    Functions
# ==============

# ============================================
# TELEGRAM FUNCTIONS
# ============================================

# upload_file <file_path> [caption]
# Upload file to Telegram chat
# Usage: upload_file "/path/to/file.zip" "Build completed"
upload_file() {
  local FILE="$1"
  local CAPTION="${2:-}"

  if ! [ -f $FILE ]; then
    error "file $FILE doesn't exist"
  fi

  chmod 777 "$FILE"

  curl -s -F "document=@${FILE}" \
    -F "chat_id=${TG_CHAT_ID}" \
    -F "caption=${CAPTION}" \
    -F "parse_mode=markdown" \
    -F "disable_web_page_preview=true" \
    "https://api.telegram.org/bot${TG_BOT_TOKEN}/sendDocument"
}

# send_msg <message>
# Send markdown message to Telegram chat
# Usage: send_msg "*Build Success!* Kernel compiled."
send_msg() {
  local MESSAGE="$1"
  curl -s -X POST "https://api.telegram.org/bot$TG_BOT_TOKEN/sendMessage" \
    -d "chat_id=$TG_CHAT_ID" \
    -d "disable_web_page_preview=true" \
    -d "parse_mode=markdown" \
    -d "text=$MESSAGE"
}

# ============================================
# KERNELSU INSTALLATION FUNCTION
# ============================================

# install_ksu <user/repo> <ref>
# Download and execute KernelSU setup script from GitHub
# Usage: install_ksu 'pershoot/KernelSU-Next' 'dev-susfs'
install_ksu() {
  local REPO="$1"
  local REF="$2"
  local URL

  if [ -z "$REPO" ] || [ -z "$REF" ]; then
    echo "Usage: install_ksu <user/repo> <ref>"
    exit 1
  fi

  URL="https://raw.githubusercontent.com/$REPO/$REF/kernel/setup.sh"
  log "Installing KernelSU from $REPO | $REF"
  curl -LSs "$URL" | bash -s "$REF"
}

# ============================================
# VARIANT DETECTION FUNCTIONS
# ============================================

# ksu_included()
# Type: bool
# Return: True (0) if $KSU is standard KernelSU variant
# 
# SUPPORTED VALUES:
#   "yes"      -> Standard KernelSU-Next (TRUE)
#   "vortexsu" -> VorteXSU (FALSE - has own block)
#   "sukisu"   -> SukiSU Ultra (FALSE - has own block)
#   "wildksu"  -> WildKSU (FALSE - has own block)
#   "no"       -> Vanilla/No Root (FALSE)
#
# Note: This function ONLY returns true for standard KSU setup.
#       Other variants have their own dedicated setup blocks in build.sh
ksu_included() {
  [ "$KSU" == "yes" ]
  return $?
}

# kpm_enabled()
# Type: bool
# Return: True (0) if variant requires KPM (Kernel Patch Module)
#
# VARIANTS WITH KPM:
#   "vortexsu" -> VorteXSU (uses KPM patch)
#   "sukisu"   -> SukiSU Ultra (uses KPM patch, same family as VorteXSU)
#
# VARIANTS WITHOUT KPM:
#   "yes"      -> Standard KernelSU-Next (no KPM)
#   "wildksu"  -> WildKSU (no KPM)
#   "no"       -> Vanilla (no KPM)
kpm_enabled() {
  [ "$KSU" == "vortexsu" ] || [ "$KSU" == "sukisu" ]
  return $?
}

# is_ksu_variant()
# Type: bool
# Return: True (0) if ANY KSU-based root is enabled (including custom variants)
# This is useful for general KSU detection regardless of variant type
is_ksu_variant() {
  [ "$KSU" == "yes" ] || [ "$KSU" == "vortexsu" ] || [ "$KSU" == "sukisu" ] || [ "$KSU" == "wildksu" ]
  return $?
}

# ============================================
# SUSFS DETECTION FUNCTION
# ============================================

# susfs_included()
# Type: bool
# Return: True (0) if $KSU_SUSFS is "true"
#
# Used by build.sh to decide whether to:
# - Clone SUSFS patches (Standard Method)
# - Apply statfs CRC fixes
# - Enable CONFIG_KSU_SUSFS
susfs_included() {
  [ "$KSU_SUSFS" == "true" ]
  return $?
}

# ============================================
# UTILITY FUNCTIONS
# ============================================

# simplify_gh_url <github-repository-url>
# Extract user/repo from full GitHub URL
# Input:  "https://github.com/user/repo.git"
# Output: "user/repo"
simplify_gh_url() {
  local URL="$1"
  echo "$URL" | sed "s|https://github.com/||g" | sed "s|.git||g"
}

# get_variant_name()
# Get human-readable variant name from $KSU value
# Returns: "KSU", "VorteXSU", "SukiSU", "WildKSU", or "VNL"
get_variant_name() {
  case "$KSU" in
    "yes")      echo "KSU" ;;
    "vortexsu") echo "VorteXSU" ;;
    "sukisu")   echo "SukiSU" ;;
    "wildksu")  echo "WildKSU" ;;
    "no")       echo "VNL" ;;
    *)          echo "Unknown" ;;
  esac
}

# ============================================
# KERNEL CONFIG FUNCTIONS
# ============================================

# config <kernel_config_options>
# Wrapper for scripts/config with defconfig file path
# Usage: 
#   config --enable CONFIG_KSU
#   config --disable CONFIG_LOCALVERSION_AUTO
#   config --set-str CONFIG_LOCALVERSION "-MyKernel"
config() {
  $KSRC/scripts/config --file $DEFCONFIG_FILE $@
}

# ============================================
# LOGGING FUNCTIONS
# ============================================

# log <message>
# Print formatted log message with timestamp prefix
log() {
  echo -e "[LOG] $(date '+%H:%M:%S') $*"
}

# error <message>
# Print error message, notify via Telegram, upload build.log, and exit(1)
error() {
  local err_txt
  err_txt=$(
    cat << EOF
*🔴 VorteX_E-Sport Build ERROR*

❌ Error: $*
📅 Time: $(date)
🐧 Kernel: ${LINUX_VERSION:-unknown}
📛 Variant: $(get_variant_name)

_Check build.log for details_
EOF
  )
  echo -e "[ERROR] $(date '+%H:%M:%S') $*"
  send_msg "$err_txt"
  upload_file "$WORKDIR/build.log"
  exit 1
}

# warn <message>
# Print warning message (non-fatal) with optional Telegram notification
warn() {
  local MESSAGE="$1"
  local NOTIFY="${2:-false}"
  
  echo -e "[WARN] $(date '+%H:%M:%S') $*"
  
  if [ "$NOTIFY" == "true" ]; then
    send_msg "⚠️ *Warning:* $MESSAGE"
  fi
}

# success <message>
# Print success message with optional Telegram notification
success() {
  local MESSAGE="$1"
  local NOTIFY="${2:-false}"
  
  echo -e "[✅ SUCCESS] $(date '+%H:%M:%S') $*"
  
  if [ "$NOTIFY" == "true" ]; then
    send_msg "✅ $MESSAGE"
  fi
}

# ============================================
# VARIANT-SPECIFIC SETUP HELPERS
# ============================================

# setup_ksu_variant()
# Main dispatcher for KSU variant setup
# Automatically calls the correct setup based on $KSU value
setup_ksu_variant() {
  log "Setting up KSU variant: $(get_variant_name)"
  
  case "$KSU" in
    "yes")
      # Standard KernelSU-Next - handled by main build.sh ksu_included block
      log "Standard KSU variant will be handled by main build block."
      ;;
    "vortexsu")
      log "VorteXSU variant will be handled by dedicated setup block."
      ;;
    "sukisu")
      log "SukiSU Ultra variant will be handled by dedicated setup block."
      ;;
    "wildksu")
      log "WildKSU variant will be handled by dedicated setup block."
      ;;
    "no")
      log "Vanilla variant selected - no root will be installed."
      ;;
    *)
      warn "Unknown KSU variant: $KSU"
      ;;
  esac
}

# apply_stack_protector_fix()
# Apply stack protector fix for GKI 5.10 (duplicate __stack_chk_guard symbol)
# Required for: All KSU variants on kernel 5.10
apply_stack_protector_fix() {
  if [ "$KVER" == "5.10" ]; then
    if [ -f "drivers/kernelsu/ksu.c" ]; then
      sed -i '/^#if.*CONFIG_STACKPROTECTOR_PER_TASK/c\#if 0 \/\/ Disabled to fix duplicate symbol' drivers/kernelsu/ksu.c || true
      log "Stack protector fix applied for $(get_variant_name)."
      return 0
    else
      log "Skipping stack protector fix (drivers/kernelsu/ksu.c not found)."
      return 1
    fi
  else
    log "Stack protector fix not required for KVER $KVER (only needed for 5.10)."
    return 0
  fi
}

# apply_statfs_susfs_fix()
# Apply manual statfs CRC fix for SuSFS on GKI 6.1/6.6
# Required when SuSFS is enabled on kernel 6.x
apply_statfs_susfs_fix() {
  if [ $(echo "$LINUX_VERSION_CODE" | head -c1) -eq 6 ]; then
    if [ "$KVER" == "6.1" ] || [ "$KVER" == "6.6" ]; then
      if [ -f "fs/statfs.c" ]; then
        log "Applying manual statfs CRC fix for $(get_variant_name) GKI $KVER..."
        sed -i '/#include <linux\/susfs_def.h>/i #ifndef __GENKSYMS__' fs/statfs.c
        sed -i '/#include <linux\/susfs_def.h>/a #endif' fs/statfs.c
        log "Statfs CRC fix applied successfully."
        return 0
      else
        warn "fs/statfs.c not found, skipping statfs fix."
        return 1
      fi
    fi
  fi
  return 0
}
