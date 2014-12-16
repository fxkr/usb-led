#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdint.h>
#include <util/delay.h>

#include "usbconfig.h"
#include "usbdrv/usbdrv.h"

#include "osccal.h"
#include "ws2812b.h"
#include "timer.h"


#define STATUS_LED_PIN      PB4
#define STATUS_LED_PORT     PORTB
#define STATUS_LED_DDR      DDRB
#define STATUS_LED_DDR_MASK (1 << STATUS_LED_PIN)


typedef struct {
  // Buffered red/green/blue channel and status led values
  uint16_t red, green, blue;
  bool status;

  // Fading parameters
  uint16_t red_target, green_target, blue_target, fade_rate;

  // Blinking parameters
  uint16_t blink_duty, blink_period;
} state_t;


state_t global_state = {
  .fade_rate = 256,
};


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
  // Verify checksum. V-USB doesn't do it.
  // (Yes, this out-of-bounds access is ok.)
  if (usbCrc16(setupData, 8 + 2) != 0x4FFE) {
    return 0;  // CRC error; ignore packet
  }

  usbRequest_t *rq = (void *)setupData;

  switch(rq->bRequest){

    // Turn everything off
    case 0:
      global_state.red = 0;
      global_state.green = 0;
      global_state.blue = 0;
      global_state.red_target = 0;
      global_state.green_target = 0;
      global_state.blue_target = 0;
      global_state.status = false;
      // fall-through

    // Make updates take effect (and stops all fading)
    case 1:
      set_status_led(global_state.status);
      ws2812b_set_rgb(global_state.red, global_state.green, global_state.blue);
      global_state.red_target = global_state.red;
      global_state.green_target = global_state.green;
      global_state.blue_target = global_state.blue;

    // Status LED
    case 2:
      global_state.status = (0 != rq->wValue.word);
      return 0;

    // Set channel values immediately (and stops all fading)
    case 3:
      global_state.red = rq->wValue.word;
      global_state.red_target = global_state.red;
      global_state.green_target = global_state.green;
      global_state.blue_target = global_state.blue;
      return 0;
    case 4:
      global_state.green = rq->wValue.word;
      global_state.red_target = global_state.red;
      global_state.green_target = global_state.green;
      global_state.blue_target = global_state.blue;
      return 0;
    case 5:
      global_state.blue = rq->wValue.word;
      global_state.red_target = global_state.red;
      global_state.green_target = global_state.green;
      global_state.blue_target = global_state.blue;
      return 0;

    // Fade channel values
    case 6:
      global_state.red_target = rq->wValue.word;
      return 0;
    case 7:
      global_state.green_target = rq->wValue.word;
      return 0;
    case 8:
      global_state.blue_target = rq->wValue.word;
      return 0;

    // Fade speed (16-bit-value per millisec)
    case 9:
      if (rq->wValue.word > 0) {
        global_state.fade_rate = rq->wValue.word;
      } else {
        global_state.red_target = global_state.red;
        global_state.green_target = global_state.green;
        global_state.blue_target = global_state.blue;
      }
      return 0;

    // Blink rate speed (16-bit-value per millisec)
    case 10:
      global_state.blink_duty = rq->wValue.word;
      global_state.blink_period = rq->wIndex.word;
      if (global_state.blink_period == 0) {
        ws2812b_set_rgb(global_state.red, global_state.green, global_state.blue);
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

// fade_to makes channels value closer to target.
// Returns true if *channel was changed, false otherwise.
bool fade_to(uint16_t *channel, uint16_t target) {

  // No fading necessary?
  if (*channel == target) {
    return false;

  // Fade up or down?
  } else if (*channel < target) {
    uint16_t delta = target - *channel;
    if (delta > global_state.fade_rate) delta = global_state.fade_rate;
    *channel += delta;
  } else {
    uint16_t delta = *channel - target;
    if (delta > global_state.fade_rate) delta = global_state.fade_rate;
    *channel -= delta;
  }

  return true;
}

int main(void) {

  // Set clock prescaler to 1 (so we'll run at 16 MHz; we'll switch to 16.5 MHz later).
  // Not actually necessary if the CKDIV8 fuse is programmed.
  CLKPR = 1<<CLKPCE;  // enable change
  CLKPR = 0;  // set factor to 1

  // Configure indicator LED pin
  STATUS_LED_DDR |= STATUS_LED_DDR_MASK;

  // Make sure the big LED is off
  ws2812b_set_rgb(0,0,0);

  // Blink small led as boot indication
  set_status_led(true);
  _delay_ms(33);
  set_status_led(false);
  _delay_ms(33);
  set_status_led(true);
  _delay_ms(33);
  set_status_led(false);

  timer_init();

  // Initialize USB and reenumerate
  usbInit();
  usbDeviceDisconnect();
  _delay_ms(250);
  usbDeviceConnect();
  sei(); // Enable interrupts. By now, all other initialization should be done.
  usbPoll();

  while (1) {
    usbPoll();

    // New millisecond?
    time_val_t now = timer_get();
    if (now.updated) {
      bool update = false;

      // Fading
      update |= fade_to(&global_state.red, global_state.red_target);
      update |= fade_to(&global_state.green, global_state.green_target);
      update |= fade_to(&global_state.blue, global_state.blue_target);

      // Blinking
      uint16_t r = global_state.red, g = global_state.green, b = global_state.blue;
      if (global_state.blink_period != 0) {
        if (now.time % global_state.blink_period == 0) {
          update = true;
        } else if (now.time % global_state.blink_period == global_state.blink_duty) {
          r = 0;
          g = 0;
          b = 0;
          update = true;
        }
      }

      if (update) {
        ws2812b_set_rgb(r, g, b);
      }

      // Status LED
      if (now.time % 1000 == 0) {
        set_status_led(true);
      } else if (now.time % 1000== 10) {
        set_status_led(false);
      }
    }
  }

  return 0;
}

