#import "../bookmaster/bookmaster.typ": *

= Setting Up Security <ug-security>

#idx("security")#idx("RAKF")
FTPD has no user database of its own. RAKF verifies every user ID and
password, and RAKF decides every access to a data set. Without RAKF nobody can
log in, and no parameter turns that off. This chapter says what FTPD asks RAKF,
what a client sees when RAKF says no, and which identity the started task
itself has. The tables, the profiles and the steps to change them are in
`docs/FTPD_RAKF_SETUP.md` in the FTPD (ftpd) repository. This book does not
repeat them.

== Checking How Users Log In <ug-security-login>

#idx("login")#idx("password")
A session ends in the state "logged in" only after RAKF has accepted the user
ID and the password and the user may use FTP (@ug-security-ftpauth). FTPD
folds both to upper case before it asks RAKF, so the case of the password
does not matter. A password longer than eight characters is cut to eight.

A client has three tries:

- *RAKF refuses the user ID or the password:* #cmd("530 Login incorrect.")
  and the client may start again with #cmd("USER").
- *The third try fails:* #cmd("530 Login incorrect. Too many attempts, disconnecting.") and FTPD closes the control connection.
- *The login works:* #cmd("230 USERID is logged on.  Working directory is \"USERID.\".") The working directory is the user ID and a period
  (@ug-data-cwd).

The count is per connection. It starts again at zero after a successful
login. The replies are the same for an unknown user and for a wrong password,
so a client cannot find out which user IDs exist.

Before the login FTPD answers only a short list of commands. The list is
#cmd("USER"), #cmd("PASS"), #cmd("QUIT"), #cmd("SYST"), #cmd("FEAT"),
#cmd("HELP"), #cmd("NOOP") and #cmd("STAT"). #cmd("AUTH") is answered with
#cmd("504 Security mechanism not implemented."). When the parameter
#cmd("SSLPROXY") is #cmd("YES"), #cmd("PBSZ") and #cmd("PROT") are answered as
well. Every other command gets #cmd("530 Not logged in.").

#note[*FTPD does not encrypt.* The user ID, the password and the data cross the
network as they are. FTPD does not do TLS itself. With #cmd("SSLPROXY=YES") it
accepts #cmd("PBSZ") and #cmd("PROT") from a client that talks to a TLS proxy
in front of FTPD (@ug-ops-config). It does not check that a proxy exists.]

== Allowing a User to Use FTP: FTPAUTH <ug-security-ftpauth>

#idx("FTPAUTH")#idx("FACILITY class")
After the password is right, FTPD asks RAKF whether the user has READ access to
the resource #cmd("FTPAUTH") in the class #cmd("FACILITY"). If not, the answer
is the same as for a wrong password, with another text:

```
530 Not authorized for FTP access.
```

The third refusal ends the connection: #cmd("530 Not authorized for FTP access. Disconnecting."). A refused attempt counts like a wrong password.

#note[*Define the profile.* When no profile covers #cmd("FTPAUTH"), RAKF
answers that the resource is not protected, and FTPD lets the user in. This is
not a safe default. RAKF gives ALTER access to resources that have no profile,
so every user who knows a password can use FTP. The setup guide has the two
lines to add.]

== Controlling Access to Data Sets <ug-security-dsn>

#idx("data set", "access")#idx("DATASET class")
For every command that touches a data set, FTPD asks RAKF whether the
_logged-in user_ may do it. The class is #cmd("DATASET"). FTPD does not use the
identity of the started task for this check.

