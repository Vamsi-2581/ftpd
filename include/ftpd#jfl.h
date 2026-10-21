#ifndef FTPD_JFL_H
#define FTPD_JFL_H
/*
** FTPD JESSTATUS filter
**
** SITE JESSTATUS= takes ALL, INPUT, ACTIVE or OUTPUT, as on z/OS, and the
** JES mode of LIST, RETR and DELE shows only the jobs in that state (#207).
** Parsing the value and matching a job against it live here, free of project
** and MVS headers so a host unit test can link them (test/tstjfl.c).
*/

/* Canonical spelling of a SITE JESSTATUS= value ("ALL", "INPUT", "ACTIVE" or
** "OUTPUT"), whatever its case; an empty value means "ALL".  NULL for
** anything else.  The result is a string constant. */
const char *ftpd_jfl_name(const char *val)                  asm("FTPJFLNM");

/* Does a job whose state is job_status ("ACTIVE", "INPUT", "OUTPUT", or one
** of the transitional states such as "XMIT") pass the filter?  filter is a
** canonical name from ftpd_jfl_name(); NULL or empty is the same as "ALL". */
int ftpd_jfl_match(const char *filter, const char *job_status)
                                                            asm("FTPJFLMT");

#endif /* FTPD_JFL_H */
