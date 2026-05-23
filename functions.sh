#!/usr/bin/env bash

#/*!
# * © 2025-2026 Arkael-Dev. All Rights Reserved.
# * Signed-off-by: kingfinix98@gmail.com
# */

# ============== Functions ==============

# Telegram Functions
upload_file() {
  local FILE="$1"
  local CAPTION="${2:-}"
  if [[ -z "$TG_CHAT_ID" ]] || [[ -z "$TG_BOT_TOKEN" ]]; then
    return 1
  fi
  chmod 777 "$FILE"
  curl -s -F "document=@${FILE}" -F "chat_id=${TG_CHAT_ID}" -F "caption=${CAPTION}" \
    -F "parse_mode=markdown" -F "disable_web_page_preview=true" \
    "https://api.telegram.org/bot${TG_BOT_TOKEN}/sendDocument"
}

send_msg() {
  local MESSAGE="$1"
  if [[ -z "$TG_CHAT_ID" ]] || [[ -z "$TG_BOT_TOKEN" ]]; then
    return 1
  fi
  curl -s -X POST "https://api.telegram.org/bot$TG_BOT_TOKEN/sendMessage" \
    -d "chat_id=$TG_CHAT_ID" -d "disable_web_page_preview=true" \
    -d "parse_mode=markdown" -d "text=$MESSAGE"
}

send_release_notification() {
  local RELEASE_TAG="${1:-unknown}"
  local TOTAL_ZIPS="${2:-0}"
  local BUILD_TIMESTAMP="${3:-$(date +%Y%m%d-%H%M)}"
  local RELEASE_REPO="${RELEASE_REPO:-Kingfinik98/build-arkael}"
  local RELEASE_LINK="https://github.com/${RELEASE_REPO}/releases/tag/${RELEASE_TAG}"

  if [[ -z "$TG_CHAT_ID" ]] || [[ -z "$TG_BOT_TOKEN" ]]; then
    return 1
  fi

  local MESSAGE="*🐧 ARKAEL KERNEL - ALL GKI RELEASE* 🚀

✅ *Build Status*: SUCCESS
📦 *Total Variants*: ${TOTAL_ZIPS} ZIPs
🐧 *Kernel Versions*: 5.10, 6.1, 6.6
📅 *Build Date*: ${BUILD_TIMESTAMP}
🔐 *Root Options*: KSU, SukiSU, VNL

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

———————————————
_Built with ❤️ by @Kingfinik98_
_© 2025-2026 Arkael Kernel_"

  curl -s -X POST "https://api.telegram.org/bot${TG_BOT_TOKEN}/sendMessage" \
    -d "chat_id=${TG_CHAT_ID}" -d "parse_mode=markdown" \
    -d "disable_web_page_preview=true" -d "text=${MESSAGE}"
}

install_ksu() {
  local REPO="$1" REF="$2"
  URL="https://raw.githubusercontent.com/$REPO/$REF/kernel/setup.sh"
  log "Installing KernelSU from $REPO | $REF"
  curl -LSs "$URL" | bash -s "$REF"
}

# Variant Detection (3 VARIANTS ONLY + sukisu)
ksu_included() { [ "$KSU" == "yes" ]; return $?; }

kpm_enabled() { [ "$KSU" == "vortexsu" ] || [ "$KSU" == "sukisu" ]; return $?; }

is_ksu_variant() { [ "$KSU" == "yes" ] || [ "$KSU" == "vortexsu" ] || [ "$KSU" == "sukisu" ]; return $?; }

susfs_included() { [ "$KSU_SUSFS" == "true" ]; return $?; }

simplify_gh_url() { echo "$1" | sed "s|https://github.com/||g" | sed "s|.git||g"; }

get_variant_name() {
  case "$KSU" in
    "yes")       echo "KSU" ;;
    "vortexsu")  echo "VorteXSU" ;;
    "sukisu")    echo "SukiSU-Ultra" ;;
    "no")        echo "VNL" ;;
    *)          echo "Unknown" ;;
  esac
}

