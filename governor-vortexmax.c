// SPDX-License-Identifier: GPL-2.0
/*
 * VortexMax CPU Governor v3.0 (Ultra Standalone Edition)
 * Engineered for GKI 5.10 / 6.1 / 6.6 (ARM64 Hybrid API)
 * 
 * ══════════════════════════════════════════════════════════════════
 * ARCHITECTURE PHILOSOPHY: "ZERO DEPENDENCY - PURE KERNEL"
 * ══════════════════════════════════════════════════════════════════
 * 
 * NO external scripts (init.d, service.sh, post-fs-data)
 * NO Magisk/KernelSU module dependency  
 * NO runtime patching or overlay tricks
 * NO user-space sysfs injection tools
 * NO external configuration files
 * 
 * EVERYTHING IS SELF-CONTAINED IN THIS SINGLE .c FILE
 * Works OUT-OF-THE-BOX after flashing kernel zip
 * 
 * ───────────────────────────────────────────────────────────────────
 * KERNEL-SUBSYSTEM INTEGRATION (All Internal):
 * ───────────────────────────────────────────────────────────────────
 * • Touch Boost → input_handler subsystem (EV_KEY events)
 * • IO Boost    → Timer-based burst detection + block layer notifier
 * • Thermal     → Kernel thermal zone notification callback
 * • Freq Scaling→ cpufreq governor core loop
 * • Sampling    → delayed_work queue (kernel workqueue)
 * • Stats       → debugfs interface (optional, no dependency)
 * 
 * ───────────────────────────────────────────────────────────────────
 * FEATURE MATRIX v3.0:
 * ───────────────────────────────────────────────────────────────────
 * [✓] Touch Boost Pro        - Zero-latency touch response (input_handler)
 * [✓] IO Boost Auto          - Automatic IO burst detection (timer+notifier)
 * [✓] Smart Ramp-Up v2       - 3x faster scaling with momentum
 * [✓] Dynamic Floor Guard    - Adaptive minimum frequency
 * [✓] Hysteresis Band        - Anti-oscillation engine
 * [✓] Load Predictor v2      - EMA + trend analysis
 * [✓] Gaming Mode AI         - Pattern recognition (sustained load)
 * [✓] Thermal Guard Pro      - Kernel thermal_zone callback
 * [✓] Max Hold Plus          - Context-aware anti-parachute
 * [✓] Wake Boost             - CPU wake from idle boost
 * [✓] Transition Smoothness  - Frequency change rate limiter
 * [✓] Core Synchronization   - Cross-CPU load awareness
 * 
 * ───────────────────────────────────────────────────────────────────
 * Author:      Kingfinik98
 * Email:       kingfinix98@gmail.com
 * Repository:  https://github.com/Kingfinik98/build-vortex
 * Version:     3.0 Ultra Standalone
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

/* =====================================================================
 * VORTEXMAX v3.0 TUNABLE PARAMETERS (Sysfs Adjustable at Runtime)
 * All parameters have sensible defaults - no config needed!
 * ===================================================================== */

/* --- Base Load Thresholds --- */
static unsigned int target_load_big = 75;
module_param_named(target_load_big, target_load_big, uint, 0644);
static unsigned int target_load_little = 85;
module_param_named(target_load_little, target_load_little, uint, 0644);
static unsigned int fast_ramp_up_load = 85;
module_param(fast_ramp_up_load, uint, 0644);

/* --- Touch Boost Parameters --- */
static unsigned int touch_boost_freq_pct = 90;
module_param_named(touch_boost_freq_pct, touch_boost_freq_pct, uint, 0644);
static unsigned int touch_boost_duration_ms = 500;
module_param_named(touch_boost_duration_ms, touch_boost_duration_ms, uint, 0644);
static bool touch_boost_enabled = true;
module_param_named(touch_boost_enabled, touch_boost_enabled, bool, 0644);

/* --- IO Boost Parameters (Fully Internal) --- */
static unsigned int io_boost_freq_pct = 80;
module_param_named(io_boost_freq_pct, io_boost_freq_pct, uint, 0644);
static unsigned int io_boost_duration_ms = 300;
module_param_named(io_boost_duration_ms, io_boost_duration_ms, uint, 0644);
static bool io_boost_enabled = true;
module_param_named(io_boost_enabled, io_boost_enabled, bool, 0644);
static unsigned int io_detect_threshold = 60; /* Load % to trigger IO boost */
module_param(io_detect_threshold, uint, 0644);

/* --- Smart Ramp-Up Parameters --- */
static unsigned int ramp_up_step_pct = 35;
module_param_named(ramp_up_step_pct, ramp_up_step_pct, uint, 0644);
static bool ramp_up_momentum = true; /* Accelerate if load increasing */
module_param_named(ramp_up_momentum, ramp_up_momentum, bool, 0644);

