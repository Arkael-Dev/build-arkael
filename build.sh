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
# ----------------------------------------------------

# --- PATCH inject.sh ---
log "Applying inject.sh patch..."
wget -qO Inject_300hz.sh https://raw.githubusercontent.com/Kingfinik98/build-vortex/refs/heads/6.x/inject_ksu/Inject_300hz.sh
bash Inject_300hz.sh
rm Inject_300hz.sh
#--------------------------------------

# --- PATCH WIFI SM8650 (GKI 6.1 ONLY) ---
if [ "$KVER" == "6.1" ]; then
  log "Applying WiFi SM8650 patch..."
  curl -LSs https://github.com/OnePlus-12-Development/android_kernel_qcom_sm8650/commit/3e0cb08.patch | patch -p1 --forward || log "WiFi SM8650 patch skipped or already applied."
fi
# ----------------------------------------

# --- ADD KSU INJECT SCRIPT ---
log "Injecting custom KSU & SuSFS configs from GitHub..."
export KSU
export KSU_SUSFS
wget -qO inject.sh https://raw.githubusercontent.com/Kingfinik98/build-vortex/refs/heads/6.x/inject_ksu/gki_defconfig.sh
bash inject.sh
rm inject.sh
# --------------------------------------
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

  # --- INSTALL KERNELSU-NEXT METODE SUPER-BUILDERS ---
  # Khusus untuk 5.10, kita ikuti struktur file di kernel/ agar Patch 70_ berhasil
  if [ "$KVER" == "5.10" ]; then
    log "Installing KernelSU-Next (Super-Builders Method)..."
    
    # 1. Ambil Pin Commit
    KSU_PIN=$(curl -s "https://raw.githubusercontent.com/Kingfinik98/Super-Builders/refs/heads/main/android12-5.10/kernelsu-next-pin.txt")
    log "Pinning KernelSU-Next to commit: $KSU_PIN"

    # 2. Clone KernelSU-Next
    git clone --depth=1 https://github.com/pershoot/KernelSU-Next KernelSU-Next
    cd KernelSU-Next
    git fetch --depth=1 origin $KSU_PIN
    git checkout $KSU_PIN
    cd $OLDPWD

    # 3. Copy Sources ke kernel/ (Agar Patch 70_ menemukan file nya)
    # Struktur: KernelSU-Next/kernel/ksu.c -> kernel/ksu.c
    log "Copying KernelSU sources to kernel/ directory..."
    cp -r KernelSU-Next/kernel/* kernel/
    
    # 4. Integrasikan Kbuild & Kconfig
    # Tambahkan obj-y ksu.o di kernel/Kbuild (atau Makefile lama)
    # KernelSU-Next/kernel/Kbuild berisi obj-y += ksu.o dll.
    # Kita append isiannya ke kernel/Kbuild kernel source.
    cat KernelSU-Next/kernel/Kbuild >> kernel/Kbuild
    
    # Integrasikan Kconfig
    # Sed kernel/Kconfig untuk menambahkan source "kernel/Kconfig"
    # Biasanya ada di akhir file.
    echo 'source "kernel/Kconfig"' >> kernel/Kconfig
    # Copy Kconfig khusus KSU ke kernel/
    cp KernelSU-Next/kernel/Kconfig kernel/Kconfig.ksu
    
    # 5. Cleanup
    rm -rf KernelSU-Next
    log "KernelSU-Next installed in kernel/ directory."
  
  else
    # Logika lama untuk 6.1/6.6 (menggunakan drivers/kernelsu)
    install_ksu 'pershoot/KernelSU-Next' 'dev-susfs'
    config --enable CONFIG_KSU
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
    
    log "Applying ZeroMount patch for VorteXSU (GKI 5.10)..."
    curl -LSs "https://raw.githubusercontent.com/Kingfinik98/Super-Builders/refs/heads/main/android12-5.10/ReSukiSU/patches/60_zeromount-android12-5.10.patch" | patch -p1 || log "ZeroMount patch skipped or already applied."
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
      log "Applying Super-Builders patches sequence (50 -> 51 -> 70 -> 60 -> Fix) for GKI 5.10..."
      
      SB_BASE="https://raw.githubusercontent.com/Kingfinik98/Super-Builders/refs/heads/main/android12-5.10/KernelSU-Next/patches"
      SB_HELPER="https://raw.githubusercontent.com/Kingfinik98/Super-Builders/refs/heads/main/android12-5.10/build-helpers"
      
      # Opsi Patch: -p1 -F3 --no-backup-if-mismatch
      PATCH_OPTS="-p1 -F3 --no-backup-if-mismatch"

      # 1. SUSFS Upstream (50_)
      log "Applying 50_add_susfs_in_gki..."
      curl -LSs "$SB_BASE/50_add_susfs_in_gki-android12-5.10.patch" | patch $PATCH_OPTS || log "Patch 50 skipped."

      # 2. Enhanced SUSFS (51_)
      log "Applying 51_enhanced_susfs..."
      curl -LSs "$SB_BASE/51_enhanced_susfs-android12-5.10.patch" | patch $PATCH_OPTS || log "Patch 51 skipped."

      # 3. KSU Safety (70_)
      # SEKARANG FILE kernel/ksu.c SUDAH ADA, JADI PATCH INI AKAN BERHASIL
      if [ "$KSU" == "yes" ]; then
        log "Applying 70_ksu_safety-kernelsu-next..."
        curl -LSs "$SB_BASE/70_ksu_safety-kernelsu-next-5.10.patch" | patch $PATCH_OPTS || log "Patch 70 skipped."
      fi

      # 4. ZeroMount (60_)
      log "Applying 60_zeromount..."
      curl -LSs "$SB_BASE/60_zeromount-android12-5.10.patch" | patch $PATCH_OPTS || log "Patch 60 skipped."

      # 5. Fix Compat Script
      log "Running fix-susfs-compat.sh..."
      curl -LSs "$SB_HELPER/fix-susfs-compat.sh" -o fix_susfs_compat.sh
      if [ -f fix_susfs_compat.sh ]; then
        chmod +x fix_susfs_compat.sh
        SUBLEVEL=$(echo $LINUX_VERSION | cut -d'.' -f3)
        bash fix_susfs_compat.sh . "$SUBLEVEL" "android12" "5.10" "$KERNEL_PATCHES" || log "Compat fix script finished with warnings."
        rm fix_susfs_compat.sh
      fi

      SUSFS_VERSION=$(grep -E '^#define SUSFS_VERSION' ./include/linux/susfs.h | cut -d' ' -f3 | sed 's/"//g')
      config --enable CONFIG_KSU_SUSFS

    else
      # --- LOGIKA LAMA UNTUK 6.1 & 6.6 ---
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
      
      # Patch Fixes 6.x
      if [ $(echo "$LINUX_VERSION_CODE" | head -c4) -eq 6630 ]; then
        patch -p1 < $KERNEL_PATCHES/susfs/namespace.c_fix.patch || true
        patch -p1 < $KERNEL_PATCHES/susfs/task_mmu.c_fix.patch || true
      elif [ $(echo "$LINUX_VERSION_CODE" | head -c4) -eq 6658 ]; then
        patch -p1 < $KERNEL_PATCHES/susfs/task_mmu.c_fix-k6.6.58.patch || true
      elif [ $(echo "$LINUX_VERSION_CODE" | head -c2) -eq 61 ]; then
        patch -p1 < $KERNEL_PATCHES/susfs/fs_proc_base.c-fix-k6.1.patch || true
        # Injection logic...
      fi

      # CRC Fix Logic
      if [ $(echo "$LINUX_VERSION_CODE" | head -c1) -eq 6 ]; then
        if [ "$KSU" == "yes" ]; then
           if [ "$KVER" == "6.1" ]; then
             sed -i '/#include <linux\/susfs_def.h>/i #ifndef __GENKSYMS__' fs/statfs.c
             sed -i '/#include <linux\/susfs_def.h>/a #endif' fs/statfs.c
           else
             patch -p1 < $KERNEL_PATCHES/susfs/fix-statfs-crc-mismatch-susfs.patch
           fi
        fi
      fi
      
      # ZeroMount for 6.1/6.6
      if [ "$KVER" == "6.1" ]; then
         if [ "$KSU" == "yes" ]; then
           curl -LSs "https://raw.githubusercontent.com/Kingfinik98/Super-Builders/refs/heads/main/android14-6.1/KernelSU-Next/patches/60_zeromount-android14-6.1.patch" | patch -p1 || log "ZM skipped."
         fi
      elif [ "$KVER" == "6.6" ]; then
         if [ "$KSU" == "yes" ]; then
           curl -LSs "https://raw.githubusercontent.com/Kingfinik98/Super-Builders/refs/heads/main/android15-6.6/KernelSU-Next/patches/60_zeromount-android15-6.6.patch" | patch -p1 || log "ZM skipped."
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

# Upload defconfig if we are doing defconfig
if [ $TODO == "defconfig" ]; then
  log "Uploading defconfig..."
  upload_file $OUTDIR/.config
  exit 0
fi

# Build the actual kernel
log "Building kernel..."
make ${MAKE_ARGS[@]}

# Check KMI Function symbol
if [ $(echo "$LINUX_VERSION_CODE" | head -c1) -eq 6 ]; then
  $KMI_CHECK "$KSRC/android/abi_gki_aarch64.stg" "$MODULE_SYMVERS" || true
else
  $KMI_CHECK "$KSRC/android/abi_gki_aarch64.xml" "$MODULE_SYMVERS" || true
fi

# --- PATCH KPM SECTION ---
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
    log "Warning: Image file not found in $PWD. Skipping KPM patch."
  fi
else
  log "Skipping KPM patch (Not VorteXSU variant)."
fi
cd $WORKDIR
# ----------------------------------------------------

## Post-compiling stuff
cd $WORKDIR

# Clone AnyKernel
log "Cloning anykernel from $(simplify_gh_url "$ANYKERNEL_REPO")"
git clone -q --depth=1 $ANYKERNEL_REPO -b $ANYKERNEL_BRANCH anykernel

# Set kernel string in anykernel
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

# Zip the anykernel
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
