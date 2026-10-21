#import "../bookmaster/bookmaster.typ": *

= Submitting Jobs and Reading Their Output <ug-jes>

#idx("JES")#idx("view")#idx("FILETYPE")
Besides MVS data sets, FTPD gives a client a view of the jobs in JES2. In this
view a client submits a job, lists the jobs, reads the output of a job, and
cancels a job. The view uses the same client commands as the data set view
(@ug-data), and the commands then do something else. A session starts in the
data set view. This chapter goes through the tasks of the JES view, one session
for each. The sessions run on one system in sequence, so the job IDs follow one
another.

The view reads the JES2 checkpoint and the spool, which FTPD finds itself; the
procedure of the started task needs no statements for them
(@ug-install-jes). When FTPD cannot open the checkpoint, #cmd("dir") and #cmd("ls") end
with #cmd("451 Unable to open JES checkpoint"), and #cmd("get") and
#cmd("delete") answer #cmd("550 Jobid ... not found ..."). #cmd("put") does not
read the checkpoint.

== Switching to the JES View and Back <ug-jes-switch>

A client switches to the view with a #cmd("SITE") command (@ug-site), which
the client command #cmd("site") sends, and back with another one:

```
ftp> site filetype=jes
ftp> site filetype=seq
```

#cmd("quote site filetype=jes") does the same, for a client without
#cmd("site"). The commands #cmd("STOR"), #cmd("LIST"), #cmd("NLST"),
#cmd("RETR") and #cmd("DELE") go to JES2 as long as the view is on. In client
commands: #cmd("put") submits a job, #cmd("dir") and #cmd("ls") list jobs,
#cmd("get") fetches output, and #cmd("delete") cancels a job. All other
commands work as before, and #cmd("pwd") shows the working directory of the
view that was active when the client switched (the data set prefix in the last
session of this chapter, or the UFS path (@ug-ufs)). #cmd("site filetype=seq")
returns to the view that was active when the client switched to JES, also
after a #cmd("cd") in the JES view. A session that never switched to JES has
the data set view as that view, so #cmd("site filetype=seq") in UFS moves it to
the data set view.

== Submitting a Job <ug-jes-submit>

#idx("job", "submit")#idx("internal reader")
The session switches to the JES view, submits a job with #cmd("put"), and
lists the jobs. The job is the file #cmd("hello.jcl") of the local directory:

