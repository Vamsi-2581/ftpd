#import "../bookmaster/bookmaster.typ": *

= Connecting and Logging In <ug-first>

#idx("first session")
This chapter shows how to connect to FTPD, log in, check where you are, and
log out. It is the first of the chapters that describe what a user does. Each
section is a task, and each task has a session that was typed at a real FTPD.

You need a running FTPD (@ug-ops), its host name and port, and a user ID that
is allowed to use FTP (@ug-security-ftpauth).

== How to Read the Sessions <ug-first-read>

#idx("session")
Every session in this book was run with the command line client #cmd("tnftp")
(version 20230507, the #cmd("ftp") of many Linux, BSD and macOS systems)
against FTPD 1.3.0 on a test system. Four things hold for all of them:

- The host is #cmd("localhost") and the port is #cmd("2121"), the port of the
  test system. Use the host name and the port of your own FTPD.
- The user ID is #cmd("IBMUSER"), the password is not shown because the
  client does not echo it, and the user may alter almost every data set on the
  test system. Your user may not, so you may get a #cmd("550") where the
  session shows a success (@ug-security-dsn).
- A line that starts with #cmd("ftp>") is typed by the reader. Every other line
  is printed by the client, either the reply of FTPD, which starts with a
  three-digit code, or a line of the client itself, such as #cmd("local: ...
  remote: ...") or the count of bytes. A line that starts with #cmd("!") is
  a command that the client hands to the shell of the local system. The
  sessions use it to show a local file.
- The sessions use passive mode, the way #cmd("tnftp") talks to FTPD by default.

Besides #cmd("tnftp") the book uses #cmd("curl") for one transfer
(@ug-data-curl). The Windows #cmd("ftp.exe") and graphical clients such as
FileZilla were not used for this edition, so the book does not say how they
differ from #cmd("tnftp").

== Choosing a Client <ug-first-client>

#idx("client")
Any FTP client works. Three points apply to FTPD in particular:

- *The client has to reach the passive port.* FTPD answers #cmd("PASV") and
  #cmd("EPSV") with a port from its passive range (#cmd("PASVPORTS"),
  @ug-ops-config). A firewall between the client and FTPD has to let those
  ports through, or the transfer fails after the login has worked. FTPD
  accepts #cmd("PORT") for active mode. It does not accept #cmd("EPRT") and
  answers #cmd("522"). The client may try it first and then use #cmd("PORT").
- *The client may treat SITE as its own command.* In #cmd("tnftp") the command
  is #cmd("site"), in lower case. The same word in upper case is not a command
  of the client, and nothing is sent. Use #cmd("quote") to send a line to FTPD
  as it is (@ug-site-case).
- *Quotes in names matter.* A name in single quotes is a full data set name
  (@ug-data-names).

== Connecting and Logging In <ug-first-login>

#idx("login")
Start the client with the host name and the port. #cmd("tnftp") asks for the
user ID and the password:

#screen(raw(read("../ex/ug-first/login.txt"), block: true))

The client prints the banner of FTPD, the #cmd("220") line, and asks for a
user ID. FTPD asks for the password with #cmd("331"), and after the right
password it answers #cmd("230") and names the working directory, which is the
user ID and a period. The line #cmd("Remote system type is MVS.") is the
client's report of the answer to #cmd("SYST"), which it sends by itself.
The command #cmd("pwd") shows the working directory again, and
#cmd("quit") ends the session.

A client that is started without a host name connects with #cmd("open"). The
client option #cmd("-n") keeps it from asking for the user ID by itself, so the
user ID is given with #cmd("user"):

#screen(raw(read("../ex/ug-first/login-open.txt"), block: true))

Without #cmd("-n") the client asks for the user ID right after #cmd("open"),
with #cmd("Name"), and takes the next line as the name. A #cmd("user IBMUSER")
typed there is read as the name #cmd("user IBMUSER"), and the login fails with
#cmd("530 Login incorrect.").

