#!/usr/bin/env bash
# DCONFIG BY kingfinix98@gmail.com

DEFCONFIG="arch/arm64/configs/gki_defconfig"

# Only enable the governor that build.sh actually injected (Kconfig + Makefile).
# Setting the other one would reference a symbol that does not exist.
case "${GOVERNOR_CHOICE,,}" in
    arkael) GOV_CFG="CONFIG_CPU_FREQ_GOV_ARKAEL=y" ;;
    *)      GOV_CFG="CONFIG_CPU_FREQ_GOV_VORTEXCORE=y" ;;
esac

echo "⚙️ Added KSU & SuSFS configuration"
cat >> "$DEFCONFIG" <<EOF
CONFIG_KSU=y
CONFIG_KPM=y
CONFIG_KSU_MULTI_MANAGER_SUPPORT=y
CONFIG_KPROBES=y
CONFIG_KPROBE_EVENTS=y
EOF

if [ "$KSU" = "SukiSU" ]; then
    if [ "$KSU_SUSFS" = "true" ]; then
        echo "🔧 Mode: SukiSU + SuSFS Enabled"
        cat >> "$DEFCONFIG" <<EOF
CONFIG_KSU_SUSFS=y
EOF
    else
        echo "🔧 Mode: SukiSU Standard (No SuSFS)"
    fi

elif [ "$KSU_SUSFS" = "true" ]; then
    echo "🔧 Mode: SuSFS Hook Enabled"
    cat >> "$DEFCONFIG" <<EOF
CONFIG_KSU_SUSFS=y
CONFIG_KSU_SUSFS_HAS_MAGIC_MOUNT=y
CONFIG_KSU_SUSFS_SUS_PATH=y
CONFIG_KSU_SUSFS_SUS_MOUNT=y
CONFIG_KSU_SUSFS_SUS_KSTAT_SPOOF_GENERIC=y
CONFIG_KSU_SUSFS_SUS_KSTAT=y
CONFIG_KSU_SUSFS_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT=y
CONFIG_KSU_SUSFS_AUTO_ADD_SUS_BIND_MOUNT=y
CONFIG_KSU_SUSFS_AUTO_ADD_SUS_KSTAT=y
CONFIG_KSU_SUSFS_SUS_OVERLAYFS=n
CONFIG_KSU_SUSFS_TRY_UMOUNT=n
CONFIG_KSU_SUSFS_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT=n
CONFIG_KSU_SUSFS_SPOOF_UNAME=y
CONFIG_KSU_SUSFS_ENABLE_LOG=y
CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS=y
CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG=y
CONFIG_KSU_SUSFS_OPEN_REDIRECT=y
CONFIG_KSU_MANUAL_HOOK=n
CONFIG_KSU_HAS_MANUAL_HOOK=n
EOF

else
    echo "🔧 Mode: Kprobes Hook Standard"
    cat >> "$DEFCONFIG" <<EOF
CONFIG_KSU_SUSFS=n
CONFIG_KSU_SUSFS_SUS_SU=n
CONFIG_KSU_MANUAL_HOOK=n
CONFIG_KSU_HAS_MANUAL_HOOK=n
CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS=n
CONFIG_KSU_SYSCALL_HOOK=n
EOF
fi

echo "⚙️ Adding Universal Performance Tuning"
cat >> "$DEFCONFIG" <<EOF
CONFIG_TMPFS_XATTR=y
CONFIG_NO_HZ=y
CONFIG_TMPFS_POSIX_ACL=y
CONFIG_IP_NF_TARGET_TTL=y
CONFIG_TCP_CONG_ADVANCED=y
CONFIG_NET_SCHED=y
CONFIG_NET_SCH_FQ=y
# CONFIG_IOSCHED_BFQ is not set
# CONFIG_BFQ_GROUP_IOSCHED is not set
CONFIG_MQ_IOSCHED_ADIOS=y
# CONFIG_DEFAULT_FQ_CODEL is not set
CONFIG_HZ_300=y
CONFIG_HZ=300
CONFIG_CPU_FREQ=y
CONFIG_SWAP=y
CONFIG_CPU_FREQ_GOV_SCHEDUTIL=y
CONFIG_CPU_FREQ_GOV_REFLEX=y
${GOV_CFG}
CONFIG_CPU_FREQ_GOV_ONDEMAND=y
CONFIG_USB_CONFIGFS_MASS_STORAGE=y

