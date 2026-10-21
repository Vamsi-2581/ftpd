#ifndef FTPD_RET_H
#define FTPD_RET_H
/*
** FTPD Retention Check
**
** One question, asked before a data set is opened for output: does its DSCB
** carry an expiration date that has not passed?  If it does, OPEN does not
** write -- it raises IEC507D on the console and the calling thread waits for
** a reply nobody sends (#198).  FTPD answers 550 instead, and this is the part
** that decides and formats, kept free of project and MVS headers so a host
** unit test can link it (test/tstret.c).
*/

typedef struct ftpd_ret_date {
    int year;               /* full year, 2026                             */
    int month;              /* 1-12                                        */
    int day;                /* 1-31                                        */
    int yday;               /* day of the year, 1-366                      */
    int permanent;          /* 99365/99366: the data set never expires     */
} ftpd_ret_date_t;

/*
** Is the data set still retained?
**
** expdt     -- DS1EXPDT, the three bytes of the format-1 DSCB: the year,
**              then the day of the year as a big-endian halfword.  MVS 3.8j
**              keeps two-digit years there (X'1A0143' is 2026.323), so a year
**              byte below 100 is 20yy; a byte of 100 or more is read as the
**              year since 1900.  All zero is "no expiration date".
** cur_year  -- today's full year, from the local clock (the clock OPEN uses).
** cur_yday  -- today's day of the year, 1-366.
** until     -- may be NULL; filled when the answer is 1.
**
** Returns 1 when OPEN for output would ask the operator, 0 otherwise.  A data
** set expires ON its expiration date: measured on MVS 3.8j, an expdt equal to
** today opens without a reply and one day later does not.  Year 99 with day
** 365 or 366 is the permanent retention that never expires.  An expdt with a
** day outside 1-366 is not a date; it counts as "no expiration date" so that
** the check never refuses on bytes it cannot read.
*/
int ftpd_ret_unexpired(const unsigned char expdt[3], int cur_year,
                       int cur_yday, ftpd_ret_date_t *until)
                                                           asm("FTPRETUN");

/*
** A whole decimal number 0..max, for the MAXRETPD value in FTPDPRM (#201).
**
** atoi() maps "abc", "" and "-5" to 0 or a negative number; for MAXRETPD 0 is
** a real setting (no retention at all), so a typo must not land on it.
** Returns 1 and sets *out when s is only digits and not above max; returns 0
** and leaves *out alone otherwise.
*/
int ftpd_ret_parse_days(const char *s, int max, int *out)
                                                           asm("FTPRETPD");

#endif /* FTPD_RET_H */
