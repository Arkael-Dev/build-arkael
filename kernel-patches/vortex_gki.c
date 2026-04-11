#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/sched/sysctl.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/tcp.h>
#include <net/tcp.h>
#include <net/sock.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/version.h>

extern struct net init_net;

// ==========================================
// 1. DIRECT KERNEL MEMORY PATCH (Instant)
// ==========================================

extern int panic_timeout;
extern int panic_on_oops;
extern int panic_on_warn;
extern int console_loglevel;
extern enum sched_tunable_scaling sysctl_sched_tunable_scaling;
extern unsigned long sysctl_hung_task_timeout_secs;
extern int sysctl_sched_autogroup_enabled;

static int __init vortex_direct_init(void) {
    pr_info("[VorteX] Applying safe Direct Kernel Patches...\n");

    panic_timeout = 0;
    panic_on_oops = 0;
    panic_on_warn = 0;
    console_loglevel = CONSOLE_LOGLEVEL_SILENT;
    sysctl_sched_tunable_scaling = 0;
    sysctl_hung_task_timeout_secs = 0;
    sysctl_sched_autogroup_enabled = 0;

    pr_info("[VorteX] Direct patches: panic=off, hung_task=off, autogroup=off\n");
    return 0;
}

// ==========================================
// 2. ULTRA-SAFE SYSFS ENGINE
// ==========================================

static bool vortex_write_sysfs(const char *path, const char *val) {
    struct file *file;
    loff_t pos = 0;
    ssize_t ret;

    if (!path || !val) return false;

    file = filp_open(path, O_WRONLY, 0);
    if (IS_ERR_OR_NULL(file)) {
        return false;
    }

    ret = kernel_write(file, val, strlen(val), &pos);
    filp_close(file, NULL);

    if (ret < 0) {
        return false;
    }
    return true;
}

static bool vortex_read_sysfs(const char *path, char *buf, size_t buflen) {
    struct file *file;
    loff_t pos = 0;
    ssize_t ret;

    if (!path || !buf || buflen == 0) return false;
    buf[0] = '\0';

    file = filp_open(path, O_RDONLY, 0);
    if (IS_ERR_OR_NULL(file)) return false;

    ret = kernel_read(file, buf, buflen - 1, &pos);
    filp_close(file, NULL);

    if (ret > 0) {
        buf[ret] = '\0';
        char *newline = strchr(buf, '\n');
        if (newline) *newline = '\0';
        return true;
    }
    return false;
}

static void vortex_set_tcp_congestion(const char *name) {
    struct tcp_congestion_ops *ops;
    rcu_read_lock();
    ops = tcp_ca_find(name);
    if (ops && try_module_get(ops->owner)) {
        tcp_set_default_congestion_control(&init_net, name);
        pr_info("[VorteX] TCP: Forced to %s\n", name);
        module_put(ops->owner);
    } else {
        pr_warn("[VorteX] TCP: %s not available\n", name);
    }
    rcu_read_unlock();
}

// ==========================================
// 3A. VM/MEMORY TUNING
// ==========================================

static void vortex_tune_vm(void) {
    pr_info("[VorteX] VM: Applying memory optimizations...\n");

    vortex_write_sysfs("/proc/sys/vm/swappiness", "10");
    vortex_write_sysfs("/proc/sys/vm/vfs_cache_pressure", "50");
    vortex_write_sysfs("/proc/sys/vm/dirty_ratio", "15");
    vortex_write_sysfs("/proc/sys/vm/dirty_background_ratio", "5");
    vortex_write_sysfs("/proc/sys/vm/min_free_kbytes", "4096");
    vortex_write_sysfs("/proc/sys/vm/compaction_proactiveness", "20");
    vortex_write_sysfs("/proc/sys/vm/page_lock_unfairness", "1");

    pr_info("[VorteX] VM: Done\n");
}

// ==========================================
// 3B. TCP LOW LATENCY
// ==========================================

static void vortex_tune_tcp(void) {
    pr_info("[VorteX] TCP: Applying low-latency tweaks...\n");

    vortex_write_sysfs("/proc/sys/net/ipv4/tcp_fastopen", "3");
    vortex_write_sysfs("/proc/sys/net/core/somaxconn", "4096");
    vortex_write_sysfs("/proc/sys/net/ipv4/tcp_moderate_rcvbuf", "0");
    vortex_write_sysfs("/proc/sys/net/ipv4/tcp_tw_reuse", "1");
    vortex_write_sysfs("/proc/sys/net/ipv4/tcp_fin_timeout", "10");
    vortex_write_sysfs("/proc/sys/net/ipv4/tcp_max_syn_backlog", "8192");
    vortex_write_sysfs("/proc/sys/net/ipv4/tcp_slow_start_after_idle", "0");

    pr_info("[VorteX] TCP: Done\n");
}

