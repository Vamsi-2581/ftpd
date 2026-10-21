#import "../bookmaster/bookmaster.typ": *

= The SITE Command <ref-site>

#idx("SITE", "keywords")
#cmd("SITE") sets parameters of the session. This chapter lists every keyword.
The _FTPD User's Guide_ explains what they are for, in "Setting Up a Session
with SITE".

== Syntax <ref-site-syntax>

```
SITE [keyword[=value] ...]
```

- FTPD splits the argument at blanks and tabs. Each word is one keyword, with
  or without #cmd("=") and a value. A word of more than 95 characters is cut.
- The verb, the keyword and the value are not case sensitive. FTPD folds the
  keyword to upper case. It folds the values of #cmd("RECFM"), #cmd("VOLUME"),
  #cmd("UNIT"), #cmd("FILETYPE"), #cmd("JESJOBNAME"), #cmd("JESOWNER") and
  #cmd("JESSTATUS") too. This holds since FTPD 1.3.0 for the three JES values.
- A keyword is at most 31 characters and a value at most 63 characters. FTPD
  cuts what is longer, without a message.
- A keyword may be shortened (@ref-site-abbr). FTPD refuses a form that is too
  short.
- FTPD applies the words from left to right. A word that is wrong does not
  stop the others.
- A session starts with the values of the #cmd("DEF") parameters
  (@ref-prm-def). A keyword that has such a parameter, given with no value,
  returns to it.

== Reply <ref-site-reply>

