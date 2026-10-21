#ifndef FTPD_ABD_H
#define FTPD_ABD_H
/*
** FTPD Abandon Check
**
** After a transfer's ABEND the recovery closes the in-flight data set with
** __fabandon().  That returns the ABEND code whenever its CLOSE abended, even
** when the library's unallocation afterwards freed the DD all the same
** (libc370#705), so the return value cannot say whether the data set is still
** held.  FTPD asks the TIOT instead (#232): is a DD of that name still there?
**
** Free of project and MVS headers on purpose, so a host unit test can link it
** with a synthetic TIOT (test/tstabd.c).  The caller supplies the TIOT.
*/

/* ftpd_abd_dd_state() answers */
#define FTPD_ABD_UNKNOWN  (-1)      /* no DD name to look for            */
#define FTPD_ABD_FREE     0         /* no such DD in the TIOT            */
#define FTPD_ABD_HELD     1         /* the DD is still allocated         */

/*
** Is a DD of this name still in the TIOT?
**
** tiot   -- the task I/O table: 24 bytes of job and step name, then the DD
**           entries, each starting with its own length byte, ended by a zero
**           length.  An entry whose status byte has X'80' set is not in use
**           (what unallocation leaves behind) and does not count.
** ddname -- the DD name, as the FILE keeps it (fp->ddname), without padding.
**
** Returns FTPD_ABD_HELD or FTPD_ABD_FREE; FTPD_ABD_UNKNOWN when tiot or
** ddname is NULL or the name is empty.  A name that another session
** allocated again in the meantime reads as held -- the answer errs on the
** side of the warning.
*/
int ftpd_abd_dd_state(const void *tiot, const char *ddname) asm("FTPABDST");

/*
** The tail of the FTPD076W text for an abandon whose CLOSE abended (arc > 0),
** by the state ftpd_abd_dd_state() found.
*/
const char *ftpd_abd_close_text(int state)                  asm("FTPABDTX");

#endif /* FTPD_ABD_H */
