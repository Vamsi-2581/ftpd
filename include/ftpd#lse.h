#ifndef FTPD_LSE_H
#define FTPD_LSE_H
/*
** FTPD List Builder Result
**
** __listds(), __listpd() and the other libc370 list builders answer NULL both
** for an empty result and for a failure, and errno tells them apart: 0 for
** empty (cleared on entry), ENOMEM when storage ran out (libc370 #61).  A
** caller that tests only the pointer reads a storage shortage as "nothing
** there" -- an empty listing and 226 (#118).
**
** The call site clears errno, makes the call, and copies errno into a local
** before any other service can overwrite it.  This is the one decision made
** from the pair, kept free of project and MVS headers so a host test can
** link it (test/tstlse.c).
*/

/*
** Did a list builder fail for lack of storage?
**
** list is the pointer it returned, err the errno copied right after the call.
** Only NULL with ENOMEM counts: a list that came back is complete whatever
** errno holds, and any other NULL is an empty result.
*/
int ftpd_list_nomem(const void *list, int err);

#endif /* FTPD_LSE_H */
