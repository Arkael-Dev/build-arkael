#/*!
# * © 2024-2026 Kingfinik98 (VorteX_E-Sport). All Rights Reserved.
# * Original Author: Kingfinik98
# * Original Repository: https://github.com/Kingfinik98/build-vortex
# * Signed-off-by: kingfinix98@gmail.com
# */
##!/usr/bin/env bash

# Constants
WORKDIR="$(pwd)"
if [ "$KVER" == "6.6" ]; then
  RELEASE="v0.3"
elif [ "$KVER" == "5.10" ]; then
  RELEASE="v0.3"
elif [ "$KVER" == "6.1" ]; then
  RELEASE="v0.1"
fi

KERNEL_NAME="VorteX_E-Sport"
USER="VorteX"
HOST="VorteX"
TIMEZONE="Asia/Jakarta"
ANYKERNEL_REPO="https://github.com/Kingfinik98/AnyKernel3"

if [ "$KVER" == "5.10" ]; then
  KERNEL_DEFCONFIG="gki_defconfig"
elif [ "$KVER" == "6.1" ]; then
  KERNEL_DEFCONFIG="gki_defconfig"
else
  KERNEL_DEFCONFIG="gki_defconfig"
fi

if [ "$KVER" == "6.6" ]; then
  KERNEL_REPO="https://github.com/ramabondanp/android_kernel_common-6.6.git"
  ANYKERNEL_BRANCH="master"
  KERNEL_BRANCH="android15-6.6-staging"
elif [ "$KVER" == "6.1" ]; then
  KERNEL_REPO="https://github.com/ramabondanp/android_kernel_common-6.1.git"
  ANYKERNEL_BRANCH="master"
  KERNEL_BRANCH="android14-6.1-staging"
elif [ "$KVER" == "5.10" ]; then
  KERNEL_REPO="https://github.com/Kingfinik98/kernel-common-android12-5.10.git"
  ANYKERNEL_BRANCH="master"
  KERNEL_BRANCH="vortex-basse"
fi
DEFCONFIG_TO_MERGE=""
GKI_RELEASES_REPO="https://github.com/Kingfinik98/build-vortex/releases"

CLANG_URL="https://github.com/greenforce-project/greenforce_clang/releases/download/20260410/gf-clang-22.1.4-20260410.tar.gz"
CLANG_BRANCH=""
AK3_ZIP_NAME="$KERNEL_NAME-REL-KVER-VARIANT-BUILD_DATE.zip"
OUTDIR="$WORKDIR/out"
KSRC="$WORKDIR/ksrc"
KERNEL_PATCHES="$WORKDIR/kernel-patches"

exec > >(tee $WORKDIR/build.log) 2>&1
trap 'error "Failed at line $LINENO [$BASH_COMMAND]"' ERR

source $WORKDIR/functions.sh

sudo timedatectl set-timezone "$TIMEZONE" || export TZ="$TIMEZONE"

log "Cloning kernel source from $(simplify_gh_url "$KERNEL_REPO")"
git clone -q --depth=1 $KERNEL_REPO -b $KERNEL_BRANCH $KSRC

