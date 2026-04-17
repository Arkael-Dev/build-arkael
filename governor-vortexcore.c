// SPDX-License-Identifier: GPL-2.0
/*
 * ══════════════════════════════════════════════════════════════════
 * VORTEX DUAL-GOVERNOR SYSTEM v4.0 (Combined Edition)
 * ══════════════════════════════════════════════════════════════════
 * 
 * ONE SINGLE .c FILE = TWO SEPARATE GOVERNORS
 * 
 * Governor 1: "vortexcore"  → Lightweight, Battery-Friendly, Daily Use
 * Governor 2: "vortexmax"   → Full-Featured, Gaming, Performance
 * 
 * How to Use:
 * 1. Flash this kernel zip to your device
 * 2. Open FKM / Kernel Adiutor / TWRP
 * 3. Select governor: "vortexcore" OR "vortexmax"
 * 4. Apply & reboot (or hot-swap without reboot)
 * 
 * Both governors DO NOT conflict because:
 * - Separate data structures (per_cpu independent)
 * - Separate eval_freq functions for each governor
 * - Only ONE governor active per CPU cluster at a time
 * 
 * ───────────────────────────────────────────────────────────────────
 * Author:      Kingfinik98
 * Email:       kingfinix98@gmail.com
 * Repository:  https://github.com/Kingfinik98/build-vortex
 * Version:     4.0 Dual-Governor Combined
 * License:     GPL-2.0
 * ══════════════════════════════════════════════════════════════════
 */

/* =====================================================================
 * HEADER INCLUDES - Standard Kernel Headers Only
 * ===================================================================== */
#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/sched/cpufreq.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/thermal.h>
#include <linux/cpu.h>


/* #############################################################################
 * #                                                                           #
 * #  GOVERNOR 1: VORTEXCORE v3.2 (Lightweight / Battery Saver)               #
 * #                                                                           #
 * ############################################################################# */

/* ---------------------------------------------------------------------
 * VORTEXCORE PARAMETERS (Sysfs Tunable)
 * Tunable via: /sys/module/governor_vortexcore/parameters/
 * --------------------------------------------------------------------- */
static unsigned int vortexcore_target_load_big = 80;
module_param_named(vortexcore_target_load_big, vortexcore_target_load_big, uint, 0644);
/* Target load threshold for big cores (CPU cluster with higher max freq) */

static unsigned int vortexcore_target_load_little = 90;
module_param_named(vortexcore_target_load_little, vortexcore_target_load_little, uint, 0644);
/* Target load threshold for LITTLE cores (efficiency cluster) */

static unsigned int vortexcore_fast_ramp_up_load = 90;
module_param(vortexcore_fast_ramp_up_load, uint, 0644);
/* Load threshold to immediately jump to maximum frequency */

/* ---------------------------------------------------------------------
 * VORTEXCORE DATA STRUCTURES
 * Per-CPU state tracking for frequency decision making
 * --------------------------------------------------------------------- */
struct vortexcore_cpu_info {
    u64 prev_cpu_idle;          /* Previous idle time snapshot */
    u64 prev_cpu_wall;           /* Previous wall time snapshot */
    unsigned int target_freq;    /* Last computed target frequency */
    unsigned int thermal_counter;/* Thermal throttle counter */
    unsigned int next_delay_ms;  /* Next sampling interval (ms) */
    unsigned int max_hold_counter; /* Anti-parachute hold counter */
};

static DEFINE_PER_CPU(struct vortexcore_cpu_info, vortexcore_info);
/* Per-CPU allocation for VortexCore state */

struct vortexcore_policy_info {
    struct delayed_work work;   /* Delayed work for sampling loop */
    struct cpufreq_policy *policy; /* Associated cpufreq policy */
};

/* ---------------------------------------------------------------------
 * VORTEXCORE CORE LOGIC - Frequency Evaluation Engine
 * Called every sampling interval to determine optimal CPU frequency
 * --------------------------------------------------------------------- */
static void vortexcore_eval_freq(struct cpufreq_policy *policy)
{
    struct vortexcore_cpu_info *info = &per_cpu(vortexcore_info, policy->cpu);
    u64 now, idle_time, delta_wall, delta_idle;
    unsigned int load, freq_target, current_freq = policy->cur;
    unsigned int thermal_max = policy->max;

    now = local_clock();
    idle_time = get_cpu_idle_time(policy->cpu, &delta_wall, 0);

    /* First run initialization - capture baseline values */
    if (info->prev_cpu_wall == 0) {
        info->prev_cpu_wall = now;
        info->prev_cpu_idle = idle_time;
        info->target_freq = current_freq;
        info->next_delay_ms = 20;
        return;
    }

    /* Calculate time deltas since last sample */
    delta_wall = now - info->prev_cpu_wall;
    delta_idle = idle_time - info->prev_cpu_idle;
    info->prev_cpu_wall = now;
    info->prev_cpu_idle = idle_time;

    /* Compute CPU load as percentage (0-100) */
    if (delta_wall == 0 || delta_idle > delta_wall)
        load = 0;
    else
        load = div64_u64(100 * (delta_wall - delta_idle), delta_wall);

    /* big.LITTLE Architecture Awareness */
    bool is_big = (policy->cpuinfo.max_freq > (policy->cpuinfo.min_freq * 2));
    unsigned int dyn_target_load = is_big ? vortexcore_target_load_big : vortexcore_target_load_little;

    /* ================================================================
     * DECISION MATRIX - Frequency Selection Algorithm
     * ================================================================ */
    
    if (load >= vortexcore_fast_ramp_up_load) {
        /* CRITICAL LOAD: Jump to maximum frequency immediately */
        freq_target = thermal_max;
        if (info->max_hold_counter < 5)
            info->max_hold_counter++; /* Activate anti-parachute hold */
            
    } else if (load > dyn_target_load) {
        /* ELEVATED LOAD: Scale frequency proportionally */
        unsigned int freq_adj = thermal_max * load / 100;
        freq_target = max(freq_adj, current_freq); /* Never decrease here */
        
    } else {
        /* NORMAL/LOW LOAD: Apply anti-parachute or decay */
        
        if (info->max_hold_counter > 0) {
            /* MAX FREQUENCY HOLD (Anti-Parachuting Mechanism)
             * Prevents sudden frequency drop after burst ends
             * Holds steady until counter expires */
            freq_target = current_freq; /* Hold current frequency */
            info->max_hold_counter--;
        } else {
            /* NORMAL DECAY LOGIC
             * Gradually reduce frequency using controlled step size */
            if (current_freq > policy->min) {
                unsigned int freq_diff = current_freq - policy->min;
                unsigned int decay_step = max(policy->min / 100, freq_diff / 20);
                freq_target = (current_freq > policy->min + decay_step) ? 
                              current_freq - decay_step : policy->min;
            } else {
                freq_target = policy->min; /* Already at minimum */
            }
        }
    }

    /* PROACTIVE THERMAL MANAGEMENT
     * Reduce frequency slightly before hitting thermal limits
     * Prevents aggressive throttling events */
    if (freq_target >= thermal_max) {
        info->thermal_counter++;
        if (info->thermal_counter > 20) {
            /* Sustained max freq - apply 5% thermal reduction */
            freq_target = (thermal_max * 95) / 100;
        }
    } else {
        /* Recover thermal counter when below max */
        if (info->thermal_counter > 0)
            info->thermal_counter--;
    }

    /* Apply frequency change if target differs from last */
    if (freq_target != info->target_freq) {
        info->target_freq = freq_target;
        __cpufreq_driver_target(policy, freq_target, CPUFREQ_RELATION_L);
    }

    /* ADAPTIVE SAMPLING RATE
     * Faster sampling under load, slower when idle */
    if (load >= vortexcore_fast_ramp_up_load || load > dyn_target_load) {
        info->next_delay_ms = 10;  /* Fast sampling (10ms) under high load */
    } else if (current_freq == policy->min) {
        info->next_delay_ms = 20;  /* Normal sampling at min freq */
    } else {
        info->next_delay_ms = 20;  /* Default sampling rate */
    }
}

