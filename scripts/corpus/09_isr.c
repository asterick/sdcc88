/* interrupt handlers + critical sections */
volatile unsigned char hw;
int counter;

void isr_simple(void) __interrupt {
  counter++;
}

void isr_calls(void) __interrupt {
  extern void handler(void);
  hw = 1;
  handler();
}

void isr_heavy(void) __interrupt {
  int a = counter;
  int b = a * 3;
  counter = b + hw;
}

void crit_block(void) {
  __critical {
    counter++;
  }
}

void crit_fn(void) __critical {
  hw = 0;
  counter = hw;
}
