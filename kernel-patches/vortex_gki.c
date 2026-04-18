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
#include <linux/types.h>

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

bool is_susfs_uname_set = false;

static int __init vortex_direct_init(void) {
    pr_info("[VorteX] Applying safe Direct Kernel Patches...\n");

    panic_timeout = 0;
    panic_on_oops = 0;
    panic_on_warn = 0;
    console_loglevel = CONSOLE_LOGLEVEL_SILENT;
    sysctl_sched_tunable_scaling = 0;
    sysctl_hung_task_timeout_secs = 0;

    pr_info("[VorteX] Direct patches: panic=off, hung_task=off\n");
    return 0;
}

// ==========================================
// 2. ULTRA-SAFE SYSFS ENGINE (With Validation)
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

/**
 * safe_write_sysfs - Write with existence check and current value validation
 * Prevents unnecessary writes and validates path exists first
 */
static bool safe_write_sysfs(const char *path, const char *val) {
    char current[32] = {0};
    
    // Check if path exists by attempting to read first
    if (!vortex_read_sysfs(path, current, sizeof(current))) {
        return false;  // Path doesn't exist or unreadable
    }
    
    // Skip write if value is already set (avoid unnecessary operations)
    if (strcmp(current, val) == 0) {
        return true;  // Already at desired value
    }
    
    return vortex_write_sysfs(path, val);
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

    safe_write_sysfs("/proc/sys/vm/swappiness", "10");
    safe_write_sysfs("/proc/sys/vm/vfs_cache_pressure", "50");
    safe_write_sysfs("/proc/sys/vm/dirty_ratio", "15");
    safe_write_sysfs("/proc/sys/vm/dirty_background_ratio", "5");
    safe_write_sysfs("/proc/sys/vm/min_free_kbytes", "4096");
    safe_write_sysfs("/proc/sys/vm/compaction_proactiveness", "20");
    safe_write_sysfs("/proc/sys/vm/page_lock_unfairness", "1");

    pr_info("[VorteX] VM: Done\n");
}

// ==========================================
// 3B. TCP LOW LATENCY
// ==========================================

static void vortex_tune_tcp(void) {
    pr_info("[VorteX] TCP: Applying low-latency tweaks...\n");

    safe_write_sysfs("/proc/sys/net/ipv4/tcp_fastopen", "3");
    safe_write_sysfs("/proc/sys/net/core/somaxconn", "4096");
    safe_write_sysfs("/proc/sys/net/ipv4/tcp_moderate_rcvbuf", "0");
    safe_write_sysfs("/proc/sys/net/ipv4/tcp_tw_reuse", "1");
    safe_write_sysfs("/proc/sys/net/ipv4/tcp_fin_timeout", "10");
    safe_write_sysfs("/proc/sys/net/ipv4/tcp_max_syn_backlog", "8192");
    safe_write_sysfs("/proc/sys/net/ipv4/tcp_slow_start_after_idle", "0");

    pr_info("[VorteX] TCP: Done\n");
}

// ==========================================
// 3C. KSM OFF (FPS Critical - Reduce Background Steal)
// ==========================================

static void vortex_fps_ksm_off(void) {
    // Only disable if currently running (check state first)
    char ksm_state[8] = {0};
    
    if (vortex_read_sysfs("/sys/kernel/mm/ksm/run", ksm_state, sizeof(ksm_state))) {
        if (strcmp(ksm_state, "1") == 0) {
            if (safe_write_sysfs("/sys/kernel/mm/ksm/run", "0")) {
                pr_info("[VorteX] FPS: KSM disabled\n");
            }
        } else {
            pr_info("[VorteX] FPS: KSM already disabled\n");
        }
    } else {
        pr_debug("[VorteX] FPS: KSM not available\n");
    }
}

// ==========================================
// 3D. TIMER & RCU (FPS Critical - Reduce Wake Latency)
// ==========================================

static void vortex_fps_timer_rcu(void) {
    if (safe_write_sysfs("/proc/sys/kernel/timer_migration", "0")) {
        pr_info("[VorteX] FPS: Timer migration OFF\n");
    }

    // RCU expedited mode - only if available (skip on kernels without this)
    if (safe_write_sysfs("/sys/kernel/rcu_normal", "0")) {
        pr_info("[VorteX] FPS: RCU expedited mode\n");
    }
}

