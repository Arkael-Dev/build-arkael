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
upload_file() {
  local FILE="$1"
  local CAPTION="${2:-}"

  if [[ -z "$TG_CHAT_ID" ]] || [[ -z "$TG_BOT_TOKEN" ]]; then
    echo "[WARN] Telegram credentials not configured, skipping upload"
    return 1
  fi

  if ! [ -f "$FILE" ]; then
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
send_msg() {
  local MESSAGE="$1"

  if [[ -z "$TG_CHAT_ID" ]] || [[ -z "$TG_BOT_TOKEN" ]]; then
    echo "[WARN] Telegram credentials not configured, skipping message"
    return 1
  fi

  curl -s -X POST "https://api.telegram.org/bot$TG_BOT_TOKEN/sendMessage" \
    -d "chat_id=$TG_CHAT_ID" \
    -d "disable_web_page_preview=true" \
    -d "parse_mode=markdown" \
    -d "text=$MESSAGE"
}

# send_release_notification <release_tag> <total_zips> <build_timestamp>
send_release_notification() {
  local RELEASE_TAG="${1:-unknown}"
  local TOTAL_ZIPS="${2:-0}"
  local BUILD_TIMESTAMP="${3:-$(date +%Y%m%d-%H%M)}"
  local RELEASE_REPO="${RELEASE_REPO:-Kingfinik98/build-vortex}"
  local RELEASE_LINK="https://github.com/${RELEASE_REPO}/releases/tag/${RELEASE_TAG}"

  if [[ -z "$TG_CHAT_ID" ]] || [[ -z "$TG_BOT_TOKEN" ]]; then
    echo "[WARN] Telegram credentials not configured, skipping release notification"
    return 1
  fi

  local MESSAGE=$(cat << EOF
*🐧 VORTEX_E-SPORT - ALL GKI RELEASE* 🚀

✅ *Build Status*: SUCCESS
📦 *Total Variants*: ${TOTAL_ZIPS} ZIPs
🐧 *Kernel Versions*: 5.10, 6.1, 6.6
📅 *Build Date*: ${BUILD_TIMESTAMP}
🔐 *Root Options*: KSU, VorteXSU, WildKSU, VNL

———————————————
📥 *DOWNLOAD RELEASE*:
[${RELEASE_TAG}](${RELEASE_LINK})
———————————————

*Available Variants:*
🔵 GKI 5.10 (Android 12)
🟢 GKI 6.1 (Android 14)
🟣 GKI 6.6 (Android 15)

Each version includes:
• VNL (Vanilla/No Root)
• KSU (Standard KernelSU)
• KSU+SuSFS (Hidden Root)
• VorteXSU (KPM Patch)
• WildKSU (Alternative)

———————————————
_Built with ❤️ by @Kingfinik98_
_© 2024-2026 VorteX_E-Sport_
EOF
  )

  curl -s -X POST "https://api.telegram.org/bot${TG_BOT_TOKEN}/sendMessage" \
    -d "chat_id=${TG_CHAT_ID}" \
    -d "parse_mode=markdown" \
    -d "disable_web_page_preview=true" \
    -d "text=${MESSAGE}"

  echo "[INFO] Release notification sent to Telegram"
}

# ============================================
# KERNELSU INSTALLATION FUNCTION
# ============================================

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
# VARIANT DETECTION FUNCTIONS (TANPA SUKISU)
# ============================================

# ksu_included()
# Type: bool
# Return: True (0) jika $KSU == "yes" (Standard KernelSU-Next)
#
# SUPPORTED VALUES (4 variants):
#   "yes"      -> Standard KernelSU-Next (TRUE)
#   "vortexsu" -> VorteXSU (FALSE - has own block)
#   "wildksu"  -> WildKSU (FALSE - has own block)
#   "no"       -> Vanilla/No Root (FALSE)
ksu_included() {
  [ "$KSU" == "yes" ]
  return $?
}

# kpm_enabled()
# Type: bool
# Return: True (0) jika variant pakai KPM patch
#
# VARIANTS DENGAN KPM:
#   "vortexsu" -> VorteXSU (KPM Enabled) ✅
#
# VARIANTS TANPA KPM:
#   "yes"      -> Standard KernelSU-Next (No KPM)
#   "wildksu"  -> WildKSU (No KPM)
#   "no"       -> Vanilla (No KPM)
kpm_enabled() {
  [ "$KSU" == "vortexsu" ]
  return $?
}

# is_ksu_variant()
# Type: bool
# Return: True (0) jika ANY KSU root enabled
is_ksu_variant() {
  [ "$KSU" == "yes" ] || [ "$KSU" == "vortexsu" ] || [ "$KSU" == "wildksu" ]
  return $?
}

# ============================================
# SUSFS DETECTION FUNCTION
# ============================================

susfs_included() {
  [ "$KSU_SUSFS" == "true" ]
  return $?
}

# ============================================
# UTILITY FUNCTIONS
# ============================================

simplify_gh_url() {
  local URL="$1"
  echo "$URL" | sed "s|https://github.com/||g" | sed "s|.git||g"
}

# get_variant_name()
# Returns: "KSU", "VorteXSU", "WildKSU", atau "VNL"
get_variant_name() {
  case "$KSU" in
    "yes")      echo "KSU" ;;
    "vortexsu") echo "VorteXSU" ;;
    "wildksu")  echo "WildKSU" ;;
    "no")       echo "VNL" ;;
    *)          echo "Unknown" ;;
  esac
}