get_kpm_status() { kpm_enabled && echo "✅ Enabled" || echo "❌ Disabled"; }
get_susfs_status() { susfs_included && echo "✅ Enabled" || echo "❌ Disabled"; }

# ========================================================================
# 🎛️ GOVERNOR SELECTION FUNCTIONS (NEW: Integrated with build.sh)
# ========================================================================

# Get governor display name (uppercase)
get_governor_name() {
  case "${GOVERNOR_CHOICE,,}" in
    "arkael")     echo "ARKAEL" ;;
    "vortexcore"|*) echo "VORTEXCORE" ;;
  esac
}

# Get governor source filename
get_governor_source_file() {
  case "${GOVERNOR_CHOICE,,}" in
    "arkael")     echo "governor-arkael.c" ;;
    "vortexcore"|*) echo "governor-vortexcore.c" ;;
  esac
}

# Get governor target name in kernel source
get_governor_target_name() {
  case "${GOVERNOR_CHOICE,,}" in
    "arkael")     echo "governor-arkael" ;;
    "vortexcore"|*) echo "governor-vortexcore" ;;
  esac
}

# Get governor config name for Kconfig/Makefile
get_governor_config_name() {
  case "${GOVERNOR_CHOICE,,}" in
    "arkael")     echo "ARKAEL" ;;
    "vortexcore"|*) echo "VORTEXCORE" ;;
  esac
}

# Validate governor choice and set defaults
validate_governor_choice() {
  # Default to vortexcore if not set
  if [[ -z "$GOVERNOR_CHOICE" ]]; then
    export GOVERNOR_CHOICE="vortexcore"
    log "⚠️ GOVERNOR_CHOICE not set, defaulting to: $GOVERNOR_CHOICE"
  fi
  
  # Validate choice
  case "${GOVERNOR_CHOICE,,}" in
    "arkael"|"vortexcore")
      log "✅ Governor validated: ${GOVERNOR_CHOICE^^} ($(get_governor_source_file))"
      ;;
    *)
      warn "Unknown governor: $GOVERNOR_CHOICE, falling back to vortexcore"
      export GOVERNOR_CHOICE="vortexcore"
      ;;
  esac
  
  # Validate source file exists
  local GOV_SOURCE="$WORKDIR/$(get_governor_source_file)"
  if [[ ! -f "$GOV_SOURCE" ]]; then
    error "❌ Governor source file not found: $GOV_SOURCE"
  fi
  
  # Validate vortex_gki file if set
  if [[ -n "$VORTEX_GKI_FILE" ]]; then
    local VORTEX_GKI_PATH="$WORKDIR/kernel-patches/${VORTEX_GKI_FILE}"
    if [[ ! -f "$VORTEX_GKI_PATH" ]]; then
      error "❌ Vortex GKI file not found: $VORTEX_GKI_PATH"
    else
      log "✅ Vortex GKI file validated: ${VORTEX_GKI_FILE}"
    fi
  else
    export VORTEX_GKI_FILE="vortex_gki.c"
    log "ℹ️ VORTEX_GKI_FILE not set, defaulting to: $VORTEX_GKI_FILE"
  fi
}

# Enable selected governor config in kernel
enable_governor_config() {
  local GOV_CONFIG="CONFIG_CPU_FREQ_GOV_$(get_governor_config_name)"
  
  log "Enabling governor config: $GOV_CONFIG"
  config --enable "$GOV_CONFIG"
  
  if [[ $? -eq 0 ]]; then
    log "✅ $GOV_CONFIG enabled successfully"
  else
    error "❌ Failed to enable $GOV_CONFIG"
  fi
}

# Log governor selection summary
log_governor_selection() {
  log ""
  log "=========================================="
  log "  🎛️ GOVERNOR SELECTION SUMMARY"
  log "=========================================="
  log "  Governor Choice : ${GOVERNOR_CHOICE^^}"
  log "  Source File     : $(get_governor_source_file)"
  log "  Target Name     : $(get_governor_target_name)"
  log "  Config Name     : CONFIG_CPU_FREQ_GOV_$(get_governor_config_name)"
  log "  Vortex GKI File : ${VORTEX_GKI_FILE:-vortex_gki.c}"
  log "=========================================="
  log ""
}