/* ---------------------------------------------------------------------
 * VORTEXCORE WORK QUEUE HANDLER
 * Kernel workqueue callback for periodic frequency evaluation
 * --------------------------------------------------------------------- */
static void vortexcore_work_handler(struct work_struct *work)
{
    struct vortexcore_policy_info *vpinfo = 
        container_of(work, struct vortexcore_policy_info, work.work);
    struct cpufreq_policy *policy = vpinfo->policy;
    struct vortexcore_cpu_info *info = &per_cpu(vortexcore_info, policy->cpu);

    /* Run frequency evaluation algorithm */
    vortexcore_eval_freq(policy);
    
    /* Reschedule next evaluation with adaptive delay */
    schedule_delayed_work_on(policy->cpu, &vpinfo->work, 
                             msecs_to_jiffies(info->next_delay_ms));
}

/* ---------------------------------------------------------------------
 * VORTEXCORE GKI GOVERNOR API STRUCT
 * Implements standard GKI (Generic Kernel Image) governor interface
 * Compatible with Android GKI 5.10 / 6.1 / 6.6
 * --------------------------------------------------------------------- */
static int vortexcore_init(struct cpufreq_policy *policy)
{
    /* Allocate per-policy private data structure */
    struct vortexcore_policy_info *vpinfo = kzalloc(sizeof(*vpinfo), GFP_KERNEL);
    if (!vpinfo)
        return -ENOMEM; /* Memory allocation failed */
    
    vpinfo->policy = policy;
    INIT_DEFERRABLE_WORK(&vpinfo->work, vortexcore_work_handler);
    policy->governor_data = vpinfo;
    return 0;
}

static void vortexcore_exit(struct cpufreq_policy *policy)
{
    /* Cleanup and free per-policy resources */
    struct vortexcore_policy_info *vpinfo = policy->governor_data;
    if (vpinfo) {
        cancel_delayed_work_sync(&vpinfo->work); /* Cancel pending work */
        kfree(vpinfo); /* Free allocated memory */
        policy->governor_data = NULL;
    }
}

static int vortexcore_start(struct cpufreq_policy *policy)
{
    unsigned int cpu;
    
    /* Initialize per-CPU state for all CPUs in this policy */
    for_each_cpu(cpu, policy->cpus) {
        struct vortexcore_cpu_info *info = &per_cpu(vortexcore_info, cpu);
        info->prev_cpu_idle = get_cpu_idle_time(cpu, &info->prev_cpu_wall, 0);
        info->target_freq = policy->cur;
        info->thermal_counter = 0;
        info->max_hold_counter = 0;
        info->next_delay_ms = 20;
    }
    
    /* Start the main governor sampling loop */
    schedule_delayed_work_on(policy->cpu,
        &((struct vortexcore_policy_info *)policy->governor_data)->work,
        msecs_to_jiffies(20));
    return 0;
}

static void vortexcore_stop(struct cpufreq_policy *policy)
{
    /* Stop the sampling loop for this policy */
    cancel_delayed_work_sync(
        &((struct vortexcore_policy_info *)policy->governor_data)->work);
}

static void vortexcore_limits(struct cpufreq_policy *policy)
{
    /* Re-evaluate when policy limits change (e.g., thermal constraint) */
    vortexcore_eval_freq(policy);
}

/* ---------------------------------------------------------------------
 * VORTEXCORE GOVERNOR REGISTRATION STRUCT
 * Registers "vortexcore" as an available cpufreq governor
 * --------------------------------------------------------------------- */
static struct cpufreq_governor vortexcore_gov = {
    .name           = "vortexcore",   /* Governor name #1 */
    .owner          = THIS_MODULE,
    .init           = vortexcore_init,
    .exit           = vortexcore_exit,
    .start          = vortexcore_start,
    .stop           = vortexcore_stop,
    .limits         = vortexcore_limits,
};


/* #############################################################################
 * #                                                                           #
 * #  GOVERNOR 2: VORTEXMAX v3.0 (Full-Featured / Gaming)                     #
 * #                                                                           #
 * ############################################################################# */

/* ---------------------------------------------------------------------
 * VORTEXMAX PARAMETERS (Sysfs Tunable)
 * All parameters adjustable at runtime via sysfs
 * Path: /sys/module/governor_vortexcore/parameters/<param_name>
 * --------------------------------------------------------------------- */

/* --- Base Load Thresholds --- */
static unsigned int target_load_big = 75;
module_param_named(target_load_big, target_load_big, uint, 0644);
/* Target load for big cores before ramping up */

static unsigned int target_load_little = 85;
module_param_named(target_load_little, target_load_little, uint, 0644);
/* Target load for LITTLE cores before ramping up */

static unsigned int fast_ramp_up_load = 85;
module_param(fast_ramp_up_load, uint, 0644);
/* Load threshold for immediate max frequency jump */

/* --- Touch Boost Parameters --- */
static unsigned int touch_boost_freq_pct = 90;
module_param_named(touch_boost_freq_pct, touch_boost_freq_pct, uint, 0644);
/* Touch boost target frequency (% of max) */

static unsigned int touch_boost_duration_ms = 500;
module_param_named(touch_boost_duration_ms, touch_boost_duration_ms, uint, 0644);
/* How long touch boost lasts after screen touch (milliseconds) */

static bool touch_boost_enabled = true;
module_param_named(touch_boost_enabled, touch_boost_enabled, bool, 0644);
/* Enable/disable touch boost feature */

/* --- IO Boost Parameters --- */
static unsigned int io_boost_freq_pct = 80;
module_param_named(io_boost_freq_pct, io_boost_freq_pct, uint, 0644);
/* IO boost target frequency (% of max) */

static unsigned int io_boost_duration_ms = 300;
module_param_named(io_boost_duration_ms, io_boost_duration_ms, uint, 0644);
/* IO boost duration after detection (milliseconds) */

static bool io_boost_enabled = true;
module_param_named(io_boost_enabled, io_boost_enabled, bool, 0644);
/* Enable/disable automatic IO burst detection */

static unsigned int io_detect_threshold = 60;
module_param(io_detect_threshold, uint, 0644);
/* Load percentage that triggers IO burst detection */

/* --- Smart Ramp-Up Parameters --- */
static unsigned int ramp_up_step_pct = 35;
module_param_named(ramp_up_step_pct, ramp_up_step_pct, uint, 0644);
/* Frequency increase step size (% of max) */

static bool ramp_up_momentum = true;
module_param_named(ramp_up_momentum, ramp_up_momentum, bool, 0644);
/* Accelerate ramp-up if load trend is increasing */

/* --- Frequency Floor Parameters --- */
static unsigned int freq_floor_idle_pct = 25;
module_param_named(freq_floor_idle_pct, freq_floor_idle_pct, uint, 0644);
/* Minimum frequency floor when idle (% of max) */

static unsigned int freq_floor_active_pct = 40;
module_param_named(freq_floor_active_pct, freq_floor_active_pct, uint, 0644);
/* Minimum frequency floor during active use (% of max) */

static unsigned int freq_floor_gaming_pct = 55;
module_param_named(freq_floor_gaming_pct, freq_floor_gaming_pct, uint, 0644);
/* Minimum frequency floor in gaming mode (% of max) */

/* --- Hysteresis Band Parameters --- */
static unsigned int hysteresis_up_pct = 10;
module_param_named(hysteresis_up_pct, hysteresis_up_pct, uint, 0644);
/* Minimum increase to act (%) */

static unsigned int hysteresis_down_pct = 8;
module_param_named(hysteresis_down_pct, hysteresis_down_pct, uint, 0644);
/* Minimum decrease to act (%) */

/* --- Max Hold Plus Parameters --- */
static unsigned int max_hold_cycles = 10;
module_param_named(max_hold_cycles, max_hold_cycles, uint, 0644);
/* Number of cycles to hold max frequency */

static unsigned int max_hold_threshold = 75;
module_param_named(max_hold_threshold, max_hold_threshold, uint, 0644);
/* Load threshold to activate max hold */

