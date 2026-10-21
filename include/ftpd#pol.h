#ifndef FTPD_POL_H
#define FTPD_POL_H
/*
** FTPD Worker Pool Size
**
** The libc370 thread manager starts no worker for a pool of 1 to 3 threads:
** connections are accepted and queued, nothing ever wakes
** (mvslovers/libc370#700, ftpd#168).  Until that is fixed in libc370 the pool
** is never smaller than FTPD_POOL_MIN; the session limit stays MAXSESSIONS.
**
** Free of project and MVS headers so a host unit test can link it
** (test/tstpol.c).
*/

/* Smallest pool that starts workers.  Remove together with ftpd_pool_size()
** once libc370#700 is fixed and FTPD requires that libc370. */
#define FTPD_POOL_MIN   4

/* Worker threads to create for a session limit of max_sessions. */
int ftpd_pool_size(int max_sessions)                        asm("FTPPOLSZ");

#endif /* FTPD_POL_H */
