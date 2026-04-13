#!/usr/bin/env bash
# VorteX Esport - Kernel Preferences (Boot Optimized)

# check binary
export PATH="/system/bin:/system/xbin:/sbin:$PATH"

# wait 5 seconds
until [ "$(getprop sys.boot_completed)" = "1" ]; do
    sleep 5
done

# give a 5 second pause during boot
sleep 5

# ========== DISABLE THERMAL UNIVERSAL ==========
for thermal in $(resetprop | awk -F '[][]' '/thermal/ {print $2}'); do
  if [[ "$(resetprop $thermal)" == "running" ]]; then
    stop ${thermal/init.svc.} 2>/dev/null
    sleep 2
    resetprop -n $thermal stopped 2>/dev/null
  fi
done
find /sys/devices/virtual/thermal -name temp -type f -exec chmod 000 {} + 2>/dev/null

# ========== ZRAM ==========
swapoff /dev/block/zram0 2>/dev/null
echo "1" > /sys/block/zram0/reset 2>/dev/null
echo "8096000000" > /sys/block/zram0/disksize 2>/dev/null
echo "zstd" > /sys/block/zram0/comp_algorithm 2>/dev/null
mkswap /dev/block/zram0 2>/dev/null
swapon /dev/block/zram0 2>/dev/null

# ========== PANIC & PRINTK ==========
echo "0" > /proc/sys/kernel/panic 2>/dev/null
echo "0" > /proc/sys/kernel/panic_on_oops 2>/dev/null
echo "0" > /proc/sys/kernel/panic_on_rcu_stall 2>/dev/null
echo "0" > /proc/sys/kernel/panic_on_warn 2>/dev/null
echo "0" > /proc/sys/vm/panic_on_oom 2>/dev/null
echo "0 0 0 0" > /proc/sys/kernel/printk 2>/dev/null
echo "off" > /proc/sys/kernel/printk_devkmsg 2>/dev/null
echo "0" > /sys/kernel/printk_mode/printk_mode 2>/dev/null
echo "0" > /proc/sys/kernel/nmi_watchdog 2>/dev/null
echo "0" > /proc/sys/kernel/compat-log 2>/dev/null

# ========== PRINTK & MODULE PARAMS ==========
echo "Y" > /sys/module/printk/parameters/console_suspend 2>/dev/null
echo "N" > /sys/module/printk/parameters/cpu 2>/dev/null
echo "Y" > /sys/module/printk/parameters/ignore_loglevel 2>/dev/null
echo "N" > /sys/module/printk/parameters/pid 2>/dev/null
echo "N" > /sys/module/printk/parameters/time 2>/dev/null
echo "Y" > /sys/module/bluetooth/parameters/disable_ertm 2>/dev/null
echo "Y" > /sys/module/bluetooth/parameters/disable_esco 2>/dev/null
echo "Y" > /sys/module/workqueue/parameters/power_efficient 2>/dev/null
echo "N" > /sys/module/sync/parameters/fsync_enabled 2>/dev/null
echo "1" > /sys/module/subsystem_restart/parameters/disable_restart_work 2>/dev/null

# ========== DISABLE DEBUGS ==========
find /sys/ -name debug_mask -exec sh -c 'echo "0" > "$1" 2>/dev/null' _ {} \;
find /sys/ -name debug_level -exec sh -c 'echo "0" > "$1" 2>/dev/null' _ {} \;
find /sys/ -name edac_mc_log_ce -exec sh -c 'echo "0" > "$1" 2>/dev/null' _ {} \;
find /sys/ -name edac_mc_log_ue -exec sh -c 'echo "0" > "$1" 2>/dev/null' _ {} \;
find /sys/ -name enable_event_log -exec sh -c 'echo "0" > "$1" 2>/dev/null' _ {} \;
find /sys/ -name log_ecn_error -exec sh -c 'echo "0" > "$1" 2>/dev/null' _ {} \;
find /sys/ -name snapshot_crashdumper -exec sh -c 'echo "0" > "$1" 2>/dev/null' _ {} \;
find /sys/kernel/debug/kgsl/kgsl-3d0/ -name '*log*' -exec sh -c 'echo "0" > "$1" 2>/dev/null' _ {} \;