/* --- Thermal Guard Parameters --- */
static unsigned int thermal_limit_pct = 92;
module_param_named(thermal_limit_pct, thermal_limit_pct, uint, 0644);
/* Soft thermal limit (% of max) */

static unsigned int thermal_counter_threshold = 15;
module_param(thermal_counter_threshold, uint, 0644);
/* Cycles above limit before taking action */

static bool thermal_hard_limit = false;
module_param_named(thermal_hard_limit, thermal_hard_limit, bool, 0644);
/* True=hard cap, False=gradual reduction */

/* --- Sampling Rate Parameters --- */
static unsigned int sample_rate_boost_ms = 4;
module_param(sample_rate_boost_ms, uint, 0644);
/* Sampling interval during boost (ms) */

static unsigned int sample_rate_active_ms = 8;
module_param(sample_rate_active_ms, uint, 0644);
/* Sampling interval during active use (ms) */

static unsigned int sample_rate_normal_ms = 16;
module_param(sample_rate_normal_ms, uint, 0644);
/* Sampling interval normal operation (ms) */

static unsigned int sample_rate_idle_ms = 24;
module_param(sample_rate_idle_ms, uint, 0644);
/* Sampling interval when idle (ms) */

/* --- Wake Boost Parameters --- */
static unsigned int wake_boost_freq_pct = 70;
module_param_named(wake_boost_freq_pct, wake_boost_freq_pct, uint, 0644);
/* Wake boost target frequency (% of max) */

static unsigned int wake_boost_duration_ms = 200;
module_param_named(wake_boost_duration_ms, wake_boost_duration_ms, uint, 0644);
/* Wake boost duration after CPU wakeup (ms) */

static bool wake_boost_enabled = true;
module_param_named(wake_boost_enabled, wake_boost_enabled, bool, 0644);
/* Enable/disable wake-from-idle boost */

/* --- Transition Rate Limiter --- */
static unsigned int max_freq_change_per_ms = 100;
module_param(max_freq_change_per_ms, uint, 0644);
/* Maximum kHz change allowed per millisecond */

static bool transition_smooth_enabled = true;
module_param_named(transition_smooth_enabled, transition_smooth_enabled, bool, 0644);
/* Enable frequency transition smoothing */

/* --- Gaming Detection Parameters --- */
static unsigned int gaming_load_threshold = 70;
module_param(gaming_load_threshold, uint, 0644);
/* Load threshold to detect gaming workload */

static unsigned int gaming_sustain_cycles = 12;
module_param(gaming_sustain_cycles, uint, 0644);
/* Consecutive cycles to confirm gaming mode */

/* ---------------------------------------------------------------------
 * VORTEXMAX DATA STRUCTURES
 * Comprehensive per-CPU state for advanced frequency management
 * --------------------------------------------------------------------- */
struct vortexmax_cpu_info {
    u64 prev_cpu_idle;              /* Previous idle time snapshot */
    u64 prev_cpu_wall;              /* Previous wall time snapshot */
    u64 last_touch_time;            /* Timestamp of last touch event */
    u64 last_io_time;               /* Timestamp of last IO burst */
    u64 last_wake_time;             /* Timestamp of last CPU wakeup */
    unsigned int target_freq;       /* Last computed target frequency */
    unsigned int thermal_counter;   /* Thermal guard accumulator */
    unsigned int next_delay_ms;     /* Next sampling interval (ms) */
    unsigned int max_hold_counter;  /* Anti-parachute hold counter */
    
    /* Load History Ring Buffer (for EMA prediction) */
    unsigned int load_history[8];   /* Circular buffer of recent loads */
    unsigned int history_idx;       /* Current write index in ring buffer */
    unsigned int avg_load_ema;      /* Exponential moving average */
    unsigned int prev_load;         /* Previous raw load value */
    
    /* Trend Detection State */
    int load_trend;                 /* -1=decreasing, 0=stable, 1=increasing */
    unsigned int trend_counter;     /* Trend confirmation counter */
    
    /* State Machine Flags */
    bool is_gaming_mode;            /* Gaming pattern detected flag */
    bool is_active;                 /* Active usage detected flag */
    unsigned int consecutive_high_load;  /* High load streak counter */
    unsigned int consecutive_low_load;   /* Low load streak counter */
    
    /* IO Burst Detection State */
    unsigned int io_burst_counter;  /* IO burst occurrence counter */
    bool io_burst_active;           /* Current IO burst active flag */
};

static DEFINE_PER_CPU(struct vortexmax_cpu_info, vortexmax_info);
/* Per-CPU allocation for VortexMax state */

struct vortexmax_policy_info {
    struct delayed_work work;       /* Delayed work for sampling loop */
    struct cpufreq_policy *policy;  /* Associated cpufreq policy */
};

/* Global State (shared across all CPUs using VortexMax) */
static bool vortexmax_initialized = false;      /* Subsystem init flag */
static atomic_t vortexmax_active_governors = ATOMIC_INIT(0); /* Active instance count */
static DEFINE_SPINLOCK(vortexmax_lock);         /* Global state lock */

/* Work queue for touch boost (global, shared across CPUs) */
static struct work_struct vortexmax_touch_boost_work;
static bool vortexmax_touch_pending = false;    /* Pending touch boost flag */

/* Thermal notification registration (global) */
static struct notifier_block vortexmax_thermal_notifier;

/* ---------------------------------------------------------------------
 * VORTEXMAX LOAD CALCULATION ENGINE
 * Advanced algorithms for smooth, predictive frequency scaling
 * --------------------------------------------------------------------- */

/**
 * vortexmax_update_load_history() - Add current load to ring buffer
 * @info: Per-CPU info structure
 * @load: Current calculated load (0-100)
 * 
 * Maintains circular buffer of recent load samples for EMA calculation
 */
static void vortexmax_update_load_history(struct vortexmax_cpu_info *info, 
                                           unsigned int load)
{
    info->prev_load = info->load_history[info->history_idx];
    info->load_history[info->history_idx] = load;
    info->history_idx = (info->history_idx + 1) % 8; /* Wrap around */
}

/**
 * vortexmax_calculate_ema() - Exponential Moving Average filter
 * @info: Per-CPU info structure  
 * @current_load: Raw load value (0-100)
 * 
 * Provides smoothed load value that reacts quickly to changes
 * Uses alpha=0.3 (~51/171) for good balance of responsiveness vs smoothness
 * 
 * Returns: Smoothed load value (0-100)
 */
static unsigned int vortexmax_calculate_ema(struct vortexmax_cpu_info *info, 
                                             unsigned int current_load)
{
    unsigned int ema;
    
    if (info->avg_load_ema == 0) {
        /* First run: initialize to current load */
        ema = current_load;
    } else {
        /* EMA formula: α * new + (1-α) * old, where α=0.3 (~51/171) */
        ema = (current_load * 51 + info->avg_load_ema * 120) / 171;
    }
    
    info->avg_load_ema = ema;
    return ema;
}

/**
 * vortexmax_detect_load_trend() - Determine load direction
 * @info: Per-CPU info structure
 * 
 * Compares recent samples vs older samples to detect increasing/decreasing trend
 * 
 * Returns: -1 (decreasing), 0 (stable), 1 (increasing)
 */
static int vortexmax_detect_load_trend(struct vortexmax_cpu_info *info)
{
    unsigned int recent_avg, older_avg;
    int i, recent_sum = 0, older_sum = 0;
    
    /* Compare last 3 samples vs 3 samples before that */
    for (i = 0; i < 3; i++) {
        int idx = (info->history_idx - 1 - i + 8) % 8;
        if (idx >= 0 && idx < 8)
            recent_sum += info->load_history[idx];
    }
    
    for (i = 3; i < 6; i++) {
        int idx = (info->history_idx - 1 - i + 8) % 8;
        if (idx >= 0 && idx < 8)
            older_sum += info->load_history[idx];
    }
    
    recent_avg = recent_sum / 3;
    older_avg = older_sum / 3;
    
    if (recent_avg > older_avg + 5)
        return 1;  /* Increasing trend */
    else if (older_avg > recent_avg + 5)
        return -1; /* Decreasing trend */
    
    return 0; /* Stable - no significant trend */
}

