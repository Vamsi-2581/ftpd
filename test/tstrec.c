/* TSTREC.C
** Tests for ftpd_rec_maxdata() and ftpd_rec_keep() -- how much of a text line
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
    CHECK_EQ(ftpd_rec_maxdata(FTPD_REC_F, 0, 0, 32767), 0,
             "F with no LRECL holds nothing");

    CHECK_EQ(ftpd_rec_keep(10, 80), 10, "a short line is kept whole");
    CHECK_EQ(ftpd_rec_keep(80, 80), 80, "a line of exactly LRECL fits");
    CHECK_EQ(ftpd_rec_keep(81, 80), 80, "one byte over is cut to LRECL");
    CHECK_EQ(ftpd_rec_keep(100000L, 80), 80, "a huge line is cut to LRECL");
    CHECK_EQ(ftpd_rec_keep(0, 80), 0, "an empty line stays empty");

    return mbt_test_summary("TSTREC");
}
