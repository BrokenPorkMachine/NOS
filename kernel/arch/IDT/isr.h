#pragma once
void isr_timer_handler(const void *hw_frame);
/* Arm or disarm the tiny init watchdog.
 * Pass number of timer ticks before panic; 0 disarms.
 */
void arm_init_watchdog(unsigned ticks);