cd $KSRC
LINUX_VERSION=$(make kernelversion)
LINUX_VERSION_CODE=${LINUX_VERSION//./}
DEFCONFIG_FILE=$(find ./arch/arm64/configs -name "$KERNEL_DEFCONFIG")

# --- PATCH INFINIX GT 20 PRO CAM (GKI 5.10 ONLY) ---
if [ "$KVER" == "5.10" ]; then
  log "📸 Applying Infinix GT 20 Pro Camera Fix..."
  curl -L "https://github.com/ramabondanp/android_kernel_common-5.10/commit/4fe04b60009e.patch" -o infinix_cam.patch
  patch -p1 < infinix_cam.patch || log "Camera patch already embedded."
  rm infinix_cam.patch
fi

# --- PATCH DRIVER SKIAVK (GKI 5.10 ONLY) ---
if [ "$KVER" == "5.10" ]; then
  log "Placing Driver Adreno SkiaVK libgsl.so..."
  mkdir -p $WORKDIR/vendor/lib64
  curl -LSs "https://raw.githubusercontent.com/Kingfinik98/build-vortex/6.x/system/vendor/lib64/libgsl.so" -o $WORKDIR/vendor/lib64/libgsl.so
  log "libgsl.so placed successfully"
fi

# --- INJECT VORTEX GPU TUNING ---
if [ "$KVER" == "5.10" ] || [ "$KVER" == "6.1" ] || [ "$KVER" == "6.6" ]; then
  log "Injecting VorteX Ultra-Safe Kernel Patch..."
  mkdir -p "$KSRC/drivers/misc"
  cp "$KERNEL_PATCHES/vortex_gki.c" "$KSRC/drivers/misc/vortex_gki.c"
  sed -i '/vortex_gki/d' "$KSRC/drivers/misc/Makefile"
  echo "obj-y += vortex_gki.o" >> "$KSRC/drivers/misc/Makefile"
fi

# --- INJECT VORTEXCORE GOVERNOR ---
#if [ "$KVER" == "5.10" ] || [ "$KVER" == "6.1" ] || [ "$KVER" == "6.6" ]; then
#  log "Injecting VortexCore Custom Governor..."
#  cp "$WORKDIR/governor-vortexcore.c" "$KSRC/drivers/cpufreq/governor-vortexcore.c"
  
#  if ! grep -q "governor-vortexcore.o" "$KSRC/drivers/cpufreq/Makefile"; then
#    echo "obj-\$(CONFIG_CPU_FREQ_GOV_VORTEXCORE) += governor-vortexcore.o" >> "$KSRC/drivers/cpufreq/Makefile"
 #   log "VortexCore added to cpufreq Makefile."
#  fi
  
#  if ! grep -q "CPU_FREQ_GOV_VORTEXCORE" "$KSRC/drivers/cpufreq/Kconfig"; then
#  s  cat << 'KCONF_EOF' >> "$KSRC/drivers/cpufreq/Kconfig"

# config CPU_FREQ_GOV_VORTEXCORE
#    tristate "VortexCore CPU frequency policy governor"
 #   depends on CPU_FREQ
  #  help
   #   VortexCore governor balances performance and efficiency for gaming and daily use.

  #    If in doubt, say N.
#KCONF_EOF
 #   log "VortexCore added to cpufreq Kconfig."
#  fi
#fi

# --- INJECT VORTEXMAX GOVERNOR ---
if [ "$KVER" == "5.10" ] || [ "$KVER" == "6.1" ] || [ "$KVER" == "6.6" ]; then
  log "Injecting VortexCore Custom Governor..."
  cp "$WORKDIR/governor-vortexmax.c" "$KSRC/drivers/cpufreq/governor-vortexmax.c"
  
  if ! grep -q "governor-vortexmax.o" "$KSRC/drivers/cpufreq/Makefile"; then
    echo "obj-\$(CONFIG_CPU_FREQ_GOV_VORTEXMAX) += governor-vortexmax.o" >> "$KSRC/drivers/cpufreq/Makefile"
    log "VortexMax added to cpufreq Makefile."
  fi
  
  if ! grep -q "CPU_FREQ_GOV_VORTEXMAX" "$KSRC/drivers/cpufreq/Kconfig"; then
    cat << 'KCONF_EOF' >> "$KSRC/drivers/cpufreq/Kconfig"

config CPU_FREQ_GOV_VORTEXMAX
    tristate "VortexCore CPU frequency policy governor"
    depends on CPU_FREQ
    help
      VortexCore governor balances performance and efficiency for gaming.

      If in doubt, say N.
KCONF_EOF
    log "VortexMax added to cpufreq Kconfig."
  fi
fi

# --- PATCH inject.sh ---
log "Applying inject.sh patch..."
wget -qO Inject_300hz.sh https://raw.githubusercontent.com/Kingfinik98/build-vortex/refs/heads/6.x/inject_ksu/Inject_300hz.sh
bash Inject_300hz.sh
rm Inject_300hz.sh

# --- PATCH WIFI SM8650 & FIX BTQCA (GKI 6.1 ONLY) ---
if [ "$KVER" == "6.1" ]; then
  log "Applying WiFi SM8650 patch..."
  curl -LSs https://github.com/OnePlus-12-Development/android_kernel_qcom_sm8650/commit/3e0cb08.patch | patch -p1 --forward || log "WiFi SM8650 patch skipped or already applied."

  TARGET_FILE="drivers/bluetooth/btqca.h"
  if [ -f "$TARGET_FILE" ]; then
    if grep -q "QCA_WCN3988" "$TARGET_FILE"; then
      log "[INFO] Patch already applied: QCA_WCN3988 exists."
    else
      sed -i '/QCA_WCN3998,/a\  QCA_WCN3988,' "$TARGET_FILE"
      log "[SUCCESS] Patch btqca applied successfully."
    fi
  else
    log "[WARNING] File $TARGET_FILE not found, skip patch."
  fi
fi

# --- ADD KSU INJECT SCRIPT ---
log "Injecting custom KSU & SuSFS configs from GitHub..."
export KSU
export KSU_SUSFS
if [ "$KVER" == "5.10" ]; then
  wget -qO inject.sh https://raw.githubusercontent.com/Kingfinik98/build-vortex/refs/heads/6.x/inject_ksu/gki_defconfig.sh
  bash inject.sh
  rm inject.sh
elif [ "$KVER" == "6.1" ] || [ "$KVER" == "6.6" ]; then
  wget -qO inject.sh https://raw.githubusercontent.com/Kingfinik98/build-vortex/refs/heads/6.x/inject_ksu/gki-deconfig-6.1.sh
  bash inject.sh
  rm inject.sh
fi
cd $WORKDIR

# Set Kernel variant (3 VARIANTS ONLY: yes, vortexsu, no)
log "Setting Kernel variant..."
case "$KSU" in
  "yes") VARIANT="KSU" ;;
  "vortexsu") VARIANT="VorteXSU" ;;
  "no") VARIANT="VNL" ;;
