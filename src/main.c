#include <stdio.h>
#include <stdbool.h>
#include <lpc8xx.h>

#include "gpio.h"
#include "uart.h"

#define _BV(x)       (1 << x)

#define GPIO_ACMP_I1 0
#define GPIO_ACMP_I2 1
#define GPIO_R220    2
#define GPIO_R1M     3

#define INTERVAL_MS  200
#define SAMPLES_MAX  50

/*
 * NOTE: every clock is 33ns.
 * 1us systick seems to be reasonable.
 * anything smaller is a heavy burden.
 * For 1pF resolution, need 1Mohm resistor.
 * 1us timer with uint32_t warps every 1.2 hours.
 * Systick reg warps every 24ms.
 * uint32_t cannot cover 5pF all the way up to 4700uF.
 * may use uint64_t or have a seperate mF instead.
 *
 * Summary:
 * SysTick INT: count ms for delay functions. needs highest priority.
 * ACMP INT: record measurement and resets the circuit. needs low priority since may depend on delay.
 * Main Loop: call report every INTERVAL_MS ms.
 */

volatile struct {
  bool     updating;
  bool     fast;
  uint64_t total;
  unsigned samples;
} data = {
  .updating = false, // TODO: change to sending
  .fast     = false,
  .total    = 0UL,
  .samples  = 0,
};

volatile static uint32_t systime_ms = 0;

void SysTick_Handler(void) {
  systime_ms ++;
}

#define CYCLE_PER_US (__SYSTEM_CLOCK/1000000)
uint32_t capmeter_timer_get_us(void) {
  return systime_ms * 1000 + 1000 - (SysTick->VAL / CYCLE_PER_US);
}

#define CYCLE_PER_MS (__SYSTEM_CLOCK/1000)
void capmeter_timer_init(void) {
  // SysTick Millisecond Timer
  while ((SysTick_Config(CYCLE_PER_MS)) != 0); // 1ms interval
  NVIC_SetPriority(SysTick_IRQn, 0); // highest priority
}

void capmeter_delay(uint32_t ms) {
  ms += systime_ms;
  while(systime_ms < ms);
}

#if 0
void capmeter_timer_handler(void) {
  if (data.updating) {
    return;
  }

  if (data.samples == 0) {
    if (!data.fast) {
      data.fast = true;
    }
    return;
  }

  if ((data.samples >= SAMPLES_MAX) && data.fast) {
    data.fast = false;
  }

  // TODO: call send data and reset data
}
#endif

void capmeter_swm_init(void) {
  // Enable SWM
  LPC_SYSCON->SYSAHBCLKCTRL |= (1<<7);

  // Enable UART
  LPC_SWM->PINASSIGN0 = 0xffff0405UL;

  // Enable ACMP
  LPC_SWM->PINENABLE0 = 0xfffffffcUL;

  // ACMP PIN IN TODO: why discharge reference?!
  LPC_GPIO_PORT->CLR0 = (_BV(GPIO_ACMP_I1) | _BV(GPIO_ACMP_I2) | _BV(GPIO_R220) | _BV(GPIO_R1M));
  LPC_GPIO_PORT->DIR0 = (_BV(GPIO_ACMP_I1) | _BV(GPIO_ACMP_I2));
}

void capmeter_discharge(void) {
  // Enable GPIO on comparator PINs
  LPC_SWM->PINENABLE0 = 0xffffffffUL;

  // All PIN LOW & OUT
  LPC_GPIO_PORT->CLR0 = (_BV(GPIO_ACMP_I1) | _BV(GPIO_ACMP_I2) | _BV(GPIO_R220) | _BV(GPIO_R1M));
  LPC_GPIO_PORT->DIR0 = (_BV(GPIO_ACMP_I1) | _BV(GPIO_ACMP_I2) | _BV(GPIO_R220) | _BV(GPIO_R1M));

  // ACMP PIN IN
  LPC_GPIO_PORT->DIR0 = (_BV(GPIO_ACMP_I1) | _BV(GPIO_ACMP_I2));

  // Enable ACMP
  LPC_SWM->PINENABLE0 = 0xfffffffcUL;
}

void capmeter_charge(bool fast) {
  // GPIO High
  // Take time snapshot
}

// TODO: split int uf and pf to avoid long div
void capmeter_send_data(uint64_t pf) {
  // Format: XXXX.XX {u|n|p}F\r
  char *msg = "   0.00 pF\r";
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

  msg[8] = unit;

  uart0Send(msg, sizeof(msg));
}

int main(void) {
  capmeter_swm_init();
  uart0Init(115200);
  capmeter_timer_init();

  uint64_t i = 0;
  while(1) {
    // TODO: use interrupt or loop for ACMP?
    // TODO: use interrupt or loop for reporting?
    capmeter_delay(250);

    i += 5;
    capmeter_send_data(i);
  }
}
