#!/usr/bin/env bash

# * © 2025-2026. All Rights Reserved.
# * Original Author: Arkael-Dev

# ============================================================================
# GKI 6.12 (android16-6.12) specific build steps
# ----------------------------------------------------------------------------
# Sourced by build.sh ONLY when KVER == 6.12.
# Nothing in this file runs for 5.10 / 6.1 / 6.6.
#
# Provided by build.sh / functions.sh:
#   WORKDIR, KSRC, DEFCONFIG_FILE
#   log / warn / error / config helpers
#
# Every function assumes the current directory is $KSRC and must run BEFORE
# `make $KERNEL_DEFCONFIG`: the config() helper edits the gki_defconfig source
# file, not the generated out/.config.
#
# NOTE: shell functions do not inherit build.sh's ERR trap, so every critical
# step here checks its own result and calls error() explicitly.
# ============================================================================

GKI612_EXT_DIR="${WORKDIR}/gki612-ext"
GKI612_WK_PATCHES="${GKI612_EXT_DIR}/kernel_patches"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

# _gki612_clone <url> <dest>: shallow clone with 3 attempts.
_gki612_clone() {
  local url="$1" dest="$2" attempt
  rm -rf "$dest"
  for attempt in 1 2 3; do
    git clone -q --depth=1 "$url" "$dest" && return 0
    rm -rf "$dest"
    [ "$attempt" -lt 3 ] && sleep 10
  done
  return 1
}

# _gki612_patch <patch-file> <label>: apply with -p1.
# A patch that is already applied is skipped; a real conflict is fatal.
_gki612_patch() {
  local file="$1" label="$2" out
  [ -f "$file" ] || error "[$label] patch file not found: $file"

  if out=$(patch -p1 --forward --dry-run < "$file" 2>&1); then
    patch -p1 --forward --no-backup-if-mismatch < "$file" > /dev/null || error "[$label] failed to apply"
    log "[SUCCESS] $label applied."
  elif grep -q "Reversed (or previously applied)" <<< "$out"; then
    log "[INFO] $label already applied, skipping."
  else
    echo "$out" >&2
    error "[$label] does not apply cleanly to this kernel tree"
  fi
}

# _gki612_kconfig <multi-line "KEY=value" list>
# Applies each entry through the build's config() helper (scripts/config on the
# defconfig). KEY alone means =y, y/n/m map to enable/disable/module, a quoted
# value is a string, anything else is a number.
_gki612_kconfig() {
  local line key value
  while IFS= read -r line; do
    line="${line#"${line%%[![:space:]]*}"}"
    line="${line%"${line##*[![:space:]]}"}"
    [ -z "$line" ] && continue
    case "$line" in \#*) continue ;; esac

    if [[ "$line" == *"="* ]]; then
      key="${line%%=*}"
      value="${line#*=}"
    else
      key="$line"
      value="y"
    fi

    case "$value" in
      y) config --enable "$key" ;;
      n) config --disable "$key" ;;
      m) config --module "$key" ;;
      \"*\") config --set-str "$key" "${value//\"/}" ;;
      *) config --set-val "$key" "$value" ;;
    esac || error "Failed to set $key=$value in $DEFCONFIG_FILE"
  done <<< "$1"
}

# _gki612_wk_patches: clone the shared kernel_patches repo once, print its path.
_gki612_wk_patches() {
  if [ ! -d "$GKI612_WK_PATCHES/common" ]; then
    _gki612_clone "https://github.com/WildKernels/kernel_patches.git" "$GKI612_WK_PATCHES" \
      || error "Failed to clone kernel_patches (NTSync / unicode patches)"
  fi
  echo "$GKI612_WK_PATCHES"
}

# ---------------------------------------------------------------------------
# KernelSU-Next compile fix
# ---------------------------------------------------------------------------

# Some KernelSU-Next commits call task_pid_nr() in compat/samsung_kdp.c without
# including <linux/sched.h>, which clang rejects
# (-Wimplicit-function-declaration). Newer commits already include it, so this
# is skipped when the header is present or the file does not exist.
gki612_fix_kernelsu_compat() {
  local f="drivers/kernelsu/compat/samsung_kdp.c"

  if [ ! -f "$f" ]; then
    log "[INFO] $f not found, skipping KernelSU compat fix."
    return 0
  fi
  if grep -q '#include <linux/sched.h>' "$f"; then
    log "[INFO] samsung_kdp.c already includes <linux/sched.h>."
    return 0
  fi

  sed -i --follow-symlinks '0,/^#include/s|^#include|#include <linux/sched.h>\n#include|' "$f"
  grep -q '#include <linux/sched.h>' "$f" \
    || error "Could not add <linux/sched.h> to samsung_kdp.c"
  log "[SUCCESS] Added <linux/sched.h> to samsung_kdp.c (task_pid_nr fix)."
}

# ---------------------------------------------------------------------------
# Baseband Guard
# ---------------------------------------------------------------------------

