#ifndef FTPD_REC_H
#define FTPD_REC_H
/*
** FTPD Record Fit
**
** A text line stored with STOR/APPE is one record.  When the line is longer
** than the data set's record length, z/OS FTP fails the transfer, or -- with
** SITE TRUNCATE -- cuts the line and says so (#207).  The size arithmetic
** lives here, free of project and MVS headers so a host unit test can link
** it (test/tstrec.c).
*/

/* RECFM family of the data set */
#define FTPD_REC_F  'F'
#define FTPD_REC_V  'V'
#define FTPD_REC_U  'U'

/* Most data bytes one record of the data set holds, at most cap.
**
** F: lrecl.  V: lrecl - 4, because LRECL counts the 4-byte RDW the program
** never sees.  U: blksize, since there is no LRECL.  0 when the data set has
** no usable length. */
int ftpd_rec_maxdata(int recfm, int lrecl, int blksize, int cap)
                                                            asm("FTPRECMX");

/* Bytes of a line of linelen bytes that go into a record of maxdata bytes:
** all of them if they fit, else maxdata. */
int ftpd_rec_keep(long linelen, int maxdata)                asm("FTPRECKP");

#endif /* FTPD_REC_H */