esac
susfs_included && VARIANT+="+SuSFS"

AK3_ZIP_NAME=${AK3_ZIP_NAME//KVER/$LINUX_VERSION}
AK3_ZIP_NAME=${AK3_ZIP_NAME//VARIANT/$VARIANT}

# Download Clang
CLANG_DIR="$WORKDIR/clang"
CLANG_BIN="${CLANG_DIR}/bin"
if [ -z "$CLANG_BRANCH" ]; then
  log "🔽 Downloading Clang..."
  wget -qO clang-archive "$CLANG_URL"
  mkdir -p "$CLANG_DIR"
  case "$(basename $CLANG_URL)" in
    *.tar.* | *.tgz) tar -xf clang-archive -C "$CLANG_DIR" ;;
    *.7z) 7z x clang-archive -o${CLANG_DIR}/ -bd -y > /dev/null ;;
    *) error "Unsupported file format" ;;
  esac
  rm clang-archive

  if [ $(find "$CLANG_DIR" -mindepth 1 -maxdepth 1 -type d | wc -l) -eq 1 ] \
    && [ $(find "$CLANG_DIR" -mindepth 1 -maxdepth 1 -type f | wc -l) -eq 0 ]; then
    SINGLE_DIR=$(find "$CLANG_DIR" -mindepth 1 -maxdepth 1 -type d)
    mv $SINGLE_DIR/* $CLANG_DIR/
    rm -rf $SINGLE_DIR
  fi
else
  log "🔽 Cloning Clang..."
  git clone --depth=1 -q "$CLANG_URL" -b "$CLANG_BRANCH" "$CLANG_DIR"
fi

# Clone GNU Assembler
log "Cloning GNU Assembler..."
GAS_DIR="$WORKDIR/gas"
git clone --depth=1 -q \
  https://android.googlesource.com/platform/prebuilts/gas/linux-x86 \
  -b main \
  "$GAS_DIR"

export PATH="${CLANG_BIN}:${GAS_DIR}:$PATH"

COMPILER_STRING=$(clang -v 2>&1 | head -n 1 | sed 's/(https..*//' | sed 's/ version//')

cd $KSRC