/**
 * vortexmax_detect_gaming_pattern() - Identify gaming workload heuristically
 * @info: Per-CPU info structure
 * @load: Current smoothed load value
 * 
 * Gaming characterized by: sustained high load (>70%), frequent interaction
 * Uses state machine with hysteresis to avoid rapid mode switching
 */
static void vortexmax_detect_gaming_pattern(struct vortexmax_cpu_info *info, 
                                             unsigned int load)
{
    if (load >= gaming_load_threshold) {
        /* High load detected - accumulate evidence for gaming mode */
        info->consecutive_high_load++;
        info->consecutive_low_load = 0; /* Reset low-load counter */
        
        if (info->consecutive_high_load >= gaming_sustain_cycles) {
            /* Confirmed: sustained gaming workload */
            info->is_gaming_mode = true;
            info->is_active = true;
        }
    } else if (load < 30) {
        /* Low load - start exit countdown from gaming mode */
        info->consecutive_low_load++;
        info->consecutive_high_load = 0;
        
        /* Exit gaming mode only after sustained low load (2x confirm time) */
        if (info->consecutive_low_load >= (gaming_sustain_cycles * 2)) {
            info->is_gaming_mode = false;
        }
        
        /* Mark inactive after shorter low-load period */
        if (info->consecutive_low_load >= 5) {
            info->is_active = false;
        }
    } else {
        /* Medium load - decay counters slowly, maintain current state */
        info->consecutive_high_load >>= 1; /* Slow decay */
        info->consecutive_low_load >>= 1;
        info->is_active = true;
    }
}

/* ---------------------------------------------------------------------
 * VORTEXMAX BOOST ENGINES
 * All internal - no external triggers or user-space dependencies
 * --------------------------------------------------------------------- */

/**
 * vortexmax_apply_touch_boost() - Calculate touch boost frequency
 * @policy: Target CPU frequency policy
 * @info: Per-CPU state information
 * @now_ns: Current timestamp in nanoseconds
 * 
 * Triggered by kernel input_handler (touchscreen events)
 * Returns boosted frequency or 0 if boost inactive/expired
 */
static unsigned int vortexmax_apply_touch_boost(struct cpufreq_policy *policy,
                                                 struct vortexmax_cpu_info *info,
                                                 u64 now_ns)
{
    u64 elapsed_us;
    unsigned int boost_freq;
    
    if (!touch_boost_enabled || info->last_touch_time == 0)
        return 0; /* Feature disabled or no touch event recorded */
    
    /* Calculate elapsed time since last touch event */
    elapsed_us = (now_ns - info->last_touch_time) / 1000ULL; /* ns → μs */
    
    if (elapsed_us < (touch_boost_duration_ms * 1000ULL)) {
        /* Within boost window - calculate boosted target frequency */
        boost_freq = (policy->max * touch_boost_freq_pct) / 100;
        
        /* Don't reduce below current frequency during boost */
        if (boost_freq < policy->cur)
            boost_freq = policy->cur;
        
        /* Extend max hold while boosting to prevent post-touch drop */
        if (info->max_hold_counter < max_hold_cycles)
            info->max_hold_counter = max_hold_cycles;
        
        return boost_freq;
    }
    
    return 0; /* Boost window expired */
}

/**
 * vortexmax_apply_io_boost() - Internal IO burst detection & boost
 * @policy: Target CPU frequency policy
 * @info: Per-CPU state information
 * @now_ns: Current timestamp in nanoseconds
 * @load: Current CPU load value
 * 
 * NO external trigger needed - detects IO patterns from load spikes
 * Typical IO pattern: sudden load jump after idle/low period
 * 
 * Returns boosted frequency or 0 if no IO burst active
 */
static unsigned int vortexmax_apply_io_boost(struct cpufreq_policy *policy,
                                              struct vortexmax_cpu_info *info,
                                              u64 now_ns,
                                              unsigned int load)
{
    u64 elapsed_us;
    unsigned int boost_freq;
    
    if (!io_boost_enabled)
        return 0; /* Feature disabled */
    
    /* Detect IO pattern: sudden load spike after idle/low period */
    if ((load >= io_detect_threshold) && 
        (info->prev_load < (io_detect_threshold - 20))) {
        /* Sudden spike detected - likely disk/network IO operation */
        info->io_burst_counter++;
        info->last_io_time = now_ns;
        info->io_burst_active = true;
    } else if (info->io_burst_active) {
        /* Check if IO burst window has expired */
        elapsed_us = (now_ns - info->last_io_time) / 1000ULL;
        if (elapsed_us > (io_boost_duration_ms * 1000ULL)) {
            info->io_burst_active = false; /* Burst ended */
        }
    }
    
    /* Apply boost if IO burst currently active */
    if (info->io_burst_active) {
        boost_freq = (policy->max * io_boost_freq_pct) / 100;
        if (boost_freq < policy->cur)
            boost_freq = policy->cur;
        return boost_freq;
    }
    
    return 0; /* No IO burst */
}

/**
 * vortexmax_apply_wake_boost() - Boost when CPU wakes from idle
 * @policy: Target CPU frequency policy
 * @info: Per-CPU state information
 * @now_ns: Current timestamp in nanoseconds
 * 
 * Prevents sluggish response after CPU exits deep sleep state
 * 
 * Returns boosted frequency or 0 if boost inactive/expired
 */
static unsigned int vortexmax_apply_wake_boost(struct cpufreq_policy *policy,
                                                struct vortexmax_cpu_info *info,
                                                u64 now_ns)
{
    u64 elapsed_us;
    unsigned int boost_freq;
    
    if (!wake_boost_enabled || info->last_wake_time == 0)
        return 0; /* Disabled or no wakeup event */
    
    elapsed_us = (now_ns - info->last_wake_time) / 1000ULL;
    
    if (elapsed_us < (wake_boost_duration_ms * 1000ULL)) {
        boost_freq = (policy->max * wake_boost_freq_pct) / 100;
        if (boost_freq < policy->cur)
            boost_freq = policy->cur;
        return boost_freq;
    }
    
    return 0; /* Wake boost expired */
}

/* ---------------------------------------------------------------------
 * VORTEXMAX FREQUENCY DECISION ENGINE
 * Core algorithms for intelligent frequency selection
 * --------------------------------------------------------------------- */

/**
 * vortexmax_get_frequency_floor() - Calculate dynamic minimum frequency
 * @policy: Target CPU frequency policy
 * @info: Per-CPU state information
 * 
 * Adapts minimum frequency based on detected usage pattern
 * Prevents frequency from dropping too low during active use
 * 
 * Returns: Calculated floor frequency in kHz
 */
static unsigned int vortexmax_get_frequency_floor(struct cpufreq_policy *policy,
                                                   struct vortexmax_cpu_info *info)
{
    unsigned int floor_pct;
    
    if (info->is_gaming_mode)
        floor_pct = freq_floor_gaming_pct;   /* Higher floor for gaming */
    else if (info->is_active)
        floor_pct = freq_floor_active_pct;    /* Medium floor for active */
    else
        floor_pct = freq_floor_idle_pct;      /* Low floor when idle */
    
    return (policy->max * floor_pct) / 100;
}

/**
 * vortexmax_apply_hysteresis() - Prevent rapid frequency oscillation
 * @requested: Requested target frequency
 * @current: Current frequency
 * @min_freq: Policy minimum frequency
 * @going_up: Direction flag (true=increase, false=decrease)
 * 
 * Only allows frequency change if outside dead zone band
 * Eliminates jitter from rapid up/down cycling near threshold
 * 
 * Returns: Frequency after hysteresis filtering
 */
static unsigned int vortexmax_apply_hysteresis(unsigned int requested,
                                                unsigned int current,
                                                unsigned int min_freq,
                                                bool going_up)
{
    unsigned int band_pct, band, lower, upper;
    
    band_pct = going_up ? hysteresis_up_pct : hysteresis_down_pct;
    band = (current * band_pct) / 100;
    
    lower = (current > band) ? (current - band) : min_freq;
    upper = current + band;
    
    if (requested >= lower && requested <= upper)
        return current; /* Stay put - within hysteresis dead zone */
    
    return requested; /* Outside zone - allow change */
}