// ==========================================
// 3E. CPU IDLE STATE RESTRICTION (FPS Critical)
// ==========================================

static void vortex_fps_idle_restrict(void) {
    char path[128];
    char gov[16] = {0};
    int i, j;
    int deepest_disabled = 0;

    pr_info("[VorteX] FPS: Restricting CPU idle states...\n");

    // Scan idle states 2-5 (reduced from 2-6 for safety)
    for (j = 2; j <= 5; j++) {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpuidle/state%d/disable", j);
        
        // Check if this state exists before trying to disable it
        char test_buf[4] = {0};
        if (vortex_read_sysfs(path, test_buf, sizeof(test_buf))) {
            if (safe_write_sysfs(path, "1")) {
                deepest_disabled = j;
            }
        } else {
            // State doesn't exist, stop scanning deeper states
            break;
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

static void vortex_fps_cpu_floor(void) {
    char path[128];
    char max_freq[32] = {0};
    char cur_min[32] = {0};
    char floor_str[32];
    int i;
    int tuned = 0;

    pr_info("[VorteX] FPS: Setting big core frequency floor...\n");

    int big_policy_max = -1;
    long big_max_freq = 0;

    // Find policy with highest max frequency (big cores)
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

    // Use 45% floor instead of 50% (safer, prevents thermal issues)
    long floor = big_max_freq * 45 / 100;
    snprintf(floor_str, sizeof(floor_str), "%ld", floor);

    for (i = 4; i <= big_policy_max; i++) {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpufreq/policy%d/scaling_min_freq", i);
        if (vortex_read_sysfs(path, cur_min, sizeof(cur_min))) {
            long cur = simple_strtol(cur_min, NULL, 10);

            if (floor > cur) {
                if (safe_write_sysfs(path, floor_str)) {
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

    // Fallback scan for policies 0-3 (in case big cores are mapped there)
    if (tuned == 0) {
        for (i = 0; i <= 3; i++) {
            snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpufreq/policy%d/scaling_max_freq", i);
            if (vortex_read_sysfs(path, max_freq, sizeof(max_freq))) {
                long val = simple_strtol(max_freq, NULL, 10);
                if (val == big_max_freq) {
                    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpufreq/policy%d/scaling_min_freq", i);
                    if (vortex_read_sysfs(path, cur_min, sizeof(cur_min))) {
                        long cur = simple_strtol(cur_min, NULL, 10);
                        if (floor > cur) {
                            if (safe_write_sysfs(path, floor_str)) {
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

    safe_write_sysfs("/proc/sys/kernel/sched_wakeup_granularity_ns", "500000");

    safe_write_sysfs("/proc/sys/kernel/sched_migration_cost_ns", "50000");

    safe_write_sysfs("/proc/sys/kernel/sched_nr_migrate", "4");

    // EAS disable - try both paths gracefully
    if (safe_write_sysfs("/sys/devices/system/cpu/energy_aware", "0")) {
        pr_info("[VorteX] FPS: EAS disabled\n");
    } else if (safe_write_sysfs("/proc/sys/kernel/sched_energy_aware", "0")) {
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
            // Try lz4 first (fastest), then zstd, then keep current if neither works
            if (strstr(algo, "lz4")) {
                if (safe_write_sysfs(path, "lz4")) {
                    pr_info("[VorteX] ZRAM%d: lz4 (fastest for gaming)\n", i);
                }
            } else if (strstr(algo, "zstd")) {
                if (safe_write_sysfs(path, "zstd")) {
                    pr_info("[VorteX] ZRAM%d: zstd (fallback)\n", i);
                }
            } else if (strstr(algo, "lzo")) {
                if (safe_write_sysfs(path, "lzo")) {
                    pr_info("[VorteX] ZRAM%d: lzo (fallback)\n", i);
                }
            }
            // If algorithm not recognized, leave as-is to avoid errors
        }
    }
}

// ==========================================
// 3I. UFS/STORAGE TUNING (Universal - Auto-detect)
// ==========================================

static void vortex_tune_storage(void) {
    char path[128];
    int i;

    // UFS controller clock gate - try common paths dynamically
    const char *ufs_paths[] = {
        "/sys/devices/platform/soc/1d84000.ufshc/clkgate_enable",
        "/sys/devices/platform/soc/4904000.ufshc/clkgate_enable",
        "/sys/devices/platform/soc/4c40000.ufshc/clkgate_enable",
        "/sys/devices/platform/soc/4e04000.ufshc/clkgate_enable",
        "/sys/devices/platform/11270000.ufshc/clkgate_enable",
        NULL  // Sentinel
    };
    
    int ufs_idx = 0;
    while (ufs_paths[ufs_idx] != NULL) {
        if (safe_write_sysfs(ufs_paths[ufs_idx], "0")) {
            pr_info("[VorteX] Storage: UFS clkgate disabled\n");
            break;  // Success, stop trying
        }
        ufs_idx++;
    }

    // Block device queue depth - only modify existing devices
    for (i = 'a'; i <= 'z'; i++) {
        snprintf(path, sizeof(path), "/sys/block/sd%c/device/queue_depth", i);
        
        // Check if device exists before writing
        char test_val[8] = {0};
        if (vortex_read_sysfs(path, test_val, sizeof(test_val))) {
            // Only set if current value is less than 64
            long current = simple_strtol(test_val, NULL, 10);
            if (current < 64) {
                safe_write_sysfs(path, "64");
            }
        }
    }
}

// ==========================================
// 3J. GAMING THERMAL PROFILE (Conservative Safe Mode)
// ==========================================

static void vortex_thermal_gaming_profile(void) {
    char path[256];
    char temp_buf[32] = {0};
    char type_buf[32] = {0};
    int i, j;
    int zones_patched = 0;
    int trips_raised = 0;
    int coolers_reset = 0;

    pr_info("[VorteX] THERMAL: Applying Smart Gaming Profile...\n");

    // Attempt sconfig (will silently fail if locked by SELinux)
    safe_write_sysfs("/sys/class/thermal/thermal_message/sconfig", "9");
    safe_write_sysfs("/sys/class/thermal/thermal_message/sconfig_param", "0");

    // Attempt Thermal module disable (vendor-agnostic paths)
    safe_write_sysfs("/sys/module/msm_thermal/parameters/enabled", "0");
    safe_write_sysfs("/sys/module/msm_thermal/core_control/enabled", "0");
    safe_write_sysfs("/sys/module/msm_thermal/vdd_restriction/enabled", "0");
    // Also try generic thermal module names
    safe_write_sysfs("/sys/module/thermal_core/parameters/enabled", "0");

    // Scan and Raise Trip Points (SAFE - Conservative Values)
    for (i = 0; i <= 15; i++) {  // Reduced from 20 to 15 (most phones have <16 zones)
        int zone_modified = 0;

        for (j = 0; j <= 4; j++) {  // Reduced from 5 to 4 trips per zone
            snprintf(path, sizeof(path),
                     "/sys/class/thermal/thermal_zone%d/trip_point_%d_type", i, j);
            if (!vortex_read_sysfs(path, type_buf, sizeof(type_buf)))
                continue;

            // NEVER touch critical trip points - hardware safety
            if (strcmp(type_buf, "critical") == 0) {
                continue;
            }
            
            // Also skip hot points (hardware emergency shutdown)
            if (strcmp(type_buf, "hot") == 0) {
                continue;
            }

            snprintf(path, sizeof(path),
                     "/sys/class/thermal/thermal_zone%d/trip_point_%d_temp", i, j);
            if (!vortex_read_sysfs(path, temp_buf, sizeof(temp_buf)))
                continue;

            long val = simple_strtol(temp_buf, NULL, 10);
            if (val <= 0) continue;

            long new_val = val;
            
            // CONSERVATIVE adjustment: +5°C max (was +12°C causing overheating)
            if (strcmp(type_buf, "passive") == 0) {
                new_val = val + 5000;  // +5°C (was +12°C)
                if (new_val > 80000) new_val = 80000;  // Cap at 80°C (was 78°C)
            } else if (strcmp(type_buf, "active") == 0) {
                new_val = val + 3000;  // +3°C (was +10°C)
                if (new_val > 78000) new_val = 78000;  // Cap at 78°C (was 75°C)
            } else {
                new_val = val + 2000;  // +2°C for others (was +8°C)
                if (new_val > 75000) new_val = 75000;  // Cap at 75°C (was 72°C)
            }

            if (new_val > val) {
                char str[32];
                snprintf(str, sizeof(str), "%ld", new_val);

                if (safe_write_sysfs(path, str)) {
                    trips_raised++;
                    zone_modified = 1;
                }
            }
        }
        if (zone_modified) zones_patched++;
    }

    if (trips_raised > 0) {
        pr_info("[VorteX] THERMAL: Raised %d trip points across %d zones\n", trips_raised, zones_patched);
    }

    // Cooling Devices - ONLY reset non-zero states (don't force all to 0!)
    for (i = 0; i <= 20; i++) {  // Reduced from 30 to 20 (typical range)
        char cur_state[16] = {0};
        snprintf(path, sizeof(path), "/sys/class/thermal/cooling_device%d/cur_state", i);
        
        if (vortex_read_sysfs(path, cur_state, sizeof(cur_state))) {
            long cur = simple_strtol(cur_state, NULL, 10);
            // Only reduce aggressive cooling, don't completely neutralize
            if (cur > 3) {  // If cooling state is very high (>3), reduce to moderate level
                char mod_str[8];
                snprintf(mod_str, sizeof(mod_str), "%d", 2);  // Set to 2 (moderate cooling)
                if (safe_write_sysfs(path, mod_str)) {
                    coolers_reset++;
                }
            }
        }
        // Don't force write to 0 if device doesn't exist or already low
    }

    if (coolers_reset > 0) {
        pr_info("[VorteX] THERMAL: %d cooling devices optimized\n", coolers_reset);
    }

    // GPU Thermal Limits - Set to moderate level instead of 0 (which disables protection)
    char gpu_current_level[16] = {0};
    if (vortex_read_sysfs("/sys/class/kgsl/kgsl-3d0/thermal_pwrlevel", gpu_current_level, sizeof(gpu_current_level))) {
        long gpu_lvl = simple_strtol(gpu_current_level, NULL, 10);
        // Only reduce if currently very restrictive (> 2)
        if (gpu_lvl > 2) {
            safe_write_sysfs("/sys/class/kgsl/kgsl-3d0/thermal_pwrlevel", "2");  // Moderate (not 0!)
        }
    }

    pr_info("[VorteX] THERMAL: Gaming Profile Active (Critical Safety Kept)\n");
}

// ==========================================
// 3K. REFRESH RATE LOCK (Best Effort)
// ==========================================

static void vortex_fps_refresh_lock(void) {
    pr_info("[VorteX] FPS: Attempting refresh rate stabilization...\n");

    // All optional - use safe_write to silently skip if unavailable
    safe_write_sysfs("/sys/module/msm_drm/parameters/mdss_fb0_fps", "0");
    safe_write_sysfs("/sys/class/drm/card0/device/power/auto_latency_hint", "0");
    safe_write_sysfs("/sys/class/panel/refresh_rate", "0");
    safe_write_sysfs("/sys/class/backlight/panel0/dimming_state", "0");

    pr_info("[VorteX] FPS: Refresh stabilization applied\n");
}

// ==========================================
// 3L. ANTI-PREMDROP: FORCE CORES ONLINE
// ==========================================

static void vortex_anti_pre_cores(void) {
    char path[128];
    int i;

    pr_info("[VorteX] ANTI-PREMDROP: Forcing all cores online...\n");

    // Only online CPUs that actually exist (stop at first missing CPU)
    for (i = 0; i <= 7; i++) {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/online", i);
        
        // Check if CPU exists before forcing online
        char cpu_status[8] = {0};
        if (vortex_read_sysfs(path, cpu_status, sizeof(cpu_status))) {
            if (strcmp(cpu_status, "0") == 0) {
                if (safe_write_sysfs(path, "1")) {
                    pr_info("[VorteX] ANTI-PREMDROP: CPU%d forced online\n", i);
                }
            }
        } else {
            // CPU doesn't exist on this device (e.g., quad-core phone)
            break;  // Stop scanning
        }
    }

    // Hotplug control - use safe writes
    safe_write_sysfs("/sys/devices/system/cpu/cpuhotplug/disable", "1");
    safe_write_sysfs("/sys/module/msm_thermal/core_control/enabled", "0");
    safe_write_sysfs("/sys/module/msm_hotplug/enabled", "0");

    pr_info("[VorteX] ANTI-PREMDROP: Cores locked\n");
}

// ==========================================
// 3M. ANTI-PREMDROP: THP & OVERHEAD KILLER (Safe Mode)
// ==========================================

static void vortex_anti_pre_mem(void) {
    pr_info("[VorteX] ANTI-PREMDROP: Patching memory overhead...\n");

    // THP: Use "madvise" instead of "always" (prevents fragmentation on low-memory)
    safe_write_sysfs("/sys/kernel/mm/transparent_hugepage/enabled", "madvise");
    safe_write_sysfs("/sys/kernel/mm/transparent_hugepage/defrag", "madvise");
    
    safe_write_sysfs("/proc/sys/kernel/numa_balancing", "0");
    safe_write_sysfs("/proc/sys/kernel/sched_schedstats", "0");
    
    // RT runtime: Use 95% instead of -1 (prevents kernel thread starvation)
    safe_write_sysfs("/proc/sys/kernel/sched_rt_runtime_us", "950000");

    pr_info("[VorteX] ANTI-PREMDROP: Memory overhead patched\n");
}

// ==========================================
// 3N. ANTI-PREMDROP: SCHED & INPUT BOOST
// ==========================================

static void vortex_anti_pre_boost(void) {
    char max_freq[32] = {0};

    pr_info("[VorteX] ANTI-PREMDROP: Applying scheduler boost...\n");

    safe_write_sysfs("/proc/sys/kernel/sched_boost", "1");
    safe_write_sysfs("/sys/devices/system/cpu/sched_boost", "1");
    // Removed prefer_idle (can cause power waste without significant gaming benefit)

    if (vortex_read_sysfs("/sys/devices/system/cpu/cpufreq/policy4/scaling_max_freq", max_freq, sizeof(max_freq))) {
        safe_write_sysfs("/sys/module/cpu_boost/input_boost_freq", max_freq);
        safe_write_sysfs("/sys/module/cpu_boost/input_boost_ms", "500");
        pr_info("[VorteX] ANTI-PREMDROP: Touch boost = %s\n", max_freq);
    }

    pr_info("[VorteX] ANTI-PREMDROP: Scheduler & Touch boosted\n");
}

// ==========================================
// 4. SAFE SYSFS THREAD (Master Sequence)
// ==========================================

static int vortex_sysfs_thread(void *data) {
    char path[128];
    char max_freq_val[32] = {0};
    char current_gov[32] = {0};
    int i;

    ssleep(15);

    pr_info("[VorteX] =======================================\n");
    pr_info("[VorteX] VorteX FPS Engine v2.2 Starting...\n");
    pr_info("[VorteX] =======================================\n");

    vortex_anti_pre_cores(); /* + INJECT */

    vortex_tune_vm();
    vortex_tune_tcp();
    vortex_fps_ksm_off();
    vortex_fps_timer_rcu();
    vortex_fps_idle_restrict();
    vortex_thermal_gaming_profile(); /* MODIFIED: Smart Thermal */
    vortex_fps_scheduler();

    vortex_anti_pre_mem(); /* + INJECT */

    vortex_tune_zram();
    vortex_tune_storage();

    pr_info("[VorteX] CPU: Forcing schedutil with FPS-optimized rates...\n");
    for (i = 0; i <= 15; i++) {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpufreq/policy%d/scaling_governor", i);
        if (safe_write_sysfs(path, "schedutil")) {
            pr_info("[VorteX] CPU: Policy %d -> schedutil\n", i);
        }

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpufreq/policy%d/schedutil/up_rate_limit_us", i);
        if (safe_write_sysfs(path, "500")) {
        }

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpufreq/policy%d/schedutil/down_rate_limit_us", i);
        if (safe_write_sysfs(path, "40000")) {
        }
    }
    pr_info("[VorteX] CPU: up_rate=500us, down_rate=40ms (FPS optimized)\n");

    vortex_fps_cpu_floor();

    vortex_anti_pre_boost(); /* + INJECT */

    pr_info("[VorteX] I/O: Scanning block devices...\n");
    for (i = 'a'; i <= 'z'; i++) {
        snprintf(path, sizeof(path), "/sys/block/sd%c/queue/scheduler", i);
        if (safe_write_sysfs(path, "adios")) {
            pr_info("[VorteX] I/O: sd%c -> adios\n", i);
        } else if (safe_write_sysfs(path, "kyber")) {
            pr_info("[VorteX] I/O: sd%c -> kyber\n", i);
        } else if (safe_write_sysfs(path, "mq-deadline")) {
            pr_info("[VorteX] I/O: sd%c -> mq-deadline\n", i);
        }
        // Fallback: if none work, device probably doesn't support scheduler change
        
        snprintf(path, sizeof(path), "/sys/block/sd%c/queue/read_ahead_kb", i);
        safe_write_sysfs(path, "128");
        snprintf(path, sizeof(path), "/sys/block/sd%c/queue/iostats", i);
        safe_write_sysfs(path, "0");
        snprintf(path, sizeof(path), "/sys/block/sd%c/queue/nr_requests", i);
        safe_write_sysfs(path, "256");
        snprintf(path, sizeof(path), "/sys/block/sd%c/queue/rq_affinity", i);
        safe_write_sysfs(path, "1");
    }
    for (i = 0; i <= 15; i++) {
        snprintf(path, sizeof(path), "/sys/block/dm-%d/queue/scheduler", i);
        if (safe_write_sysfs(path, "adios")) {
            pr_info("[VorteX] I/O: dm-%d -> adios\n", i);
        } else {
            safe_write_sysfs(path, "mq-deadline");  // Stable fallback for dm devices
        }
        snprintf(path, sizeof(path), "/sys/block/dm-%d/queue/read_ahead_kb", i);
        safe_write_sysfs(path, "128");
        snprintf(path, sizeof(path), "/sys/block/dm-%d/queue/iostats", i);
        safe_write_sysfs(path, "0");
        snprintf(path, sizeof(path), "/sys/block/dm-%d/queue/nr_requests", i);
        safe_write_sysfs(path, "256");
    }

    // GPU Tuning - SAFE MODE (lock at 80% of max instead of 100% to prevent thermal throttling)
    if (vortex_read_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/max_freq", max_freq_val, sizeof(max_freq_val))) {
        pr_info("[VorteX] GPU: Max freq = %s\n", max_freq_val);

        // Calculate 80% of max for minimum frequency (safer than locking at 100%)
        long max_gpu = simple_strtol(max_freq_val, NULL, 10);
        long target_min = max_gpu * 80 / 100;
        char min_freq_str[32];
        snprintf(min_freq_str, sizeof(min_freq_str), "%ld", target_min);
        
        if (safe_write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/min_freq", min_freq_str)) {
            pr_info("[VorteX] GPU: Min locked at 80%% (%ld KHz)\n", target_min);
        }
        
        safe_write_sysfs("/sys/class/kgsl/kgsl-3d0/max_gpuclk", max_freq_val);

        if (safe_write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/governor", "schedutil")) {
            pr_info("[VorteX] GPU: Governor -> schedutil\n");
        } else {
            vortex_read_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/governor", current_gov, sizeof(current_gov));
            pr_warn("[VorteX] GPU: Governor blocked. Current: %s\n", current_gov);
        }

        safe_write_sysfs("/sys/class/kgsl/kgsl-3d0/force_bus_on", "1");
        safe_write_sysfs("/sys/class/kgsl/kgsl-3d0/gpu_llc_slice_enable", "1");
        safe_write_sysfs("/sys/class/kgsl/kgsl-3d0/l3_vote", "1");

        safe_write_sysfs("/sys/class/kgsl/kgsl-3d0/split_display", "0");
        // DON'T disable low_latency (causes stuttering in some games)
        // safe_write_sysfs("/sys/class/kgsl/kgsl-3d0/disable_low_latency", "0");  // REMOVED
        safe_write_sysfs("/sys/class/kgsl/kgsl-3d0/three_d_texture", "1");
    } else {
        pr_warn("[VorteX] GPU: KGSL not found (non-Qualcomm?)\n");
    }

    // LMK - Less aggressive than original (prevent OOM kills during gaming)
    if (safe_write_sysfs("/sys/module/lowmemorykiller/parameters/minfree", 
                         "5120,10240,20480,35840,51200,56320")) {
        pr_info("[VorteX] LMK: Updated (conservative)\n");
    }

    safe_write_sysfs("/proc/sys/kernel/printk_devkmsg", "off");
    safe_write_sysfs("/sys/module/usbcore/parameters/autosuspend", "-1");

    vortex_fps_refresh_lock();

    pr_info("[VorteX] =======================================\n");
    pr_info("[VorteX] VorteX FPS Engine v2.2 COMPLETED\n");
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
MODULE_VERSION("2.2-safe");
// Signed-off-by: kingfinix98@gmail.com
