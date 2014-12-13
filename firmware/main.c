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


typedef struct {
  uint8_t red, green, blue;
  bool status;
} state_t;

state_t global_state;


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
      global_state.red = 0;
      global_state.green = 0;
      global_state.blue = 0;
      global_state.status = false;
      // fall-through

    // Make updates take effect
    case 1:
      set_status_led(global_state.status);
      ws2812b_set_rgb(global_state.red, global_state.green, global_state.blue);

    // Update channel values
    case 2:
      global_state.status = (0 != rq->wValue.word);
      return 0;
    case 3:
      global_state.red = rq->wValue.word & 0xff;
      return 0;
    case 4:
      global_state.green = rq->wValue.word & 0xff;
      return 0;
    case 5:
      global_state.blue = rq->wValue.word & 0xff;
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