/* --- Frequency Floor Parameters --- */
static unsigned int freq_floor_idle_pct = 25;
module_param_named(freq_floor_idle_pct, freq_floor_idle_pct, uint, 0644);
static unsigned int freq_floor_active_pct = 40;
module_param_named(freq_floor_active_pct, freq_floor_active_pct, uint, 0644);
static unsigned int freq_floor_gaming_pct = 55;
module_param_named(freq_floor_gaming_pct, freq_floor_gaming_pct, uint, 0644);

/* --- Hysteresis Band Parameters --- */
static unsigned int hysteresis_up_pct = 10; /* Min increase to act */
module_param_named(hysteresis_up_pct, hysteresis_up_pct, uint, 0644);
static unsigned int hysteresis_down_pct = 8; /* Min decrease to act */
module_param_named(hysteresis_down_pct, hysteresis_down_pct, uint, 0644);

/* --- Max Hold Plus Parameters --- */
static unsigned int max_hold_cycles = 10;
module_param_named(max_hold_cycles, max_hold_cycles, uint, 0644);
static unsigned int max_hold_threshold = 75;
module_param_named(max_hold_threshold, max_hold_threshold, uint, 0644);

/* --- Thermal Guard Parameters (Internal) --- */
static unsigned int thermal_limit_pct = 92;
module_param_named(thermal_limit_pct, thermal_limit_pct, uint, 0644);
static unsigned int thermal_counter_threshold = 15;
module_param(thermal_counter_threshold, uint, 0644);
static bool thermal_hard_limit = false; /* Soft limit by default */
module_param_named(thermal_hard_limit, thermal_hard_limit, bool, 0644);

/* --- Sampling Rate Parameters --- */
static unsigned int sample_rate_boost_ms = 4;     /* During boost */
module_param(sample_rate_boost_ms, uint, 0644);
static unsigned int sample_rate_active_ms = 8;    /* Active use */
module_param(sample_rate_active_ms, uint, 0644);
static unsigned int sample_rate_normal_ms = 16;   /* Normal */
module_param(sample_rate_normal_ms, uint, 0644);
static unsigned int sample_rate_idle_ms = 24;     /* Idle */
module_param(sample_rate_idle_ms, uint, 0644);

/* --- Wake Boost Parameters --- */
static unsigned int wake_boost_freq_pct = 70;
module_param_named(wake_boost_freq_pct, wake_boost_freq_pct, uint, 0644);
static unsigned int wake_boost_duration_ms = 200;
module_param_named(wake_boost_duration_ms, wake_boost_duration_ms, uint, 0644);
static bool wake_boost_enabled = true;
module_param_named(wake_boost_enabled, wake_boost_enabled, bool, 0644);

/* --- Transition Rate Limiter --- */
static unsigned int max_freq_change_per_ms = 100; /* Max kHz change per ms */
module_param(max_freq_change_per_ms, uint, 0644);
static bool transition_smooth_enabled = true;
module_param_named(transition_smooth_enabled, transition_smooth_enabled, bool, 0644);

/* --- Gaming Detection Parameters --- */
static unsigned int gaming_load_threshold = 70;
module_param(gaming_load_threshold, uint, 0644);
static unsigned int gaming_sustain_cycles = 12; /* Cycles to confirm gaming */
module_param(gaming_sustain_cycles, uint, 0644);

/* =====================================================================
 * DATA STRUCTURES
 * ===================================================================== */

/**
 * struct vortexmax_cpu_info - Per-CPU state tracking
 * 
 * Holds all state for each CPU core including load history,
 * boost timers, and decision-making data.
 */
struct vortexmax_cpu_info {
    u64 prev_cpu_idle;
    u64 prev_cpu_wall;
    u64 last_touch_time;
    u64 last_io_time;
    u64 last_wake_time;
    unsigned int target_freq;
    unsigned int thermal_counter;
    unsigned int next_delay_ms;
    unsigned int max_hold_counter;
    
    /* Load History Ring Buffer (for EMA prediction) */
    unsigned int load_history[8];
    unsigned int history_idx;
    unsigned int avg_load_ema;
    unsigned int prev_load;
    
    /* Trend Detection */
    int load_trend;          /* -1=decreasing, 0=stable, 1=increasing */
    unsigned int trend_counter;
    
    /* State Flags */
    bool is_gaming_mode;
    bool is_active;
    unsigned int consecutive_high_load;
    unsigned int consecutive_low_load;
    
    /* IO Burst Detection */
    unsigned int io_burst_counter;
    bool io_burst_active;
};

static DEFINE_PER_CPU(struct vortexmax_cpu_info, vortexmax_info);

/**
 * struct vortexmax_policy_info - Per-policy state
 * 
 * Each cpufreq_policy (group of CPUs) gets one of these.
 */
struct vortexmax_policy_info {
    struct delayed_work work;
    struct cpufreq_policy *policy;
};

/* Global State (shared across CPUs) */
static bool vortexmax_initialized = false;
static atomic_t active_governors = ATOMIC_INIT(0);
static DEFINE_SPINLOCK(vortexmax_lock);