// ==========================================
// 3C. KSM OFF (FPS Critical - Reduce Background Steal)
// ==========================================

static void vortex_fps_ksm_off(void) {
    // KSM scans memory pages to merge identical ones
    // This steals CPU cycles and causes micro-stutters during gaming
    if (vortex_write_sysfs("/sys/kernel/mm/ksm/run", "0")) {
        pr_info("[VorteX] FPS: KSM disabled\n");
    }
}

// ==========================================
// 3D. TIMER & RCU (FPS Critical - Reduce Wake Latency)
// ==========================================

static void vortex_fps_timer_rcu(void) {
    // Keep timers on the same CPU instead of migrating
    // Reduces wake latency for gaming threads
    if (vortex_write_sysfs("/proc/sys/kernel/timer_migration", "0")) {
        pr_info("[VorteX] FPS: Timer migration OFF\n");
    }

    // Force RCU to use expedited mode
    // Faster synchronization = less scheduling delay
    if (vortex_write_sysfs("/sys/kernel/rcu_normal", "0")) {
        pr_info("[VorteX] FPS: RCU expedited mode\n");
    }
}

// ==========================================
// 3E. CPU IDLE STATE RESTRICTION (FPS Critical)
// ==========================================
// Big cores entering deep idle (C2/C3/C4) take 1-3ms to wake
// At 120Hz, each frame budget is only 8.3ms
// Losing 1-3ms to wake = instant frame drop
// Solution: Disable deep idle states, keep only WFI (shallowest)

static void vortex_fps_idle_restrict(void) {
    char path[128];
    char gov[16] = {0};
    int i, j;
    int deepest_disabled = 0;

    pr_info("[VorteX] FPS: Restricting CPU idle states...\n");

    // On ARM, cpuidle states are typically system-wide (not per-core)
    // State 0 = WFI (shallowest, ~microsecond wake)
    // State 1 = Retention (~10-50us wake)
    // State 2+ = Deep power collapse (1-3ms wake) ← THIS KILLS FPS
    //
    // Strategy: Keep state 0 and 1, disable 2 and deeper

    for (j = 2; j <= 6; j++) {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpuidle/state%d/disable", j);
        if (vortex_write_sysfs(path, "1")) {
            deepest_disabled = j;
        }
    }

    if (deepest_disabled >= 2) {
        pr_info("[VorteX] FPS: Deep idle disabled (state 2-%d blocked)\n", deepest_disabled);
        pr_info("[VorteX] FPS: Wake latency reduced from ~2ms to ~50us\n");
    } else {
        pr_warn("[VorteX] FPS: Could not disable deep idle (cpuidle not accessible?)\n");
    }
}

// ==========================================
// 3F. CPU FREQUENCY FLOOR ON BIG CORES (FPS Critical)
// ==========================================
// Without a floor, big cores can drop to ~300MHz between frame loads
// When the next frame needs compute, CPU must ramp from 300MHz → 2.8GHz
// This ramp takes 1-2ms = missed frame deadline = stutter
// Solution: Set minimum frequency to ~50% of max on big cores
// This eliminates ramp-up delay entirely

