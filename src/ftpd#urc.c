/*
** FTPD UFSD return code mapping
**
** See include/ftpd#urc.h.
*/
#include "ufsdrc.h"

#include "ftpd#rep.h"
#include "ftpd#urc.h"

/* --------------------------------------------------------------------
** UFSD RC -> FTP reply code mapping
** ----------------------------------------------------------------- */
int
ftpd_ufs_rc_to_ftp(int rc)
{
    switch (rc) {
    case UFSD_RC_OK:          return FTP_250;
    case UFSD_RC_NOFILE:      return FTP_550;
    case UFSD_RC_EXIST:       return FTP_550;
    case UFSD_RC_NOTDIR:      return FTP_550;
    case UFSD_RC_ISDIR:       return FTP_550;
    case UFSD_RC_NOSPACE:     return FTP_452;
    case UFSD_RC_NOINODES:    return FTP_452;
    case UFSD_RC_IO:          return FTP_451;
    case UFSD_RC_BADFD:       return FTP_451;
    case UFSD_RC_NOTEMPTY:    return FTP_550;
    case UFSD_RC_NAMETOOLONG: return FTP_553;
    case UFSD_RC_ROFS:        return FTP_550;
    case UFSD_RC_EACCES:      return FTP_550;
    default:                  return FTP_451;
    }
}

/* --------------------------------------------------------------------
** UFSD RC -> human-readable message
** ----------------------------------------------------------------- */
const char *
ftpd_ufs_rc_message(int rc)
{
    switch (rc) {
    case UFSD_RC_OK:          return "Success";
    case UFSD_RC_NOFILE:      return "File or directory not found";
    case UFSD_RC_EXIST:       return "File or directory already exists";
    case UFSD_RC_NOTDIR:      return "Not a directory";
    case UFSD_RC_ISDIR:       return "Is a directory";
    case UFSD_RC_NOSPACE:     return "No space left on device";
    case UFSD_RC_NOINODES:    return "No inodes available";
    case UFSD_RC_IO:          return "I/O error";
    case UFSD_RC_BADFD:       return "Bad file descriptor";
    case UFSD_RC_NOTEMPTY:    return "Directory not empty";
    case UFSD_RC_NAMETOOLONG: return "Path name too long";
    case UFSD_RC_ROFS:        return "Read-only file system";
    case UFSD_RC_EACCES:      return "Permission denied";
    default:                  return "UFS error";
    }
}