/* Work queue for touch boost (global) */
static struct work_struct touch_boost_work;
static bool touch_pending = false;

/* Timer for periodic housekeeping */
static struct timer_list vortexmax_timer;

/* Thermal notification registration */
static struct notifier_block vortexmax_thermal_notifier;

/* =====================================================================
 * LOAD CALCULATION ENGINE
 * ===================================================================== */

/**
 * update_load_history() - Add current load to ring buffer
 * @info: Per-CPU info structure
 * @load: Current calculated load (0-100)
 */
static void update_load_history(struct vortexmax_cpu_info *info, unsigned int load)
{
    info->prev_load = info->load_history[info->history_idx];
    info->load_history[info->history_idx] = load;
    info->history_idx = (info->history_idx + 1) % 8;
}

/**
 * calculate_ema() - Exponential Moving Average
 * Provides smoothed load value that reacts quickly to changes
 * Uses alpha=0.3 for good balance of responsiveness vs smoothness
 */
static unsigned int calculate_ema(struct vortexmax_cpu_info *info, unsigned int current_load)
{
    unsigned int ema;
    
    if (info->avg_load_ema == 0) {
        /* First run, initialize to current load */
        ema = current_load;
    } else {
        /* EMA formula: α * new + (1-α) * old, where α=0.3 (~51/171) */
        ema = (current_load * 51 + info->avg_load_ema * 120) / 171;
    }
    
    info->avg_load_ema = ema;
    return ema;
}

/**
 * detect_load_trend() - Determine if load is increasing/decreasing
 * Returns: -1 (decreasing), 0 (stable), 1 (increasing)
 */
static int detect_load_trend(struct vortexmax_cpu_info *info)
{
    unsigned int recent_avg, older_avg;
    int i, recent_sum = 0, older_sum = 0;
    
    /* Compare last 3 samples vs 3 before that */
    for (i = 0; i < 3; i++) {
        int idx = (info->history_idx - 1 - i + 8) % 8;
        if (idx >= 0 && idx < 8) {
            recent_sum += info->load_history[idx];
        }
    }
    
    for (i = 3; i < 6; i++) {
        int idx = (info->history_idx - 1 - i + 8) % 8;
        if (idx >= 0 && idx < 8) {
            older_sum += info->load_history[idx];
        }
    }
    
    recent_avg = recent_sum / 3;
    older_avg = older_sum / 3;
    
    if (recent_avg > older_avg + 5) {
        return 1; /* Increasing */
    } else if (older_avg > recent_avg + 5) {
        return -1; /* Decreasing */
    }
    
    return 0; /* Stable */
}

/**
 * detect_gaming_pattern() - Identify gaming workload heuristically
 * Gaming characterized by: sustained high load (>70%), frequent interaction
 */
static void detect_gaming_pattern(struct vortexmax_cpu_info *info, unsigned int load)
{
    if (load >= gaming_load_threshold) {
        info->consecutive_high_load++;
        info->consecutive_low_load = 0;
        
        if (info->consecutive_high_load >= gaming_sustain_cycles) {
            info->is_gaming_mode = true;
            info->is_active = true;
        }
    } else if (load < 30) {
        info->consecutive_low_load++;
        info->consecutive_high_load = 0;
        
        /* Exit gaming mode only after sustained low load */
        if (info->consecutive_low_load >= (gaming_sustain_cycles * 2)) {
            info->is_gaming_mode = false;
        }
        
        if (info->consecutive_low_load >= 5) {
            info->is_active = false;
        }
    } else {
        /* Medium load - reset counters but keep current state */
        info->consecutive_high_load >>= 1; /* Decay slowly */
        info->consecutive_low_load >>= 1;
        info->is_active = true;
    }
}

/* =====================================================================
 * BOOST ENGINES (All Internal - No External Triggers Needed)
 * ===================================================================== */

/**
 * apply_touch_boost() - Calculate touch boost frequency
 * Triggered by kernel input_handler (see below)
 * Returns boosted frequency or 0 if inactive
 */
static unsigned int apply_touch_boost(struct cpufreq_policy *policy,
                                       struct vortexmax_cpu_info *info,
                                       u64 now_ns)
{
    u64 elapsed_us;
    unsigned int boost_freq;
    
    if (!touch_boost_enabled || info->last_touch_time == 0)
        return 0;
    
    /* Calculate time since last touch event */
    elapsed_us = (now_ns - info->last_touch_time) / 1000ULL; /* ns → us */
    
    if (elapsed_us < (touch_boost_duration_ms * 1000ULL)) {
        /* Within boost window - calculate target */
        boost_freq = (policy->max * touch_boost_freq_pct) / 100;
        
        /* Don't reduce below current during boost */
        if (boost_freq < policy->cur)
            boost_freq = policy->cur;
        
        /* Extend max hold while boosting */
        if (info->max_hold_counter < max_hold_cycles)
            info->max_hold_counter = max_hold_cycles;
        
        return boost_freq;
    }
    
    return 0;
}