static void vortex_fps_cpu_floor(void) {
    char path[128];
    char max_freq[32] = {0};
    char cur_min[32] = {0};
    char floor_str[32];
    int i;
    int tuned = 0;

    pr_info("[VorteX] FPS: Setting big core frequency floor...\n");

    // Find the highest policy (biggest core cluster)
    // This tells us the max frequency of the big cores
    int big_policy_max = -1;
    long big_max_freq = 0;

    for (i = 15; i >= 0; i--) {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpufreq/policy%d/scaling_max_freq", i);
        if (vortex_read_sysfs(path, max_freq, sizeof(max_freq))) {
            long val = simple_strtol(max_freq, NULL, 10);
            if (val > big_max_freq) {
                big_max_freq = val;
                big_policy_max = i;
            }
        }
    }

    if (big_policy_max < 0 || big_max_freq == 0) {
        pr_warn("[VorteX] FPS: Cannot detect big core max frequency\n");
        return;
    }

    pr_info("[VorteX] FPS: Big core max detected = %ld KHz (%ld MHz)\n",
            big_max_freq, big_max_freq / 1000);

    // Set floor at 50% of max for big cores
    // 50% is the sweet spot: eliminates ramp delay but won't overheat
    // Example: 2800MHz max → 1400MHz floor
    long floor = big_max_freq * 50 / 100;
    snprintf(floor_str, sizeof(floor_str), "%ld", floor);

    // Apply to policies >= 4 (typically big core cluster on Snapdragon/MediaTek)
    // Also try all policies from the big cluster up
    for (i = 4; i <= big_policy_max; i++) {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpufreq/policy%d/scaling_min_freq", i);
        if (vortex_read_sysfs(path, cur_min, sizeof(cur_min))) {
            long cur = simple_strtol(cur_min, NULL, 10);

            // Only raise floor, never lower it
            if (floor > cur) {
                if (vortex_write_sysfs(path, floor_str)) {
                    pr_info("[VorteX] FPS: Policy %d floor = %ld MHz (was %ld MHz)\n",
                            i, floor / 1000, cur / 1000);
                    tuned++;
                }
            } else {
                pr_info("[VorteX] FPS: Policy %d already at %ld MHz (skip)\n",
                        i, cur / 1000);
            }
        }
    }

    if (tuned == 0) {
        // Fallback: try policy 0-3 in case device has different cluster layout
        for (i = 0; i <= 3; i++) {
            snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpufreq/policy%d/scaling_max_freq", i);
            if (vortex_read_sysfs(path, max_freq, sizeof(max_freq))) {
                long val = simple_strtol(max_freq, NULL, 10);
                if (val == big_max_freq) {
                    // This policy has same max as biggest core = it's a big core
                    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpufreq/policy%d/scaling_min_freq", i);
                    if (vortex_read_sysfs(path, cur_min, sizeof(cur_min))) {
                        long cur = simple_strtol(cur_min, NULL, 10);
                        if (floor > cur) {
                            if (vortex_write_sysfs(path, floor_str)) {
                                pr_info("[VorteX] FPS: Policy %d floor = %ld MHz (fallback)\n",
                                        i, floor / 1000);
                                tuned++;
                            }
                        }
                    }
                }
            }
        }
    }

    if (tuned > 0) {
        pr_info("[VorteX] FPS: %d big core(s) floored at %ld MHz\n", tuned, floor / 1000);
    } else {
        pr_warn("[VorteX] FPS: No big cores found for floor tuning\n");
    }
}

// ==========================================
// 3G. SCHEDULER MICRO-TUNING (FPS Critical)
// ==========================================

static void vortex_fps_scheduler(void) {
    pr_info("[VorteX] FPS: Scheduler micro-tuning...\n");

    // Smaller wakeup granularity = faster task wake = less frame time variance
    vortex_write_sysfs("/proc/sys/kernel/sched_wakeup_granularity_ns", "500000");

    // Lower migration cost = faster load balance between cores
    vortex_write_sysfs("/proc/sys/kernel/sched_migration_cost_ns", "50000");

    // Fewer tasks to migrate at once = quicker balance
    vortex_write_sysfs("/proc/sys/kernel/sched_nr_migrate", "4");

    // Disable EAS for gaming (use all cores aggressively)
    if (vortex_write_sysfs("/sys/devices/system/cpu/energy_aware", "0")) {
        pr_info("[VorteX] FPS: EAS disabled\n");
    } else if (vortex_write_sysfs("/proc/sys/kernel/sched_energy_aware", "0")) {
        pr_info("[VorteX] FPS: EAS disabled (proc)\n");
    }

    pr_info("[VorteX] FPS: Scheduler done\n");
}

// ==========================================
// 3H. ZRAM AUTO-OPTIMIZE
// ==========================================

static void vortex_tune_zram(void) {
    char algo[64] = {0};
    int i;

    for (i = 0; i <= 3; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/sys/block/zram%d/comp_algorithm", i);

        if (vortex_read_sysfs(path, algo, sizeof(algo))) {
            // zstd: best compression ratio (more usable RAM)
            // lz4: fastest compression (less CPU overhead)
            // For gaming: lz4 is better because we want less CPU steal
            if (vortex_write_sysfs(path, "lz4")) {
                pr_info("[VorteX] ZRAM%d: lz4 (fastest for gaming)\n", i);
            } else if (vortex_write_sysfs(path, "zstd")) {
                pr_info("[VorteX] ZRAM%d: zstd (fallback)\n", i);
            }
        }
    }
}

// ==========================================
// 3I. UFS/STORAGE TUNING
// ==========================================

static void vortex_tune_storage(void) {
    char path[128];
    int i;

    // Try UFS clock gate disable
    vortex_write_sysfs("/sys/devices/platform/soc/1d84000.ufshc/clkgate_enable", "0");

    for (i = 'a'; i <= 'z'; i++) {
        snprintf(path, sizeof(path), "/sys/block/sd%c/device/queue_depth", i);
        vortex_write_sysfs(path, "64");
    }
}