/**
 * vortexmax_smart_ramp_up() - Aggressive but controlled frequency increase
 * @current: Current frequency
 * @target_max: Maximum available frequency
 * @load: Current CPU load
 * @trend: Load direction (-1, 0, or 1)
 * 
 * Implements momentum-based acceleration:
 * - Base step from configurable parameter
 * - 1.5x speedup if load increasing (momentum)
 * - Additional 1.5x for extreme load (>90%)
 * 
 * Returns: New target frequency (capped at target_max)
 */
static unsigned int vortexmax_smart_ramp_up(unsigned int current,
                                             unsigned int target_max,
                                             unsigned int load,
                                             int trend)
{
    unsigned int step, new_freq;
    
    /* Calculate base step size from parameter */
    step = (target_max * ramp_up_step_pct) / 100;
    
    /* Apply momentum multiplier based on trend direction */
    if (ramp_up_momentum && trend == 1) {
        /* Load is increasing - accelerate ramp up by 1.5x */
        step += step / 2;
    }
    
    /* Extra aggression for very high load scenarios */
    if (load >= 90) {
        step += step / 2; /* Another 1.5x multiplier for extreme load */
    }
    
    new_freq = current + step;
    
    /* Cap at maximum available frequency */
    return (new_freq > target_max) ? target_max : new_freq;
}

/**
 * vortexmax_smooth_decay() - Gradual frequency reduction
 * @current: Current frequency
 * @floor: Minimum allowed frequency (floor)
 * @info: Per-CPU state information
 * 
 * Implements variable-rate decay:
 * - 3x slower in gaming mode (prevents stutter)
 * - 2x slower during active use
 * - Normal rate when idle
 * 
 * Returns: New decayed frequency (floor minimum)
 */
static unsigned int vortexmax_smooth_decay(unsigned int current,
                                            unsigned int floor,
                                            struct vortexmax_cpu_info *info)
{
    unsigned int diff, step;
    
    if (current <= floor)
        return floor; /* Already at or below floor */
    
    diff = current - floor;
    
    /* Base decay step calculation */
    step = max(floor / 100, diff / 35);
    
    /* Apply mode-based decay rate modifier */
    if (info->is_gaming_mode) {
        step = max(1, step / 3);  /* 3x slower - anti-stutter for gaming */
    } else if (info->is_active) {
        step = max(1, step / 2);  /* 2x smoother for active use */
    }
    
    return (current > floor + step) ? current - step : floor;
}

/**
 * vortexmax_apply_transition_limit() - Rate-limit frequency changes
 * @requested: Requested target frequency
 * @current: Current frequency
 * @sample_ms: Sampling interval in milliseconds
 * 
 * Prevents too-fast transitions that can cause instability
 * Enforces maximum kHz change per millisecond limit
 * 
 * Returns: Rate-limited frequency value
 */
static unsigned int vortexmax_apply_transition_limit(unsigned int requested,
                                                       unsigned int current,
                                                       unsigned int sample_ms)
{
    unsigned int max_delta;
    
    if (!transition_smooth_enabled)
        return requested; /* Feature disabled - pass through */
    
    /* Calculate maximum allowed change for this sampling period */
    max_delta = max_freq_change_per_ms * sample_ms;
    
    if (requested > current) {
        /* Frequency increasing */
        return ((requested - current) <= max_delta) ? 
               requested : (current + max_delta);
    } else {
        /* Frequency decreasing */
        return ((current - requested) <= max_delta) ? 
               requested : (current - max_delta);
    }
}

/**
 * vortexmax_apply_thermal_guard() - Intelligent thermal management
 * @freq_target: Proposed target frequency
 * @thermal_max: Maximum available frequency (may be thermally limited)
 * @info: Per-CPU state information
 * 
 * Multi-stage thermal protection:
 * 1. Soft limit at configurable % of max
 * 2. Accumulates counter when above soft limit
 * 3. Takes action only after sustained exceedance
 * 4. Two modes: hard cap or gradual reduction
 * 
 * Returns: Thermally-adjusted frequency
 */
static unsigned int vortexmax_apply_thermal_guard(unsigned int freq_target,
                                                   unsigned int thermal_max,
                                                   struct vortexmax_cpu_info *info)
{
    unsigned int soft_limit = (thermal_max * thermal_limit_pct) / 100;
    
    if (freq_target <= soft_limit) {
        /* Below soft limit - recover thermal counter */
        if (info->thermal_counter > 0)
            info->thermal_counter -= 2; /* Decay faster than increment */
        return freq_target; /* Pass through unchanged */
    }
    
    /* Above soft limit - accumulate thermal stress */
    info->thermal_counter++;
    
    if (info->thermal_counter < thermal_counter_threshold) {
        /* Temporary excursion allowed - tolerate short bursts */
        return freq_target;
    }
    
    /* Sustained above limit - take protective action */
    if (thermal_hard_limit) {
        /* Hard cap mode: strict frequency ceiling */
        return soft_limit;
    } else {
        /* Soft reduction mode: gradual pull-down toward safe level */
        unsigned int excess = freq_target - soft_limit;
        unsigned int reduction = excess / 2; /* Reduce by half the excess */
        
        /* Ensure minimum step size (at least 2% of max) */
        if (reduction < (thermal_max / 50))
            reduction = thermal_max / 50;
            
        return freq_target - reduction;
    }
}

/* ---------------------------------------------------------------------
 * VORTEXMAIN EVALUATION FUNCTION - The Brain of VortexMax
 * Main decision engine called every sampling interval
 * Implements priority-based boost system + normal scaling logic
 * --------------------------------------------------------------------- */