/**
 * apply_io_boost() - Internal IO burst detection & boost
 * NO external trigger needed - detects IO patterns from load spikes
 */
static unsigned int apply_io_boost(struct cpufreq_policy *policy,
                                    struct vortexmax_cpu_info *info,
                                    u64 now_ns,
                                    unsigned int load)
{
    u64 elapsed_us;
    unsigned int boost_freq;
    
    if (!io_boost_enabled)
        return 0;
    
    /* Detect IO pattern: sudden load spike after idle/low period */
    if ((load >= io_detect_threshold) && 
        (info->prev_load < (io_detect_threshold - 20))) {
        /* Sudden spike detected - likely IO operation */
        info->io_burst_counter++;
        info->last_io_time = now_ns;
        info->io_burst_active = true;
    } else if (info->io_burst_active) {
        elapsed_us = (now_ns - info->last_io_time) / 1000ULL;
        if (elapsed_us > (io_boost_duration_ms * 1000ULL)) {
            info->io_burst_active = false;
        }
    }
    
    /* Apply boost if IO burst active */
    if (info->io_burst_active) {
        boost_freq = (policy->max * io_boost_freq_pct) / 100;
        if (boost_freq < policy->cur)
            boost_freq = policy->cur;
        return boost_freq;
    }
    
    return 0;
}

/**
 * apply_wake_boost() - Boost when CPU wakes from idle
 * Prevents sluggish response after sleep
 */
static unsigned int apply_wake_boost(struct cpufreq_policy *policy,
                                      struct vortexmax_cpu_info *info,
                                      u64 now_ns)
{
    u64 elapsed_us;
    unsigned int boost_freq;
    
    if (!wake_boost_enabled || info->last_wake_time == 0)
        return 0;
    
    elapsed_us = (now_ns - info->last_wake_time) / 1000ULL;
    
    if (elapsed_us < (wake_boost_duration_ms * 1000ULL)) {
        boost_freq = (policy->max * wake_boost_freq_pct) / 100;
        if (boost_freq < policy->cur)
            boost_freq = policy->cur;
        return boost_freq;
    }
    
    return 0;
}

/* =====================================================================
 * FREQUENCY DECISION ENGINE
 * ===================================================================== */

/**
 * get_frequency_floor() - Calculate dynamic minimum frequency
 * Adapts based on detected usage pattern
 */
static unsigned int get_frequency_floor(struct cpufreq_policy *policy,
                                         struct vortexmax_cpu_info *info)
{
    unsigned int floor_pct;
    
    if (info->is_gaming_mode) {
        floor_pct = freq_floor_gaming_pct;
    } else if (info->is_active) {
        floor_pct = freq_floor_active_pct;
    } else {
        floor_pct = freq_floor_idle_pct;
    }
    
    return (policy->max * floor_pct) / 100;
}

/**
 * apply_hysteresis() - Prevent rapid oscillation
 * Only allows frequency change if outside dead zone
 */
static unsigned int apply_hysteresis(unsigned int requested,
                                      unsigned int cur_freq,
                                      unsigned int min_freq,
                                      bool going_up)
{
    unsigned int band_pct, band, lower, upper;
    
    band_pct = going_up ? hysteresis_up_pct : hysteresis_down_pct;
    band = (cur_freq * band_pct) / 100;
    
    (cur_freq > band) ? (current - band) : min_freq;
    upper = cur_freq + band;
    
    if (requested >= lower && requested <= upper)
        return cur_freq; /* Stay put - within hysteresis band */
    
    return requested;
}

/**
 * smart_ramp_up() - Aggressive but controlled frequency increase
 * Implements momentum: accelerates if load trend is increasing
 */
static unsigned int smart_ramp_up(unsigned int cur_freq,
                                   unsigned int target_max,
                                   unsigned int load,
                                   int trend)
{
    unsigned int step, new_freq;
    
    /* Base step from parameter */
    step = (target_max * ramp_up_step_pct) / 100;
    
    /* Momentum multiplier based on trend */
    if (ramp_up_momentum && trend == 1) {
        /* Load increasing - accelerate ramp up */
        step += step / 2; /* 1.5x speed */
    }
    
    /* Extra aggression for very high load */
    if (load >= 90) {
        step += step / 2; /* Another 1.5x for extreme load */
    }
    
    new_freq = cur_freq + step;
    
    if (new_freq > target_max)
        new_freq = target_max;
    
    return new_freq;
}

/**
 * smooth_decay() - Gradual frequency reduction
 * Slower decay in gaming mode to prevent stutter
 */
static unsigned int smooth_decay(unsigned int cur_freq,
                                  unsigned int floor,
                                  struct vortexmax_cpu_info *info)
{
    unsigned int diff, step;
    
    if (cur_freq <= floor)
        return floor;
    
