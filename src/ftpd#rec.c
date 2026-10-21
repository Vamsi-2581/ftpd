/*
** FTPD Record Fit
**
** See include/ftpd#rec.h.
*/
#include "ftpd#rec.h"

int
ftpd_rec_maxdata(int recfm, int lrecl, int blksize, int cap)
{
    int max;

    if (recfm == FTPD_REC_V)
        max = lrecl - 4;
    else if (recfm == FTPD_REC_U)
        max = blksize;
    else
        max = lrecl;

    if (max < 0)
        max = 0;
    return max > cap ? cap : max;
}

int
ftpd_rec_keep(long linelen, int maxdata)
{
    return linelen > (long)maxdata ? maxdata : (int)linelen;
}