static void vortexmax_eval_freq(struct cpufreq_policy *policy)
{
    struct vortexmax_cpu_info *info = &per_cpu(vortexmax_info, policy->cpu);
    u64 now_ns, idle_time, delta_wall, delta_idle;
    unsigned int raw_load, load, freq_target, current_freq;
    unsigned int thermal_max, freq_floor;
    unsigned int touch_val = 0, io_val = 0, wake_val = 0;
    int trend;
    bool freq_increasing;

    /* Get current timestamps and frequencies */
    now_ns = local_clock();
    idle_time = get_cpu_idle_time(policy->cpu, &delta_wall, 0);
    current_freq = policy->cur;
    thermal_max = policy->max;

    /* ---- FIRST RUN INITIALIZATION ---- */
    if (info->prev_cpu_wall == 0) {
        /* Initialize all state variables on first execution */
        info->prev_cpu_wall = now_ns;
        info->prev_cpu_idle = idle_time;
        info->target_freq = current_freq;
        info->next_delay_ms = sample_rate_normal_ms;
        info->last_touch_time = 0;
        info->last_io_time = 0;
        info->last_wake_time = now_ns; /* Treat init as wake event */
        info->history_idx = 0;
        info->avg_load_ema = 0;
        info->prev_load = 0;
        info->load_trend = 0;
        info->is_gaming_mode = false;
        info->is_active = false;
        info->consecutive_high_load = 0;
        info->consecutive_low_load = 0;
        info->io_burst_counter = 0;
        info->io_burst_active = false;
        memset(info->load_history, 0, sizeof(info->load_history));
        return; /* Skip evaluation until next cycle */
    }

    /* ---- LOAD CALCULATION ---- */
    delta_wall = now_ns - info->prev_cpu_wall;
    delta_idle = idle_time - info->prev_cpu_idle;
    info->prev_cpu_wall = now_ns;
    info->prev_cpu_idle = idle_time;

    /* Compute raw CPU load as percentage */
    if (delta_wall == 0 || delta_idle > delta_wall)
        raw_load = 0;
    else
        raw_load = div64_u64(100 * (delta_wall - delta_idle), delta_wall);

    /* Apply EMA smoothing for stable load reading */
    vortexmax_update_load_history(info, raw_load);
    load = vortexmax_calculate_ema(info, raw_load);
    
    /* Detect load trend direction (increasing/decreasing/stable) */
    trend = vortexmax_detect_load_trend(info);
    info->load_trend = trend;
    
    /* Run gaming pattern recognition state machine */
    vortexmax_detect_gaming_pattern(info, load);

    /* Determine core type (big vs LITTLE) for threshold selection */
    bool is_big = (policy->cpuinfo.max_freq > (policy->cpuinfo.min_freq * 2));
    unsigned int dyn_target = is_big ? target_load_big : target_load_little;
    
    /* Get dynamic frequency floor based on usage mode */
    freq_floor = vortexmax_get_frequency_floor(policy, info);

    /* ================================================================
     * PRIORITY-BASED DECISION MATRIX
     * Higher priority boosts override lower priority calculations
     * This ensures responsive user experience
     * ================================================================ */
    
    /* 1. TOUCH BOOST (Highest Priority - User Interaction)
     * Activated on touchscreen press-down events
     * Provides immediate response to user input */
    touch_val = vortexmax_apply_touch_boost(policy, info, now_ns);
    if (touch_val > 0) {
        freq_target = touch_val;
        goto apply_final; /* Skip normal scaling - boost takes precedence */
    }

    /* 2. WAKE BOOST (Second Priority - Just woke from idle)
     * Prevents sluggish response after CPU deep sleep exit */
    wake_val = vortexmax_apply_wake_boost(policy, info, now_ns);
    if (wake_val > 0) {
        freq_target = wake_val;
        goto apply_final;
    }

    /* 3. IO BOOST (Third Priority - Detected IO burst)
     * Auto-activated on storage/network load spikes */
    io_val = vortexmax_apply_io_boost(policy, info, now_ns, load);
    if (io_val > 0) {
        freq_target = io_val;
        goto apply_final;
    }

    /* 4. NORMAL SCALING LOGIC (No boost active) */
    if (load >= fast_ramp_up_load) {
        /* === CRITICAL LOAD - Maximum Response === */
        freq_target = thermal_max; /* Jump to maximum */
        
        /* Activate anti-parachute hold */
        if (info->max_hold_counter < max_hold_cycles)
            info->max_hold_counter++;

    } else if (load > dyn_target) {
        /* === ELEVATED LOAD - Smart Ramp Up === */
        freq_target = vortexmax_smart_ramp_up(current_freq, thermal_max, load, trend);
        
        /* Partial hold for elevated load scenario */
        if (info->max_hold_counter < (max_hold_cycles / 2))
            info->max_hold_counter++;

    } else if (info->max_hold_counter > 0) {
        /* === SUSTAIN MODE - Anti-Parachute === */
        if (load > (dyn_target - 15)) {
            /* Still moderate load - hold steady */
            freq_target = current_freq;
            info->max_hold_counter--;
        } else {
            /* Low load - begin smooth decay toward floor */
            freq_target = vortexmax_smooth_decay(current_freq, freq_floor, info);
            info->max_hold_counter--;
        }
    } else {
        /* === NORMAL DECAY MODE === */
        freq_target = vortexmax_smooth_decay(current_freq, freq_floor, info);
    }

apply_final:

    /* ---- FREQUENCY GUARD RAIL APPLICATION ---- */
    
    /* Enforce minimum frequency floor */
    if (freq_target < freq_floor)
        freq_target = freq_floor;

    /* Apply thermal guard (prevent overheating) */
    freq_target = vortexmax_apply_thermal_guard(freq_target, thermal_max, info);
    
    /* Determine direction for hysteresis calculation */
    freq_increasing = (freq_target > current_freq);
    
    /* Apply hysteresis band (prevent oscillation) */
    freq_target = vortexmax_apply_hysteresis(freq_target, current_freq, 
                                              policy->min, freq_increasing);

    /* Apply transition rate limiter (smooth changes) */
    freq_target = vortexmax_apply_transition_limit(freq_target, current_freq,
                                                    info->next_delay_ms);

    /* Hard clamps to policy limits */
    if (freq_target > thermal_max)
        freq_target = thermal_max;
    if (freq_target < policy->min)
        freq_target = policy->min;

    /* Apply frequency change to hardware */
    if (freq_target != info->target_freq) {
        info->target_freq = freq_target;
        __cpufreq_driver_target(policy, freq_target, CPUFREQ_RELATION_L);
    }

    /* ---- ADAPTIVE SAMPLING RATE SELECTION ---- */
    /* Faster sampling during boost/high-load, slower when idle */
    if (touch_val > 0 || wake_val > 0) {
        info->next_delay_ms = sample_rate_boost_ms;      /* 4ms - Ultra fast */
    } else if (io_val > 0 || load >= fast_ramp_up_load) {
        info->next_delay_ms = sample_rate_boost_ms;       /* 4ms - Fast */
    } else if (info->is_gaming_mode || load > dyn_target) {
        info->next_delay_ms = sample_rate_active_ms;      /* 8ms - Medium */
    } else if (info->is_active || load > 40) {
        info->next_delay_ms = sample_rate_normal_ms;      /* 16ms - Normal */
    } else {
        info->next_delay_ms = sample_rate_idle_ms;        /* 24ms - Power saving */
    }
}


/* ---------------------------------------------------------------------
 * VORTEXMAX INPUT SUBSYSTEM INTEGRATION - Touch Boost Handler
 * Pure kernel space implementation - no userspace involvement
 * Hooks into Linux input subsystem for zero-latency touch detection
 * --------------------------------------------------------------------- */

/**
 * vortexmax_do_touch_boost() - Work queue handler for touch boost
 * @work: Work structure (unused)
 * 
 * Updates touch timestamp on ALL CPUs for synchronous cross-core boost
 * Called from process context by workqueue subsystem
 */
static void vortexmax_do_touch_boost(struct work_struct *work)
{
    unsigned int cpu;
    struct vortexmax_cpu_info *info;
    
    /* Broadcast touch event to all CPU cores */
    for_each_possible_cpu(cpu) {
        info = &per_cpu(vortexmax_info, cpu);
        info->last_touch_time = local_clock(); /* Record touch timestamp */
        info->is_active = true; /* Mark as active usage */
        info->consecutive_low_load = 0; /* Reset idle counter */
    }
    
    vortexmax_touch_pending = false; /* Clear pending flag */
    /* Memory barriers handled implicitly by workqueue */
}

/**
 * vortexmax_input_event() - Callback for input events from kernel
 * @handle: Input handle structure
 * @type: Input event type (EV_KEY, EV_ABS, etc.)
 * @code: Input event code (BTN_TOUCH, BTN_MOUSE, etc.)
 * @value: Event value (1=press, 0=release)
 * 
 * Registered with kernel input subsystem
 * Filters for touch/mouse press-down events only
 * 
 * Returns: false (don't consume event - let other handlers see it too)
 */
static bool vortexmax_input_event(struct input_handle *handle,
                                   unsigned int type,
                                   unsigned int code,
                                   int value)
{
    /* We only care about key events (touch/mouse button presses) */
    if (type == EV_KEY) {
        /* Check for relevant input device types */
        if (code == BTN_TOUCH || code == BTN_MOUSE || 
            code == BTN_TOOL_PEN || code == BTN_TOOL_FINGER ||
            code == BTN_STYLUS || code == BTN_STYLUS2) {
            
            if (value == 1) {  /* Press down event (not release) */
                if (!vortexmax_touch_pending) {
                    vortexmax_touch_pending = true;
                    schedule_work(&vortexmax_touch_boost_work);
                }
            }
        }
    }
    
    return false; /* Don't consume the event - allow other handlers to process */
}

/**
 * vortexmax_input_connect() - Connect to compatible input devices
 * @handler: Input handler requesting connection
 * @dev: Input device candidate
 * @id: Matching device ID
 * 
 * Only connects to touchscreens, mice, and stylus devices
 * Rejects keyboards, gamepads, and other irrelevant devices
 * 
 * Returns: 0 on success, negative error on failure
 */