config() {
  if [[ -z "$DEFCONFIG_FILE" ]] || [[ ! -f "$DEFCONFIG_FILE" ]]; then
    error "DEFCONFIG_FILE not found or not set"
  fi
  $KSRC/scripts/config --file $DEFCONFIG_FILE $@
}

log() { echo -e "[LOG] $(date '+%H:%M:%S') $*"; }

error() {
  local err_txt="*🔴 Arkael Build ERROR*\n\n❌ Error: $*\n📅 Time: $(date)\n🐧 Kernel: ${LINUX_VERSION:-unknown}\n📛 Variant: $(get_variant_name)\n🎛️ Governor: $(get_governor_name)"
  echo -e "[ERROR] $(date '+%H:%M:%S') $*"
  send_msg "$err_txt"
  [[ -n "$WORKDIR" ]] && [[ -f "$WORKDIR/build.log" ]] && upload_file "$WORKDIR/build.log"
  exit 1
}

warn() {
  local MESSAGE="$1" NOTIFY="${2:-false}"
  echo -e "[WARN] $(date '+%H:%M:%S') $*"
  [[ "$NOTIFY" == "true" ]] && send_msg "⚠️ *Warning:* $MESSAGE"
}

success() {
  local MESSAGE="$1" NOTIFY="${2:-false}"
  echo -e "[✅ SUCCESS] $(date '+%H:%M:%S') $*"
  [[ "$NOTIFY" == "true" ]] && send_msg "✅ $MESSAGE"
}

matrix_log() {
  local KVER_LOG="${1:-$KVER}" VARIANT_LOG="${2:-$(get_variant_name)}"
  local GOV_LOG="${3:-$(get_governor_name)}"
  echo ""
  echo "=========================================="
  echo "  🐧 ARKAEL BUILD INFO"
  echo "  📌 Kernel : ${KVER_LOG} | 🔐 Variant: ${VARIANT_LOG}"
  echo "  ⚡ KPM   : $(get_kpm_status) | 🛡️ SuSFS: $(get_susfs_status)"
  echo "  🎛️ Gov   : ${GOV_LOG} | 📁 GKI: ${VORTEX_GKI_FILE:-vortex_gki.c}"
  echo "=========================================="
  echo ""
}

setup_ksu_variant() {
  log "Setting up KSU variant: $(get_variant_name)"
  case "$KSU" in
    "yes")       log "Standard KSU variant." ;;
    "vortexsu")  log "VorteXSU variant (KPM Enabled)." ;;
    "sukisu")    log "SukiSU-Ultra variant (KPM Enabled)." ;;
    "no")        log "Vanilla variant (No Root)." ;;
    *)          warn "Unknown KSU variant: $KSU" ;;
  esac
}

apply_stack_protector_fix() {
  if [ "$KVER" == "5.10" ] && [ -f "drivers/kernelsu/ksu.c" ]; then
    sed -i '/^#if.*CONFIG_STACKPROTECTOR_PER_TASK/c\#if 0 \/\/ Disabled to fix duplicate symbol' drivers/kernelsu/ksu.c || true
    log "Stack protector fix applied for $(get_variant_name)."
  fi
}

apply_statfs_susfs_fix() {
  if [[ -z "$LINUX_VERSION_CODE" ]]; then return 1; fi
  if [ $(echo "$LINUX_VERSION_CODE" | head -c1) -eq 6 ]; then
    if ([ "$KVER" == "6.1" ] || [ "$KVER" == "6.6" ]) && [ -f "fs/statfs.c" ]; then
      log "Applying statfs CRC fix for $(get_variant_name) GKI $KVER..."
      sed -i '/#include <linux\/susfs_def.h>/i #ifndef __GENKSYMS__' fs/statfs.c
      sed -i '/#include <linux\/susfs_def.h>/a #endif' fs/statfs.c
    fi
  fi
}