gki612_setup_bbg() {
  log "Setting up Baseband Guard..."
  local setup="$GKI612_EXT_DIR/bbg-setup.sh"

  curl -fLSs --retry 3 -o "$setup" \
    "https://github.com/vc-teahouse/Baseband-guard/raw/main/setup.sh" \
    || error "Failed to download the Baseband Guard setup script"
  [ -s "$setup" ] || error "Baseband Guard setup script is empty"

  # setup.sh does the real work first and only prints an LSM hint at the end,
  # using gawk-only awk syntax. Its exit code is unreliable on mawk, so the
  # result is verified below instead of trusting the exit code.
  bash "$setup" || warn "Baseband Guard setup.sh exited non-zero (usually the gawk-only LSM hint), verifying result..."

  [ -e security/baseband-guard/baseband_guard.c ] || error "Baseband Guard source was not linked into security/"
  grep -q 'baseband-guard' security/Makefile || error "Baseband Guard missing from security/Makefile"
  grep -q 'security/baseband-guard/Kconfig' security/Kconfig || error "Baseband Guard missing from security/Kconfig"

  # Put baseband_guard right after selinux in every default of `config LSM`
  # (gki_defconfig does not set CONFIG_LSM, so the Kconfig default decides).
  sed -i '/^config LSM$/,/^[[:space:]]*help$/{ /^[[:space:]]*default/ { /baseband_guard/! s/selinux/selinux,baseband_guard/ } }' security/Kconfig
  grep -q 'baseband_guard' security/Kconfig || error "baseband_guard not found in the LSM list of security/Kconfig"

  config --enable CONFIG_BBG
  log "[SUCCESS] Baseband Guard ready."
}

# ---------------------------------------------------------------------------
# Networking
# ---------------------------------------------------------------------------

gki612_setup_networking() {
  log "Setting up networking configs..."
  _gki612_kconfig "$(cat << 'EOF'
CONFIG_IP_SET=y
CONFIG_IP_SET_MAX=65534
CONFIG_IP_SET_BITMAP_IP=y
CONFIG_IP_SET_BITMAP_IPMAC=y
CONFIG_IP_SET_BITMAP_PORT=y
CONFIG_IP_SET_HASH_IP=y
CONFIG_IP_SET_HASH_IPMARK=y
CONFIG_IP_SET_HASH_IPPORT=y
CONFIG_IP_SET_HASH_IPPORTIP=y
CONFIG_IP_SET_HASH_IPPORTNET=y
CONFIG_IP_SET_HASH_IPMAC=y
CONFIG_IP_SET_HASH_MAC=y
CONFIG_IP_SET_HASH_NETPORTNET=y
CONFIG_IP_SET_HASH_NET=y
CONFIG_IP_SET_HASH_NETNET=y
CONFIG_IP_SET_HASH_NETPORT=y
CONFIG_IP_SET_HASH_NETIFACE=y
CONFIG_IP_SET_LIST_SET=y
CONFIG_NETFILTER_XT_MATCH_ADDRTYPE=y
CONFIG_NETFILTER_XT_SET=y
CONFIG_NETFILTER_XT_TARGET_LOG=y
CONFIG_NETFILTER_XT_MATCH_RECENT=y
CONFIG_IP6_NF_NAT=y
CONFIG_IP6_NF_TARGET_MASQUERADE=y
CONFIG_TCP_CONG_ADVANCED=y
CONFIG_TCP_CONG_BBR=y
CONFIG_TCP_CONG_CUBIC=y
CONFIG_TCP_CONG_BIC=y
CONFIG_TCP_CONG_WESTWOOD=y
CONFIG_TCP_CONG_HTCP=y
CONFIG_DEFAULT_BBR=y
CONFIG_NET_SCH_FQ=y
CONFIG_NET_SCH_FQ_CODEL=y
CONFIG_NET_SCH_CAKE=y
CONFIG_NET_ACT_CONNMARK=y
CONFIG_IP_NF_TARGET_TTL=y
CONFIG_IP6_NF_TARGET_HL=y
CONFIG_IP6_NF_MATCH_HL=y
CONFIG_WIREGUARD=y
CONFIG_CIFS=y
CONFIG_NETWORK_FILESYSTEMS=y
CONFIG_NETFS_SUPPORT=y
CONFIG_KEYS=y
CONFIG_CIFS_XATTR=y
CONFIG_CIFS_POSIX=y
EOF
)"
}

# ---------------------------------------------------------------------------
# DroidSpaces
# ---------------------------------------------------------------------------

