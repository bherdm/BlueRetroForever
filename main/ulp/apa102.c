#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include <driver/rtc_io.h>
#include "soc/rtc_cntl_reg.h"
#include "ulp.h"
#include "ulp_apa102.h"
#include "soc/rtc.h"

extern const uint8_t ulp_bin_start[] asm("_binary_ulp_apa102_bin_start");
extern const uint8_t ulp_bin_end[] asm("_binary_ulp_apa102_bin_end");

void apa102_init(void)
{
    int clk_src = rtc_clk_fast_freq_get();
    printf("STC CLK SRC: %d\n", clk_src);
    rtc_clk_fast_freq_set(RTC_FAST_FREQ_XTALD4);
    clk_src = rtc_clk_fast_freq_get();
    printf("STC CLK SRC: %d\n", clk_src);

    rtc_gpio_init(GPIO_NUM_2);
    rtc_gpio_set_direction(GPIO_NUM_2, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_init(GPIO_NUM_4);
    rtc_gpio_set_direction(GPIO_NUM_4, RTC_GPIO_MODE_OUTPUT_ONLY);

    ulp_load_binary(0, ulp_bin_start, (ulp_bin_end - ulp_bin_start) / sizeof(uint32_t));
    ulp_run((&ulp_main - RTC_SLOW_MEM));

    // uint64_t sleep_time = 86400*1000000;
    // esp_sleep_enable_timer_wakeup(sleep_time);
    // esp_deep_sleep_start();
}