generate_info_file() {
  local OUTPUT_PATH="${1:-$WORKDIR/artifacts/info.txt}"
  mkdir -p "$(dirname "$OUTPUT_PATH")"
  cat > "$OUTPUT_PATH" << EOF
LINUX_VERSION=${LINUX_VERSION:-unknown}
SUSFS_VERSION=${SUSFS_VERSION:-N/A}
KERNEL_NAME=${KERNEL_NAME:-Arkael-Kernel}
RELEASE=${RELEASE:-v0.3}
RELEASE_REPO=${RELEASE_REPO:-Kingfinik98/build-arkael}
GOVERNOR=${GOVERNOR_CHOICE:-vortexcore}
VORTEX_GKI_FILE=${VORTEX_GKI_FILE:-vortex_gki.c}
EOF
  log "Info file generated: $OUTPUT_PATH"
}

validate_artifact() {
  [[ ! -f "$1" ]] && { warn "Artifact not found: $1"; return 1; }
  local SIZE=$(stat -f%z "$1" 2>/dev/null || stat -c%s "$1" 2>/dev/null || echo "0")
  [[ "$SIZE" -lt 1024 ]] && { warn "Artifact too small: $1"; return 1; }
  log "Artifact validated: $1 ($(( SIZE / 1024 ))KB)"
}

cleanup_build_artifacts() {
  log "Cleaning up build artifacts..."
  rm -rf "$WORKDIR/ksrc" "$WORKDIR/clang" "$WORKDIR/gas" "$WORKDIR/susfs" 2>/dev/null || true
  rm -f "$WORKDIR/"*.patch "$WORKDIR/Inject_300hz.sh" "$WORKDIR/inject.sh" 2>/dev/null || true
  log "Cleanup completed."
}

validate_github_actions_env() {
  local ERRORS=0
  
  # Validate required environment variables
  [[ -z "$KVER" ]] && { warn "KVER not set"; ((ERRORS++)); } || log "KVER: $KVER ✓"
  [[ -z "$KSU" ]] && { export KSU="yes"; warn "KSU defaulting to yes"; } || log "KSU: $KSU ✓"
  [[ -z "$TODO" ]] && { export TODO="kernel"; warn "TODO defaulting to kernel"; } || log "TODO: $TODO ✓"
  
  # Validate new governor options
  validate_governor_choice
  
  # Optional services
  [[ -n "$TG_CHAT_ID" && -n "$TG_BOT_TOKEN" ]] && log "Telegram: ✓" || warn "Telegram: Not configured"
  [[ -n "$GH_TOKEN" ]] && log "GitHub Token: ✓" || warn "GitHub Token: Not configured"
  
  # Print summary
  log "Variant: $(get_variant_name) | KPM: $(get_kpm_status) | SuSFS: $(get_susfs_status)"
  log "Governor: $(get_governor_name) | Vortex GKI: ${VORTEX_GKI_FILE:-vortex_gki.c}"
  
  [[ $ERRORS -gt 0 ]] && error "Validation failed with $ERRORS errors"
  log "Environment validation passed!"
}

print_build_header() {
  echo ""
  echo "╔══════════════════════════════════════════════════════════════╗"
  echo "║            🐧 ARKAEL KERNEL BUILD SYSTEM 🐧                  ║"
  echo "╠══════════════════════════════════════════════════════════════╣"
  printf "║  Kernel: %-16s Governor: %-18s ║\n" "${KVER:-unknown}" "$(get_governor_name)"
  printf "║  Variant: %-16s KPM: %-18s ║\n" "$(get_variant_name)" "$(get_kpm_status)"
  printf "║  SuSFS: %-17s GKI File: %-16s ║\n" "$(get_susfs_status)" "${VORTEX_GKI_FILE:-vortex.gki.c}"
  echo "╚══════════════════════════════════════════════════════════════╝"
  echo ""
}

# ========================================================================
# 🔧 GOVERNOR INJECTION HELPERS (NEW: For build.sh integration)
# ========================================================================