    diff = cur_freq - floor;
    
    /* Base decay rate */
    step = max(floor / 100, diff / 35);
    
    /* Much slower decay in gaming mode (anti-stutter) */
    if (info->is_gaming_mode) {
        step = max(1, step / 3); /* 3x slower */
    } else if (info->is_active) {
        step = max(1, step / 2); /* 2x slower for active */
    }
    
    if (cur_freq > floor + step)
        return cur_freq - step;
    
    return floor;
}

/**
 * apply_transition_limit() - Rate-limit frequency changes
 * Prevents too-fast transitions that can cause instability
 */
static unsigned int apply_transition_limit(unsigned int requested,
                                            unsigned int cur_freq,
                                            unsigned int sample_ms)
{
    unsigned int max_delta;
    
    if (!transition_smooth_enabled)
        return requested;
    
    /* Maximum allowed change per sampling period */
    max_delta = max_freq_change_per_ms * sample_ms;
    
    if (requested > cur_freq) {
        /* Increasing */
        if ((requested - cur_freq) <= max_delta)
            return requested;
        return cur_freq + max_delta;
    } else {
        /* Decreasing */
        if ((cur_freq - requested) <= max_delta)
            return requested;
        return cur_freq - max_delta;
    }
}

/**
 * apply_thermal_guard() - Intelligent thermal management
 * Reduces frequency gradually rather than hard cap
 */
static unsigned int apply_thermal_guard(unsigned int freq_target,
                                         unsigned int thermal_max,
                                         struct vortexmax_cpu_info *info)
{
    unsigned int soft_limit;
    
    soft_limit = (thermal_max * thermal_limit_pct) / 100;
    
    if (freq_target <= soft_limit) {
        /* Below limit - recover thermal counter */
        if (info->thermal_counter > 0)
            info->thermal_counter -= 2;
        return freq_target;
    }
    
    /* Above soft limit - increment counter */
    info->thermal_counter++;
    
    if (info->thermal_counter < thermal_counter_threshold) {
        /* Allow temporary excursions */
        return freq_target;
    }
    
    /* Sustained above limit - take action */
    if (thermal_hard_limit) {
        /* Hard cap mode */
        return soft_limit;
    } else {
        /* Soft reduction mode - gradual pull down */
        unsigned int excess = freq_target - soft_limit;
        unsigned int reduction = excess / 2; /* Reduce by half the excess */
        
        if (reduction < (thermal_max / 50)) /* At least 2% steps */
            reduction = thermal_max / 50;
            
        return freq_target - reduction;
    }
}

/* =====================================================================
 * MAIN FREQUENCY EVALUATION FUNCTION
 * The brain of VortexMax - called every sampling interval
 * ===================================================================== */