# get_kpm_status()
get_kpm_status() {
  if kpm_enabled; then
    echo "✅ Enabled"
  else
    echo "❌ Disabled"
  fi
}

# get_susfs_status()
get_susfs_status() {
  if susfs_included; then
    echo "✅ Enabled"
  else
    echo "❌ Disabled"
  fi
}

# ============================================
# KERNEL CONFIG FUNCTION
# ============================================

config() {
  if [[ -z "$DEFCONFIG_FILE" ]] || [[ ! -f "$DEFCONFIG_FILE" ]]; then
    error "DEFCONFIG_FILE not found or not set"
  fi
  $KSRC/scripts/config --file $DEFCONFIG_FILE $@
}

# ============================================
# LOGGING FUNCTIONS
# ============================================

log() {
  echo -e "[LOG] $(date '+%H:%M:%S') $*"
}

error() {
  local err_txt
  err_txt=$(
    cat << EOF
*🔴 VorteX_E-Sport Build ERROR*

❌ Error: $*
📅 Time: $(date)
🐧 Kernel: ${LINUX_VERSION:-unknown}
📛 Variant: $(get_variant_name)
📦 Matrix: ${KERNEL_VERSION:-unknown} | ${KSU:-unknown} | SuSFS:${KSU_SUSFS:-false}

_Check build.log for details_
EOF
  )
  echo -e "[ERROR] $(date '+%H:%M:%S') $*"
  send_msg "$err_txt"
  
  if [[ -n "$WORKDIR" ]] && [[ -f "$WORKDIR/build.log" ]]; then
    upload_file "$WORKDIR/build.log"
  fi
  
  exit 1
}

warn() {
  local MESSAGE="$1"
  local NOTIFY="${2:-false}"
  echo -e "[WARN] $(date '+%H:%M:%S') $*"
  if [ "$NOTIFY" == "true" ]; then
    send_msg "⚠️ *Warning:* $MESSAGE"
  fi
}

success() {
  local MESSAGE="$1"
  local NOTIFY="${2:-false}"
  echo -e "[✅ SUCCESS] $(date '+%H:%M:%S') $*"
  if [ "$NOTIFY" == "true" ]; then
    send_msg "✅ $MESSAGE"
  fi
}

# matrix_log <kernel_version> <variant_name> <ksu_value> <susfs_value>
matrix_log() {
  local KVER_LOG="${1:-$KVER}"
  local VARIANT_LOG="${2:-$(get_variant_name)}"
  local KSU_LOG="${3:-$KSU}"
  local SUSFS_LOG="${4:-$KSU_SUSFS}"

  echo ""
  echo "=========================================="
  echo "  🐧 MATRIX BUILD INFO"
  echo "=========================================="
  echo "  📌 Kernel Version : ${KVER_LOG}"
  echo "  🔐 Variant       : ${VARIANT_LOG}"
  echo "  🏷️  KSU Value     : ${KSU_LOG}"
  echo "  🛡️  SuSFS         : ${SUSFS_LOG}"
  echo "  ⚡ KPM Patch     : $(get_kpm_status)"
  echo "  ⏰  Time          : $(date '+%Y-%m-%d %H:%M:%S')"
  echo "=========================================="
  echo ""
}

# ============================================
# VARIANT SETUP HELPERS (TANPA SUKISU)
# ============================================

setup_ksu_variant() {
  log "Setting up KSU variant: $(get_variant_name)"
  
  case "$KSU" in
    "yes")
      log "Standard KSU variant will be handled by main build block."
      ;;
    "vortexsu")
      log "VorteXSU variant will be handled by dedicated setup block."
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
    log "Stack protector fix not required for KVER $KVER."
    return 0
  fi
}

