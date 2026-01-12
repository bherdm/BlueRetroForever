/*
 * Lightweight harness helpers to exercise the mouse mapper math on target or host.
 * Not executed automatically; invoke mouse_test_harness_example() manually when debugging.
 */

#include <stdio.h>
#include <stddef.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/mouse_mapper.h"

static void mouse_test_harness_run(const int32_t (*samples)[2], size_t sample_cnt,
                                   uint64_t ts_step_us, const struct mouse_mapper_cfg *override_cfg) {
    struct mouse_mapper_cfg cfg;
    struct mouse_mapper_state st;

    if (override_cfg) {
        cfg = *override_cfg;
    }
    else {
        mouse_mapper_default_cfg(&cfg);
    }
    mouse_mapper_init(&st, &cfg);

    uint64_t ts_us = 0;
    for (size_t i = 0; i < sample_cnt; i++) {
        int32_t ox = 0;
        int32_t oy = 0;
        ts_us += ts_step_us;
        mouse_mapper_process(&cfg, &st, samples[i][0], samples[i][1], ts_us, &ox, &oy);
        printf("# mouse_harness[%zu]: in(%ld,%ld) -> out(%ld,%ld) carry(%.3f,%.3f) ema(%.3f,%.3f)\n",
               i, (long)samples[i][0], (long)samples[i][1], (long)ox, (long)oy,
               (double)st.carry_x, (double)st.carry_y, (double)st.ema_vx, (double)st.ema_vy);
    }
}

void mouse_test_harness_example(void) {
    static const int32_t seq[][2] = {
        {1, 0},
        {0, 1},
        {2, 2},
        {0, 0},
        {-1, -1},
    };

    mouse_test_harness_run(seq, ARRAY_SIZE(seq), 1000, NULL);
}
