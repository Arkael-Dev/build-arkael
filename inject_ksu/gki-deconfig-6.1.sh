#!/usr/bin/env bash

# =========================================
# 🔧 SET & CLEAN CONFIG HELPERS (FIXED)
# =========================================
DEFCONFIG_FILE="arch/arm64/configs/gki_defconfig"

clean_config() {
  # It is mandatory to delete these two formats so that they do not crash in GKI 6.1
  sed -i "/^$1=/d" $DEFCONFIG_FILE
  sed -i "/^# $1 is not set/d" $DEFCONFIG_FILE
}

set_config() {
  clean_config "$1"
  echo "$1" >> $DEFCONFIG_FILE
}

# =========================================
# 🔑 ADD KERNELSU BASE & DEPENDENCIES
# =========================================
echo "⚙️ Added KSU & SuSFS configuration"

set_config "CONFIG_KSU=y"
set_config "CONFIG_KPM=y"
set_config "CONFIG_KSU_MULTI_MANAGER_SUPPORT=y"
set_config "CONFIG_KPROBES=y"
set_config "CONFIG_KPROBE_EVENTS=y"

# =========================================
# 🛡️ ADD HOOK & SUSFS SUPPORT
# =========================================
if [ "$KSU" == "SukiSU" ]; then
  if [ "$KSU_SUSFS" = "true" ]; then
    echo "🔧 Mode: SukiSU + SuSFS Enabled"
    set_config "CONFIG_KSU_SUSFS=y"
  else
    echo "🔧 Mode: SukiSU Standard (No SuSFS)"
  fi
elif [ "$KSU_SUSFS" = "true" ]; then
  echo "🔧 Mode: SuSFS Hook Enabled"
  set_config "CONFIG_KSU_SUSFS=y"
  set_config "CONFIG_KSU_SUSFS_HAS_MAGIC_MOUNT=y"
  set_config "CONFIG_KSU_SUSFS_SUS_PATH=y"
  set_config "CONFIG_KSU_SUSFS_SUS_MOUNT=y"
  set_config "CONFIG_KSU_SUSFS_SUS_KSTAT_SPOOF_GENERIC=y"
  set_config "CONFIG_KSU_SUSFS_SUS_KSTAT=y"
  set_config "CONFIG_KSU_SUSFS_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT=y"
  set_config "CONFIG_KSU_SUSFS_AUTO_ADD_SUS_BIND_MOUNT=y"
  set_config "CONFIG_KSU_SUSFS_AUTO_ADD_SUS_KSTAT=y"
  set_config "CONFIG_KSU_SUSFS_SUS_OVERLAYFS=n"
  set_config "CONFIG_KSU_SUSFS_TRY_UMOUNT=n"
  set_config "CONFIG_KSU_SUSFS_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT=n"
  set_config "CONFIG_KSU_SUSFS_ENABLE_LOG=y"
  set_config "CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS=y"
  set_config "CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG=y"
  set_config "CONFIG_KSU_SUSFS_OPEN_REDIRECT=y"
  set_config "CONFIG_KSU_MANUAL_HOOK=n"
  set_config "CONFIG_KSU_HAS_MANUAL_HOOK=n"
else
  echo "🔧 Mode: Kprobes Hook Standard"
  set_config "CONFIG_KSU_SUSFS=n"
  set_config "CONFIG_KSU_SUSFS_SUS_SU=n"
  set_config "CONFIG_KSU_MANUAL_HOOK=n"
  set_config "CONFIG_KSU_HAS_MANUAL_HOOK=n"
  set_config "CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS=n"
  set_config "CONFIG_KSU_SYSCALL_HOOK=n"
fi

# =========================================
# ⚡ BOOST PERFORMANCE (CLEANED)
# =========================================
echo "⚙️ Adding Universal Performance Tuning"

set_config "CONFIG_CPU_FREQ=y"
set_config "CONFIG_CPU_FREQ_GOV_SCHEDUTIL=y"
set_config "CONFIG_CPU_FREQ_GOV_VORTEXCORE=y"
set_config "CONFIG_CPU_FREQ_GOV_ONDEMAND=y"

# Networking extras (Westwood must be active for vortex_gki.c)
set_config "CONFIG_IP_NF_TARGET_TTL=y"
set_config "CONFIG_NET_SCH_FQ=y"
set_config "CONFIG_NET_SCH_CAKE=y"
set_config "CONFIG_TCP_CONG_ADVANCED=y"
set_config "CONFIG_TCP_CONG_WESTWOOD=y"
set_config "CONFIG_IP6_NF_TARGET_HL=y"
set_config "CONFIG_IP6_NF_MATCH_HL=y"

# =========================================
# 🚫 REMOVE DEBUG FLAGS (FIXED LOGIC)
# =========================================
echo "Disable useless debugging configs for performance and resources"

set_config "CONFIG_UBSAN=n"
set_config "CONFIG_PAGE_OWNER=n"
set_config "CONFIG_RCU_TRACE=n"
set_config "CONFIG_SECTION_MISMATCH_WARN_ONLY=y"
