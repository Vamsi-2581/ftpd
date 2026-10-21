#ifndef FTPD_ABC_H
#define FTPD_ABC_H
/*
** FTPD ABEND Code Text
**
** try() returns 0x00sssuuu for an ABEND: the system completion code in bits
** 12-23, the user completion code in the low 12 bits.  A system ABEND always
** has sss != 0, so a value above 0xFFF is system and anything from 1 to 0xFFF
** is a user ABEND.  A negative value means the ESTAE could not be created and
** the code never ran; zero is a clean return.  The recovery messages
** (FTPD070E, FTPD077E) name the code, and each used to decode it on its own.
**
** Free of project and MVS headers on purpose, so a host unit test can link it
** (test/tstabc.c).
*/

#define FTPD_ABC_LEN  8             /* room for "Sxxx" or "Unnnn" and a NUL */

/*
** Write the code that try() returned as "S0C4" (system) or "U0123" (user)
** into buf, which must hold FTPD_ABC_LEN bytes.
**
** Returns 1 when rc holds an ABEND code.  Returns 0 and leaves buf empty when
** it does not (rc <= 0): the caller decides whether that is a clean run or a
** failed ESTAE.
*/
int ftpd_abc_text(int rc, char *buf) asm("FTPABCTX");

#endif /* FTPD_ABC_H */