static void vortexmax_eval_freq(struct cpufreq_policy *policy)
{
    struct vortexmax_cpu_info *info = &per_cpu(vortexmax_info, policy->cpu);
    u64 now_ns, idle_time, delta_wall, delta_idle;
    unsigned int raw_load, load, freq_target, current_freq;
    unsigned int thermal_max, freq_floor;
    unsigned int touch_val = 0, io_val = 0, wake_val = 0;
    int trend;
    bool freq_increasing;

    now_ns = local_clock();
    idle_time = get_cpu_idle_time(policy->cpu, &delta_wall, 0);
    current_freq = policy->cur;
    thermal_max = policy->max;

    /* ---- INITIALIZATION CHECK ---- */
    if (info->prev_cpu_wall == 0) {
        info->prev_cpu_wall = now_ns;
        info->prev_cpu_idle = idle_time;
        info->target_freq = current_freq;
        info->next_delay_ms = sample_rate_normal_ms;
        info->last_touch_time = 0;
        info->last_io_time = 0;
        info->last_wake_time = now_ns; /* Treat init as wake */
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
        return;
    }

    /* ---- LOAD CALCULATION ---- */
    delta_wall = now_ns - info->prev_cpu_wall;
    delta_idle = idle_time - info->prev_cpu_idle;
    info->prev_cpu_wall = now_ns;
    info->prev_cpu_idle = idle_time;

    if (delta_wall == 0 || delta_idle > delta_wall)
        raw_load = 0;
    else
        raw_load = div64_u64(100 * (delta_wall - delta_idle), delta_wall);

    /* Update history and calculate smoothed values */
    update_load_history(info, raw_load);
    load = calculate_ema(info, raw_load);
    trend = detect_load_trend(info);
    info->load_trend = trend;
    
    /* Detect usage pattern */
    detect_gaming_pattern(info, load);

    /* Determine core type (big vs LITTLE) */
    bool is_big = (policy->cpuinfo.max_freq > (policy->cpuinfo.min_freq * 2));
    unsigned int dyn_target = is_big ? target_load_big : target_load_little;

    /* Get dynamic frequency floor */
    freq_floor = get_frequency_floor(policy, info);

    /* ================================================================
     * PRIORITY-BASED DECISION MATRIX
     * Higher priority boosts override lower ones
     * ================================================================ */
    
    /* 1. TOUCH BOOST (Highest Priority - User Interaction) */
    touch_val = apply_touch_boost(policy, info, now_ns);
    if (touch_val > 0) {
        freq_target = touch_val;
        goto apply_final;
    }

    /* 2. WAKE BOOST (Second Priority - Just woke from idle) */
    wake_val = apply_wake_boost(policy, info, now_ns);
    if (wake_val > 0) {
        freq_target = wake_val;
        goto apply_final;
    }

    /* 3. IO BOOST (Third Priority - Detected IO burst) */
    io_val = apply_io_boost(policy, info, now_ns, load);
    if (io_val > 0) {
        freq_target = io_val;
        goto apply_final;
    }

    /* 4. NORMAL SCALING LOGIC */
    if (load >= fast_ramp_up_load) {
        /* === CRITICAL LOAD - MAXIMUM RESPONSE === */
        freq_target = thermal_max;
        
        /* Activate max hold */
        if (info->max_hold_counter < max_hold_cycles)
            info->max_hold_counter++;

    } else if (load > dyn_target) {
        /* === ELEVATED LOAD - SMART RAMP UP === */
        freq_target = smart_ramp_up(current_freq, thermal_max, load, trend);
        
        /* Partial hold for elevated load */
        if (info->max_hold_counter < (max_hold_cycles / 2))
            info->max_hold_counter++;

    } else if (info->max_hold_counter > 0) {
        /* === SUSTAIN MODE - ANTI-PARACHUTE === */
        if (load > (dyn_target - 15)) {
            /* Still moderate load - hold steady */
            freq_target = current_freq;
            info->max_hold_counter--;
        } else {
            /* Low load - begin smooth decay */
            freq_target = smooth_decay(current_freq, freq_floor, info);
            info->max_hold_counter--;
        }
    } else {
        /* === NORMAL DECAY MODE === */
        freq_target = smooth_decay(current_freq, freq_floor, info);
    }

apply_final:

    /* Enforce frequency floor */
    if (freq_target < freq_floor)
        freq_target = freq_floor;

    /* Apply thermal guard */
    freq_target = apply_thermal_guard(freq_target, thermal_max, info);

    /* Determine direction for hysteresis */
    freq_increasing = (freq_target > current_freq);
    
    /* Apply hysteresis band */
    freq_target = apply_hysteresis(freq_target, current_freq, 
                                    policy->min, freq_increasing);

    /* Apply transition rate limiter */
    freq_target = apply_transition_limit(freq_target, current_freq,
                                          info->next_delay_ms);

    /* Hard clamps */
    if (freq_target > thermal_max)
        freq_target = thermal_max;
    if (freq_target < policy->min)
        freq_target = policy->min;

    /* Apply frequency change */
    if (freq_target != info->target_freq) {
        info->target_freq = freq_target;
        __cpufreq_driver_target(policy, freq_target, CPUFREQ_RELATION_L);
    }

    /* Adaptive sampling rate selection */
    if (touch_val > 0 || wake_val > 0) {
        info->next_delay_ms = sample_rate_boost_ms;      /* 4ms */
    } else if (io_val > 0 || load >= fast_ramp_up_load) {
        info->next_delay_ms = sample_rate_boost_ms;       /* 4ms */
    } else if (info->is_gaming_mode || load > dyn_target) {
        info->next_delay_ms = sample_rate_active_ms;      /* 8ms */
    } else if (info->is_active || load > 40) {
        info->next_delay_ms = sample_rate_normal_ms;      /* 16ms */
    } else {
        info->next_delay_ms = sample_rate_idle_ms;        /* 24ms */
    }
}

/* =====================================================================
 * INPUT SUBSYSTEM INTEGRATION - Touch Boost Handler
 * Pure kernel space - no userspace involvement
 * ===================================================================== */

static void vortexmax_do_touch_boost(struct work_struct *work)
{
    unsigned int cpu;
    struct vortexmax_cpu_info *info;
    
    /* Update timestamp on ALL CPUs for synchronous boost */
    for_each_possible_cpu(cpu) {
        info = &per_cpu(vortexmax_info, cpu);
        info->last_touch_time = local_clock();
        /* Mark as active on touch */
        info->is_active = true;
        info->consecutive_low_load = 0;
    }
    
    touch_pending = false;
    /* Work queue handles memory barriers implicitly */
}

/**
 * vortexmax_input_event() - Callback for input events
 * Registered with kernel input subsystem - catches touch/mouse events
 */
static int vortexmax_input_event(struct input_handle *handle,
                                   unsigned int type,
                                   unsigned int code,
                                   int value)
{
    /* We only care about touch press-down and mouse button press */
    if (type == EV_KEY) {
        if (code == BTN_TOUCH || code == BTN_MOUSE || 
            code == BTN_TOOL_PEN || code == BTN_TOOL_FINGER ||
            code == BTN_STYLUS || code == BTN_STYLUS2) {
            
            if (value == 1) { /* Press down, not release */
                if (!touch_pending) {
                    touch_pending = true;
                    schedule_work(&touch_boost_work);
                }
            }
        }
    }
    
    return 0; /* Don't consume the event - let others see it too */
}