apply_statfs_susfs_fix() {
  if [[ -z "$LINUX_VERSION_CODE" ]]; then
    warn "LINUX_VERSION_CODE not set, skipping statfs fix"
    return 1
  fi

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

# ============================================
# ARTIFACT MANAGEMENT FUNCTIONS
# ============================================

generate_info_file() {
  local OUTPUT_PATH="${1:-$WORKDIR/artifacts/info.txt}"
  
  mkdir -p "$(dirname "$OUTPUT_PATH")"
  
  cat > "$OUTPUT_PATH" << EOF
LINUX_VERSION=${LINUX_VERSION:-unknown}
SUSFS_VERSION=${SUSFS_VERSION:-N/A}
KERNEL_NAME=${KERNEL_NAME:-VorteX_E-Sport}
RELEASE=${RELEASE:-v0.3}
RELEASE_REPO=${RELEASE_REPO:-Kingfinik98/build-vortex}
BUILD_DATE=$(date '+%Y-%m-%d %H:%M:%S')
BUILD_VARIANT=$(get_variant_name)
KERNEL_VERSION=${KVER:-unknown}
EOF

  log "Info file generated: $OUTPUT_PATH"
}

validate_artifact() {
  local FILE="$1"
  
  if [[ ! -f "$FILE" ]]; then
    warn "Artifact not found: $FILE"
    return 1
  fi
  
  local SIZE=$(stat -f%z "$FILE" 2>/dev/null || stat -c%s "$FILE" 2>/dev/null || echo "0")
  
  if [[ "$SIZE" -lt 1024 ]]; then
    warn "Artifact too small ($SIZE bytes): $FILE"
    return 1
  fi
  
  log "Artifact validated: $FILE ($(( SIZE / 1024 ))KB)"
  return 0
}

cleanup_build_artifacts() {
  local KEEP_LOGS="${1:-false}"
  
  log "Cleaning up build artifacts..."
  
  rm -rf "$WORKDIR/ksrc" 2>/dev/null || true
  rm -rf "$WORKDIR/clang" 2>/dev/null || true
  rm -rf "$WORKDIR/gas" 2>/dev/null || true
  rm -rf "$WORKDIR/susfs" 2>/dev/null || true
  rm -f "$WORKDIR/*.patch" 2>/dev/null || true
  rm -f "$WORKDIR/Inject_300hz.sh" 2>/dev/null || true
  rm -f "$WORKDIR/inject.sh" 2>/dev/null || true
  
  if [[ "$KEEP_LOGS" != "true" ]]; then
    rm -f "$WORKDIR/build.log" 2>/dev/null || true
  fi
  
  log "Cleanup completed."
}

# ============================================
# ENVIRONMENT VALIDATION
# ============================================

validate_github_actions_env() {
  local ERRORS=0
  
  echo ""
  echo "=========================================="
  echo "  🔍 VALIDATING GITHUB ACTIONS ENVIRONMENT"
  echo "=========================================="
  
  if [[ -z "$KVER" ]]; then
    warn "KVER (Kernel Version) not set"
    ((ERRORS++))
  else
    log "KVER: $KVER ✓"
  fi
  
  if [[ -z "$KSU" ]]; then
    warn "KSU (Variant) not set, defaulting to 'yes'"
    export KSU="yes"
  else
    log "KSU: $KSU ✓"
  fi
  
  if [[ -z "$TODO" ]]; then
    warn "TODO (Build Type) not set, defaulting to 'kernel'"
    export TODO="kernel"
  else
    log "TODO: $TODO ✓"
  fi
  
  if [[ -n "$TG_CHAT_ID" ]] && [[ -n "$TG_BOT_TOKEN" ]]; then
    log "Telegram: Configured ✓"
  else
    warn "Telegram: Not configured (notifications disabled)"
  fi
  
  if [[ -n "$GH_TOKEN" ]]; then
    log "GitHub Token: Configured ✓"
  else
    warn "GitHub Token: Not configured (release features disabled)"
  fi
  
  log "Variant Details:"
  log "  • Name: $(get_variant_name)"
  log "  • KPM: $(get_kpm_status)"
  log "  • SuSFS: $(get_susfs_status)"
  
  echo "=========================================="
  echo ""
  
  if [[ $ERRORS -gt 0 ]]; then
    error "Environment validation failed with $ERRORS error(s)"
  fi
  
  log "Environment validation passed!"
  return 0
}

print_build_header() {
  echo ""
  echo "╔══════════════════════════════════════════════════════════════╗"
  echo "║                                                              ║"
  echo "║            🐧 VORTEX_E-SPORT BUILD SYSTEM 🐧                  ║"
  echo "║                                                              ║"
  echo "╠══════════════════════════════════════════════════════════════╣"
  echo "║  Kernel Version : ${KVER:-unknown}$(printf '%*s' $((20 - ${#KVER:-0})) '')║"
  echo "║  Variant       : $(get_variant_name)$(printf '%*s' $((20 - ${#$(get_variant_name):-0})) '')║"
  echo "║  SuSFS         : $(get_susfs_status)$(printf '%*s' $((17 - ${#$(get_susfs_status):-0})) '')║"
  echo "║  KPM Patch     : $(get_kpm_status)$(printf '%*s' $((16 - ${#$(get_kpm_status):-0})) '')║"
  echo "║  Build Type    : ${TODO:-kernel}$(printf '%*s' $((19 - ${#TODO:-kernel})) '')║"
  echo "║  Timestamp     : $(date '+%Y-%m-%d %H:%M:%S')                              ║"
  echo "╚══════════════════════════════════════════════════════════════╝"
  echo ""
}
