/* TSTUER.C
** Tests for ftpd_ufs_rc_to_ftp() -- the UFSD return code to FTP reply
** mapping behind ftpd_ufs_error() (issue #151).
**
** UFSD_RC_NOINODES comes from allocating an inode, which only a create does
** (STOR/APPE through ufs_fopen(), MKD).  INODES is fixed when the filesystem
** is formatted, so a full inode table stays full until a reformat: a 4xx
** reply invites the retry that meets it again.  The cases pin NOINODES to
** 552 and every other code to the reply it had before.
**
** ftpd#urc.c is free of project and MVS headers, so this is a DUAL test.
*/
#include <stdio.h>
#include <string.h>

#include "ufsdrc.h"
#include "ftpd#rep.h"
#include "ftpd#urc.h"
#include <mbtcheck.h>

int
main(void)
{
    printf("=== FTPD UFSD return code mapping tests (#151) ===\n");

    CHECK(ftpd_ufs_rc_to_ftp(UFSD_RC_NOINODES) == FTP_552,
          "NOINODES is permanent: 552, not a retryable 452");

    CHECK(ftpd_ufs_rc_to_ftp(UFSD_RC_OK) == FTP_250, "OK -> 250");
    CHECK(ftpd_ufs_rc_to_ftp(UFSD_RC_NOFILE) == FTP_550, "NOFILE -> 550");
    CHECK(ftpd_ufs_rc_to_ftp(UFSD_RC_EXIST) == FTP_550, "EXIST -> 550");
    CHECK(ftpd_ufs_rc_to_ftp(UFSD_RC_NOTDIR) == FTP_550, "NOTDIR -> 550");
    CHECK(ftpd_ufs_rc_to_ftp(UFSD_RC_ISDIR) == FTP_550, "ISDIR -> 550");
    CHECK(ftpd_ufs_rc_to_ftp(UFSD_RC_NOSPACE) == FTP_452,
          "NOSPACE stays 452 here (STOR answers 552 itself)");
    CHECK(ftpd_ufs_rc_to_ftp(UFSD_RC_IO) == FTP_451, "IO -> 451");
    CHECK(ftpd_ufs_rc_to_ftp(UFSD_RC_BADFD) == FTP_451, "BADFD -> 451");
    CHECK(ftpd_ufs_rc_to_ftp(UFSD_RC_NOTEMPTY) == FTP_550, "NOTEMPTY -> 550");
    CHECK(ftpd_ufs_rc_to_ftp(UFSD_RC_NAMETOOLONG) == FTP_553,
          "NAMETOOLONG -> 553");
    CHECK(ftpd_ufs_rc_to_ftp(UFSD_RC_ROFS) == FTP_550, "ROFS -> 550");
    CHECK(ftpd_ufs_rc_to_ftp(UFSD_RC_EACCES) == FTP_550, "EACCES -> 550");
    CHECK(ftpd_ufs_rc_to_ftp(UFSD_RC_NOREQ) == FTP_451,
          "an unlisted code -> 451");
    CHECK(ftpd_ufs_rc_to_ftp(-1) == FTP_451, "an unknown code -> 451");

    CHECK(strcmp(ftpd_ufs_rc_message(UFSD_RC_NOINODES),
                 "No inodes available") == 0,
          "NOINODES message is unchanged");

    return mbt_test_summary("TSTUER");
}