# Inject selected governor into kernel source
inject_governor_to_kernel() {
  local KSRC_DIR="$1"
  
  if [[ -z "$KSRC_DIR" ]] || [[ ! -d "$KSRC_DIR" ]]; then
    error "Kernel source directory not provided or invalid: $KSRC_DIR"
  fi
  
  local GOV_SOURCE="$WORKDIR/$(get_governor_source_file)"
  local GOV_TARGET="$KSRC_DIR/drivers/cpufreq/$(get_governor_target_name).c"
  local GOV_MAKEFILE="$KSRC_DIR/drivers/cpufreq/Makefile"
  local GOV_KCONFIG="$KSRC_DIR/drivers/cpufreq/Kconfig"
  local GOV_CONFIG_NAME="$(get_governor_config_name)"
  local GOV_TARGET_NAME="$(get_governor_target_name)"
  local GOV_DISPLAY_NAME="$(get_governor_name)"
  
  log "Injecting ${GOV_DISPLAY_NAME} governor into kernel source..."
  
  # Copy governor source file
  cp "$GOV_SOURCE" "$GOV_TARGET"
  log "✅ Copied $(get_governor_source_file) → ${GOV_TARGET_NAME}.c"
  
  # Add to Makefile if not exists
  if ! grep -q "${GOV_TARGET_NAME}.o" "$GOV_MAKEFILE"; then
    echo "obj-\$(CONFIG_CPU_FREQ_GOV_${GOV_CONFIG_NAME}) += ${GOV_TARGET_NAME}.o" >> "$GOV_MAKEFILE"
    log "✅ Added ${GOV_TARGET_NAME}.o to cpufreq Makefile"
  else
    log "ℹ️ ${GOV_TARGET_NAME}.o already in Makefile"
  fi
  
  # Add to Kconfig if not exists
  if ! grep -q "CPU_FREQ_GOV_${GOV_CONFIG_NAME}" "$GOV_KCONFIG"; then
    cat << KCONF_EOF >> "$GOV_KCONFIG"

config CPU_FREQ_GOV_${GOV_CONFIG_NAME}
    tristate "${GOV_DISPLAY_NAME} CPU frequency policy governor"
    depends on CPU_FREQ
    help
      ${GOV_DISPLAY_NAME} governor balances performance and efficiency for gaming and daily use.

      If in doubt, say N.
KCONF_EOF
    log "✅ Added CPU_FREQ_GOV_${GOV_CONFIG_NAME} to cpufreq Kconfig"
  else
    log "ℹ️ CPU_FREQ_GOV_${GOV_CONFIG_NAME} already in Kconfig"
  fi
  
  log "✅ ${GOV_DISPLAY_NAME} governor injection completed successfully!"
}

# Inject vortex_gki.c file into kernel source
inject_vortex_gki_to_kernel() {
  local KSRC_DIR="$1"
  
  if [[ -z "$KSRC_DIR" ]] || [[ ! -d "$KSRC_DIR" ]]; then
    error "Kernel source directory not provided or invalid: $KSRC_DIR"
  fi
  
  local VORTEX_GKI_SOURCE="$WORKDIR/kernel-patches/${VORTEX_GKI_FILE}"
  local VORTEX_GKI_TARGET="$KSRC_DIR/drivers/misc/vortex_gki.c"
  local MISC_MAKEFILE="$KSRC_DIR/drivers/misc/Makefile"
  
  log "Injecting Vortex GKI file (${VORTEX_GKI_FILE}) into kernel source..."
  
  # Ensure directory exists
  mkdir -p "$KSRC_DIR/drivers/misc"
  
  # Copy vortex_gki.c
  cp "$VORTEX_GKI_SOURCE" "$VORTEX_GKI_TARGET"
  log "✅ Copied ${VORTEX_GKI_FILE} → drivers/misc/vortex_gki.c"
  
  # Update Makefile
  sed -i '/vortex_gki/d' "$MISC_MAKEFILE"
  echo "obj-y += vortex_gki.o" >> "$MISC_MAKEFILE"
  log "✅ Updated drivers/misc/Makefile with vortex_gki.o"
  
  log "✅ Vortex GKI injection completed successfully!"
}
