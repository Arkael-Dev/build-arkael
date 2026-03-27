#!/usr/bin/env bash

# Constants
WORKDIR="$(pwd)"
if [ "$KVER" == "6.6" ]; then
  RELEASE="v0.3"
elif [ "$KVER" == "5.10" ]; then
  RELEASE="v0.3"
elif [ "$KVER" == "6.1" ]; then
  RELEASE="v0.1"
fi

KERNEL_NAME="VorteX_Flux"
USER="VorteX"
HOST="VorteX"
TIMEZONE="Asia/Jakarta"
ANYKERNEL_REPO="https://github.com/Kingfinik98/AnyKernel3"

# Fixed Logic
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
  KERNEL_BRANCH="android14-6.1"
elif [ "$KVER" == "5.10" ]; then
  KERNEL_REPO="https://github.com/ramabondanp/android_kernel_common-5.10.git"
  ANYKERNEL_BRANCH="master"
  KERNEL_BRANCH="android12-5.10-staging"
fi
DEFCONFIG_TO_MERGE=""
GKI_RELEASES_REPO="https://github.com/Kingfinik98/gki-builder"

CLANG_URL="https://github.com/greenforce-project/greenforce_clang/releases/download/20260302/gf-clang-23.0.0-20260302.tar.gz"
CLANG_BRANCH=""
AK3_ZIP_NAME="$KERNEL_NAME-REL-KVER-VARIANT-BUILD_DATE.zip"
OUTDIR="$WORKDIR/out"
KSRC="$WORKDIR/ksrc"
KERNEL_PATCHES="$WORKDIR/kernel-patches"

# Handle error
exec > >(tee $WORKDIR/build.log) 2>&1
trap 'error "Failed at line $LINENO [$BASH_COMMAND]"' ERR

# Import functions
source $WORKDIR/functions.sh

# Set timezone
sudo timedatectl set-timezone "$TIMEZONE" || export TZ="$TIMEZONE"

# Clone kernel source
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

# --- PATCH inject.sh ---
log "Applying inject.sh patch..."
wget -qO Inject_300hz.sh https://raw.githubusercontent.com/Kingfinik98/build-vortex/refs/heads/6.x/inject_ksu/Inject_300hz.sh
bash Inject_300hz.sh
rm Inject_300hz.sh

# --- PATCH WIFI SM8650 (GKI 6.1 ONLY) ---
if [ "$KVER" == "6.1" ]; then
  log "Applying WiFi SM8650 patch..."
  curl -LSs https://github.com/OnePlus-12-Development/android_kernel_qcom_sm8650/commit/3e0cb08.patch | patch -p1 --forward || log "WiFi SM8650 patch skipped or already applied."
fi

# --- ADD KSU INJECT SCRIPT ---
log "Injecting custom KSU & SuSFS configs from GitHub..."
export KSU
export KSU_SUSFS
wget -qO inject.sh https://raw.githubusercontent.com/Kingfinik98/build-vortex/refs/heads/6.x/inject_ksu/gki_defconfig.sh
bash inject.sh
rm inject.sh
cd $WORKDIR

# Set Kernel variant
log "Setting Kernel variant..."
case "$KSU" in
  "yes") VARIANT="KSU" ;;
  "vortexsu") VARIANT="VorteXSU" ;;
  "no") VARIANT="VNL" ;;
esac
susfs_included && VARIANT+="+SuSFS"

# Replace Placeholder in zip name
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
    *.tar.* | *.tgz)
      tar -xf clang-archive -C "$CLANG_DIR"
      ;;
    *.7z)
      7z x clang-archive -o${CLANG_DIR}/ -bd -y > /dev/null
      ;;
    *)
      error "Unsupported file format"
      ;;
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

# Extract clang version
COMPILER_STRING=$(clang -v 2>&1 | head -n 1 | sed 's/(https..*//' | sed 's/ version//')

cd $KSRC

