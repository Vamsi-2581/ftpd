#ifndef FTPD_URC_H
#define FTPD_URC_H
/*
** FTPD UFSD Return Code Mapping
**
** Free of project and MVS headers on purpose: the mapping is pure, and a
** host unit test can link it (test/tstuer.c).
*/

/*
** Map UFSD return code to FTP reply code.
*/
int ftpd_ufs_rc_to_ftp(int ufsd_rc)                        asm("FTPUFRC");

/*
** Get human-readable error message for UFSD return code.
*/
const char *ftpd_ufs_rc_message(int ufsd_rc)                asm("FTPUFMSG");

#endif /* FTPD_URC_H */