## KernelSU setup
if ksu_included; then
  for KSU_PATH in drivers/staging/kernelsu drivers/kernelsu KernelSU KernelSU-Next; do
    if [ -d $KSU_PATH ]; then
      log "KernelSU driver found in $KSU_PATH, Removing..."
      KSU_DIR=$(dirname "$KSU_PATH")
      [ -f "$KSU_DIR/Kconfig" ] && sed -i '/kernelsu/d' $KSU_DIR/Kconfig
      [ -f "$KSU_DIR/Makefile" ] && sed -i '/kernelsu/d' $KSU_DIR/Makefile
      rm -rf $KSU_PATH
    fi
  done

  install_ksu 'pershoot/KernelSU-Next' 'dev-susfs'
  config --enable CONFIG_KSU

  cd KernelSU-Next
  patch -p1 < $KERNEL_PATCHES/ksu/ksun-add-more-managers-support.patch
  cd $OLDPWD
  
  log "Applying fix for undefined SUSFS symbols (KernelSU-Next)..."
  if [ -f "drivers/kernelsu/supercalls.c" ]; then
    sed -i 's/#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME/#if 0 \/\* CONFIG_KSU_SUSFS_SPOOF_UNAME Disabled to fix build \*\//' drivers/kernelsu/supercalls.c || true
    log "SUSFS symbol fix applied for KernelSU-Next."
  else
    log "Skipping obsolete SUSFS uname fix (Handled natively in KernelSU-Next supercalls.c)."
  fi

  if [ "$KVER" == "5.10" ]; then
    log "Applying fix for duplicate symbol __stack_chk_guard (GKI 5.10)..."
    if [ -f "drivers/kernelsu/ksu.c" ]; then
      sed -i '/^#if.*CONFIG_STACKPROTECTOR_PER_TASK/c\#if 0 \/\/ Disabled to fix duplicate symbol' drivers/kernelsu/ksu.c || true
      log "Stack protector fix applied."
    else
      log "Skipping obsolete stack protector fix (File not found in KernelSU-Next)."
    fi
  fi

# --- VorteXSU Setup Block (KPM ENABLED) ---
elif [ "$KSU" == "vortexsu" ]; then
  log "Setting up VorteXSU for KVER $KVER..."
  
  log "Running VorteXSU setup from main branch..."
  curl -LSs "https://raw.githubusercontent.com/Kingfinik98/VortexSU/refs/heads/main/kernel/setup.sh" | bash -s main
  if [ "$KVER" == "5.10" ]; then
    log "Applying SUSFS patches for GKI 5.10 (VorteXSU Method)..."
    SUSFS_BRANCH="gki-android12-5.10"
    git clone https://gitlab.com/simonpunk/susfs4ksu/ -b $SUSFS_BRANCH sus
    rm -rf sus/.git
    susfs=sus/kernel_patches
    cp -r $susfs/fs .
    cp -r $susfs/include .
    cp -r $susfs/50_add_susfs_in_${SUSFS_BRANCH}.patch .
    patch -p1 < 50_add_susfs_in_${SUSFS_BRANCH}.patch || true
    SUSFS_VERSION=$(grep -E '^#define SUSFS_VERSION' ./include/linux/susfs.h | cut -d' ' -f3 | sed 's/"//g')
    config --enable CONFIG_KPM
    config --enable CONFIG_KSU_MULTI_MANAGER_SUPPORT
    config --enable CONFIG_KSU_SUSFS
    log "[✓] VorteXSU & SUSFS patched for $KVER."
  else
    config --enable CONFIG_KSU_SUSFS
    log "SUSFS config enabled for $KVER. Applying patches in Standard block..."
  fi
fi

# ============================================
# WILDKSU DIHAPUS (Error kompilasi sucompat.c)
# ============================================