The user ID and the password are folded to upper case before RAKF checks them.
After a successful login FTPD sets the working directory to the user ID
followed by a period (@ug-data-names).

#tab(caption: [Client commands and the commands FTPD receives at login])[
  #table(columns: (1.5in, 1.1in, 1fr),
    [You type], [FTPD receives], [FTPD replies],
    [#cmd("open host port")], [(the connection)], [#cmd("220 text"), where the
      text is the value of #cmd("BANNER"). The default is
      #cmd("MVS 3.8j FTPD Server").],
    [#cmd("user IBMUSER")], [#cmd("USER")], [#cmd("331 Send password please.")],
    [the password], [#cmd("PASS")], [#cmd("230 IBMUSER is logged on.  Working
      directory is \"IBMUSER.\".")],
    [#cmd("pwd")], [#cmd("PWD")], [#cmd("257 \"'IBMUSER.'\" is working
      directory.") The client prints #cmd("Remote directory: 'IBMUSER.'").],
    [#cmd("quit")], [#cmd("QUIT")], [#cmd("221 Quit command received.
      Goodbye.") Then FTPD closes the connection.],
  )
]

The _FTPD Reference_ has the table of all client commands and the commands they
send, "Client Commands and Protocol Commands".

== When the Login Fails <ug-first-fail>

#idx("login", "attempts")
A wrong password gets #cmd("530 Login incorrect."), and the client says
#cmd("Login failed."). After the third failed attempt FTPD closes the
connection, and the next command of the client finds it closed:

#screen(raw(read("../ex/ug-first/login-wrong.txt"), block: true))

A correct password is not enough: the user ID also needs read access to the
profile #cmd("FTPAUTH") (@ug-security-ftpauth). A user ID without it gets
#cmd("530 Not authorized for FTP access."), and that counts as an attempt too.
Here the user ID #cmd("MVSCE02") has a good password and no access:

#screen(raw(read("../ex/ug-first/login-noauth.txt"), block: true))

The connection stays open, but the session is not logged in. #cmd("tnftp")
knows this and refuses #cmd("pwd") itself, with its own line #cmd("Unable to
determine remote directory"). FTPD answers #cmd("530 Not logged in.") to a
command that a client sends anyway, except for the few commands that work
before a login (see "Before and After the Login" in the _FTPD Reference_). If a
login is refused although the password is right, ask the administrator
whether the user ID may use FTP.

== Reading the Replies <ug-first-replies>

#idx("reply codes")
Every reply of FTPD starts with a three-digit code, and the client prints it.
The first digit says how the command went:

#deflist(width: 0.9in,
  [#cmd("1xx")], [started: the server begins to send or receive data and
    will send another reply when it is done.],
  [#cmd("2xx")], [done.],
  [#cmd("3xx")], [more input is needed, such as the password after the user
    ID.],
  [#cmd("4xx")], [failed for now. The client may try again, but if the
    cause is a lack of storage in the started task, trying again will not
    help.],
  [#cmd("5xx")], [refused. The client has to change the command.],
)

A reply of several lines has a hyphen after the code on every line but the
last, as the reply to #cmd("site") shows when a value is wrong
(@ug-site-form). All codes that FTPD sends are in "Reply Codes" in the _FTPD
Reference_. Typical errors, and what to do about them, are in @ug-errors.

#note[*Space is permanent.* A lack of space in a data set or in a UFS file
system is answered with #cmd("552"), not with a #cmd("4xx") code, because
the same transfer fails again until the space is raised
(@ug-data-space).]

== Changing and Leaving <ug-first-leave>

#cmd("cd") changes the prefix for names without quotes (@ug-data-cwd),
#cmd("pwd") shows it, and #cmd("quit") ends the session with #cmd("221"). A
client that sends nothing for 300 seconds, the default of #cmd("IDLETIMEOUT"),
is disconnected with #cmd("421 Idle timeout, closing connection").