#tab(caption: [The access that each command needs])[
  #table(columns: (1.6in, 1fr),
    [Command], [Access],
    [#cmd("RETR")], [READ],
    [#cmd("LIST") and #cmd("NLST") of the members of a partitioned data set],
      [READ],
    [#cmd("STOR"), #cmd("APPE")], [UPDATE],
    [#cmd("DELE"), #cmd("MKD"), #cmd("RMD")], [ALTER],
    [#cmd("RNFR"); #cmd("RNTO") for the new name too], [ALTER],
  )
]

A refusal is answered with the name of the data set:

```
550 Access denied to IBMUSER.PAYROLL.DATA
```

When no profile covers the data set, RAKF says so, and FTPD allows the access.
This is the answer that data set OPEN, IDCAMS and the catalog accept as well. A
system without #cmd("DATASET") profiles lets every user do everything with
every data set, and FTPD only shows that.

#note[*A list of data sets shows names, not contents.* A catalog list
(#cmd("LIST") without a partitioned data set as working directory) returns
every name below the prefix, also names of data sets that the user may not
read. The profile protects the contents: #cmd("RETR") of such a data set is
refused with #cmd("550"). This is how the catalog works on MVS, and the same
is true for #cmd("LISTCAT") in TSO.]

== Giving the Started Task Its Identity <ug-security-stc>

#idx("started task", "identity")#idx("FTPD004I")#idx("FTPD004W")
The started task is not a user, and it has an identity of its own: the one that
RAKF gives it when the system starts it. FTPD uses this identity for what the
started task itself does. It puts it back after every operation that runs
under a user's identity and after every ABEND that it recovers from.

Since FTPD 1.3.0 FTPD takes the identity as RAKF gave it. It reports the
identity at startup:

```
FTPD004I STC RUNS AS STC/STCGROUP
```

Earlier releases logged on as the user #cmd("FTPD") in the group #cmd("USER") at
startup. FTPD 1.3.0 does not, and the line for #cmd("FTPD") in the RAKF table
of users is no longer used by FTPD.

What the message shows depends on the RAKF release:

- *RAKF maps the name of the started task to an identity:* the message shows
  that identity. This is what you want, with an identity that has no
  Operations authority. RAKF 2.0.0 does this: the identity is the user ID in
  the table of users that is named like the started procedure,
  #cmd("FTPD"), with the group and the Operations flag of its entry.
  @ug-install-rakfsetup says what the group of that user needs.
- *RAKF has no such mapping:* the message shows #cmd("STC/STCGROUP"), the
  identity that RAKF gives every started task. #cmd("STC") has Operations
  authority. This is the case on TK5 before Update 6 and on RAKF 1.2.x. On
  RAKF 2.0.0 it is the case when the table of users has no user ID
  #cmd("FTPD").
- *The address space has no security environment:* the message is
  #cmd("FTPD004W NO SECURITY ENVIRONMENT FOR THE STC: RAKF PERMITS ANY ACCESS WITHOUT AN ACEE"). RAKF treats the missing environment as "access permitted",
  so the checks that do not run in a user's session permit everything. Do not
  start FTPD like this in production.

#note[*The identity of the started task has to stay least-privilege.* Sessions
run in parallel and share one security field of the address space. When a
session ends in an ABEND, or when two operations meet, a concurrent session
can for a moment run under the identity of the started task. With a low
identity such a session can only lose authority. With #cmd("STC/STCGROUP")
it could gain authority for the moment. So do not give the started task
access to data sets, and map it to a low identity if your RAKF can. Read the
message at every start.]

== Checking the Jobs That Users Submit <ug-security-jes>

#idx("JES", "security")
When a client stores a job in the JES view (@ug-jes), FTPD adds the
user ID and the password of the session to the JOB statement
(#cmd("USER=") and #cmd("PASSWORD=")), so the job runs as that user and not as
the started task. FTPD keeps the password in the memory of the session for
this. FTPD looks for the JOB statement, adds #cmd("NOTIFY=DUMMY") when the
statement has no #cmd("NOTIFY"), and does not look at the rest of the JCL. It
adds the two operands only when it finds a JOB statement. What the job may do is what
RAKF allows this user.

#note[*Other users' jobs.* #cmd("get") and #cmd("delete") in the JES view find a
job by its ID and do not compare its owner with the user ID of the session
(@ug-jes-output). A user who has FTP access can fetch the output of another
user's job when the ID is known, and can cancel it. Do not give FTP access to a
user who must not have this.]
