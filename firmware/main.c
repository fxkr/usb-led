#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdint.h>
#include <util/delay.h>

#include "usbconfig.h"
#include "usbdrv/usbdrv.h"

#include "osccal.h"
#include "ws2812b.h"


#define STATUS_LED_PIN      PB4
#define STATUS_LED_PORT     PORTB
#define STATUS_LED_DDR      DDRB
#define STATUS_LED_DDR_MASK (1 << STATUS_LED_PIN)


/* Turn the green status LED on/off. */
void set_status_led(bool on_off)
{
  if (on_off) {
    STATUS_LED_PORT &= ~(1 << STATUS_LED_PIN);
  } else {
    STATUS_LED_PORT |= (1 << STATUS_LED_PIN);
  }
}

// usbFunctionSetup handles USB Control Transfers.
extern usbMsgLen_t usbFunctionSetup(uchar setupData[8])
{
  usbRequest_t *rq = (void *)setupData;

  switch(rq->bRequest){

    // Turn everything off
    case 0:
      set_status_led(true);
      ws2812b_set_rgb(0,0,0);
      return 0;

    // Set status LED
    case 1:
      if (rq->wLength.word == 1) {
        set_status_led(rq->wValue.word == 0);
      }
      return 0;

    // Set RGB LED
    case 2:
      if (rq->wLength.word == 3) {
        ws2812b_set_rgb(rq->wValue.bytes[0], rq->wValue.bytes[0], rq->wValue.bytes[0]);
      }
      return 0;

    // Ignore unknown requests
    default:
      return 0;
  }
}

// hadUsbReset calibrates the internal 16 MHz RC oscillator to run at the
// 16.5 MHz needed by V-USB after reset.
extern void hadUsbReset() {
  calibrateOscillatorASM();
}

int main(void) {

  // Set clock prescaler to 1 (so we'll run at 16 MHz; we'll switch to 16.5 MHz later).
  // Not actually necessary if the CKDIV8 fuse is programmed.
  CLKPR = 1<<CLKPCE;
  CLKPR = 0;

  // Configure indicator LED pin
  STATUS_LED_DDR |= STATUS_LED_DDR_MASK;

  // Make sure the big LED is off
  ws2812b_set_rgb(0,0,0);

  // Blink small led as boot indication
  set_status_led(true);
  _delay_ms(10);
  set_status_led(false);

  // Initialize USB
  usbInit();

  // Reenumerate
  usbDeviceDisconnect();
  _delay_ms(250);
  usbDeviceConnect();
  sei();
  usbPoll();

  while (1) {
    usbPoll();
  }

  return 0;
}

