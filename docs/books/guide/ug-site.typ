#import "../bookmaster/bookmaster.typ": *

= Setting Up a Session with SITE <ug-site>

#idx("SITE")
#cmd("SITE") sets the parameters of the session: how FTPD allocates a new
data set, which view it works in, and which jobs it shows. A setting holds
until the session ends or until a later #cmd("SITE") changes it. Each session
starts with the values from the #cmd("FTPDPRM") parameters whose names begin
with #cmd("DEF") (@ug-ops-config).

== The Form of the Command <ug-site-form>

#cmd("SITE") takes one or more words, separated by blanks. A word is a
keyword, or a keyword, an equals sign and a value:

```
SITE RECFM=FB LRECL=80 BLKSIZE=3120
SITE CYLINDERS PRIMARY=5 SECONDARY=2
```

FTPD applies the words from left to right and sends one reply for the whole
command:

- *All words are known:* #cmd("200 SITE command was accepted").
- *A value is wrong:* the setting stays as it was. The reply is a multiple
  line #cmd("200") that names the problem on the first line and ends with
  #cmd("SITE command was accepted"). The other words of the command still
  apply. If several words are wrong, the reply names the first.
- *#cmd("RETPD") is above the limit of the server, or the word is #cmd("RDW")
  or a #cmd("SBSENDEOL") that FTPD does not handle:* the reply is a #cmd("501")
  with the reason, such as
  #cmd("501 RETPD=n exceeds the server limit of N days; keeping the current value"),
  and the setting stays as it was. If the command has
  more words, FTPD applies them, and the reply has two lines: the text above
  with #cmd("501-"), and #cmd("501 Other parameters on this command were applied.")
  A script must not read this #cmd("501") as "nothing changed".
- *A word is not known:* the reply is the same kind of #cmd("200"), and the
  first line is #cmd("Unrecognized parameter 'WORD' on SITE command."). It is
  not an error code. Read the first line of the reply, not only the code.
- *No word at all:* #cmd("202 SITE not necessary; you may proceed").

A keyword without a value takes the setting back to the value it had at the
start of the session. So #cmd("SITE PRIMARY") undoes #cmd("SITE PRIMARY=200").
For #cmd("DIRECTORY") that is the value of #cmd("DEFDIRECTORY"). #cmd("RETPD")
has no such parameter. A session starts without retention, and the bare
keyword switches it off.

A keyword may be shortened: #cmd("SITE PRI=5 SEC=2 CY") is
#cmd("SITE PRIMARY=5 SECONDARY=2 CYLINDERS"). Every keyword has a shortest form, and a form that is
too short is not a keyword. The word #cmd("PR=5") gets
#cmd("Unrecognized parameter 'PR=5' on SITE command.") and sets nothing. The
_FTPD Reference_ lists the shortest forms in "The SITE Command".

== The Keywords <ug-site-keys>