static int vortexmax_input_connect(struct input_handler *handler,
                                    struct input_dev *dev,
                                    const struct input_device_id *id)
{
    struct input_handle *handle;
    int err;
    
    /* Device capability check: must have EV_KEY + touch/mouse/pen buttons */
    if (!(test_bit(EV_KEY, dev->evbit) &&
          (test_bit(BTN_TOUCH, dev->keybit) || 
           test_bit(BTN_MOUSE, dev->keybit) ||
           test_bit(BTN_TOOL_PEN, dev->keybit) ||
           test_bit(BTN_TOOL_FINGER, dev->keybit) ||
           test_bit(BTN_STYLUS, dev->keybit))))
        return -ENODEV; /* Not a device we care about */
    
    /* Allocate handle structure for this connection */
    handle = kzalloc(sizeof(*handle), GFP_KERNEL);
    if (!handle)
        return -ENOMEM; /* Out of memory */
    
    handle->dev = dev;
    handle->handler = handler;
    handle->name = "vortexmax";
    
    /* Register handle with input subsystem */
    err = input_register_handle(handle);
    if (err)
        goto err_free;
    
    /* Open device to start receiving events */
    err = input_open_device(handle);
    if (err)
        goto err_unreg;
    
    return 0; /* Success */
    
err_unreg:
    input_unregister_handle(handle);
err_free:
    kfree(handle);
    return err;
}

/**
 * vortexmax_input_disconnect() - Clean up on device disconnect
 * @handle: Handle being disconnected
 */
static void vortexmax_input_disconnect(struct input_handle *handle)
{
    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
}

/* Input device ID matching table - which devices we connect to */
static const struct input_device_id vortexmax_ids[] = {
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_KEYBIT,
        .evbit = { BIT_MASK(EV_KEY) },
        .keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
    },
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_KEYBIT,
        .evbit = { BIT_MASK(EV_KEY) },
        .keybit = { [BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_MOUSE) },
    },
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_KEYBIT,
        .evbit = { BIT_MASK(EV_KEY) },
        .keybit = { [BIT_WORD(BTN_TOOL_PEN)] = BIT_MASK(BTN_TOOL_PEN) },
    },
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_KEYBIT,
        .evbit = { BIT_MASK(EV_KEY) },
        .keybit = { [BIT_WORD(BTN_TOOL_FINGER)] = BIT_MASK(BTN_TOOL_FINGER) },
    },
    { },  /* Terminating zero entry - MUST be last */
};

/* Input handler structure - registers us with kernel input subsystem */
static struct input_handler vortexmax_input_handler = {
    .event        = vortexmax_input_event,
    .connect      = vortexmax_input_connect,
    .disconnect   = vortexmax_input_disconnect,
    .name         = "vortexmax",
    .id_table     = vortexmax_ids,
};

/* ---------------------------------------------------------------------
 * VORTEXMAX THERMAL NOTIFIER INTEGRATION
 * Optional: Reacts to kernel thermal events (if CONFIG_THERMAL enabled)
 * Pre-emptively informs governors when temperature rising
 * --------------------------------------------------------------------- */
#ifdef CONFIG_THERMAL

/**
 * vortexmax_thermal_notify() - Callback for thermal zone events
 * @nb: Notifier block (standard kernel notifier API)
 * @event: Thermal event type
 * @data: Pointer to thermal_zone_device
 * 
 * Monitors CPU and SoC thermal zones only
 * Pre-increments thermal counter when approaching passive limit
 * Allows governor to reduce frequency before hard throttling kicks in
 * 
 * Returns: NOTIFY_OK if handled, NOTIFY_DONE if ignored
 */
static int vortexmax_thermal_notify(struct notifier_block *nb,
                                     unsigned long event, void *data)
{
    struct thermal_zone_device *tz = data;
    int temp;
    
    /* We only care about temperature sampling events */
    if (event != THERMAL_EVENT_TEMP_SAMPLING)
        return NOTIFY_DONE;
    
    if (!tz)
        return NOTIFY_DONE;
    
    /* Filter: only react to CPU and SoC thermal zones */
    if (strncmp(tz->type, "cpu-", 4) != 0 &&
        strncmp(tz->type, "soc_thermal", 11) != 0)
        return NOTIFY_DONE;
    
    /* Get current zone temperature */
    temp = thermal_zone_get_temp(tz);
    if (temp <= 0 || temp == THERMAL_TEMP_INVALID)
        return NOTIFY_DONE; /* Invalid reading */
    
    /* If approaching passive threshold, pre-emptively warn all CPUs */
    if (temp > (tz->passive * 1000)) { /* passive is in millidegrees Celsius */
        unsigned int cpu;
        struct vortexmax_cpu_info *info;
        
        for_each_possible_cpu(cpu) {
            info = &per_cpu(vortexmax_info, cpu);
            /* Bump thermal counter to trigger guard sooner */
            if (info->thermal_counter < thermal_counter_threshold)
                info->thermal_counter += 2;
        }
    }
    
    return NOTIFY_OK; /* Event handled successfully */
}
#endif /* CONFIG_THERMAL */

/* ---------------------------------------------------------------------
 * VORTEXMAX WORK QUEUE HANDLER
 * Kernel delayed_work callback for main sampling loop
 * --------------------------------------------------------------------- */
static void vortexmax_work_handler(struct work_struct *work)
{
    struct vortexmax_policy_info *vpinfo = 
        container_of(work, struct vortexmax_policy_info, work.work);
    struct cpufreq_policy *policy = vpinfo->policy;

    /* Run the main frequency evaluation algorithm */
    vortexmax_eval_freq(policy);
    
    /* Reschedule with adaptive delay based on current state */
    schedule_delayed_work_on(
        policy->cpu, 
        &vpinfo->work, 
        msecs_to_jiffies(per_cpu(vortexmax_info, policy->cpu).next_delay_ms)
    );
}

/* ---------------------------------------------------------------------
 * VORTEXMAX GKI GOVERNOR API STRUCT
 * Standard GKI governor interface implementation
 * Compatible with Android GKI 5.10 / 6.1 / 6.6
 * --------------------------------------------------------------------- */

/**
 * vortexmax_init() - Initialize governor for a policy
 * @policy: cpufreq policy to initialize
 * 
 * Allocates per-policy private data structure
 * Sets up delayed work for sampling loop
 * 
 * Returns: 0 on success, negative error on failure
 */
static int vortexmax_init(struct cpufreq_policy *policy)
{
    struct vortexmax_policy_info *vpinfo;
    
    /* Allocate policy-private data */
    vpinfo = kzalloc(sizeof(*vpinfo), GFP_KERNEL);
    if (!vpinfo)
        return -ENOMEM; /* Allocation failed */
    
    vpinfo->policy = policy;
    INIT_DEFERRABLE_WORK(&vpinfo->work, vortexmax_work_handler);
    policy->governor_data = vpinfo;
    
    return 0;
}

/**
 * vortexmax_exit() - Clean up governor for a policy
 * @policy: cpufreq policy being cleaned up
 * 
 * Cancels pending work and frees allocated memory
 */
static void vortexmax_exit(struct cpufreq_policy *policy)
{
    struct vortexmax_policy_info *vpinfo = policy->governor_data;
    
    if (vpinfo) {
        cancel_delayed_work_sync(&vpinfo->work); /* Wait for completion */
        kfree(vpinfo); /* Free memory */
        policy->governor_data = NULL;
    }
}

/**
 * vortexmax_start() - Start governor on a policy
 * @policy: cpufreq policy to start governing
 * 
 * Initializes per-CPU state for all CPUs in policy
 * Registers global subsystems (input handler, thermal notifier)
 * Starts main sampling loop
 * 
 * Returns: 0 on success
 */