#screen(```
//FTPHELLO JOB (ACCT),'FTP TEST',CLASS=A,MSGCLASS=H
//STEP1    EXEC PGM=IEBGENER
//SYSPRINT DD SYSOUT=H
//SYSUT1   DD *
HELLO FROM A JOB THAT FTPD SUBMITTED
/*
//SYSUT2   DD SYSOUT=H
//SYSIN    DD DUMMY
```)

#screen(raw(read("../ex/ug-jes/j-submit.txt"), block: true))

#cmd("put") sends the JCL to the JES2 internal reader (#cmd("STOR")). FTPD
ignores the file name of the command, and the client prints the name of the
local file for both sides. The reply to the command shows the transfer: the
start is #cmd("125 Sending Job to JES internal reader FIXrecfm 80"), and the end
is a reply of two lines, #cmd("250-It is known to JES as JOB00024") and
#cmd("250 Transfer completed successfully."). The job ID is the one that the
following commands use. The job in the example ends with the return code 0000
by the time #cmd("dir") lists it.

The job's output stayed in the class #cmd("H") because the JCL says
#cmd("MSGCLASS=H") and #cmd("SYSOUT=H"). A class that the system purges at once
leaves nothing to fetch (@ug-jes-output). Use a held class for the output you
want to read.

FTPD reads the JCL line by line. A line is one card of 80 columns. A line that
is longer is cut at column 80, a shorter one is filled with blanks, and a
carriage return at the end of a line is removed. FTPD converts the text to
EBCDIC only in text mode (#cmd("ascii")). In binary mode it sends the bytes as
they come and does not find the JOB statement, so it adds nothing to it. Send
JCL as text.

FTPD looks for the JOB statement, the first card that has the word JOB between
blanks. It changes the statement in this way:

- At the end of the JOB statement, FTPD adds a comma and then up to
  three continuation cards.
- The first card is #cmd("NOTIFY=DUMMY,"). FTPD adds it only if the JOB
  statement has no #cmd("NOTIFY") yet.
- The second card is #cmd("USER=") with the user ID of the session.
- The third card is #cmd("PASSWORD=") with the password of the session.

The job runs under the identity of the user, not under the one of the started
task (@ug-security-jes). FTPD does not look for a #cmd("USER=") or a
#cmd("PASSWORD=") that the JCL has already, so leave them out. FTPD adds the
comma only if the last line of the JOB statement ends before column 71. Keep
that line shorter.

FTPD does not check the JCL. A JCL error shows in the list of the job
(@ug-jes-list), as the text #cmd("JCL ERROR") for the job.

If FTPD cannot hand the JCL over, the replies are
#cmd("451 Unable to open JES internal reader"),
#cmd("451 Failed to submit JCL to internal reader") and
#cmd("451 Failed to close internal reader"). A data connection that cannot be
opened gives #cmd("425 Cannot open data connection").

== Listing Jobs <ug-jes-list>

#idx("job", "list")
#cmd("dir"), #cmd("ls") (#cmd("LIST")) and #cmd("nlist") (#cmd("NLST")) give the
same list of jobs. In the session above, the reply that opens the list names
the filters of the session (below):

#screen(```
125 List started OK for JESJOBNAME=*, JESSTATUS=ALL and JESOWNER=IBMUSER
```)

The list has one line for each job after the header:

#tab(caption: [The columns of the job list])[
  #table(columns: (1.1in, 1fr),
    [Column], [Meaning],
    [JOBNAME, JOBID, OWNER], [The name, the ID and the user ID of the owner.],
    [STATUS], [#cmd("ACTIVE"), #cmd("INPUT"), #cmd("XMIT"), #cmd("SETUP"),
      #cmd("RECEIVE"), #cmd("OUTPUT") or #cmd("UNKNOWN").],
    [CLASS], [#cmd("STC") for a started task and #cmd("TSU") for a TSO user.
      For a job it is the execution class.],
    [(no title)], [For a job in the status #cmd("OUTPUT") the result: for
      example #cmd("RC=0004"), #cmd("ABEND S0C4"), #cmd("ABEND U0123") or
      #cmd("JCL ERROR").],
  )
]

The list is not what the client asks for with the argument. FTPD takes the
filters from the session. The next session submits a job with a JCL error, and
sets the three filters one after the other:

#screen(raw(read("../ex/ug-jes/j-filter.txt"), block: true))

The new job #cmd("FTPBAD") is in the list with #cmd("JCL ERROR") (the JCL of
the file #cmd("bad.jcl") has a misspelled keyword on its #cmd("EXEC")
statement). The filters are:

- *The owner:* #cmd("site jesowner=name") sets it. Without it the owner is the
  user ID of the session, so a user sees the own jobs. FTPD compares the
  beginning of the owner of each job with the value. #cmd("site jesowner=*")
  lists the jobs of all users. The session set it to a name that has no jobs, and the
  list was empty. The header #cmd("125") names the filter that applies.
- *The job name:* #cmd("site jesjobname=name") limits the list to the jobs
  with this name. The session used #cmd("FTPH*"), and the list had only the job
  of the first session.
- *The status:* #cmd("site jesstatus=status") limits the list to the jobs in
  one status: #cmd("INPUT"), #cmd("ACTIVE") or #cmd("OUTPUT"). The default is
  #cmd("ALL"), every status. The session asked for #cmd("ACTIVE"), and no job
  was active. A job in one of the other states of the column STATUS
  (#cmd("XMIT"), #cmd("SETUP"), #cmd("RECEIVE"), #cmd("UNKNOWN")) is listed with
  #cmd("ALL") only. A status that is not one of the four is refused with a
  reply that says so in its first line, as the last #cmd("site") of the session
  shows, and the filter that was set stays.

The values are not case sensitive. A client can set them back to everything
with #cmd("site jesowner=*"), #cmd("site jesjobname=*") and
#cmd("site jesstatus=all"), as the session does.

The list has at most 200 jobs. When it reaches 200 jobs, the list ends with
these two lines, and jobs after the 200th are not shown. The lines also appear
for a list of exactly 200 jobs, when nothing was left out:

#screen(```
250-JESENTRYLIMIT of 200 reached.  Additional entries not displayed
250 List completed successfully.
```)

Narrow the filters to see the others.

#note[*A job ID as the argument.* An argument that starts with #cmd("JOB"), in
any case, is read as a job ID and gives the list of the spool files of that job
(the next section). It is not a filter. To list jobs with a name that starts
with #cmd("JOB"), use #cmd("site jesjobname").]

== Reading the Output of a Job <ug-jes-output>

#idx("job", "output")#idx("spool")
The command #cmd("dir JOB00024") lists the spool files of the job, one line for
each, with the step, the procedure step, the output class and the DD name. The
command #cmd("get") (#cmd("RETR")) fetches them. The session lists the spool
files, fetches all of them in one file, fetches the third one alone, and asks
for two that do not exist:

#screen(raw(read("../ex/ug-jes/j-output.txt"), block: true))

- *#cmd("get JOB00024 out.txt"):* all output files of the job, one after the
  other, in one local file. FTPD writes the line #cmd(" !! END OF JES SPOOL FILE !!")
  after every file. The start is #cmd("125 Sending all spool files for requested Jobid").
  The local file starts with the job log.
- *#cmd("get JOB00024.3 spool3.txt"):* only the third output file. The number
  counts the output files of the job from 1. The start is #cmd("125 Sending data
  set") and the name of the file: the owner, the job name, the job ID, the ID of
  the data set in JES2 (#cmd("D0000004") in the example, not the number that
  was typed) and the DD name.

The list of the spool files has eight lines, and only five of them can be
fetched. The number after the point does not count the input files of the job:
the JCL that was read (#cmd("JESJCLIN")), the input text (#cmd("JESINTXT")) and the data of #cmd("SYSIN") that is part of the job (#cmd("SYSUT1") of
the example). In the example #cmd("JOB00024.3") is therefore the third of
#cmd("JESMSGLG"), #cmd("JESJCL"), #cmd("JESYSMSG"), #cmd("SYSPRINT") and
#cmd("SYSUT2"), and #cmd("JOB00024.9") is past the five. The first of the two
error lines at the end of the session shows this reply, and the second one is
for a job that JES2 does not know. The byte count of the list is the number of
records times the record length of the file, so it is not exact.

FTPD converts the text to ASCII in text mode (#cmd("ascii")). A line longer than
253 characters is cut. A spool file that holds no data keeps its number, and
a #cmd("get") of all the files sends nothing for it. The end is
#cmd("250 Transfer completed successfully."). A job whose output went to a
class that the system purges has nothing to fetch.

The error replies are:

#tab(caption: [The error replies of RETR in the JES view])[
  #table(columns: (2.6in, 1fr),
    [Reply], [Cause],
    [#cmd("501 Missing job ID")], [No argument.],
    [#cmd("550 Jobid JOB00042 not found for JESJOBNAME=*, JESSTATUS=ALL, JESOWNER=USERID")],
      [JES2 does not know the job, or the job is not in the status that
      #cmd("JESSTATUS") names. The text shows the filters of the session.],
    [#cmd("550 Spool file 5 not found for Jobid JOB00042, 3 spool file(s) available")],
      [The number after the point is 0 or larger than the number of output
      files. The text says how many files the job has.],
    [#cmd("550 Spool data set for JOB00042 no longer available.")],
      [The job is in the list of JES2, but its spool data set is gone.],
    [#cmd("451 Error reading spool data set for JOB00042.")],
      [The read of the spool failed. What FTPD sent is incomplete.],
    [#cmd("426 Data connection closed, transfer aborted.")],
      [The client closed the data connection.],
  )
]

#note[*The job ID and the status.* #cmd("get") and #cmd("delete") find the job
by its ID. The filters #cmd("JESOWNER") and #cmd("JESJOBNAME") do not apply,
and FTPD does not compare the owner of the job with the user ID of the
session. The 550 text names them, but they do not take part in the search.
#cmd("JESSTATUS") does take part: with #cmd("site jesstatus=active") the output
of a job that has ended cannot be fetched, and a job that is not active cannot
be cancelled. Set #cmd("JESSTATUS") back to #cmd("ALL") first.]

== Cancelling a Job and Leaving the View <ug-jes-cancel>

#idx("job", "cancel")
#cmd("delete JOB00042") (#cmd("DELE")) cancels the job and removes its output
from the spool. The session cancels the two jobs of the earlier sessions, lists
the jobs after each, and returns to the data set view:

#screen(raw(read("../ex/ug-jes/j-delete.txt"), block: true))

The reply is #cmd("250 Cancel successful"). The second #cmd("delete") of the
same job gets the #cmd("550"): the job is gone. The other errors are:

- #cmd("501 Missing job ID") without an argument.
- #cmd("550 Access denied - cannot cancel JOB00042") when JES2 refuses.
- #cmd("550 Failed to cancel JOB00042 (rc=n)") for any other code of JES2, and
  the #cmd("550 Jobid ... not found") reply for a job that JES2 does not know.

#cmd("site filetype=seq") ends the JES view, and #cmd("pwd") shows the working
directory of the view that was active before. Any other value of #cmd("FILETYPE") ends the view too.
