#ifndef _TIMER_H
#define _TIMER_H

typedef struct {
  unsigned long time;
  bool updated;
} time_val_t;

void timer_init();
time_val_t timer_get();

#endif
