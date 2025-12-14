#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include <driver/rtc_io.h>
#include "soc/rtc_cntl_reg.h"
#include "ulp.h"
#include "soc/rtc.h"

#define APA102_BRIGHTNESS_MASK 0x00001F00
#define APA102_BLUE_MASK 0x000000FF
#define APA102_GREEN_MASK 0x0000FF00
#define APA102_RED_MASK 0x000000FF

extern uint32_t ulp_main;
extern uint32_t ulp_leds_array[32];
extern const uint8_t ulp_bin_start[] asm("_binary_ulp_apa102_bin_start");
extern const uint8_t ulp_bin_end[] asm("_binary_ulp_apa102_bin_end");

struct apa102_frame {
    uint32_t brightness_blue;
    uint32_t green_red;
};
static struct apa102_frame *leds_array = (struct apa102_frame *)&ulp_leds_array;

void apa102_init(void)
{
    rtc_clk_fast_freq_set(RTC_FAST_FREQ_XTALD4);

    rtc_gpio_init(GPIO_NUM_2);
    rtc_gpio_set_direction(GPIO_NUM_2, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_init(GPIO_NUM_4);
    rtc_gpio_set_direction(GPIO_NUM_4, RTC_GPIO_MODE_OUTPUT_ONLY);

    ulp_load_binary(0, ulp_bin_start, (ulp_bin_end - ulp_bin_start) / sizeof(uint32_t));
    ulp_run((&ulp_main - RTC_SLOW_MEM));

    for (uint32_t i = 0; i < 16; i++) {
        leds_array[i].brightness_blue = 0xE300;
        leds_array[i].green_red = 0x0000;
    }
}

void apa102_update(void)
{
    static bool dir = true;
    static uint8_t brightness = 6;

    leds_array[0].brightness_blue &= ~APA102_BLUE_MASK;
    leds_array[0].brightness_blue |= brightness;
    leds_array[1].brightness_blue &= ~APA102_BLUE_MASK;
    leds_array[1].brightness_blue |= brightness;

    leds_array[2].green_red &= ~APA102_RED_MASK;
    leds_array[2].green_red |= brightness;
    leds_array[3].green_red &= ~APA102_RED_MASK;
    leds_array[3].green_red |= brightness;

    leds_array[4].green_red &= ~APA102_GREEN_MASK;
    leds_array[4].green_red |= brightness << 8;
    leds_array[5].green_red &= ~APA102_GREEN_MASK;
    leds_array[5].green_red |= brightness << 8;

    leds_array[6].brightness_blue &= ~APA102_BLUE_MASK;
    leds_array[6].brightness_blue |= brightness;
    leds_array[7].brightness_blue &= ~APA102_BLUE_MASK;
    leds_array[7].brightness_blue |= brightness;
    leds_array[6].green_red &= ~APA102_RED_MASK;
    leds_array[6].green_red |= brightness;
    leds_array[7].green_red &= ~APA102_RED_MASK;
    leds_array[7].green_red |= brightness;

    leds_array[8].green_red = 0;
    leds_array[8].green_red |= brightness | ((brightness << 5) & 0xFF00);

    if (dir) {
        brightness += 3;
    }
    else {
        brightness -= 3;
    }
    if (brightness > 144 || brightness <= 6) {
        dir ^= true;
    }
}