#tab(caption: [SITE keywords for new data sets])[
  #table(columns: (1.35in, 1fr),
    [Keyword], [Effect],
    [#cmd("RECFM=f")], [the record format of a new data set, such as
      #cmd("FB"), #cmd("VB") or #cmd("U"). FTPD folds it to upper case and
      does not check it.],
    [#cmd("LRECL=n")], [the record length.],
    [#cmd("BLKSIZE=n")], [the block size, also written #cmd("BLOCKSIZE=n"). For a fixed record format that
      is not a multiple of the record length, FTPD rounds it down to one,
      and the reply says so with #cmd("BLOCKSIZE must be a multiple of LRECL for RECFM FB").],
    [#cmd("DCBDSN=dsn")], [the record format, record length and block size of
      a data set that exists, copied at once from its label. Give the full name,
      with or without quotes. A name that is not cataloged, a pattern or a
      member is refused with a #cmd("501"), and the values stay. A
      #cmd("RECFM"), #cmd("LRECL") or #cmd("BLKSIZE") after it replaces its one
      value. Without a value the three return to the defaults. See the
      _FTPD Reference_, "DCBDSN".],
    [#cmd("PRIMARY=n")], [the primary space quantity, 1 to 16777215. Another
      value is refused with #cmd("PRIMARY must be 1-16777215; keeping the current value").],
    [#cmd("SECONDARY=n")], [the secondary quantity, 0 to 16777215. Zero is a
      valid request for no secondary extents.],
    [#cmd("TRACKS"), #cmd("CYLINDERS")], [the unit of the quantities.],
    [#cmd("BLOCKS")], [the unit is the block size. FTPD refuses it when the
      block size is zero: #cmd("BLOCKS needs a non-zero BLKSIZE; allocation unit unchanged").],
    [#cmd("DIRECTORY=n")], [the number of directory blocks of a partitioned
      data set that #cmd("MKD") creates, 1 to 16777215. Without it #cmd("MKD")
      uses #cmd("DEFDIRECTORY"), which is 10 unless the administrator changed
      it. The number is fixed when the data set is created. A full directory
      cannot be extended (@ug-data-dirfull).],
    [#cmd("RETPD=n")], [the retention period in days of a data set that
      #cmd("MKD") or #cmd("STOR") creates, 0 to 9999. 0 is no retention.
      The administrator sets a limit with #cmd("MAXRETPD"), 365 days unless
      changed. More is answered with a #cmd("501"), and the setting stays.
      A member of a partitioned data set and a data set that exists keep what
      they have. See the note below.],
    [#cmd("UNIT=u")], [the unit for a new data set. Without it the system
      chooses.],
    [#cmd("VOLUME=v")], [the volume for a new data set. FTPD refuses a volume
      that is not mounted: #cmd("VOLUME is not mounted; keeping the current value"). Without it the system chooses.],
  )
]

#note[*A retained data set is not stored into again.* After
#cmd("SITE RETPD") a script that stores the same name twice gets a
#cmd("550") with the expiration date on the second #cmd("STOR"). A partitioned
data set that #cmd("MKD") creates with a retention period takes no member
until it expires. See @ug-data-retained.]

FTPD does not check #cmd("LRECL") and #cmd("RECFM") itself. It passes them to
the allocation, and a combination that the system rejects shows when
#cmd("STOR") creates the data set (@ug-data-stor).

#tab(caption: [SITE keywords for the mode of the session])[
  #table(columns: (1.35in, 1fr),
    [Keyword], [Effect],
    [#cmd("FILETYPE=JES")], [switch to the JES view (@ug-other-jes).],
    [#cmd("FILETYPE=SEQ")], [back to the view that was active before. Any
      other value does the same.],
    [#cmd("TRAILINGBLANKS")], [switch the blanks at the end of a record in text
      transfers between kept and dropped. Each use of the keyword switches
      again. A session starts with the blanks dropped.],
    [#cmd("JESJOBNAME=n"), #cmd("JESOWNER=n")], [the filters of the JES job
      list (@ug-other-jes).],
    [#cmd("JESSTATUS=s")], [the status filter of the JES view: #cmd("ALL"),
      #cmd("INPUT"), #cmd("ACTIVE") or #cmd("OUTPUT"), in any case. It applies
      to #cmd("LIST"), #cmd("NLST"), #cmd("RETR") and #cmd("DELE")
      (@ug-other-jes). No value: #cmd("ALL"). Another value, such as
      #cmd("HELD"), keeps the filter that was set, and the reply starts with
      #cmd("200-JESSTATUS must be ALL, INPUT, ACTIVE or OUTPUT; keeping the current value").
      An abbreviation of a status is another value.],
    [#cmd("TRUNCATE")], [switch between failing and cutting for a text line
      that is longer than the record of the data set. Each use of the keyword
      switches again. A session starts with the switch off: the upload fails
      (@ug-data-long).],
  )
]

FTPD refuses two keywords of the z/OS server because it cannot do what they
ask. The reply is a #cmd("501"), and nothing changes:

- #cmd("RDW") asks for the record descriptor words of variable records to be
  kept in the transfer. FTPD does not keep them. The reply is
  #cmd("501 SITE RDW is not supported: FTPD does not keep record descriptor words").
- #cmd("SBSENDEOL=value") sets the line end of text files. FTPD handles the
  line end CRLF only. #cmd("SBSENDEOL=CRLF") in any case, and the keyword with
  no value, are answered with #cmd("200") and change nothing. Any other value
  is answered with
  #cmd("501 SBSENDEOL=LF is not supported: FTPD only handles CRLF"). The text
  names the value as the client typed it.

A script that sets one of them must read the code of the reply. Before FTPD
1.3.0 the reply was #cmd("200") for both, and the setting did nothing.
With more words on the line FTPD applies them, and the reply has two lines
(@ug-site-form).

FTPD accepts #cmd("DATACLAS"), #cmd("STORCLAS") and #cmd("MGMTCLAS") and does
nothing with them, so that scripts written for the z/OS server run unchanged.
There are no storage classes on MVS 3.8j.

#cmd("JESINTERFACELEVEL") is not a keyword of #cmd("SITE"). FTPD answers that
the parameter is not known. In the parameters (@ug-ops-config) it is ignored.

== Upper and Lower Case <ug-site-case>

#idx("SITE", "case")#idx("case")
The verb, the keywords and the values are not case sensitive. FTPD folds all
of them to upper case before it acts. These commands are one command:

```
SITE LRECL=80
site lrecl=80
Site Lrecl=80
```

This includes the values of #cmd("JESJOBNAME"), #cmd("JESOWNER") and
#cmd("JESSTATUS"). Before FTPD 1.3.0 FTPD kept these three as the client had
typed them, so #cmd("jesowner=ibmuser") and #cmd("JESOWNER=IBMUSER") were two
different settings, and the #cmd("550") reply for a job that is not found
showed the owner in lower case. Since FTPD 1.3.0 the filter is upper case
whatever the client typed.

#note[*The command line client may refuse the word SITE.* The BSD
#cmd("ftp") client and tnftp, the #cmd("ftp") of many Linux, BSD and macOS
systems, implement #cmd("site") as a command of their own. They know it in
lower case only. #cmd("SITE") or #cmd("Site") gets the answer
#cmd("?Invalid command.") from the client, and nothing reaches FTPD. Type
#cmd("site") in lower case, or use #cmd("quote"), which sends the line to the
server as it is.]

```
ftp> site lrecl=80
ftp> quote SITE RECFM=FB
```
