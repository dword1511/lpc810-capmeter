#include <stdio.h>
#include <stdbool.h>
#include <lpc8xx.h>

#include "acmp.h"
#include "uart.h"

#define _BV(x)       (1 << x)
#define UNLIKELY(e)  (__builtin_expect(!!(e), false))

#define GPIO_ACMP_I1 0
#define GPIO_ACMP_I2 1
#define GPIO_R220    2
#define GPIO_R1M     3

#define ACMPIN_REF   ACMPIN_I1
#define ACMPIN_CAP   ACMPIN_I2

#define INTERVAL_MS  200
#define BAUDRATE     115200

#define CYCLE_PER_US (__SYSTEM_CLOCK/1000000)
#define CYCLE_PER_MS (__SYSTEM_CLOCK/1000)
#define GPIO_FLOAT   (~(3 << 3))

// 200ms / 1M = 200nF (fast-mode kicks-in for C > 200nF)
// 200nF * 220R = 44us
// 200ms / 44us = 4546 measurements = 1M/220 (magic! stupid me)
#define RES_FRATIO   (1000000/220 + 1)
#define SAMPLES_MAX  RES_FRATIO

/*
 * NOTE: every clock is ~33ns. Voltage input hooked up to ACMP_I2.
 * For 1pF resolution, need 1Mohm resistor and 1us resolution.
 * 1us timer with uint32_t warps every 1.2 hours.
 * 24-bit systick register warps every 24ms.
 * uint32_t cannot cover 5pF all the way up to 4700uF.
 * So use uint64_t. (or have a seperate mF instead)
 *
 * SysTick INT: count ms for delay functions. needs highest priority.
 * Main Loop: take and record measurement and resets the circuit.
 *            check timestamp and report results every INTERVAL_MS ms.
 */

/*
 * NOTE: system performance:
 * 18-20pF minimum (due to stray capacitance, can be calibrated. ~12pF on PCB, 7pF on test leads)
 * 5Hz data output (for C's that are not too large)
 * Sensitive to resistance (ESR & leakage)
 * 1pf takes 1us. switches to fast mode at 200nF. minimum measurement in fastmode takes 44us.
 */

// TODO: Recv cmd from uart
// H = hold (just new line)
// Z = set distributed capacitors
// L = leakage assesment

volatile static uint32_t systime_ms = 0;

void SysTick_Handler(void) {
  // Keep it as short as possible, to reduce error.
  systime_ms ++;
}

inline uint32_t capmeter_timer_get_us(void) {
  return systime_ms * 1000 + 1000 - (SysTick->VAL / CYCLE_PER_US);
}

void capmeter_timer_init(void) {
  // SysTick Millisecond Timer
  while ((SysTick_Config(CYCLE_PER_MS)) != 0); // 1ms interval
  NVIC_SetPriority(SysTick_IRQn, 0);           // highest priority
}

void capmeter_delay(uint32_t ms) {
  ms += systime_ms;
  while(systime_ms < ms);
}

void capmeter_swm_init(void) {
  // Enable SWM
  LPC_SYSCON->SYSAHBCLKCTRL |= (1<<7);

  // Enable UART
  LPC_SWM->PINASSIGN0 = 0xffff0405UL;

  // Enable ACMP
  LPC_SWM->PINENABLE0 = 0xfffffffcUL;

  // ACMP PINs IN, All PINs FLOAT
  LPC_GPIO_PORT->CLR0 = (_BV(GPIO_ACMP_I1) | _BV(GPIO_ACMP_I2) | _BV(GPIO_R220) | _BV(GPIO_R1M));
  LPC_IOCON->PIO0_2  &= GPIO_FLOAT;
  LPC_IOCON->PIO0_3  &= GPIO_FLOAT;
  LPC_GPIO_PORT->DIR0 = (_BV(GPIO_R220) | _BV(GPIO_R1M));
}