#tab(caption: [Replies to SITE])[
  #table(columns: (2.6in, 1fr),
    [Reply], [When],
    [#cmd("200 SITE command was accepted")], [All words are known and have good
      values.],
    [#cmd("200-Unrecognized parameter 'WORD' on SITE command.") and
      #cmd("200 SITE command was accepted")], [At least one word is not a
      keyword, or is a form of a keyword that is too short (@ref-site-abbr).
      The line names the first, as the client typed it, with its value. The
      other words still apply.],
    [#cmd("200-text") and #cmd("200 SITE command was accepted")], [A value was
      out of range or refused. The text names the first problem
      (@ref-site-keys).],
    [#cmd("501 RETPD=n exceeds the server limit of N days; keeping the current value")],
      [#cmd("RETPD") asks for more days than #cmd("MAXRETPD") allows
      (@ref-site-retpd-limit), and it is the only word of the command.],
    [#cmd("501-RETPD=n exceeds the server limit of N days; keeping the current value") and
      #cmd("501 Other parameters on this command were applied.")], [The same,
      with more than one word on the line.],
    [#cmd("501 SITE RDW is not supported: FTPD does not keep record descriptor words")],
      [The word #cmd("RDW") is on the line, and it is the only word
      (@ref-site-keys).],
    [#cmd("501 SBSENDEOL=value is not supported: FTPD only handles CRLF")],
      [#cmd("SBSENDEOL") has a value other than #cmd("CRLF"), and it is the
      only word. The text names the value as the client typed it.],
    [#cmd("501-text") and
      #cmd("501 Other parameters on this command were applied.")], [One of the
      three #cmd("501") cases above, with more than one word on the line.],
    [#cmd("202 SITE not necessary; you may proceed")], [No word.],
  )
]

The code is #cmd("200") in the first three cases, #cmd("501") when #cmd("RETPD")
is above the limit or when the line has #cmd("RDW") or a #cmd("SBSENDEOL") that
FTPD does not handle, and #cmd("202") for no word. A script must read the first
line.

The #cmd("501") takes the place of the other replies. If the same line has a
refused word and also a wrong value or a word that is not a keyword,
the reply is the #cmd("501"), and the text about the other word is not sent.
The other words are applied in every case. If more than one word is refused,
the #cmd("501") names the first of them, counted from the left, whichever of
#cmd("RETPD"), #cmd("RDW") and #cmd("SBSENDEOL") it is.

== Abbreviations <ref-site-abbr>

#idx("SITE", "abbreviations")#idx("abbreviations")
Each keyword has a shortest form. FTPD takes a word as a keyword when the part
before the equals sign, folded to upper case, is the full name or the start of
the full name and at least as long as the shortest form. So #cmd("PRI"),
#cmd("PRIM") and #cmd("PRIMARY") are one keyword, and #cmd("PR") is none. The
full name always wins over a shorter keyword that it starts with, which keeps
#cmd("BLOCKS") apart from #cmd("BLOCKSIZE").

#tab(caption: [The shortest form of each SITE keyword])[
  #table(columns: (1.8in, 1.2in, 1fr),
    [Keyword], [Shortest form], [Remark],
    [#cmd("BLKSIZE")], [#cmd("BLK")], [],
    [#cmd("BLOCKSIZE")], [#cmd("BLOCKSI")], [The same keyword as
      #cmd("BLKSIZE").],
    [#cmd("BLOCKS")], [#cmd("BL")], [#cmd("BLO") and #cmd("BLOC") are
      #cmd("BLOCKS") too, not the block size.],
    [#cmd("CYLINDERS")], [#cmd("CY")], [],
    [#cmd("DATACLASS")], [#cmd("DATAC")], [],
    [#cmd("DIRECTORY")], [#cmd("D")], [],
    [#cmd("FILETYPE")], [#cmd("FILE")], [],
    [#cmd("JESJOBNAME")], [#cmd("JESJOBN")], [],
    [#cmd("JESOWNER")], [#cmd("JESO")], [],
    [#cmd("JESSTATUS")], [#cmd("JESST")], [],
    [#cmd("LRECL")], [#cmd("LR")], [],
    [#cmd("MGMTCLASS")], [#cmd("MG")], [],
    [#cmd("PRIMARY")], [#cmd("PRI")], [],
    [#cmd("RDW")], [#cmd("RDW")], [No shorter form.],
    [#cmd("RECFM")], [#cmd("REC")], [],
    [#cmd("RETPD")], [#cmd("RET")], [],
    [#cmd("SBSENDEOL")], [#cmd("SBSENDEOL")], [No shorter form.],
    [#cmd("SECONDARY")], [#cmd("SEC")], [],
    [#cmd("STORCLASS")], [#cmd("STO")], [],
    [#cmd("TRACKS")], [#cmd("TR")], [],
    [#cmd("TRAILINGBLANKS")], [#cmd("TRAIL")], [#cmd("TRAILING") is a longer
      form of it and works.],
    [#cmd("TRUNCATE")], [#cmd("TRUN")], [],
    [#cmd("UNIT")], [#cmd("U")], [],
    [#cmd("VOLUME")], [#cmd("VOL")], [],
  )
]

The table shows what FTPD accepts. It does not claim that these are the
shortest forms of the z/OS server.

A word that is too short, that fits no keyword, or that would fit two, is
answered like any word that is not a keyword (@ref-site-reply): the reply names
it with #cmd("Unrecognized parameter") and no setting changes for that word.
The text in the reply is the word as the client typed it, with its value, as
#cmd("Unrecognized parameter 'PR=5' on SITE command."). With the keywords of
this version no form fits two keywords. The names of #cmd("DATACLAS"),
#cmd("STORCLAS") and #cmd("MGMTCLAS") that FTPD took before 1.3.0 are starts
of the full names, and still work.

== Keywords <ref-site-keys>

#tab(caption: [SITE keywords])[
  #table(columns: (1.2in, 1fr),
    [Keyword], [Effect and rules],
    [#cmd("RECFM=f")], [The record format of a new data set. Upper case. No
      check. No value: the value of #cmd("DEFRECFM").],
    [#cmd("LRECL=n")], [The record length. FTPD converts the value as a number
      and does not check it. No value: #cmd("DEFLRECL").],
    [#cmd("BLKSIZE=n"), #cmd("BLOCKSIZE=n")], [The block size. Both names are one
      keyword. If the record format starts with
      #cmd("F"), the record length is above 0 and the block size is not a
      multiple of it, FTPD rounds the block size down to a multiple (at least
      one record) and sends the text #cmd("BLOCKSIZE must be a multiple of LRECL for RECFM FB"). No value: #cmd("DEFBLKSIZE").],
    [#cmd("PRIMARY=n")], [The primary quantity, 1 to 16777215. Another value:
      the setting stays and the text is #cmd("PRIMARY must be 1-16777215; keeping the current value"). No value: #cmd("DEFPRIMARY").],
    [#cmd("SECONDARY=n")], [The secondary quantity, 0 to 16777215. Another
      value: #cmd("SECONDARY must be 0-16777215; keeping the current value").
      No value: #cmd("DEFSECONDARY").],
    [#cmd("DIRECTORY=n")], [The directory blocks for #cmd("MKD"), 1 to 16777215.
      The value must be a whole decimal number. Another value: the setting stays
      and the text is #cmd("DIRECTORY must be 1-16777215; keeping the current value"). No value: #cmd("DEFDIRECTORY") (10 unless the
      parameters say otherwise). The number is fixed when the data set is
      created. A member #cmd("STOR") into a full directory answers
      #cmd("552") (@ref-reply-552).],
    [#cmd("RETPD=n")], [The retention period of a new data set in days, 0 to
      9999. The value must be a whole decimal number. 0 is no retention. It
      applies as @ref-site-retpd says. A value that is not a number from 0 to
      9999: the setting stays and the text is
      #cmd("RETPD must be 0-9999; keeping the current value"). A number above
      #cmd("MAXRETPD"): the setting stays and the reply is a #cmd("501")
      (@ref-site-retpd-limit). No
      value: no retention. A session starts without one, and there is no
      #cmd("DEF") parameter for it.],
    [#cmd("TRACKS")], [The unit of the quantities is the track.],
    [#cmd("CYLINDERS")], [The unit of the quantities is the cylinder.],
    [#cmd("BLOCKS")], [The unit is the block. With a block size of 0 or less:
      the unit stays, and the text is #cmd("BLOCKS needs a non-zero BLKSIZE; allocation unit unchanged").],
    [#cmd("VOLUME=v")], [The volume for a new data set. Upper case. FTPD checks
      that the volume is mounted. If not: the setting stays, and the text is
      #cmd("VOLUME is not mounted; keeping the current value"). No value:
      #cmd("DEFVOLUME").],
    [#cmd("UNIT=u")], [The unit for a new data set. Upper case. No check. No
      value: #cmd("DEFUNIT").],
    [#cmd("FILETYPE=t")], [#cmd("JES") (case does not matter) switches to the
      JES view and keeps the view that was active. Any other value, or none,
      ends the JES view and returns to the kept view.],
    [#cmd("TRAILINGBLANKS")], [A switch. Each use turns on or off the sending of the
      blanks at the end of a record in text mode.],
    [#cmd("JESJOBNAME=n")], [The job name for the job list. Upper case.
      #cmd("JESJOBNAME=") with no value ends the filter and
      lists all job names.],
    [#cmd("JESOWNER=n")], [The owner for the job list. Upper case. #cmd("*")
      means all owners. Without it the owner is the user of the session.],
    [#cmd("JESSTATUS=s")], [The status filter of the JES view: #cmd("ALL"),
      #cmd("INPUT"), #cmd("ACTIVE") or #cmd("OUTPUT"), in any case, stored in
      upper case. No value: #cmd("ALL"), which is also the value at the start.
      Another value, an abbreviation too: the setting stays, and the text is
      #cmd("JESSTATUS must be ALL, INPUT, ACTIVE or OUTPUT; keeping the current value").
      It acts as @ref-site-jesstatus says.],
    [#cmd("TRUNCATE")], [A switch. Each use turns on or off the cutting of a
      text line that is longer than the record (@ref-site-truncate). Off at
      the start.],
    [#cmd("RDW")], [Refused with a #cmd("501"). FTPD does not keep record
      descriptor words in a transfer. Nothing changes.],
    [#cmd("SBSENDEOL=v")], [#cmd("CRLF"), in any case, is accepted and changes
      nothing: it is the only line end that FTPD writes and reads. No value is
      accepted in the same way. Any other value is refused with a #cmd("501").
      Nothing changes.],
    [#cmd("DATACLASS"), #cmd("STORCLASS"), #cmd("MGMTCLASS")], [Accepted and
      ignored, with any value. MVS 3.8j has no SMS.],
  )
]

Every other word is #cmd("Unrecognized parameter"). That includes
#cmd("JESINTERFACELEVEL"), which is a parameter of the configuration member
(@ref-prm) and not a keyword of #cmd("SITE").

== TRUNCATE <ref-site-truncate>

#idx("TRUNCATE")#idx("451", "line too long")
#cmd("SITE TRUNCATE") decides what #cmd("STOR") and #cmd("APPE") do with a line
of a text transfer (#cmd("TYPE A")) that is longer than one record of the
data set in the MVS view. The data bytes that one record holds:

#tab(caption: [The longest line that fits])[
  #table(columns: (1.6in, 1fr),
    [Record format], [Data bytes],
    [F, FB], [LRECL],
    [V, VB, VBS], [LRECL minus 4. The 4 bytes are the record descriptor word.
      A data set with #cmd("VB") and #cmd("LRECL=14") takes 10.],
    [U], [BLKSIZE],
  )
]

If the data set reports no length at all (the record length is zero, or the
block size is zero for format U), the limit is 32767 bytes, the size of the
buffer. A variable data set with a record length of 4 or less has no data
bytes: every line that is not empty is too long. FTPD counts the bytes of the line as they come
from the client, without the line end and without any CR. A line that has
exactly the number fits.

- *Switch off* (the start of every session): the transfer fails at the first
  line that is too long. The reply is
  #cmd("451 File transfer failed. File contains records that are longer than the LRECL (n data bytes). Use SITE TRUNCATE to cut them; the data set is incomplete."),
  where n is the number of the table. A data set that this command created
  is removed. A data set that existed is left as the failed transfer left it.
- *Switch on:* FTPD keeps the first n bytes of the line, drops the rest,
  and goes on with the next line. If it cut at least one line, the reply is
  #cmd("250 Transfer completed successfully (data was truncated)."), and the
  log has a warning with the number of lines. If no line was too long, the
  reply is the plain #cmd("250 Transfer completed successfully.").

The last line of the data is a line even without a line end. The switch has no
effect on #cmd("TYPE I") and #cmd("TYPE E"), which cut the data into records by
length, and none in the UFS and JES views. Before FTPD 1.3.0 the keyword
did nothing, and FTPD cut every long line, with a plain #cmd("250").

== JESSTATUS <ref-site-jesstatus>

#idx("JESSTATUS")
#cmd("SITE JESSTATUS=s") limits the JES view to the jobs in one status. It acts on:

- #cmd("LIST") and #cmd("NLST"): the list has the jobs in that status only. The
  header #cmd("125 List started OK for JESJOBNAME=n, JESSTATUS=s and JESOWNER=o")
  names the filter.
- #cmd("LIST JOBnnnnn"): a job outside the filter gives
  #cmd("550 No job found with ID JOBnnnnn for JESSTATUS=s").
- #cmd("RETR") and #cmd("DELE"): a job outside the filter gives
  #cmd("550 Jobid JOBnnnnn not found for JESJOBNAME=n, JESSTATUS=s, JESOWNER=o").

The statuses are those of the STATUS column of the job list. #cmd("INPUT"),
#cmd("ACTIVE") and #cmd("OUTPUT") select the jobs with that text in the column.
#cmd("ALL") selects every job, also the ones with the status #cmd("XMIT"),
#cmd("SETUP"), #cmd("RECEIVE") or #cmd("UNKNOWN"). The filter works together
with the owner and the job name for the list. For #cmd("RETR") and #cmd("DELE")
the job ID decides, and the status is the only filter that takes part. The
list of 200 jobs counts the jobs that pass the filter.

== RETPD <ref-site-retpd>

#idx("RETPD")#idx("retention period")
#cmd("SITE RETPD=n") gives the data sets that the session creates a retention
period of _n_ days. FTPD passes it to the allocation of:

- a partitioned data set that #cmd("MKD") creates, and
- a sequential data set that #cmd("STOR") or #cmd("APPE") creates because the
  name does not exist.

It does not apply to

- a member that #cmd("STOR") or #cmd("APPE") adds to a partitioned data set,
- a data set that exists: FTPD sends no retention period for it,
- the UFS and JES views.

The value stays for the session. #cmd("SITE RETPD=0") and #cmd("SITE RETPD")
with no value both switch it off.

If the system refuses the allocation with the retention period, the reply to
#cmd("STOR") is #cmd("550 Cannot allocate dataset NAME") and FTPD removes the
data set that it had created for the transfer.

=== The Limit of the Server <ref-site-retpd-limit>

#idx("MAXRETPD")#idx("retention period", "limit")#idx("501", "RETPD")
The parameter #cmd("MAXRETPD") in #cmd("FTPDPRM") (@ref-prm-ses) is the most
days that #cmd("SITE RETPD") takes. Without the parameter it is 365. FTPD
checks the number after it has checked the range 0 to 9999.

A number up to the limit, the limit itself too, is taken, and the reply is
#cmd("200"). A number above the limit is refused. The retention period of the
session stays as it was, and the reply is a #cmd("501"). With one word on the
line:

```
501 RETPD=400 exceeds the server limit of 365 days; keeping the current value
```

With more than one word on the line, FTPD applies the other words, and the
reply has two lines:

```
501-RETPD=400 exceeds the server limit of 365 days; keeping the current value
501 Other parameters on this command were applied.
```

The first number in the text is the number as FTPD read it. The second is
#cmd("MAXRETPD").

- *0 and the bare keyword* are never refused, also with #cmd("MAXRETPD=0").
  With that value a session cannot ask for any retention.
- *A number above 9999* keeps the #cmd("200") with the text of @ref-site-keys.
  It is not a case of the limit.

The limit applies to the number of #cmd("SITE RETPD") only. A session starts
without a retention period, so the limit has nothing to correct at the start.
FTPD reads #cmd("MAXRETPD") once, at start, like every other
parameter. #cmd("/F FTPD,CONFIG") shows the value that FTPD uses
(@ref-con-config).

=== STOR and APPE over a Retained Data Set <ref-site-retained>

#idx("retention period", "STOR and APPE")#idx("IEC507D")#idx("550", "retained data set")
A data set that was allocated with a retention period carries an expiration
date in its label. Before #cmd("STOR") or #cmd("APPE") opens an existing data
set of the MVS view for output, FTPD reads that date. When the date is later
than today, FTPD does not open the data set. The reply is a #cmd("550"), and
the session goes on with the next command:

#tab(caption: [Replies for a retained data set])[
  #table(columns: (3.2in, 1fr),
    [Reply], [When],
    [#cmd("550 NAME is retained until YYYY-MM-DD (YYYY.DDD) and is not opened for writing before then.")],
      [#cmd("STOR"). The date is given as year, month and day, and as year
      and day of the year.],
    [#cmd("550 NAME is retained until YYYY-MM-DD (YYYY.DDD) and is not opened for appending before then.")],
      [#cmd("APPE"), the same.],
    [#cmd("550 NAME has a permanent retention period and is not opened for writing.")],
      [#cmd("STOR") over a data set with the expiration date 99365 or 99366,
      which never expires. For #cmd("APPE") the last word is
      #cmd("appending").],
    [#cmd("550 NAME has the expiration date 99000 and is not opened for writing.")],
      [#cmd("STOR") over a data set with the expiration date 99000, year 99
      and day 0. For #cmd("APPE") the last word is #cmd("appending").],
  )
]

- *The date 99000.* The value is no calendar date, but the system treats the
  data set as retained all the same: without the check, the open would ask the
  operator with #cmd("IEC507D"). FTPD refuses it with its own reply and writes
  #cmd("STOR: NAME has the expiration date 99000, refused") to the log
  (#cmd("APPE:") for an append). Other values with day 0, such as 26000, count
  as no expiration date.

- *The date.* A data set expires on its expiration date. On that day
  #cmd("STOR") and #cmd("APPE") open it, and from the next day on they get
  the #cmd("550"). FTPD compares with the date of the local clock of the
  system.
- *The label decides, not the session.* A session without
  #cmd("SITE RETPD") gets the same reply for a data set that another session
  or a job allocated with a retention period.
- *A member.* The label of the partitioned data set decides, and the reply
  names the partitioned data set, not the member. A partitioned data set that
  #cmd("MKD") created under #cmd("SITE RETPD") takes no member until it
  expires.
- *The order of the checks.* The name and the security check come first. Then
  come the #cmd("APPE") refusals for a member that exists and for a
  partitioned data set without a member (_FTPD User's Guide_, "Appending").
  The retention check follows them and runs before FTPD allocates or opens
  anything.
- *Not checked.* #cmd("DELE") and #cmd("RMD") do not look at the expiration
  date. On the system where this was measured, both removed a retained data
  set (#cmd("250 NAME deleted.")).
- *No label, no check.* When FTPD cannot read the label, it goes on as it
  does for a data set without retention.

A server of FTPD 1.2.0 or earlier makes no such check. The system asks the
operator with message #cmd("IEC507D") before it opens the data set, and the
session waits for the answer.
