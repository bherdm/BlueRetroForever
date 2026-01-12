#ifndef MOUSE_MAPPER_H_
#define MOUSE_MAPPER_H_

#include <stdint.h>
#include <stdbool.h>

struct mouse_mapper_cfg {
    float sensitivity;      /* Base gain on velocity. */
    float accel;            /* Soft acceleration exponent (1.0 = linear). */
    float smoothing_alpha;  /* EMA factor [0.01,1.0]. */
    int32_t max_per_frame;  /* Clamp of per-frame delta. */
    bool pointer_mode;      /* true: pointer; false: legacy joystick emulation. */
    bool carry_enabled;     /* Preserve sub-pixel via accumulator. */
    float poll_interval_s;  /* Nominal poll interval in seconds. */
};

struct mouse_mapper_state {
    float carry_x;
    float carry_y;
    float ema_vx;
    float ema_vy;
    uint64_t last_ts_us;
};

#ifdef __cplusplus
extern "C" {
#endif

void mouse_mapper_init(struct mouse_mapper_state *st, const struct mouse_mapper_cfg *cfg);
void mouse_mapper_reset(struct mouse_mapper_state *st);
void mouse_mapper_process(const struct mouse_mapper_cfg *cfg, struct mouse_mapper_state *st,
                          int32_t dx, int32_t dy, uint64_t ts_us,
                          int32_t *out_x, int32_t *out_y);

/* Defaults suitable for Mario Artist pointer use. */
void mouse_mapper_default_cfg(struct mouse_mapper_cfg *cfg);

#ifdef __cplusplus
}
#endif

#endif /* MOUSE_MAPPER_H_ */