gki612_setup_droidspaces() {
  log "Setting up DroidSpaces-OSS support..."
  local repo="$GKI612_EXT_DIR/Droidspaces-OSS"

  _gki612_clone "https://github.com/ravindu644/Droidspaces-OSS.git" "$repo" \
    || error "Failed to clone Droidspaces-OSS"

  _gki612_patch \
    "$repo/Documentation/resources/kernel-patches/GKI/kernel-6.12/001.GKI-6.12-or-above-fix_sysvipc_kabi.patch" \
    "DroidSpaces sysvipc kABI fix"

  # Symbols DroidSpaces needs; added only when not already exported.
  grep -q 'EXPORT_SYMBOL[A-Z_]*(put_ipc_ns)' ipc/namespace.c \
    || printf '\nEXPORT_SYMBOL_GPL(put_ipc_ns);\n' >> ipc/namespace.c
  grep -q 'EXPORT_SYMBOL[A-Z_]*(init_ipc_ns)' ipc/msgutil.c \
    || printf '\nEXPORT_SYMBOL_GPL(init_ipc_ns);\n' >> ipc/msgutil.c

  _gki612_kconfig "$(cat << 'EOF'
CONFIG_PID_NS=y
CONFIG_SYSVIPC=y
CONFIG_POSIX_MQUEUE=y
CONFIG_IPC_NS=y
CONFIG_DEVTMPFS=y
CONFIG_BINFMT_MISC=y
CONFIG_BINFMT_SCRIPT=y
CONFIG_BINFMT_ELF=y
CONFIG_USER_NS=y
EOF
)"
}

# ---------------------------------------------------------------------------
# NTSync
# ---------------------------------------------------------------------------

gki612_setup_ntsync() {
  log "Applying NTSync patches..."
  local dir
  dir="$(_gki612_wk_patches)/common/ntsync"

  # android16-6.12 ships an older ntsync; remove it so the base patch can add the full driver.
  rm -f include/uapi/linux/ntsync.h drivers/misc/ntsync.c

  _gki612_patch "$dir/ntsync_compat_android16-6.12.patch" "NTSync compat (android16-6.12)"
  _gki612_patch "$dir/ntsync_base.patch" "NTSync base"
  config --enable CONFIG_NTSYNC
}

# ---------------------------------------------------------------------------
# Unicode bypass fix
# ---------------------------------------------------------------------------

gki612_apply_unicode_fix() {
  log "Applying unicode bypass fix..."
  _gki612_patch "$(_gki612_wk_patches)/common/unicode_bypass_fix_6.1+.patch" "unicode bypass fix"
}

# ---------------------------------------------------------------------------
# Misc configs
# ---------------------------------------------------------------------------

gki612_setup_misc_configs() {
  log "Setting up misc kernel configs..."
  _gki612_kconfig "$(cat << 'EOF'
CONFIG_OVERLAY_FS=y
CONFIG_TMPFS_XATTR=y
CONFIG_TMPFS_POSIX_ACL=y
CONFIG_KALLSYMS=y
CONFIG_KALLSYMS_ALL=y
CONFIG_BPF_EVENTS=y
CONFIG_KPROBE_EVENTS=y
CONFIG_UPROBES=y
CONFIG_UPROBE_EVENTS=y
CONFIG_FUSE_BPF=y
EOF
)"

  # BTF needs pahole (package: dwarves). Only force it on when the runner has it.
  if grep -q '^CONFIG_DEBUG_INFO_BTF=y' "$DEFCONFIG_FILE"; then
    log "[INFO] CONFIG_DEBUG_INFO_BTF already enabled in defconfig."
  elif command -v pahole > /dev/null 2>&1; then
    config --enable CONFIG_DEBUG_INFO_BTF
  else
    warn "pahole not found, leaving CONFIG_DEBUG_INFO_BTF untouched"
  fi
}

# ---------------------------------------------------------------------------
# Entry point (called once from build.sh)
# ---------------------------------------------------------------------------

gki612_install_pahole() {
  log "[INFO] Installing latest pahole (apt pahole hits the 4096 per-CPU var limit)"
  sudo apt-get install -y -qq cmake libdw-dev libelf-dev zlib1g-dev || error "pahole deps install failed"
  local d="$WORKDIR/pahole-src"
  rm -rf "$d"
  git clone --depth=1 --recurse-submodules --shallow-submodules \
    https://git.kernel.org/pub/scm/devel/pahole/pahole.git "$d" || error "pahole clone failed"
  ( cd "$d" && cmake -B build -D__LIB=lib -DCMAKE_BUILD_TYPE=Release . \
    && cmake --build build -j"$(nproc)" \
    && sudo cmake --install build ) || error "pahole build failed"
  sudo ldconfig
  hash -r
  log "[INFO] pahole: $(pahole --version) at $(command -v pahole)"
}

gki612_setup_all() {
  log "=== GKI 6.12 specific setup ==="
  [ -f scripts/config ] || error "gki612: must run from the kernel source root ($KSRC)"
  mkdir -p "$GKI612_EXT_DIR"

  gki612_install_pahole
  gki612_fix_kernelsu_compat
  gki612_setup_bbg
  gki612_setup_networking
  gki612_setup_droidspaces
  gki612_setup_ntsync
  gki612_apply_unicode_fix
  gki612_setup_misc_configs

  rm -rf "$GKI612_EXT_DIR"
  log "=== GKI 6.12 specific setup done ==="
}
