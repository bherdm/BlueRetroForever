#include "soc/soc_ulp.h"
#include "soc/rtc_io_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc_cntl_reg.h"

#define DATA_LO WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S+10, 1, 1)
#define DATA_HI WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TS_S+10, 1, 1)
#define CLK_LO WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TC_S+12, 1, 1)
#define CLK_HI WRITE_RTC_REG(RTC_GPIO_OUT_W1TC_REG, RTC_GPIO_OUT_DATA_W1TS_S+12, 1, 1)
#define CLK_PULSE CLK_HI; CLK_LO

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
      
      // SOF
      stage_rst
      1:
         stage_inc 1
         CLK_PULSE
         jumps 1b, 32, lt

      jump main
