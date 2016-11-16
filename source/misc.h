#ifndef MISC_H
#define MISC_H

#include <csp/arch/csp_semaphore.h>

csp_mutex_t bno_lock;

void blink(int pin);

#endif