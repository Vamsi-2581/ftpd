/*
** FTPD Retention Check
**
** See include/ftpd#ret.h.
*/
#include <string.h>

#include "ftpd#ret.h"

static int
month_days(int year, int month)
{
    switch (month) {
    case 2:
        return ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
            ? 29 : 28;
    case 4: case 6: case 9: case 11:
        return 30;
    default:
        return 31;
    }
}

/* --------------------------------------------------------------------
** Is the data set still retained?  See ftpd#ret.h for the contract.
** ----------------------------------------------------------------- */
int
ftpd_ret_unexpired(const unsigned char expdt[3], int cur_year, int cur_yday,
                   ftpd_ret_date_t *until)
{
    int year;
    int yday;
    int permanent;
    int m;
    int left;

    year = expdt[0] < 100 ? 2000 + expdt[0] : 1900 + expdt[0];
    yday = (expdt[1] << 8) | expdt[2];

    if (yday < 1 || yday > 366)
        return 0;

    permanent = (expdt[0] == 99 && yday >= 365);

    if (!permanent) {
        if (year < cur_year)
            return 0;
        if (year == cur_year && yday <= cur_yday)
            return 0;
    }

    if (until) {
        memset(until, 0, sizeof(*until));
        until->year = year;
        until->yday = yday;
        until->permanent = permanent;

        left = yday;
        for (m = 1; m < 12 && left > month_days(year, m); m++)
            left -= month_days(year, m);
        until->month = m;
        until->day = left;
    }

    return 1;
}
