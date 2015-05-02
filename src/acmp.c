#include "acmp.h"

void acmp_init(void) {
  // Power up and enbale
  LPC_SYSCON->PDRUNCFG &= ~( (1 << 15) | (1 << 3) );
  LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 19);

  // Reset the block
  LPC_SYSCON->PRESETCTRL &= ~(1 << 12);
  LPC_SYSCON->PRESETCTRL |=  (1 << 12);

  // Setup PINs
  LPC_IOCON->PIO0_0   &= ~(0x3 << 3);
  LPC_IOCON->PIO0_1   &= ~(0x3 << 3);
  LPC_SWM->PINENABLE0 &= ~(0x3 << 0);

  // Async output (why get 2 FFs in the way anyway?)
  LPC_CMP->CTRL &= ~(1 << 6);

  // No hysteresis by default
  acmp_set_hysteresis(0);
}

void acmp_set_input(uint32_t input_p, uint32_t input_n) {
  LPC_CMP->CTRL &= ~((0x7 << 8) | (0x7 << 11));
  LPC_CMP->CTRL |= ((0x7 & input_p) << 8);
  LPC_CMP->CTRL |= ((0x7 & input_n) << 11);
}

void acmp_set_hysteresis(uint32_t level) {
  LPC_CMP->CTRL &= ~(0x3 << 25);
  LPC_CMP->CTRL |= (level << 25);
}
