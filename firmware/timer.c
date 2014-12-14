#include <stdbool.h>
#include <util/atomic.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include "timer.h"


volatile time_val_t time_val;


/* Triggered every ~1ms. */
ISR(TIMER1_COMPA_vect)
{
  time_val.time++;
  time_val.updated = true;
}

void timer_init()
{
  // Clear Timer on Compare (CTC) (with OCR1C) | /128 Prescaler
  TCCR1 = (1<<CTC1) | (1<<CS13);

  // Compare match
  // #Counts = 16.5 MHz (Clock) / 128 (Prescaler) / 1000 (ms/s) = 128.90625 = 129
  OCR1A = 129 - 1;  // Have compare match after 129 counts (0...128)
  OCR1C = 129 - 1;  // Reset to 0 after 129 counts (note: not an overflow)

  // Enable Compare Match interrupt
  TIMSK |= (1 << OCIE1A);
}

bool timer_updated()
{
  return time_val.updated;
}

/* Returns current timer value with millisecond precision.
 *
 * Overflows to 0 after 2^32 ms (~ 49 days).
 */
time_val_t timer_get()
{
    time_val_t result;
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
      result.time = time_val.time;
      result.updated = time_val.updated;
      time_val.updated = false;
    }
    return result;
}

