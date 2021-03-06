#include <util/delay.h>

#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include "ws2812b.h"


#define WS2812B_LED_PIN        3
#define WS2812B_LED_DDR_MASK   (1 << WS2812B_LED_PIN)
#define WS2812B_LED_PORT       PORTB
#define WS2812B_LED_DDR        DDRB


// Gamma correction table. Maps 9 bit inputs to 8 bit outputs.
// Noteworthy properties:
//
//   1. Only one entry maps to 0x00 (off).
//   2. Uses the full intensity range (0x00-0xFF).
//   3. Covers 252 out of 255 possible outputs (183 with fixed LSB);
//      a good tradeoff between memory usage and resolution.
//
// The Attiny85 is an 8-bit MCU with 512 Byte SRAM: not enough RAM to
// hold it, and not even enough bits to address it in load instructions.
// So we use PROGMEM to put it in SRAM and pgm_read_byte_near to read from it.
PROGMEM const unsigned char GAMMA[512] = {
  0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, // 0x000
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, // 0x008
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, // 0x010
  0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, // 0x018
  0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, // 0x020
  0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, // 0x028
  0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, // 0x030
  0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, // 0x038
  0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, // 0x040
  0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06, 0x06, // 0x048
  0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07, // 0x050
  0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x08, 0x08, // 0x058
  0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, // 0x060
  0x09, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0b, // 0x068
  0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0c, 0x0c, 0x0c, // 0x070
  0x0c, 0x0c, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0e, // 0x078
  0x0e, 0x0e, 0x0e, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f, // 0x080
  0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x11, 0x11, // 0x088
  0x11, 0x11, 0x12, 0x12, 0x12, 0x12, 0x13, 0x13, // 0x090
  0x13, 0x13, 0x14, 0x14, 0x14, 0x14, 0x15, 0x15, // 0x098
  0x15, 0x15, 0x16, 0x16, 0x16, 0x16, 0x17, 0x17, // 0x0a0
  0x17, 0x17, 0x18, 0x18, 0x18, 0x19, 0x19, 0x19, // 0x0a8
  0x19, 0x1a, 0x1a, 0x1a, 0x1b, 0x1b, 0x1b, 0x1b, // 0x0b0
  0x1c, 0x1c, 0x1c, 0x1d, 0x1d, 0x1d, 0x1e, 0x1e, // 0x0b8
  0x1e, 0x1f, 0x1f, 0x1f, 0x20, 0x20, 0x20, 0x21, // 0x0c0
  0x21, 0x21, 0x22, 0x22, 0x22, 0x23, 0x23, 0x23, // 0x0c8
  0x24, 0x24, 0x24, 0x25, 0x25, 0x25, 0x26, 0x26, // 0x0d0
  0x26, 0x27, 0x27, 0x28, 0x28, 0x28, 0x29, 0x29, // 0x0d8
  0x29, 0x2a, 0x2a, 0x2b, 0x2b, 0x2b, 0x2c, 0x2c, // 0x0e0
  0x2d, 0x2d, 0x2d, 0x2e, 0x2e, 0x2f, 0x2f, 0x30, // 0x0e8
  0x30, 0x30, 0x31, 0x31, 0x32, 0x32, 0x33, 0x33, // 0x0f0
  0x33, 0x34, 0x34, 0x35, 0x35, 0x36, 0x36, 0x37, // 0x0f8
  0x37, 0x37, 0x38, 0x38, 0x39, 0x39, 0x3a, 0x3a, // 0x100
  0x3b, 0x3b, 0x3c, 0x3c, 0x3d, 0x3d, 0x3e, 0x3e, // 0x108
  0x3f, 0x3f, 0x40, 0x40, 0x41, 0x41, 0x42, 0x42, // 0x110
  0x43, 0x43, 0x44, 0x44, 0x45, 0x45, 0x46, 0x46, // 0x118
  0x47, 0x47, 0x48, 0x48, 0x49, 0x49, 0x4a, 0x4b, // 0x120
  0x4b, 0x4c, 0x4c, 0x4d, 0x4d, 0x4e, 0x4e, 0x4f, // 0x128
  0x50, 0x50, 0x51, 0x51, 0x52, 0x52, 0x53, 0x54, // 0x130
  0x54, 0x55, 0x55, 0x56, 0x57, 0x57, 0x58, 0x58, // 0x138
  0x59, 0x5a, 0x5a, 0x5b, 0x5b, 0x5c, 0x5d, 0x5d, // 0x140
  0x5e, 0x5f, 0x5f, 0x60, 0x60, 0x61, 0x62, 0x62, // 0x148
  0x63, 0x64, 0x64, 0x65, 0x66, 0x66, 0x67, 0x68, // 0x150
  0x68, 0x69, 0x6a, 0x6a, 0x6b, 0x6c, 0x6c, 0x6d, // 0x158
  0x6e, 0x6e, 0x6f, 0x70, 0x71, 0x71, 0x72, 0x73, // 0x160
  0x73, 0x74, 0x75, 0x76, 0x76, 0x77, 0x78, 0x78, // 0x168
  0x79, 0x7a, 0x7b, 0x7b, 0x7c, 0x7d, 0x7e, 0x7e, // 0x170
  0x7f, 0x80, 0x81, 0x81, 0x82, 0x83, 0x84, 0x84, // 0x178
  0x85, 0x86, 0x87, 0x88, 0x88, 0x89, 0x8a, 0x8b, // 0x180
  0x8c, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x90, 0x91, // 0x188
  0x92, 0x93, 0x94, 0x95, 0x95, 0x96, 0x97, 0x98, // 0x190
  0x99, 0x9a, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, // 0x198
  0xa0, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, // 0x1a0
  0xa7, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, // 0x1a8
  0xae, 0xaf, 0xb0, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, // 0x1b0
  0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, // 0x1b8
  0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc1, 0xc2, 0xc3, // 0x1c0
  0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, // 0x1c8
  0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, // 0x1d0
  0xd4, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, // 0x1d8
  0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, // 0x1e0
  0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec, 0xed, // 0x1e8
  0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf4, 0xf5, 0xf6, // 0x1f0
  0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfd, 0xfe, 0xff, // 0x1f8
};


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
  #error('requires 16.5 MHz clock frequency')
  #endif

  __asm volatile(
    "       ldi  %[bitNum],8   \n\t"  // For 8 bits
    "next_bit%=:               \n\t"  // Begin
    "       out  %[port],%[hi] \n\t"  //   Rising Flank
    "       rjmp .+0           \n\t"
    "       rjmp .+0           \n\t"
    "       sbrs %[byte],7     \n\t"  //   Skip following instruction if MSB is set:
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
void ws2812b_set_rgb(uint16_t r, uint16_t g, uint16_t b)
{
  // Gamma correction and resolution reduction (16 -> 9 -> 8 bit)
  uint8_t rb = (uint8_t)pgm_read_byte_near(GAMMA + (r>>7));
  uint8_t gb = (uint8_t)pgm_read_byte_near(GAMMA + (g>>7));
  uint8_t bb = (uint8_t)pgm_read_byte_near(GAMMA + (b>>7));

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
  ws2812b_send_byte(gb, hiMask, loMask);
  ws2812b_send_byte(rb, hiMask, loMask);
  ws2812b_send_byte(bb, hiMask, loMask);

  // Reenable interrupts that were enabled
  SREG = sreg_prev;

  // Update takes effect after 50us silence.
  _delay_us(50);
}

