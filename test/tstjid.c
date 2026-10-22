/* TSTJID.C
** Tests for ftpd_jid_valid() -- the shape check of the job ID that RETR and
** DELE in the JES mode pass to jesjob() (#267).  Before it, a wildcard ID
** made jesjob() load every job it matched and the request was refused
** afterwards.
**
** ftpd#jid.c is free of project and MVS headers, so this is a DUAL test.
*/
#include <stdio.h>
#include <string.h>

#include "ftpd#jid.h"
#include <mbtcheck.h>

int
main(void)
{
    CHECK(ftpd_jid_valid("JOB00001") == 1, "JOB00001");
    CHECK(ftpd_jid_valid("JOB12345") == 1, "JOB12345");
    CHECK(ftpd_jid_valid("JOB00000") == 1, "JOB00000");
    CHECK(ftpd_jid_valid("JOB99999") == 1, "JOB99999");
    CHECK(ftpd_jid_valid("STC00045") == 1, "STC00045");
    CHECK(ftpd_jid_valid("TSU00012") == 1, "TSU00012");

    CHECK(ftpd_jid_valid(NULL) == 0, "NULL");
    CHECK(ftpd_jid_valid("") == 0, "empty");

    CHECK(ftpd_jid_valid("JOB*") == 0, "JOB* wildcard");
    CHECK(ftpd_jid_valid("JOB0000*") == 0, "wildcard in the digits");
    CHECK(ftpd_jid_valid("JOB?0001") == 0, "? wildcard");
    CHECK(ftpd_jid_valid("*") == 0, "bare *");
    CHECK(ftpd_jid_valid("***") == 0, "***");
    CHECK(ftpd_jid_valid("STC*") == 0, "STC* wildcard");
    CHECK(ftpd_jid_valid("TSU%%%%%") == 0, "% wildcard");

    CHECK(ftpd_jid_valid("JOB") == 0, "prefix only");
    CHECK(ftpd_jid_valid("JOB1") == 0, "one digit");
    CHECK(ftpd_jid_valid("JOB0001") == 0, "four digits");
    CHECK(ftpd_jid_valid("JOB000012") == 0, "six digits");
    CHECK(ftpd_jid_valid("JOB00001 ") == 0, "trailing blank");
    CHECK(ftpd_jid_valid(" JOB00001") == 0, "leading blank");
    CHECK(ftpd_jid_valid("JOB00001.1") == 0, "spool file suffix");
    CHECK(ftpd_jid_valid("JOB00001XXXXXXXXXXXXXXXX") == 0, "long");

    CHECK(ftpd_jid_valid("ABC00001") == 0, "wrong prefix");
    CHECK(ftpd_jid_valid("J0000001") == 0, "J plus seven digits");
    CHECK(ftpd_jid_valid("JO000001") == 0, "JO prefix");
    CHECK(ftpd_jid_valid("JOA00001") == 0, "JOA prefix");
    CHECK(ftpd_jid_valid("STD00045") == 0, "STD prefix");
    CHECK(ftpd_jid_valid("TSO00012") == 0, "TSO prefix");
    CHECK(ftpd_jid_valid("job00001") == 0, "lower case is the caller's job");
    CHECK(ftpd_jid_valid("JOB0000A") == 0, "letter among the digits");
    CHECK(ftpd_jid_valid("JOB-0001") == 0, "sign among the digits");
    CHECK(ftpd_jid_valid("JOBA0001") == 0, "letter first among the digits");
    CHECK(ftpd_jid_valid("00000001") == 0, "digits only");

    return mbt_test_summary("TSTJID");
}