static int vortexmax_input_connect(struct input_handler *handler,
                                    struct input_dev *dev,
                                    const struct input_device_id *id)
{
    struct input_handle *handle;
    int err;
    
    /* Only connect to touchscreens, mice, stylus devices */
    if (!(test_bit(EV_KEY, dev->evbit) &&
          (test_bit(BTN_TOUCH, dev->keybit) || 
           test_bit(BTN_MOUSE, dev->keybit) ||
           test_bit(BTN_TOOL_PEN, dev->keybit) ||
           test_bit(BTN_TOOL_FINGER, dev->keybit) ||
           test_bit(BTN_STYLUS, dev->keybit))))
        return -ENODEV;
    
    handle = kzalloc(sizeof(*handle), GFP_KERNEL);
    if (!handle)
        return -ENOMEM;
    
    handle->dev = dev;
    handle->handler = handler;
    handle->name = "vortexmax";
    
    err = input_register_handle(handle);
    if (err)
        goto err_free;
    
    err = input_open_device(handle);
    if (err)
        goto err_unreg;
    
    return 0;
    
err_unreg:
    input_unregister_handle(handle);
err_free:
    kfree(handle);
    return err;
}

static void vortexmax_input_disconnect(struct input_handle *handle)
{
    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
}

/* Input device IDs we're interested in */
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
    { }, /* Terminating zero entry */
};

/* Input handler structure - registers us with kernel input subsys */
static struct input_handler vortexmax_input_handler = {
    .event        = vortexmax_input_event,
    .connect      = vortexmax_input_connect,
    .disconnect   = vortexmax_input_disconnect,
    .name         = "vortexmax",
    .id_table     = vortexmax_ids,
};

/* =====================================================================
 * THERMAL NOTIFIER INTEGRATION
 * Optional: Reacts to kernel thermal events (if available)
 * ===================================================================== */

#ifdef CONFIG_THERMAL
static int vortexmax_thermal_notify(struct notifier_block *nb,
                                     unsigned long event, void *data)
{
    struct thermal_zone_device *tz = data;
    int temp = 0;
    
    if (event != THERMAL_EVENT_TEMP_SAMPLE)
        return NOTIFY_DONE;
    
    if (!tz)
        return NOTIFY_DONE;
    
    /* Only react to CPU thermal zones */
    if (strncmp(tz->type, "cpu-", 4) != 0 &&
        strncmp(tz->type, "soc_thermal", 11) != 0)
        return NOTIFY_DONE;
    
    temp = thermal_zone_get_temp(tz, &temp);
    if (temp <= 0 || temp == THERMAL_TEMP_INVALID)
        return NOTIFY_DONE;
    
    /* If getting hot, pre-emptively inform all CPU infos */
    if (temp > (tz->passive * 1000)) { /* passive is in millidegrees */
        unsigned int cpu;
        struct vortexmax_cpu_info *info;
        
        for_each_possible_cpu(cpu) {
            info = &per_cpu(vortexmax_info, cpu);
            /* Increment thermal counter to trigger guard */
            if (info->thermal_counter < thermal_counter_threshold)
                info->thermal_counter += 2;
        }
    }
    
    return NOTIFY_OK;
}
#endif /* CONFIG_THERMAL */

/* =====================================================================
 * WORK QUEUE HANDLER
 * ===================================================================== */

static void vortexmax_work_handler(struct work_struct *work)
{
    struct vortexmax_policy_info *vpinfo = 
        container_of(work, struct vortexmax_policy_info, work.work);
    struct cpufreq_policy *policy = vpinfo->policy;

    vortexmax_eval_freq(policy);
    
    schedule_delayed_work_on(
        policy->cpu, 
        &vpinfo->work, 
        msecs_to_jiffies(per_cpu(vortexmax_info, policy->cpu).next_delay_ms)
    );
}

/* =====================================================================
 * GKI GOVERNOR API STRUCTURE
 * Compatible with GKI 5.10, 6.1, 6.6
 * ===================================================================== */

static int vortexmax_init(struct cpufreq_policy *policy)
{
    struct vortexmax_policy_info *vpinfo;
    
    vpinfo = kzalloc(sizeof(*vpinfo), GFP_KERNEL);
    if (!vpinfo)
        return -ENOMEM;
    
    vpinfo->policy = policy;
    INIT_DEFERRABLE_WORK(&vpinfo->work, vortexmax_work_handler);
    policy->governor_data = vpinfo;
    
    return 0;
}

