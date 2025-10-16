#include "soc/soc_ulp.h"
#include "soc/rtc_io_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc_cntl_reg.h"

#define DATA_LO WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S+10, 1, 1)
#define DATA_HI WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TS_S+10, 1, 1)
#define CLK_LO WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S+12, 1, 1)
#define CLK_HI WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TS_S+12, 1, 1)
#define CLK_PULSE CLK_HI; CLK_LO
#define W R0
#define LED_IDX R1
#define MASK R2

.bss

   .global variable
variable:
   .long 0

   .global test_array
test_array:
   .space 64

.data

   .global test_array_index
test_array_index:
   .long 512

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
         move MASK, 1
         2:
            lsh MASK, MASK, 1
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
