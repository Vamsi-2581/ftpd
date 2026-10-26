#ifndef FTPD_LMP_H
#define FTPD_LMP_H
/*
** FTPD LIST/NLST Member Pattern Argument
**
** z/OS FTP answers LIST 'IBMUSER.CNTL(*)' with the member list of that PDS
** without a prior CWD.  The Zowe FTP plugin sends exactly that for
** `zowe zftp ls members`, and for every data set download before it fetches
** (#302).  FTPD used to list members only when the current directory was the
** PDS, so the argument was taken as a data set prefix and matched nothing.
**
** Free of project and MVS headers on purpose, so a host unit test can link it
** (test/tstlmp.c).
*/

/*
** Split a LIST/NLST argument of the form DSN(pattern).
**
** arg     -- the argument as the client sent it:  'DSN(pattern)'  absolute
**            (the closing quote is optional, as for every other command), or
**            DSN(pattern)  relative to cwd.
** cwd     -- the working directory prefix, with its trailing dot.
** dsn     -- receives the data set name: uppercase, no quotes, no trailing
**            dot, cwd prepended when arg was unquoted.
** pattern -- receives the member pattern, uppercase and cut to 8 characters
**            like a member name; empty for DSN() and DSN(*).  '*' and '%'
**            stay in it for the directory reader to match.
**
** Returns:
**    1  arg is a DSN(pattern) form; dsn and pattern are filled in
**    0  arg is not (no '(' , nothing but an optional quote after the
**       closing ')', or a ')' before the '(') -- dsn and pattern untouched
**   -1  the name is too long for dsn
**   -2  the DSN part holds a wildcard
**   -3  the DSN part holds a character a data set name may not hold
**
** The negative codes are those of resolve_dsn() (ftpd#mvs.c), so the caller
** hands them to the same dsn_error().
*/
int ftpd_lmp_split(const char *arg, const char *cwd,
                   char *dsn, int dsnsz,
                   char *pattern, int patsz)             asm("FTPLMPSP");

#endif /* FTPD_LMP_H */
