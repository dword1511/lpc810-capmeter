#ifndef _ACMP_H_
#define _ACMP_H_

#include <stdbool.h>

#include <lpc8xx.h>

#define ACMPIN_DAC   0
#define ACMPIN_I1    1
#define ACMPIN_I2    2
#define ACMPIN_900mV 6
#define ACMPIN_GND   7

#define ACMPHYS_0mV  0
#define ACMPHYS_5mV  1
#define ACMPHYS_10mV 2
#define ACMPHYS_20mV 3

#define ACMP_OUT     ((LPC_CMP->CTRL & (0x1 << 21)) >> 21)

void acmp_init(void);
void acmp_set_input(uint32_t input_p, uint32_t input_n);
void acmp_set_hysteresis(uint32_t level);

#endif

