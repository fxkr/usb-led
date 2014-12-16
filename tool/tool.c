#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libusb.h>

#define VID 0xF055
#define PID 0x49D9


uint16_t str_to_uint16(char *str) {
  errno = 0;
  char *endptr;
  long val = strtol(str, &endptr, 0);
  if (errno != 0) {
    return 0;
  } else if (*endptr != '\0') {
    errno = EINVAL;
    return 0;
  } else if (val < 0 || val > 65535) {
    errno = EINVAL;
    return 0;
  }
  return val;
}


libusb_device_handle* open_device() {
  libusb_init(NULL);
  libusb_device_handle *hDev = libusb_open_device_with_vid_pid(NULL, VID, PID);

  if (hDev == NULL) {
    printf("error: device not found\n");
    return NULL;
  }

  if (libusb_kernel_driver_active(hDev,0)) {
    printf("detaching device from kernel\n");
    int ret = libusb_detach_kernel_driver(hDev,0);
    if (ret < 0 )
    {
      printf("error: %s\n", libusb_error_name(ret));
      return NULL;
    }
  }

  return hDev;
}


bool perform_control_transfer(libusb_device_handle *hDev,
  uint8_t request, uint16_t value, uint16_t index)
{
  int ret = libusb_control_transfer(
      hDev,
      LIBUSB_ENDPOINT_OUT
        | LIBUSB_REQUEST_TYPE_VENDOR
        | LIBUSB_RECIPIENT_DEVICE,
      request,
      value,
      index,
      NULL,  // data
      0,  // data length
      250);  // timeout in ms

  if (ret < 0) {
    printf("error: %s\n", libusb_error_name(ret));
    return NULL;
  }

  return true;
}

int main(int argc, char** argv)
{
  if (argc == 2 && 0 == strcmp("off", argv[1])) {
    libusb_device_handle *hDev = open_device();
    if (hDev == NULL) return 1;
    if (!perform_control_transfer(hDev, 0, 0, 0)) return 1;  // Turn off immediately
    return 0;

  } else if (argc == 5 && 0 == strcmp("set", argv[1])) {

    int e = 0;
    uint16_t r = str_to_uint16(argv[2]); e |= errno;
    uint16_t g = str_to_uint16(argv[3]); e |= errno;
    uint16_t b = str_to_uint16(argv[4]); e |= errno;
    if (e != 0) {
      printf("error: values must be numbers in range 0-65535\n");
      return 1;
    }

    libusb_device_handle *hDev = open_device();
    if (hDev == NULL) return 1;
    if (!perform_control_transfer(hDev, 3, r, 0)) return 1;  // Set red
    if (!perform_control_transfer(hDev, 4, g, 0)) return 1;  // Set green
    if (!perform_control_transfer(hDev, 5, b, 0)) return 1;  // Set blue
    if (!perform_control_transfer(hDev, 1, 0, 0)) return 1;  // Commit
    return 0;

  } else if ((argc == 5 || argc == 6) && 0 == strcmp("fade", argv[1])) {
    int speed;
    if (argc == 6) {
      speed = str_to_uint16(argv[5]);
      if (errno != 0) {
        printf("error: values must be numbers in range 0-65535\n");
        return 1;
      }
    } else {
      speed = 256;
    }

    int e = 0;
    uint16_t r = str_to_uint16(argv[2]); e |= errno;
    uint16_t g = str_to_uint16(argv[3]); e |= errno;
    uint16_t b = str_to_uint16(argv[4]); e |= errno;
    if (e != 0) {
      printf("error: values must be numbers in range 0-65535\n");
      return 1;
    }

    libusb_device_handle *hDev = open_device();
    if (hDev == NULL) return 1;
    if (!perform_control_transfer(hDev, 9, speed, 0)) return 1;  // Set fade speed
    if (!perform_control_transfer(hDev, 6, r, 0)) return 1;  // Fade red to r
    if (!perform_control_transfer(hDev, 7, g, 0)) return 1;  // Fade green to g
    if (!perform_control_transfer(hDev, 8, b, 0)) return 1;  // Fade blue to b
    return 0;

  } else if (argc == 3 && 0 == strcmp("blink", argv[1]) && 0 == strcmp("off", argv[2])) {

    libusb_device_handle *hDev = open_device();
    if (hDev == NULL) return 1;
    if (!perform_control_transfer(hDev, 10, 0, 0)) return 1;  // Disable blinking
    return 0;

  } else if ((argc == 3 || argc == 4) && 0 == strcmp("blink", argv[1])) {
    long duty = 0, period = 0;

    duty = str_to_uint16(argv[2]);
    if (errno != 0 || duty < 0 || duty > 65535) {
      printf("error: values must be numbers in range 0-65535\n");
      return 1;
    }

    if (argc == 4) {
      period = str_to_uint16(argv[3]);
      if (errno != 0 || period < 0 || period > 65535) {
        printf("error: values must be numbers in range 0-65535\n");
        return 1;
      } else if (duty > period) {
        printf("error: duty time must be less than or equal to period\n");
        return 1;
      }
    } else {
      period = duty;
      duty /= 2;
    }

    libusb_device_handle *hDev = open_device();
    if (hDev == NULL) return 1;
    if (!perform_control_transfer(hDev, 10, duty, period)) return 1;  // Set blink params
    return 0;

  } else if (argc == 3 && 0 == strcmp("status", argv[1]) && 0 == strcmp("on", argv[2])) {
    libusb_device_handle *hDev = open_device();
    if (hDev == NULL) return 1;
    if (!perform_control_transfer(hDev, 2, 1, 0)) return 1;  // Set status
    if (!perform_control_transfer(hDev, 1, 0, 0)) return 1;  // Commit
    return 0;

  } else if (argc == 3 && 0 == strcmp("status", argv[1]) && 0 == strcmp("off", argv[2])) {
    libusb_device_handle *hDev = open_device();
    if (hDev == NULL) return 1;
    if (!perform_control_transfer(hDev, 2, 0, 0)) return 1;  // Set status
    if (!perform_control_transfer(hDev, 1, 0, 0)) return 1;  // Commit
    return 0;

  } else {
    printf("usage:\n");
    printf("  set <r> <g> <b>\n");
    printf("  fade <r> <g> <b> [<speed>]\n");
    printf("  status (on|off)\n");
    printf("  blink <duty-ms> [<period-ms>]\n");
    printf("  blink off\n");
    printf("  off\n");
    return 1;
  }
}