# TCP Congestion Control (BBRv3 does not exist in this 5.10 tree -> plain BBR)
CONFIG_TCP_CONG_WESTWOOD=n
CONFIG_TCP_CONG_HTCP=n
CONFIG_TCP_CONG_BBR=y
CONFIG_DEFAULT_BBR=y
# CONFIG_DEFAULT_CUBIC is not set

# IPv6
CONFIG_IPV6_ROUTER_PREF=y
CONFIG_IPV6_ROUTE_INFO=y
CONFIG_IPV6_OPTIMISTIC_DAD=y

# IPC
CONFIG_SYSVIPC=y
CONFIG_POSIX_MQUEUE=y

# Namespaces
CONFIG_IPC_NS=y
CONFIG_PID_NS=y

# HW Access Support
CONFIG_DEVTMPFS=y

# --- Optional but recommended ---
# Networking (Docker/NAT support)
CONFIG_NETFILTER_XT_MATCH_ADDRTYPE=y

# UFW support
CONFIG_NETFILTER_XT_TARGET_REJECT=y
CONFIG_NETFILTER_XT_TARGET_LOG=y
CONFIG_NETFILTER_XT_MATCH_RECENT=y

# Fail2ban support
CONFIG_IP_SET=y
CONFIG_IP_SET_HASH_IP=y
CONFIG_IP_SET_HASH_NET=y
CONFIG_NETFILTER_XT_SET=y

# xattr for tmpfs (NixOS compatibility)
CONFIG_TMPFS_XATTR=y
EOF

if [ "$KVER" = "5.10" ]; then
    echo "⚙️ Adding VorteX Native C-Script Dependencies (5.10 Only)"
    cat >> "$DEFCONFIG" <<EOF
CONFIG_DEVFREQ_GOV_PERFORMANCE=y
EOF

    echo "⚙️ Added LTO & Compiler Optimization (KVER 5.10 Only)"
    cat >> "$DEFCONFIG" <<EOF
CONFIG_LTO=y
CONFIG_LTO_CLANG=y
CONFIG_ARCH_SUPPORTS_LTO_CLANG=y
CONFIG_ARCH_SUPPORTS_LTO_CLANG_THIN=y
CONFIG_HAS_LTO_CLANG=y
# CONFIG_LTO_NONE is not set
# CONFIG_LTO_CLANG_FULL is not set
CONFIG_LTO_CLANG_THIN=y
EOF
else
    echo "⚙️ LTO & VorteX Optimization skipped (For KVER 6.1 & 6.6)"
fi

# ==========================================
# VorteX Native FPS Engine Include
# ==========================================
echo "⚙️ Adding VorteX Native FPS Engine Module"
cat >> "$DEFCONFIG" <<EOF
CONFIG_KSM=y
CONFIG_CPU_IDLE=y
EOF

# ==========================================
# Dedupe: keep only the LAST occurrence of every CONFIG_* symbol.
# Everything above is appended with `cat >>`, so earlier copies (from the
# stock defconfig or from this script) would otherwise trigger
# "override: reassigning" warnings. Last value wins, same as Kconfig.
# ==========================================
echo "🧹 Deduplicating $DEFCONFIG"
awk '
{
    lines[NR] = $0
    if (match($0, /^(# )?CONFIG_[A-Za-z0-9_]+/)) {
        k = substr($0, RSTART, RLENGTH)
        sub(/^# /, "", k)
        key[NR] = k
        last[k] = NR
    }
}
END {
    for (i = 1; i <= NR; i++)
        if (!(i in key) || last[key[i]] == i)
            print lines[i]
}
' "$DEFCONFIG" > "$DEFCONFIG.tmp" && mv "$DEFCONFIG.tmp" "$DEFCONFIG"
