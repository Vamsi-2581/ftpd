/* TSTREC.C
** Tests for ftpd_rec_maxdata() -- how much of a text line
** fits in one record (#207).  STOR used to cut an overlong line without a
** word and answer 250.
**
** ftpd#rec.c is free of project and MVS headers, so this is a DUAL test.
*/
#include <stdio.h>

#include "ftpd#rec.h"
#include <mbtcheck.h>

int
main(void)
{
    CHECK_EQ(ftpd_rec_maxdata(FTPD_REC_F, 80, 3120, 32767), 80,
             "F: LRECL");
    CHECK_EQ(ftpd_rec_maxdata(FTPD_REC_V, 255, 3120, 32767), 251,
             "V: LRECL less the RDW");
    CHECK_EQ(ftpd_rec_maxdata(FTPD_REC_U, 0, 6144, 32767), 6144,
             "U: BLKSIZE");
    CHECK_EQ(ftpd_rec_maxdata(FTPD_REC_F, 40000, 0, 32767), 32767,
             "capped by the buffer");
    CHECK_EQ(ftpd_rec_maxdata(FTPD_REC_V, 4, 0, 32767), 0,
             "V with LRECL 4 holds no data");
    CHECK_EQ(ftpd_rec_maxdata(FTPD_REC_V, 2, 0, 32767), 0,
             "V never goes negative");
    CHECK_EQ(ftpd_rec_maxdata(FTPD_REC_F, 0, 0, 32767), -1,
             "F with no LRECL: length unknown, not zero");
    CHECK_EQ(ftpd_rec_maxdata(FTPD_REC_V, 0, 0, 32767), -1,
             "V with no LRECL: length unknown, not zero");
    CHECK_EQ(ftpd_rec_maxdata(FTPD_REC_U, 80, 0, 32767), -1,
             "U with no BLKSIZE: length unknown");

    return mbt_test_summary("TSTREC");
}