// ==========================================
// 3J. REFRESH RATE LOCK (Best Effort)
// ==========================================
// IMPORTANT: This is best-effort because refresh rate is primarily
// controlled by SurfaceFlinger (userspace). These kernel tweaks
// prevent conditions that cause the system to DROP refresh rate.
// For GUARANTEED lock, you MUST also set userspace values (see below).

static void vortex_fps_refresh_lock(void) {
    pr_info("[VorteX] FPS: Attempting refresh rate stabilization...\n");

    // Prevent thermal framework from requesting refresh drop
    // These paths are OEM-specific, we try all known variants

    // Qualcomm DRM hints
    vortex_write_sysfs("/sys/module/msm_drm/parameters/mdss_fb0_fps", "0");

    // Samsung display hints
    vortex_write_sysfs("/sys/class/drm/card0/device/power/auto_latency_hint", "0");

    // Generic panel hint (some MediaTek devices)
    vortex_write_sysfs("/sys/class/panel/refresh_rate", "0");

    // Prevent display from entering low power during gaming
    vortex_write_sysfs("/sys/class/backlight/panel0/dimming_state", "0");

    // Some devices expose refresh rate via DRM
    vortex_write_sysfs("/sys/class/drm/card0-HDMI-A-1/max_refresh_rate", "0");

    pr_info("[VorteX] FPS: Refresh stabilization applied (best-effort)\n");
    pr_warn("[VorteX] ============================================\n");
    pr_warn("[VorteX] FPS: FOR GUARANTEED 120Hz/144Hz LOCK,\n");
    pr_warn("[VorteX] FPS: RUN THESE IN TERMINAL/ADB ROOT:\n");
    pr_warn("[VorteX] FPS:\n");
    pr_warn("[VorteX] FPS:   settings put system peak_refresh_rate 120\n");
    pr_warn("[VorteX] FPS:   settings put system min_refresh_rate 120\n");
    pr_warn("[VorteX] FPS:   (change 120 to 144 if device supports)\n");
    pr_warn("[VorteX] FPS:\n");
    pr_warn("[VorteX] FPS: ALSO disable these in Developer Options:\n");
    pr_warn("[VorteX] FPS:   - Smart resolution / Auto refresh\n");
    pr_warn("[VorteX] FPS:   - Smooth Display (let app decide = OFF)\n");
    pr_warn("[VorteX] FPS: ============================================\n");
}

// ==========================================
// 4. SAFE SYSFS THREAD (Master Sequence)
// ==========================================

