#ifndef _NOS_TIME_H
#define _NOS_TIME_H

#include <stdint.h>

typedef long time_t;

struct tm {
    int tm_sec;   /* seconds [0,60] */
    int tm_min;   /* minutes [0,59] */
    int tm_hour;  /* hour [0,23] */
    int tm_mday;  /* day of month [1,31] */
    int tm_mon;   /* month of year [0,11] */
    int tm_year;  /* years since 1900 */
    int tm_wday;  /* day of week [0,6] (Sunday=0) */
    int tm_yday;  /* day of year [0,365] */
    int tm_isdst; /* daylight savings flag */
};

time_t time(time_t *t);

#endif