void capmeter_discharge(void) {
  // Setup comparator for monitoring the discharge.
  // Do not just wait, 220R * 4700uF > 1s
  // You can get another sample while discharging, but currently not doing this,
  // especially where LPC810's sink only provides 4mA before rising to 0.4V...
  acmp_set_input(ACMPIN_CAP, ACMPIN_GND); // If this does not work, we will have to use the voltage ladder for a small offset

  // All PINs LOW
  LPC_GPIO_PORT->DIR0 = (_BV(GPIO_R220) | _BV(GPIO_R1M));
  LPC_GPIO_PORT->CLR0 = (_BV(GPIO_R220) | _BV(GPIO_R1M));

  while (ACMP_OUT);
}

uint64_t capmeter_charge(bool fast) {
  uint64_t us_start, us_stop;
  uint32_t last_ms = systime_ms;

  // Setup comparator
  acmp_set_input(ACMPIN_REF, ACMPIN_CAP);

  // GPIO High (to be specific, hold low, configure direction, then pull the correct pin high)
  LPC_GPIO_PORT->DIR0 = fast ? _BV(GPIO_R220) : _BV(GPIO_R1M);
  LPC_GPIO_PORT->SET0 = (_BV(GPIO_R220) | _BV(GPIO_R1M));

  // Take time snapshot
  us_start = capmeter_timer_get_us();

  // wait comparator. use identical operations to minimize error.
  while (ACMP_OUT) {
    if (UNLIKELY(!fast && ((systime_ms - last_ms) > INTERVAL_MS))) {
      return 0;
    }
  }
  us_stop = capmeter_timer_get_us();

  // Calculate diff, convert to pF (*4546UL in fast)
  // TODO: what will happen when time warps? 0xffffffffffffffffUL
  if (fast) {
    return (us_stop - us_start) * RES_FRATIO;
  } else {
    return (us_stop - us_start);
  }
}

// TODO: pass calibration, output '-' sign & 0 if <0
// TODO: output ' E    ...' if > 9999.99uF
// TODO: add a blinky dot
void capmeter_send_data(uint64_t pf, bool fast) {
  char msg[13];
  char unit = 'p';
  uint32_t remainder = 0;
  bool leading = true;

  if (pf > 1000000UL) {
    unit = 'u';
    remainder = pf % 1000000UL;
    remainder /= 1000UL;
    pf /= 1000000UL;
  } else if (pf > 1000UL) {
    unit = 'n';
    remainder = pf % 1000UL;
    pf /= 1000UL;
  }

  if (pf >= 1000UL) {
    msg[0] = (pf / 1000UL) + 0x30;
    leading = false;
  } else {
    msg[0] = ' ';
  }

  pf %= 1000UL;
  if ((pf >= 100UL) || (!leading)) {
    msg[1] = (pf / 100UL) + 0x30;
    leading = false;
  } else {
    msg[1] = ' ';
  }

  pf %= 100UL;
  if ((pf >= 10UL) || (!leading)) {
    msg[2] = (pf / 10UL) + 0x30;
  } else {
    msg[2] = ' ';
  }

  pf %= 10UL;
  msg[3] = pf + 0x30;

  msg[4] = '.';

  msg[5] = (remainder / 100UL) + 0x30;
  remainder %= 100UL;
  msg[6] = (remainder / 10UL) + 0x30;

  msg[7] = ' ';
  msg[8] = unit;
  msg[9] = 'F';

  msg[10] = ' ';
  if (fast) {
    msg[11] = 'L';
  } else {
    msg[11] = ' ';
  }

  msg[12] = '\r';
  uart0Send(msg, 13);
}

int main(void) {
  capmeter_swm_init();
  uart0Init(BAUDRATE);
  capmeter_timer_init();
  acmp_init();

  uint32_t last_report_ms = 0;
  bool     fast           = false; // gets enabled automatically to measure big capacitors with 220R resistor
  uint64_t sum            = 0UL;
  uint32_t samples        = 0;

  while(true) {
    sum += capmeter_charge(fast);
    samples ++;
    capmeter_discharge();

    // Handle systime_ms warpping but not too carefully, it is rare
    if ((systime_ms - last_report_ms > INTERVAL_MS) || (last_report_ms > systime_ms)) {
      last_report_ms = systime_ms;
      capmeter_send_data(sum / samples, fast);

      if (samples == 1) {
        fast = true;
      }
      if (samples > SAMPLES_MAX) {
        fast = false;
      }
      sum = 0UL;
      samples = 0;
    }
  }
}