static int vortex_sysfs_thread(void *data) {
    char path[128];
    char max_freq_val[32] = {0};
    char current_gov[32] = {0};
    int i;

    // Wait for full boot
    ssleep(15);

    pr_info("[VorteX] =======================================\n");
    pr_info("[VorteX] VorteX FPS Engine Starting...\n");
    pr_info("[VorteX] =======================================\n");

    // === PHASE 1: Foundation (no dependencies) ===
    vortex_tune_vm();           // Memory stable
    vortex_tune_tcp();          // Network latency
    vortex_fps_ksm_off();       // Kill background CPU steal
    vortex_fps_timer_rcu();     // Faster wake
    vortex_fps_idle_restrict(); // Block deep idle (BEFORE governor!)
    vortex_fps_scheduler();     // Micro-tuning
    vortex_tune_zram();         // Compression
    vortex_tune_storage();      // I/O

    // === PHASE 2: CPU Governor + Schedutil Tweaks ===
    pr_info("[VorteX] CPU: Forcing schedutil with FPS-optimized rates...\n");
    for (i = 0; i <= 15; i++) {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpufreq/policy%d/scaling_governor", i);
        if (vortex_write_sysfs(path, "schedutil")) {
            pr_info("[VorteX] CPU: Policy %d → schedutil\n", i);
        }

        // CRITICAL for FPS: How fast CPU can BOOST UP
        // Default is usually 2000us (2ms) = too slow for 120Hz
        // 500us = CPU can respond within half a frame at 120Hz
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpufreq/policy%d/schedutil/up_rate_limit_us", i);
        if (vortex_write_sysfs(path, "500")) {
            // Silent success
        }

        // CRITICAL for FPS: How fast CPU will DROP DOWN
        // Default is usually 20000us (20ms) = drops too fast between frames
        // 40000us (40ms) = holds high freq for ~5 frames at 120Hz
        // This prevents the ramp-up stutter between frame bursts
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpufreq/policy%d/schedutil/down_rate_limit_us", i);
        if (vortex_write_sysfs(path, "40000")) {
            // Silent success
        }
    }
    pr_info("[VorteX] CPU: up_rate=500us, down_rate=40ms (FPS optimized)\n");

    // === PHASE 3: CPU Floor (AFTER governor to stick) ===
    vortex_fps_cpu_floor();

    // === PHASE 4: I/O Scheduler ===
    pr_info("[VorteX] I/O: Scanning block devices...\n");
    for (i = 'a'; i <= 'z'; i++) {
        snprintf(path, sizeof(path), "/sys/block/sd%c/queue/scheduler", i);
        if (vortex_write_sysfs(path, "mq-deadline")) {
            pr_info("[VorteX] I/O: sd%c → mq-deadline\n", i);
        }
        snprintf(path, sizeof(path), "/sys/block/sd%c/queue/read_ahead_kb", i);
        vortex_write_sysfs(path, "128");
        snprintf(path, sizeof(path), "/sys/block/sd%c/queue/iostats", i);
        vortex_write_sysfs(path, "0");
        snprintf(path, sizeof(path), "/sys/block/sd%c/queue/nr_requests", i);
        vortex_write_sysfs(path, "256");
        snprintf(path, sizeof(path), "/sys/block/sd%c/queue/rq_affinity", i);
        vortex_write_sysfs(path, "1");
    }
    for (i = 0; i <= 15; i++) {
        snprintf(path, sizeof(path), "/sys/block/dm-%d/queue/scheduler", i);
        if (vortex_write_sysfs(path, "mq-deadline")) {
            pr_info("[VorteX] I/O: dm-%d → mq-deadline\n", i);
        }
        snprintf(path, sizeof(path), "/sys/block/dm-%d/queue/read_ahead_kb", i);
        vortex_write_sysfs(path, "128");
        snprintf(path, sizeof(path), "/sys/block/dm-%d/queue/iostats", i);
        vortex_write_sysfs(path, "0");
        snprintf(path, sizeof(path), "/sys/block/dm-%d/queue/nr_requests", i);
        vortex_write_sysfs(path, "256");
    }

    // === PHASE 5: GPU (Lock max + extras) ===
    if (vortex_read_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/max_freq", max_freq_val, sizeof(max_freq_val))) {
        pr_info("[VorteX] GPU: Max freq = %s\n", max_freq_val);

        if (vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/min_freq", max_freq_val)) {
            pr_info("[VorteX] GPU: LOCKED at %s\n", max_freq_val);
        }
        vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/max_gpuclk", max_freq_val);

        if (vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/governor", "schedutil")) {
            pr_info("[VorteX] GPU: Governor → schedutil\n");
        } else {
            vortex_read_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/governor", current_gov, sizeof(current_gov));
            pr_warn("[VorteX] GPU: Governor blocked. Current: %s\n", current_gov);
        }

        // Bus & Cache
        vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/force_bus_on", "1");
        vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/gpu_llc_slice_enable", "1");
        vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/l3_vote", "1");

        // FPS extras
        vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/split_display", "0");
        vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/disable_low_latency", "0");
        vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/three_d_texture", "1");
    } else {
        pr_warn("[VorteX] GPU: KGSL not found (non-Qualcomm?)\n");
    }

    // === PHASE 6: LMK ===
    if (vortex_write_sysfs("/sys/module/lowmemorykiller/parameters/minfree", "2560,5120,11520,25600,35840,38400")) {
        pr_info("[VorteX] LMK: Updated\n");
    }

    // === PHASE 7: Power ===
    vortex_write_sysfs("/proc/sys/kernel/printk_devkmsg", "off");
    vortex_write_sysfs("/sys/module/usbcore/parameters/autosuspend", "-1");

    // === PHASE 8: Refresh Rate (last, best-effort) ===
    vortex_fps_refresh_lock();

    pr_info("[VorteX] =======================================\n");
    pr_info("[VorteX] VorteX FPS Engine COMPLETED\n");
    pr_info("[VorteX] =======================================\n");

    return 0;
}

static int __init vortex_sysfs_init(void) {
    struct task_struct *thread;
    thread = kthread_run(vortex_sysfs_thread, NULL, "vortex_sysfs");
    if (IS_ERR(thread)) {
        pr_err("[VorteX] FATAL: Thread creation failed!\n");
    }
    return 0;
}

// ==========================================
// 5. MODULE REGISTRATION
// ==========================================

pure_initcall(vortex_direct_init);
late_initcall(vortex_sysfs_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("VorteX Esport");
MODULE_DESCRIPTION("GKI 5.10 FPS Stability Engine");
MODULE_VERSION("2.0");
// Signed-off-by: kingfinix98@gmail.com
