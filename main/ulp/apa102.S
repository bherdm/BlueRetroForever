#include "soc/soc_ulp.h"
#include "soc/rtc_io_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc_cntl_reg.h"

#define DATA_LO WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S+10, 1, 1)
#define DATA_HI WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S+10, 1, 1)
#define CLK_LO WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S+12, 1, 1)
#define CLK_HI WRITE_RTC_REG(RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_DATA_W1TS_S+12, 1, 1)
#define CLK_PULSE CLK_HI; CLK_LO

#define W R0
#define MASK R1
#define LED_IDX R2
#define LED_DATA R3

.bss

   .global leds_array
leds_array:
   .space 128

.text

   .global main
   main:
      DATA_LO
      CLK_LO

   loop:
      // SOF
      DATA_LO
      stage_rst
      1:
         stage_inc 1
         CLK_PULSE
         jumps 1b, 32, lt

      // LEDS DATA
      move LED_IDX, 0
      1:
         move W, leds_array
         add W, W, LED_IDX
         ld LED_DATA, W, 0
         move MASK, 0x8000
         2:
            and W, LED_DATA, MASK
            jumpr set_data, 0, gt
            DATA_LO
            jump clk_pulse
         set_data:
            DATA_HI
         clk_pulse:
            CLK_PULSE
            rsh MASK, MASK, 1
            move W, MASK
            jumpr 2b, 0, gt
         
         add LED_IDX, LED_IDX, 1
         move W, LED_IDX
         jumpr 1b, 32, lt

      // EOF
      DATA_HI
      stage_rst
      1:
         stage_inc 1
         CLK_PULSE
         jumps 1b, 32, lt

      jump loop
