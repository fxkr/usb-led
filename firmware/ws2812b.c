#include <avr/interrupt.h>
#include <util/delay.h>

#include "ws2812b.h"


#define WS2812B_LED_PIN        3
#define WS2812B_LED_DDR_MASK   (1 << WS2812B_LED_PIN)
#define WS2812B_LED_PORT       PORTB
#define WS2812B_LED_DDR        DDRB


/*
 * Send a single byte representing a channel value to a WS2812B.
 *
 * Bytes are sent MSB first. Each bit is sent as a 1250ns signal of which
 * the first 350ns ("0") or 900ns ("1") are high and the rest is low (+-150ns each).
 * At 16.5 MHz (for which this function was written) each clock cycle takes 60.6ns.
 */
__attribute__((optimize(0)))
__attribute__((always_inline))
static inline void ws2812b_send_byte(uint8_t byte, uint8_t hiMask, uint8_t loMask) {
  uint8_t bitNum;

  #if F_CPU!=16500000
  #error('requires 16.5 MHz clock period')
  #endif

  __asm volatile(
    "       ldi  %[bitNum],8   \n\t"  // For 8 bits
    "next_bit%=:               \n\t"  // Begin
    "       out  %[port],%[hi] \n\t"  //   Rising Flank
    "       rjmp .+0           \n\t"
    "       rjmp .+0           \n\t"
    "       sbrs %[byte],7     \n\t"  //   If MSB Set (else nop)
    "       out  %[port],%[lo] \n\t"  //     Early Falling Flank (means "0")
    "       lsl  %[byte]       \n\t"  //   Upshift (promote next bit to MSB)
    "       nop                \n\t"
    "       rjmp .+0           \n\t"
    "       rjmp .+0           \n\t"
    "       rjmp .+0           \n\t"
    "       out  %[port],%[lo] \n\t"  //   Late Falling Flank (means "1")
    "       rjmp .+0           \n\t"
    "       dec  %[bitNum]     \n\t"  //   Next Bit
    "       brne next_bit%=    \n\t"  // End For

    : [bitNum] "=&d" (bitNum)
    : [byte]   "r"   (byte),
      [port]   "I"   (_SFR_IO_ADDR(WS2812B_LED_PORT)),
      [hi]     "r"   (hiMask),
      [lo]     "r"   (loMask)
  );
}

/*
 * Set the WS2812Bs color.
 *
 * It expects 3 bytes, which are (in this order) the green/red/blue
 * channel values. For more details, see the send_byte function.
 * (Note: WS2812Bs can be daisy-chained together.)
 */
void ws2812b_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t pinMask = WS2812B_LED_DDR_MASK;

  WS2812B_LED_DDR |= pinMask;

  uint8_t hiMask, loMask;
  uint8_t sreg_prev;

  hiMask = pinMask;
  loMask = ~hiMask & WS2812B_LED_PORT;
  hiMask |= WS2812B_LED_PORT;

  // Disable interrupts
  sreg_prev = SREG;
  cli();

  // WS2812B uses GRB channel order
  ws2812b_send_byte(g, hiMask, loMask);
  ws2812b_send_byte(r, hiMask, loMask);
  ws2812b_send_byte(b, hiMask, loMask);

  // Reenable interrupts that were enabled
  SREG = sreg_prev;

  // Update takes effect after 50us silence.
  _delay_us(50);
}