static int vortexmax_start(struct cpufreq_policy *policy)
{
    unsigned int cpu;
    struct vortexmax_cpu_info *info;
    
    /* Initialize per-CPU state for all CPUs managed by this policy */
    for_each_cpu(cpu, policy->cpus) {
        info = &per_cpu(vortexmax_info, cpu);
        
        /* Capture initial idle/wall times for first load calculation */
        info->prev_cpu_idle = get_cpu_idle_time(cpu, &info->prev_cpu_wall, 0);
        info->target_freq = policy->cur;
        info->thermal_counter = 0;
        info->max_hold_counter = 0;
        info->next_delay_ms = sample_rate_normal_ms;
        info->last_touch_time = 0;
        info->last_io_time = 0;
        info->last_wake_time = local_clock(); /* Treat startup as wake event */
        info->history_idx = 0;
        info->avg_load_ema = 0;
        info->prev_load = 0;
        info->load_trend = 0;
        info->trend_counter = 0;
        info->is_gaming_mode = false;
        info->is_active = false;
        info->consecutive_high_load = 0;
        info->consecutive_low_load = 0;
        info->io_burst_counter = 0;
        info->io_burst_active = false;
        memset(info->load_history, 0, sizeof(info->load_history));
    }
    
    /* Register global subsystems (only once, regardless of CPU count) */
    spin_lock(&vortexmax_lock);
    if (!vortexmax_initialized) {
        /* Initialize touch boost workqueue handler */
        INIT_WORK(&vortexmax_touch_boost_work, vortexmax_do_touch_boost);
        
        /* Register input handler for touch boost (kernel input subsys) */
        input_register_handler(&vortexmax_input_handler);
        
#ifdef CONFIG_THERMAL
        /* Register thermal notification callback */
        vortexmax_thermal_notifier.notifier_call = vortexmax_thermal_notify;
        register_thermal_notifier(&vortexmax_thermal_notifier);
#endif
        
        vortexmax_initialized = true; /* Mark as initialized */
    }
    spin_unlock(&vortexmax_lock);
    
    atomic_inc(&vortexmax_active_governors); /* Track active instances */
    
    /* Start the main governor sampling loop */
    schedule_delayed_work_on(
        policy->cpu,
        &((struct vortexmax_policy_info *)policy->governor_data)->work,
        msecs_to_jiffies(sample_rate_normal_ms)
    );
    
    return 0;
}

/**
 * vortexmax_stop() - Stop governor on a policy
 * @policy: cpufreq policy to stop governing
 * 
 * Stops sampling loop
 * Cleans up global resources if last instance stopping
 */
static void vortexmax_stop(struct cpufreq_policy *policy)
{
    /* Cancel the sampling loop for this policy */
    cancel_delayed_work_sync(
        &((struct vortexmax_policy_info *)policy->governor_data)->work
    );
    
    /* If this was the last active governor instance, clean up globals */
    if (atomic_dec_and_test(&vortexmax_active_governors)) {
        spin_lock(&vortexmax_lock);
        
        /* Cancel any pending touch boost work */
        cancel_work_sync(&vortexmax_touch_boost_work);
        
        /* Unregister from input subsystem */
        input_unregister_handler(&vortexmax_input_handler);
        
#ifdef CONFIG_THERMAL
        /* Unregister thermal notification */
        unregister_thermal_notifier(&vortexmax_thermal_notifier);
#endif
        
        vortexmax_initialized = false; /* Reset init flag */
        spin_unlock(&vortexmax_lock);
    }
}

/**
 * vortexmax_limits() - Handle policy limit changes
 * @policy: cpufreq policy whose limits changed
 * 
 * Re-evaluates frequency when constraints change
 * (e.g., thermal throttling adjusts max frequency)
 */
static void vortexmax_limits(struct cpufreq_policy *policy)
{
    vortexmax_eval_freq(policy);
}

/* ---------------------------------------------------------------------
 * VORTEXMAX GOVERNOR REGISTRATION STRUCT
 * Defines "vortexmax" as available cpufreq governor name
 * --------------------------------------------------------------------- */
static struct cpufreq_governor vortexmax_gov = {
    .name           = "vortexmax",    /* Governor name #2 */
    .owner          = THIS_MODULE,
    .init           = vortexmax_init,
    .exit           = vortexmax_exit,
    .start          = vortexmax_start,
    .stop           = vortexmax_stop,
    .limits         = vortexmax_limits,
};


/* #############################################################################
 * #                                                                           #
 * #  MODULE INITIALIZATION - REGISTER BOTH GOVERNORS SIMULTANEOUSLY          #
 * #                                                                           #
 * ############################################################################# */

/**
 * vortex_dual_module_init() - Module entry point
 * 
 * Registers both "vortexcore" and "vortexmax" governors with kernel
 * Called when module is loaded (insmod / built-in)
 * 
 * Returns: 0 on success, error code if both registrations fail
 */
static int __init vortex_dual_module_init(void)
{
    int ret_core, ret_max;
    
    /* Register Governor 1: vortexcore (Battery Saver / Daily Use) */
    ret_core = cpufreq_register_governor(&vortexcore_gov);
    if (ret_core) {
        pr_err("VortexCore: Registration failed (%d)\n", ret_core);
    } else {
        pr_info("VortexCore v3.2 registered successfully\n");
        pr_info("  -> Type: Lightweight / Battery Saver\n");
        pr_info("  -> Features: Anti-Parachute, Fast Ramp-up, Proactive Thermal\n");
    }
    
    /* Register Governor 2: vortexmax (Gaming / High Performance) */
    ret_max = cpufreq_register_governor(&vortexmax_gov);
    if (ret_max) {
        pr_err("VortexMax: Registration failed (%d)\n", ret_max);
    } else {
        pr_info("VortexMax v3.0 registered successfully\n");
        pr_info("  -> Type: Full-Featured / Gaming\n");
        pr_info("  -> Features: TouchBoost, IOBoost, WakeBoost, SmartRamp\n");
        pr_info("  ->          GamingAI, ThermalGuard, Hysteresis, EMA\n");
    }
    
    /* Print module banner to kernel log */
    pr_info("╔══════════════════════════════════════════╗\n");
    pr_info("║  Vortex Dual-Governor System v4.0 Loaded  ║\n");
    pr_info("╠══════════════════════════════════════════╣\n");
    pr_info("║  Available Governors:                      ║\n");
    pr_info("║  • vortexcore  (Daily use / Battery save)  ║\n");
    pr_info("║  • vortexmax   (Gaming / High perf)        ║\n");
    pr_info("╠══════════════════════════════════════════╣\n");
    pr_info("║  Select via: FKM / Kernel Adiutor / TWRP   ║\n");
    pr_info("╚══════════════════════════════════════════╝\n");
    pr_info("Author: Kingfinik98 <kingfinix98@gmail.com>\n");
    
    /* Return success if at least one governor registered successfully */
    return (ret_core && ret_max) ? ret_max : 0;
}

/**
 * vortex_dual_module_exit() - Module cleanup
 * 
 * Unregisters both governors and cleans up global resources
 * Called when module is removed (rmmod / shutdown)
 */
static void __exit vortex_dual_module_exit(void)
{
    /* Safety cleanup: ensure VortexMax global resources are freed */
    if (vortexmax_initialized) {
        cancel_work_sync(&vortexmax_touch_boost_work);
        input_unregister_handler(&vortexmax_input_handler);
#ifdef CONFIG_THERMAL
        unregister_thermal_notifier(&vortexmax_thermal_notifier);
#endif
        vortexmax_initialized = false;
    }
    
    /* Unregister both governors from kernel */
    cpufreq_unregister_governor(&vortexcore_gov);
    cpufreq_unregister_governor(&vortexmax_gov);
    
    pr_info("Vortex Dual-Governor System unloaded\n");
}

/* Module entry/exit points */
module_init(vortex_dual_module_init);
module_exit(vortex_dual_module_exit);


/* #############################################################################
 * #                                                                           #
 * #  MODULE METADATA                                                          #
 * #                                                                           #
 * ############################################################################# */

MODULE_AUTHOR("Kingfinik98 <kingfinix98@gmail.com>");
MODULE_DESCRIPTION("Vortex Dual-Governor: vortexcore (Battery) + vortexmax (Gaming)");
MODULE_LICENSE("GPL");
MODULE_VERSION("4.0-Dual");