# SUSFS (Standard Logic for KernelSU yes & VorteXSU 6.1/6.6)
if susfs_included; then
  # Skip standard SUSFS if already handled by VorteXSU 5.10 custom method
  if [ "$KSU" != "vortexsu" ] || ([ "$KSU" == "vortexsu" ] && ([ "$KVER" == "6.1" ] || [ "$KVER" == "6.6" ])); then
    log "Applying kernel-side susfs patches (Standard Method)"
    SUSFS_DIR="$WORKDIR/susfs"
    SUSFS_PATCHES="${SUSFS_DIR}/kernel_patches"
    if [ "$KVER" == "6.6" ]; then
      SUSFS_BRANCH=gki-android15-6.6
    elif [ "$KVER" == "6.1" ]; then
      SUSFS_BRANCH=gki-android14-6.1
    elif [ "$KVER" == "5.10" ]; then
      SUSFS_BRANCH=gki-android12-5.10
    fi
    git clone --depth=1 -q https://gitlab.com/simonpunk/susfs4ksu -b $SUSFS_BRANCH $SUSFS_DIR
    cp -R $SUSFS_PATCHES/fs/* ./fs
    cp -R $SUSFS_PATCHES/include/* ./include
    patch -p1 < $SUSFS_PATCHES/50_add_susfs_in_${SUSFS_BRANCH}.patch || true
    
    if [ $(echo "$LINUX_VERSION_CODE" | head -c4) -eq 6630 ]; then
      patch -p1 < $KERNEL_PATCHES/susfs/namespace.c_fix.patch || true
      patch -p1 < $KERNEL_PATCHES/Susfs/task_mmu.c_fix.patch || true
    elif [ $(echo "$LINUX_VERSION_CODE" | head -c4) -eq 6658 ]; then
      patch -p1 < $KERNEL_PATCHES/Susfs/task_mmu.c_fix-k6.6.58.patch || true
    elif [ $(echo "$LINUX_VERSION_CODE" | head -c2) -eq 61 ]; then
      patch -p1 < $KERNEL_PATCHES/susfs/fs_proc_base.c-fix-k6.1.patch || true
      
      NS_INJECT_FILE="$WORKDIR/.ns_inject_tmp"
      
      cat << 'EOF' > "$NS_INJECT_FILE"

#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
#include <linux/susfs_def.h>
extern bool susfs_is_current_ksu_domain(void);
extern bool susfs_is_current_zygote_domain(void);
extern bool susfs_is_boot_completed_triggered;
extern bool susfs_is_sdcard_android_data_decrypted;

static DEFINE_IDA(susfs_mnt_id_ida);
static DEFINE_IDA(susfs_mnt_group_ida);

#define DEFAULT_KSU_MNT_ID 100000
#define DEFAULT_KSU_MNT_GROUP_ID 100000
#define VFSMOUNT_MNT_FLAGS_KSU_UNSHARED_MNT BIT(24)
#define CL_COPY_MNT_NS BIT(25)
#endif

EOF

      if ! grep -q "static DEFINE_IDA(susfs_mnt_id_ida);" ./fs/namespace.c; then
        sed -i '/#include "internal.h"/r '"$NS_INJECT_FILE" ./fs/namespace.c
        log "SUSFS definitions injected successfully."
      else
        log "SUSFS definitions already exist."
      fi
      
      rm -f "$NS_INJECT_FILE"

    elif [ $(echo "$LINUX_VERSION_CODE" | head -c3) -eq 510 ]; then
      if [ "$KSU" != "vortexsu" ]; then
        patch -p1 < $KERNEL_PATCHES/susfs/pershoot-susfs-k5.10.patch || true
      fi
    fi

    if [ $(echo "$LINUX_VERSION_CODE" | head -c1) -eq 6 ]; then
      if [ "$KSU" == "yes" ]; then
        if [ "$KVER" == "6.1" ]; then
          log "Applying manual statfs CRC fix for KernelSU Next GKI 6.1..."
          sed -i '/#include <linux\/susfs_def.h>/i #ifndef __GENKSYMS__' fs/statfs.c
          sed -i '/#include <linux\/susfs_def.h>/a #endif' fs/statfs.c
        elif [ "$KVER" == "6.6" ]; then
          log "Applying manual statfs CRC fix for KernelSU Next GKI 6.6..."
          sed -i '/#include <linux\/susfs_def.h>/i #ifndef __GENKSYMS__' fs/statfs.c
          sed -i '/#include <linux\/susfs_def.h>/a #endif' fs/statfs.c
        else
          patch -p1 < $KERNEL_PATCHES/Susfs/fix-statfs-crc-mismatch-susfs.patch || true
        fi
      elif [ "$KSU" == "vortexsu" ] && ([ "$KVER" == "6.1" ] || [ "$KVER" == "6.6" ]); then
        log "Applying manual statfs CRC fix for VorteXSU GKI $KVER..."
        sed -i '/#include <linux\/susfs_def.h>/i #ifndef __GENKSYMS__' fs/statfs.c
        sed -i '/#include <linux\/susfs_def.h>/a #endif' fs/statfs.c
      fi
    fi

    SUSFS_VERSION=$(grep -E '^#define SUSFS_VERSION' ./include/linux/susfs.h | cut -d' ' -f3 | sed 's/"//g')
    config --enable CONFIG_KSU_SUSFS
  else
    log "Skipping standard SUSFS patch (Handled by VorteXSU 5.10 custom method)."
  fi
else
  config --disable CONFIG_KSU_SUSFS
fi

# set localversion
if [ $TODO == "kernel" ]; then
  LATEST_COMMIT_HASH=$(git rev-parse --short HEAD)
  if [ $STATUS == "BETA" ]; then
    SUFFIX="$LATEST_COMMIT_HASH"
  else
    SUFFIX="${RELEASE}@${LATEST_COMMIT_HASH}"
  fi
  config --set-str CONFIG_LOCALVERSION "-$KERNEL_NAME/$SUFFIX"
  config --disable CONFIG_LOCALVERSION_AUTO
  sed -i 's/echo "+"/# echo "+"/g' scripts/setlocalversion
fi

export KBUILD_BUILD_USER="$USER"
export KBUILD_BUILD_HOST="$HOST"
export KBUILD_BUILD_TIMESTAMP=$(date)
export KCFLAGS="-w"
if [ $(echo "$LINUX_VERSION_CODE" | head -c1) -eq 6 ]; then
  MAKE_ARGS=(
    LLVM=1
    ARCH=arm64
    CROSS_COMPILE=aarch64-linux-gnu-
    CROSS_COMPILE_COMPAT=arm-linux-gnueabi-
    -j$(nproc --all)
    O=$OUTDIR
  )
else
  MAKE_ARGS=(
    LLVM=1
    LLVM_IAS=1
    ARCH=arm64
    CROSS_COMPILE=aarch64-linux-gnu-
    CROSS_COMPILE_COMPAT=arm-linux-gnueabi-
    -j$(nproc --all)
    O=$OUTDIR
  )
fi

KERNEL_IMAGE="$OUTDIR/arch/arm64/boot/Image"
MODULE_SYMVERS="$OUTDIR/Module.symvers"
if [ $(echo "$LINUX_VERSION_CODE" | head -c1) -eq 6 ]; then
  KMI_CHECK="$WORKDIR/py/kmi-check-6.x.py"
else
  KMI_CHECK="$WORKDIR/py/kmi-check-5.x.py"
fi

text=$(
  cat << EOF
🐧 *Linux Version*: $LINUX_VERSION
📅 *Build Date*: $KBUILD_BUILD_TIMESTAMP
📛 *KernelSU*: ${KSU}
ඞ *SuSFS*: $(susfs_included && echo "$SUSFS_VERSION" || echo "None")
🔰 *Compiler*: $COMPILER_STRING
EOF
)

## Build GKI
log "Generating config..."
make ${MAKE_ARGS[@]} $KERNEL_DEFCONFIG

log "Enabling VorteX kernel dependencies..."
config --enable CONFIG_TCP_CONG_WESTWOOD
config --enable CONFIG_DEVFREQ_GOV_SCHEDUTIL
config --enable CONFIG_CPU_FREQ_GOV_VORTEXCORE

if [ "$KVER" == "5.10" ] || [ "$KVER" == "6.1" ] || [ "$KVER" == "6.6" ]; then
  config --enable CONFIG_ANDROID_LOW_MEMORY_KILLER
  config --enable CONFIG_KSM
  config --enable CONFIG_CPU_IDLE
fi

if [ "$DEFCONFIG_TO_MERGE" ]; then
  log "Merging configs..."
  if [ -f "scripts/kconfig/merge_config.sh" ]; then
    for config in $DEFCONFIG_TO_MERGE; do
      make ${MAKE_ARGS[@]} scripts/kconfig/merge_config.sh $config
    done
  else
    error "scripts/kconfig/merge_config.sh does not exist in the kernel source"
  fi
  make ${MAKE_ARGS[@]} olddefconfig
fi

if [ $TODO == "defconfig" ]; then
  log "Uploading defconfig..."
  upload_file $OUTDIR/.config
  exit 0
fi

log "Building kernel..."
make ${MAKE_ARGS[@]}

if [ $(echo "$LINUX_VERSION_CODE" | head -c1) -eq 6 ]; then
  $KMI_CHECK "$KSRC/android/abi_gki_aarch64.stg" "$MODULE_SYMVERS" || true
else
  $KMI_CHECK "$KSRC/android/abi_gki_aarch64.xml" "$MODULE_SYMVERS" || true
fi

# --- KPM SECTION (ONLY VORTEXSU) ---
log "Applying KPM Patch..."
if [ "$KSU" == "vortexsu" ]; then
  cd $OUTDIR/arch/arm64/boot
  if [ -f Image ]; then
    echo "✅ Image found, applying KPM patch for ${VARIANT}..."
    curl -LSs "https://github.com/Kingfinik98/SukiSU_patch/raw/refs/heads/main/kpm/patch_linux" -o patch
    chmod 777 patch
    ./patch
    if [ -f oImage ]; then
      mv -f oImage Image
      ls -lh Image
      log "✅ KPM Patch applied successfully for ${VARIANT}."
    else
      log "Error: oImage not found!"
    fi
  else
    log "Warning: Image file not found in $PWD. Skipping KPM patch."
  fi
else
  log "Skipping KPM patch (Not KPM-enabled variant: ${VARIANT})."
fi
cd $WORKDIR

## Post-compiling stuff
cd $WORKDIR

log "Cloning anykernel from $(simplify_gh_url "$ANYKERNEL_REPO")"
git clone -q --depth=1 $ANYKERNEL_REPO -b $ANYKERNEL_BRANCH anykernel

if [ $STATUS == "BETA" ]; then
  BUILD_DATE=$(date -d "$KBUILD_BUILD_TIMESTAMP" +"%Y%m%d-%H%M")
  AK3_ZIP_NAME=${AK3_ZIP_NAME//BUILD_DATE/$BUILD_DATE}
  AK3_ZIP_NAME=${AK3_ZIP_NAME//-REL/}
  sed -i \
    "s/kernel.string=.*/kernel.string=${KERNEL_NAME} ${LINUX_VERSION} (${BUILD_DATE}) ${VARIANT}/g" \
    $WORKDIR/anykernel/anykernel.sh
