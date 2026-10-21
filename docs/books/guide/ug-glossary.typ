#import "../bookmaster/bookmaster.typ": *

= Glossary <ug-glossary>

This glossary defines the terms of FTPD, and of FTP, MVS and JES2 as far as this
book uses them. A term printed in _italics_ in a definition has an entry of
its own.

#let g(..items) = deflist(width: 1.45in, ..items.pos().enumerate().map(
  ((i, x)) => if calc.even(i) { par(justify: false, strong(x)) } else { x }))

#g(
  [ABEND], [The abnormal end of a program. FTPD catches the ABEND of a single
    command and keeps the session (@ug-ops-abend).],
  [ACEE], [The accessor environment element. It holds the identity of a user
    for the security product. A session of FTPD has its own ACEE for the time
    of a command (@ug-security-stc).],
  [control connection], [The TCP connection on which a client sends commands
    and FTPD sends replies. It is the one that the client opens to the
    _port_ of FTPD.],
  [CWD], [The FTP command that changes the working directory. In FTPD it also
    chooses between the data set view and the _UFS_ view (@ug-other-ufs).],
  [data connection], [The TCP connection that carries the content of a
    file or a list. A command opens one for each transfer.],
  [FACILITY class], [A resource class of the security product. FTPD checks the
    resource #cmd("FTPAUTH") in it to decide who may log in
    (@ug-security-ftpauth).],
  [FTPD], [The FTP server for MVS 3.8j.],
  [HFS], [The name that FTP replies use for a file system with directories.
    In FTPD it is _UFS_.],
  [identity window], [The time in which FTPD runs a command under the _ACEE_ of
    the user and not under that of the _started task_ (@ug-security-stc).],
  [inode], [The record of a file or directory in _UFS_. A file system has a
    fixed number of them (@ug-other-ufs).],
  [internal reader], [The JES2 interface that takes JCL from a program
    and submits it as a job (@ug-other-jes).],
  [JES2], [The job entry subsystem of MVS 3.8j. It queues jobs and keeps their
    output in the _spool_.],
  [member], [One part of a _PDS_, addressed as #cmd("DSN(MEMBER)").],
  [passive mode], [The way of opening a _data connection_ in which the client
    connects to a port of FTPD. The commands are #cmd("PASV") and
    #cmd("EPSV"). The ports come from #cmd("PASVPORTS") (@ug-ops-config).],
  [PDS], [A partitioned data set: a data set that holds members and a
    directory. FTPD shows it as a directory (@ug-data-pds).],
  [RAKF], [The security product that FTPD uses on MVS 3.8j. It checks user ID
    and password, and the access to data sets (@ug-security).],
  [spool], [The storage of JES2 for the input and output of jobs.],
  [started task], [A program that the operator starts with #cmd("S")
    and that runs without a user. FTPD runs as one (@ug-ops-start).],
  [UFS], [The Unix-like file system that the address space _UFSD_ provides.],
  [UFSD], [The address space that serves the file system _UFS_. FTPD needs it
    for the UFS view, and works without it for the rest.],
  [view], [One of the three ways in which FTPD shows the host to a client: the
    data sets, the jobs of _JES2_ and the files of _UFS_.],
)
