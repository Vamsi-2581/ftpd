#ifndef FTPD_JID_H
#define FTPD_JID_H
/*
** FTPD job ID shape check
**
** The job ID of RETR and DELE in the JES mode comes from the client and goes
** to jesjob(), which loads every job a query such as JOB* matches before the
** caller sees that none is the one named (#267).  The shape is checked first,
** free of project and MVS headers so a host unit test can link it
** (test/tstjid.c).
*/

/* Is id a JES2 job ID of MVS 3.8j: JOB, STC or TSU followed by exactly five
** digits, nothing before or after?  The caller upper-cases id.  NULL and ""
** are not job IDs. */
int ftpd_jid_valid(const char *id)                          asm("FTPJIDOK");

#endif /* FTPD_JID_H */