else
  AK3_ZIP_NAME=${AK3_ZIP_NAME//-BUILD_DATE/}
  AK3_ZIP_NAME=${AK3_ZIP_NAME//REL/$RELEASE}
  sed -i \
    "s/kernel.string=.*.*/kernel.string=${KERNEL_NAME} ${RELEASE} ${LINUX_VERSION} ${VorteX} ${VARIANT}/g" \
    $WORKDIR/anykernel/anykernel.sh
fi

cd anykernel
log "Zipping anykernel..."
cp $KERNEL_IMAGE .
zip -r9 $WORKDIR/$AK3_ZIP_NAME ./*
cd $OLDPWD

if [ $STATUS != "BETA" ]; then
  echo "BASE_NAME=$KERNEL_NAME-$VARIANT" >> $GITHUB_ENV
  mkdir -p $WORKDIR/artifacts
  mv $WORKDIR/*.zip $WORKDIR/artifacts
fi

if [ $LAST_BUILD == "true" ] && [ $STATUS != "BETA" ]; then
  (
    echo "LINUX_VERSION=$LINUX_VERSION"
    echo "SUSFS_VERSION=$(curl -s https://gitlab.com/simonpunk/susfs4ksu/raw/gki-android15-6.6/kernel_patches/include/linux/susfs.h | grep -E '^#define SUSFS_VERSION' | cut -d ' ' -f3 | sed 's/"//g')"
    echo "KERNEL_NAME=$KERNEL_NAME"
    echo "RELEASE_REPO=$(simplify_gh_url "$GKI_RELEASES_REPO")"
  ) >> $WORKDIR/artifacts/info.txt
fi

if [ $STATUS == "BETA" ]; then
  upload_file "$WORKDIR/$AK3_ZIP_NAME" "$text"
  upload_file "$WORKDIR/build.log"
else
  send_msg "✅ Build Succeeded for $VorteX ${VARIANT} variant."
fi

exit 0