## KernelSU setup
if ksu_included; then
  # Remove existing KernelSU drivers
  for KSU_PATH in drivers/staging/kernelsu drivers/kernelsu KernelSU KernelSU-Next; do
    if [ -d $KSU_PATH ]; then
      log "KernelSU driver found in $KSU_PATH, Removing..."
      KSU_DIR=$(dirname "$KSU_PATH")

      [ -f "$KSU_DIR/Kconfig" ] && sed -i '/kernelsu/d' $KSU_DIR/Kconfig
      [ -f "$KSU_DIR/Makefile" ] && sed -i '/kernelsu/d' $KSU_DIR/Makefile

      rm -rf $KSU_PATH
    fi
  done

  # --- GKI 5.10 Super-Builders Method (Local Clone) ---
  if [ "$KVER" == "5.10" ]; then
    log "Preparing Super-Builders patches..."
    
    # 1. Clone repo Super-Builders
    SB_DIR="$WORKDIR/super-builders"
    git clone --depth=1 -q https://github.com/Kingfinik98/Super-Builders.git $SB_DIR
    
    # Set path ke folder patches
    PATCH_DIR="$SB_DIR/android12-5.10/KernelSU-Next/patches"
    HELPER_DIR="$SB_DIR/android12-5.10/build-helpers"
    
    log "Installing KernelSU-Next (pershoot/dev-susfs)..."
    git clone -q https://github.com/pershoot/KernelSU-Next drivers/kernelsu
    cd drivers/kernelsu
    git checkout dev-susfs || error "Failed to checkout dev-susfs"
    
    # Apply Patch 70 (Local File)
    log "Applying 70_ksu_safety-kernelsu-next-5.10.patch..."
    patch -p1 -F3 --no-backup-if-mismatch < "$PATCH_DIR/70_ksu_safety-kernelsu-next-5.10.patch" || log "Patch 70 skipped."
    
    cd $OLDPWD
    
    # Setup Symlink & Kconfig
    log "Setting up KernelSU symlinks..."
    ln -sf drivers/kernelsu/kernel KernelSU-Next
    echo 'source "drivers/kernelsu/Kconfig"' >> drivers/Kconfig
    sed -i 's/^obj-y\s*+=\s*$/obj-y += kernelsu\n&/' drivers/Makefile
    
    log "KernelSU-Next setup done."

  else
    # Metode standar untuk 6.1 / 6.6
    install_ksu 'pershoot/KernelSU-Next' 'dev-susfs'
    config --enable CONFIG_KSU

    cd KernelSU-Next
    patch -p1 < $KERNEL_PATCHES/ksu/ksun-add-more-managers-support.patch
    cd $OLDPWD
  fi

# --- VorteXSU Setup Block ---
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
    
    # Patch ZeroMount VorteXSU (Menggunakan Local Clone jika sudah ada, atau fallback ke repo)
    # Asumsi SB_DIR sudah di clone di atas atau buat jika belum
    if [ ! -d "$SB_DIR" ]; then
       SB_DIR="$WORKDIR/super-builders"
       git clone --depth=1 -q https://github.com/Kingfinik98/Super-Builders.git $SB_DIR
    fi
    
    log "Applying ZeroMount patch for VorteXSU (GKI 5.10)..."
    patch -p1 < "$SB_DIR/android12-5.10/ReSukiSU/patches/60_zeromount-android12-5.10.patch" || log "ZeroMount patch skipped."
  else
    config --enable CONFIG_KSU_SUSFS
    log "SUSFS config enabled for $KVER. Applying patches in Standard block..."
  fi
fi