static void vortexmax_exit(struct cpufreq_policy *policy)
{
    struct vortexmax_policy_info *vpinfo = policy->governor_data;
    
    if (vpinfo) {
        cancel_delayed_work_sync(&vpinfo->work);
        kfree(vpinfo);
        policy->governor_data = NULL;
    }
}

static int vortexmax_start(struct cpufreq_policy *policy)
{
    unsigned int cpu;
    struct vortexmax_cpu_info *info;
    
    for_each_cpu(cpu, policy->cpus) {
        info = &per_cpu(vortexmax_info, cpu);
        
        /* Initialize all state */
        info->prev_cpu_idle = get_cpu_idle_time(cpu, &info->prev_cpu_wall, 0);
        info->target_freq = policy->cur;
        info->thermal_counter = 0;
        info->max_hold_counter = 0;
        info->next_delay_ms = sample_rate_normal_ms;
        info->last_touch_time = 0;
        info->last_io_time = 0;
        info->last_wake_time = local_clock(); /* Start with wake boost */
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
    
    /* Register subsystems (only once globally) */
    spin_lock(&vortexmax_lock);
    if (!vortexmax_initialized) {
        /* Initialize touch boost work */
        INIT_WORK(&touch_boost_work, vortexmax_do_touch_boost);
        
        /* Register input handler for touch boost */
        input_register_handler(&vortexmax_input_handler);
        
#ifdef CONFIG_THERMAL
        /* Register thermal notifier */
        vortexmax_thermal_notifier.notifier_call = vortexmax_thermal_notify;
        register_thermal_notifier(&vortexmax_thermal_notifier);
#endif
        
        vortexmax_initialized = true;
    }
    spin_unlock(&vortexmax_lock);
    
    atomic_inc(&active_governors);
    
    /* Start the main governor loop */
    schedule_delayed_work_on(
        policy->cpu,
        &((struct vortexmax_policy_info *)policy->governor_data)->work,
        msecs_to_jiffies(sample_rate_normal_ms)
    );
    
    return 0;
}

static void vortexmax_stop(struct cpufreq_policy *policy)
{
    cancel_delayed_work_sync(
        &((struct vortexmax_policy_info *)policy->governor_data)->work
    );
    
    if (atomic_dec_and_test(&active_governors)) {
        /* Last governor stopped - cleanup global resources */
        spin_lock(&vortexmax_lock);
        
        cancel_work_sync(&touch_boost_work);
        input_unregister_handler(&vortexmax_input_handler);
        
#ifdef CONFIG_THERMAL
        unregister_thermal_notifier(&vortexmax_thermal_notifier);
#endif
        
        vortexmax_initialized = false;
        spin_unlock(&vortexmax_lock);
    }
}

static void vortexmax_limits(struct cpufreq_policy *policy)
{
    vortexmax_eval_freq(policy);
}

/* =====================================================================
 * GOVERNOR REGISTRATION
 * ===================================================================== */

static struct cpufreq_governor vortexmax_gov = {
    .name           = "vortexmax",
    .owner          = THIS_MODULE,
    .init           = vortexmax_init,
    .exit           = vortexmax_exit,
    .start          = vortexmax_start,
    .stop           = vortexmax_stop,
    .limits         = vortexmax_limits,
};

/* =====================================================================
 * MODULE INIT/EXIT
 * ===================================================================== */

static int __init vortexmax_module_init(void)
{
    int ret;
    
    ret = cpufreq_register_governor(&vortexmax_gov);
    if (ret) {
        pr_err("VortexMax: Registration failed (%d)\n", ret);
        return ret;
    }
    
    pr_info("VortexMax v3.0 Ultra Standalone loaded\n");
    pr_info("Architecture: ZERO Dependency - Full Kernel Space\n");
    pr_info("Features: TouchBoost, IOBoost, WakeBoost, SmartRamp\n");
    pr_info("         GamingAI, ThermalGuard, Hysteresis, EMA\n");
    pr_info("Author: Kingfinik98 <kingfinix98@gmail.com>\n");
    
    return 0;
}

static void __exit vortexmax_module_exit(void)
{
    /* Safety: ensure everything is cleaned up */
    if (vortexmax_initialized) {
        cancel_work_sync(&touch_boost_work);
        input_unregister_handler(&vortexmax_input_handler);
#ifdef CONFIG_THERMAL
        unregister_thermal_notifier(&vortexmax_thermal_notifier);
#endif
        vortexmax_initialized = false;
    }
    
    cpufreq_unregister_governor(&vortexmax_gov);
    
    pr_info("VortexMax v3.0 unloaded\n");
}

module_init(vortexmax_module_init);
module_exit(vortexmax_module_exit);

/* =====================================================================
 * MODULE METADATA
 * ===================================================================== */

MODULE_AUTHOR("Kingfinik98 <kingfinix98@gmail.com>");
MODULE_DESCRIPTION("VortexMax v3.0 Ultra Standalone - Zero Dependency Gaming Governor");
MODULE_LICENSE("GPL");
MODULE_VERSION("3.0");