# ========== SCHEDULER ==========
for sched in /sys/kernel/debug/sched_features/*; do
  echo "NO_GENTLE_FAIR_SLEEPERS" > "$sched" 2>/dev/null
  echo "NO_HRTICK" > "$sched" 2>/dev/null
  echo "NO_DOUBLE_TICK" > "$sched" 2>/dev/null
  echo "NO_RT_RUNTIME_SHARE" > "$sched" 2>/dev/null
  echo "NEXT_BUDDY" > "$sched" 2>/dev/null
  echo "NO_TTWU_QUEUE" > "$sched" 2>/dev/null
  echo "UTIL_EST" > "$sched" 2>/dev/null
  echo "ARCH_CAPACITY" > "$sched" 2>/dev/null
  echo "ARCH_POWER" > "$sched" 2>/dev/null
  echo "ENERGY_AWARE" > "$sched" 2>/dev/null
done
echo "0" > /proc/sys/kernel/sched_tunable_scaling 2>/dev/null
echo "westwood" > /proc/sys/net/ipv4/tcp_congestion_control 2>/dev/null

# ========== LMK & I/O ==========
echo "2560,5120,11520,25600,35840,38400" > /sys/module/lowmemorykiller/parameters/minfree 2>/dev/null
for queue in /sys/block/mmcblk*/queue; do
  echo "adios" > "$queue/scheduler" 2>/dev/null
  echo "0" > "$queue/add_random" 2>/dev/null
  echo "0" > "$queue/iostats" 2>/dev/null
  echo "128" > "$queue/read_ahead_kb" 2>/dev/null
  echo "64" > "$queue/nr_requests" 2>/dev/null
done

# ========== FSTRIM & CACHE ==========
fstrim -v /system /vendor /data /cache /metadata /odm /system_ext /product 2>/dev/null
echo "3" > /proc/sys/vm/drop_caches 2>/dev/null

# ========== SURFACEFLINGER & RENDERING ==========
setprop debug.sf.disable_backpressure 1 2>/dev/null
setprop debug.sf.latch_unsignaled 1 2>/dev/null
setprop debug.sf.enable_hwc_vds 1 2>/dev/null
setprop debug.sf.early_phase_offset_ns 500000 2>/dev/null
setprop debug.sf.early_app_phase_offset_ns 500000 2>/dev/null
setprop debug.sf.early_gl_phase_offset_ns 3000000 2>/dev/null
setprop debug.sf.early_gl_app_phase_offset_ns 15000000 2>/dev/null
setprop debug.sf.high_fps_early_phase_offset_ns 6100000 2>/dev/null
setprop debug.sf.high_fps_early_gl_phase_offset_ns 650000 2>/dev/null
setprop debug.sf.high_fps_late_app_phase_offset_ns 100000 2>/dev/null
setprop debug.sf.phase_offset_threshold_for_next_vsync_ns 6100000 2>/dev/null

# ========== CPU & GPU GOVERNOR ==========
for governor in /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; do
  echo "performance" > "$governor" 2>/dev/null
done
echo "performance" > /sys/class/kgsl/kgsl-3d0/devfreq/governor 2>/dev/null
echo "UnityMain, libunity.so" > /proc/sys/kernel/sched_lib_name 2>/dev/null
echo "255" > /proc/sys/kernel/sched_lib_mask_force 2>/dev/null

# ========== RESETPROP ==========
resetprop -n persist.sys.dalvik.hyperthreading true 2>/dev/null
resetprop -n persist.sys.dalvik.multithread true 2>/dev/null
resetprop -n debug.sf.showupdates 0 2>/dev/null
resetprop -n debug.sf.showcpu 0 2>/dev/null
resetprop -n debug.sf.showbackground 0 2>/dev/null
resetprop -n debug.sf.showfps 0 2>/dev/null
resetprop -n debug.sf.hw 1 2>/dev/null
resetprop -n ro.hwui.texture_cache_size 72 2>/dev/null
resetprop -n ro.hwui.layer_cache_size 48 2>/dev/null
resetprop -n ro.hwui.r_buffer_cache_size 8 2>/dev/null
resetprop -n ro.hwui.path_cache_size 32 2>/dev/null
resetprop -n ro.hwui.gradient_cache_size 1 2>/dev/null
resetprop -n ro.hwui.drop_shadow_cache_size 6 2>/dev/null
resetprop -n ro.hwui.texture_cache_flushrate 0.4 2>/dev/null
resetprop -n ro.hwui.text_small_cache_width 1024 2>/dev/null
resetprop -n ro.hwui.text_small_cache_height 1024 2>/dev/null
resetprop -n ro.hwui.text_large_cache_width 2048 2>/dev/null
resetprop -n ro.hwui.text_large_cache_height 2048 2>/dev/null
resetprop -n ro.iorapd.enable false 2>/dev/null
resetprop -n iorapd.perfetto.enable false 2>/dev/null
resetprop -n iorapd.readahead.enable false 2>/dev/null
resetprop -n persist.device_config.runtime_native_boot.iorap_readahead_enable false 2>/dev/null

cmd package bg-dexopt-job 2>/dev/null