# SUSFS (Standard Logic)
if susfs_included; then
  if [ "$KSU" != "vortexsu" ] || ([ "$KSU" == "vortexsu" ] && ([ "$KVER" == "6.1" ] || [ "$KVER" == "6.6" ])); then
    log "Applying kernel-side susfs patches (Standard Method)"
    
    if [ "$KVER" == "5.10" ]; then
      log "Applying Super-Builders patches sequence (50 -> 51 -> 60 -> Fix)..."
      
      # Gunakan variabel $SB_DIR yang sudah di clone di blok ksu_included
      # Jika belum ada (misal bypass logic), clone sekarang
      if [ ! -d "$SB_DIR" ]; then
         SB_DIR="$WORKDIR/super-builders"
         git clone --depth=1 -q https://github.com/Kingfinik98/Super-Builders.git $SB_DIR
      fi
      
      PATCH_DIR="$SB_DIR/android12-5.10/KernelSU-Next/patches"
      HELPER_DIR="$SB_DIR/android12-5.10/build-helpers"
      
      PATCH_OPTS="-p1 -F3 --no-backup-if-mismatch"

      # 1. SUSFS Upstream (50_)
      log "Applying 50_add_susfs_in_gki..."
      patch $PATCH_OPTS < "$PATCH_DIR/50_add_susfs_in_gki-android12-5.10.patch" || log "Patch 50 skipped."

      # 2. Enhanced SUSFS (51_)
      log "Applying 51_enhanced_susfs..."
      patch $PATCH_OPTS < "$PATCH_DIR/51_enhanced_susfs-android12-5.10.patch" || log "Patch 51 skipped."

      # Patch 70 sudah diapply
      log "Skipping Patch 70 (already applied)."

      # 3. ZeroMount (60_)
      log "Applying 60_zeromount..."
      patch $PATCH_OPTS < "$PATCH_DIR/60_zeromount-android12-5.10.patch" || log "Patch 60 skipped."

      # 4. Fix Compat Script
      log "Running fix-susfs-compat.sh..."
      if [ -f "$HELPER_DIR/fix-susfs-compat.sh" ]; then
        chmod +x "$HELPER_DIR/fix-susfs-compat.sh"
        SUBLEVEL=$(echo $LINUX_VERSION | cut -d'.' -f3)
        bash "$HELPER_DIR/fix-susfs-compat.sh" . "$SUBLEVEL" "android12" "5.10" "$KERNEL_PATCHES" || log "Compat fix finished with warnings."
      else
        log "Error: fix-susfs-compat.sh not found in cloned repo."
      fi

      SUSFS_VERSION=$(grep -E '^#define SUSFS_VERSION' ./include/linux/susfs.h | cut -d' ' -f3 | sed 's/"//g')
      config --enable CONFIG_KSU_SUSFS
      
      # Cleanup repo clone
      rm -rf $SB_DIR

    else
      # --- LOGIKA UNTUK 6.1 & 6.6 ---
      SUSFS_DIR="$WORKDIR/susfs"
      SUSFS_PATCHES="${SUSFS_DIR}/kernel_patches"
      if [ "$KVER" == "6.6" ]; then
        SUSFS_BRANCH=gki-android15-6.6
      elif [ "$KVER" == "6.1" ]; then
        SUSFS_BRANCH=gki-android14-6.1
      fi
      git clone --depth=1 -q https://gitlab.com/simonpunk/susfs4ksu -b $SUSFS_BRANCH $SUSFS_DIR
      cp -R $SUSFS_PATCHES/fs/* ./fs
      cp -R $SUSFS_PATCHES/include/* ./include
      patch -p1 < $SUSFS_PATCHES/50_add_susfs_in_${SUSFS_BRANCH}.patch || true
      
      # PATCH FIXES
      if [ $(echo "$LINUX_VERSION_CODE" | head -c4) -eq 6630 ]; then
        patch -p1 < $KERNEL_PATCHES/susfs/namespace.c_fix.patch || true
        patch -p1 < $KERNEL_PATCHES/susfs/task_mmu.c_fix.patch || true
      elif [ $(echo "$LINUX_VERSION_CODE" | head -c4) -eq 6658 ]; then
        patch -p1 < $KERNEL_PATCHES/susfs/task_mmu.c_fix-k6.6.58.patch || true
      elif [ $(echo "$LINUX_VERSION_CODE" | head -c2) -eq 61 ]; then
        patch -p1 < $KERNEL_PATCHES/susfs/fs_proc_base.c-fix-k6.1.patch || true
        
        log "Injecting SUSFS definitions for GKI 6.1..."
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
        fi
        rm -f "$NS_INJECT_FILE"
      fi

      # CRC Fix Logic
      if [ $(echo "$LINUX_VERSION_CODE" | head -c1) -eq 6 ]; then
        if [ "$KSU" == "yes" ]; then
          if [ "$KVER" == "6.1" ]; then
            log "Applying manual statfs CRC fix for KernelSU Next GKI 6.1..."
            sed -i '/#include <linux\/susfs_def.h>/i #ifndef __GENKSYMS__' fs/statfs.c
            sed -i '/#include <linux\/susfs_def.h>/a #endif' fs/statfs.c
          else
            log "Applying statfs CRC fix patch (KernelSU Next)..."
            patch -p1 < $KERNEL_PATCHES/susfs/fix-statfs-crc-mismatch-susfs.patch
          fi
        elif [ "$KSU" == "vortexsu" ] && [ "$KVER" == "6.1" ]; then
          log "Applying manual statfs CRC fix for VorteXSU GKI 6.1..."
          sed -i '/#include <linux\/susfs_def.h>/i #ifndef __GENKSYMS__' fs/statfs.c
          sed -i '/#include <linux\/susfs_def.h>/a #endif' fs/statfs.c
        fi
      fi

      # ZeroMount patches for 6.1 & 6.6
      # Untuk 6.1/6.6 kita bisa clone repo SB juga jika perlu, tapi di sini pakai curl saja karena filenya spesifik
      # Namun untuk konsistensi, kita clone SB repo sekali saja di awal untuk 5.10, di sini kita bisa pakai curl jika SB_DIR tidak ada
      if [ "$KVER" == "6.1" ]; then
        if [ "$KSU" == "yes" ]; then
          log "Applying ZeroMount patch for KernelSU-Next (GKI 6.1)..."
          curl -LSs "https://raw.githubusercontent.com/Kingfinik98/Super-Builders/main/android14-6.1/KernelSU-Next/patches/60_zeromount-android14-6.1.patch" | patch -p1 || log "ZeroMount patch skipped."
        elif [ "$KSU" == "vortexsu" ]; then
          log "Applying ZeroMount patch for VorteXSU (GKI 6.1)..."
          curl -LSs "https://raw.githubusercontent.com/Kingfinik98/Super-Builders/main/android14-6.1/ReSukiSU/patches/60_zeromount-android14-6.1.patch" | patch -p1 || log "ZeroMount patch skipped."
        fi
      elif [ "$KVER" == "6.6" ]; then
        if [ "$KSU" == "yes" ]; then
          log "Applying ZeroMount patch for KernelSU-Next (GKI 6.6)..."
          curl -LSs "https://raw.githubusercontent.com/Kingfinik98/Super-Builders/main/android15-6.6/KernelSU-Next/patches/60_zeromount-android15-6.6.patch" | patch -p1 || log "ZeroMount patch skipped."
        elif [ "$KSU" == "vortexsu" ]; then
          log "Applying ZeroMount patch for VorteXSU (GKI 6.6)..."
          curl -LSs "https://raw.githubusercontent.com/Kingfinik98/Super-Builders/main/android15-6.6/ReSukiSU/patches/60_zeromount-android15-6.6.patch" | patch -p1 || log "ZeroMount patch skipped."
        fi
      fi

      SUSFS_VERSION=$(grep -E '^#define SUSFS_VERSION' ./include/linux/susfs.h | cut -d' ' -f3 | sed 's/"//g')
      config --enable CONFIG_KSU_SUSFS
    fi
  else
    log "Skipping standard SUSFS patch (Handled by VorteXSU or logic elsewhere)."
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

# Declare needed variables
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

log "Applying KPM Patch..."
if [ "$KSU" == "vortexsu" ]; then
  cd $OUTDIR/arch/arm64/boot
  if [ -f Image ]; then
    echo "✅ Image found, applying KPM patch..."
    curl -LSs "https://github.com/Kingfinik98/SukiSU_patch/raw/refs/heads/main/kpm/patch_linux" -o patch
    chmod 777 patch
    ./patch
    if [ -f oImage ]; then
      mv -f oImage Image
      ls -lh Image
      log "✅ KPM Patch applied successfully."
    else
      log "Error: oImage not found!"
    fi
  else
    log "Warning: Image file not found. Skipping KPM patch."
  fi
else
  log "Skipping KPM patch (Not VorteXSU variant)."
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
    "s/kernel.string=.*/kernel.string=${KERNEL_NAME} ${RELEASE} ${LINUX_VERSION} ${VARIANT}/g" \
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
    echo "SUSFS_VERSION=$(curl -s https://gitlab.com/simonpunk/susfs4ksu/raw/gki-android15-6.6/kernel_patches/include/linux/susfs.h | grep -E '^#define SUSFS_VERSION' | cut -d' ' -f3 | sed 's/"//g')"
    echo "KERNEL_NAME=$KERNEL_NAME"
    echo "RELEASE_REPO=$(simplify_gh_url "$GKI_RELEASES_REPO")"
  ) >> $WORKDIR/artifacts/info.txt
fi

if [ $STATUS == "BETA" ]; then
  upload_file "$WORKDIR/$AK3_ZIP_NAME" "$text"
  upload_file "$WORKDIR/build.log"
else
  send_msg "✅ Build Succeeded for $VARIANT variant."
fi

exit 0
