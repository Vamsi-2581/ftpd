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
      keyword. The line names the first. The other words still apply.],
    [#cmd("200-text") and #cmd("200 SITE command was accepted")], [A value was
      out of range or refused. The text names the first problem
      (@ref-site-keys).],
    [#cmd("202 SITE not necessary; you may proceed")], [No word.],
  )
]

The code is #cmd("200") in all cases but the last. A script must read the first
line.

== Keywords <ref-site-keys>

#tab(caption: [SITE keywords])[
  #table(columns: (1.2in, 1fr),
    [Keyword], [Effect and rules],
    [#cmd("RECFM=f")], [The record format of a new data set. Upper case. No
      check. No value: the value of #cmd("DEFRECFM").],
    [#cmd("LRECL=n")], [The record length. FTPD converts the value as a number
      and does not check it. No value: #cmd("DEFLRECL").],
    [#cmd("BLKSIZE=n")], [The block size. If the record format starts with
      #cmd("F"), the record length is above 0 and the block size is not a
      multiple of it, FTPD rounds the block size down to a multiple (at least
      one record) and sends the text #cmd("BLOCKSIZE must be a multiple of LRECL for RECFM FB"). No value: #cmd("DEFBLKSIZE").],
    [#cmd("PRIMARY=n")], [The primary quantity, 1 to 16777215. Another value:
      the setting stays and the text is #cmd("PRIMARY must be 1-16777215; keeping the current value"). No value: #cmd("DEFPRIMARY").],
    [#cmd("SECONDARY=n")], [The secondary quantity, 0 to 16777215. Another
      value: #cmd("SECONDARY must be 0-16777215; keeping the current value").
      No value: #cmd("DEFSECONDARY").],
    [#cmd("DIRECTORY=n")], [The directory blocks for #cmd("MKD"), 1 to 16777215.
      Another value: #cmd("DIRECTORY must be 1-16777215; keeping the current value"). No value: the setting is cleared and #cmd("MKD") uses 10.],
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
    [#cmd("TRAILING")], [A switch. Each use turns on or off the sending of the
      blanks at the end of a record in text mode.],
    [#cmd("JESJOBNAME=n")], [The job name for the job list. Upper case.
      #cmd("JESJOBNAME=") with no value ends the filter and
      lists all job names.],
    [#cmd("JESOWNER=n")], [The owner for the job list. Upper case. #cmd("*")
      means all owners. Without it the owner is the user of the session.],
    [#cmd("JESSTATUS=s")], [Accepted and stored in upper case. The job list does
      not use it.],
    [#cmd("TRUNCATE"), #cmd("RDW")], [Switches that are accepted. No part of
      FTPD reads them.],
    [#cmd("SBSENDEOL")], [Accepted. It does nothing.],
    [#cmd("DATACLAS"), #cmd("STORCLAS"), #cmd("MGMTCLAS")], [Accepted and
      ignored, with any value. MVS 3.8j has no SMS.],
  )
]

Every other word is #cmd("Unrecognized parameter"). That includes
#cmd("JESINTERFACELEVEL"), which is a parameter of the configuration member
(@ref-prm) and not a keyword of #cmd("SITE").
