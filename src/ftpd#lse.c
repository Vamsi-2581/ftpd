/*
** FTPD List Builder Result -- see ftpd#lse.h
*/
#include <errno.h>
#include <stddef.h>

#include "ftpd#lse.h"

int
ftpd_list_nomem(const void *list, int err)
{
    return list == NULL && err == ENOMEM;
}
