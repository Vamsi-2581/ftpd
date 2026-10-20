#import "../bookmaster/bookmaster.typ": *

= A First Session <ug-first>

#idx("first session")
This chapter goes through one short session: connect, log in, look around,
and log out. It uses the control connection only, so every reply shown is
what FTPD sends, whatever client is used. Moving data comes in the next
chapter.

You need a running FTPD (@ug-ops), its host name and port, and a user ID
that is allowed to use FTP (@ug-security-ftpauth). The examples use the
port 2121 and the user ID #cmd("IBMUSER").

== Choosing a Client <ug-first-client>

#idx("client")
Any FTP client works. Three points apply to FTPD in particular:

- *The client must use passive or active mode with a port the network
  allows.* FTPD answers #cmd("PASV") and #cmd("EPSV") with a port from its
  passive range (#cmd("PASVPORTS"), @ug-ops-config), and it accepts
  #cmd("PORT"). It does not accept #cmd("EPRT")\; a client that tries it
  is told #cmd("522") and falls back.
- *A BSD client treats SITE as its own command.* The #cmd("ftp") of BSD
  systems and #cmd("tnftp") know #cmd("site") in lower case only. Typed as
  #cmd("SITE") or #cmd("Site"), the client answers #cmd("?Invalid command.")
  and never contacts FTPD. Type #cmd("site") in lower case, or use
  #cmd("quote"), which sends the line unchanged (@ug-site-case).
- *Quotes in names matter.* A name in single quotes is a full data set
  name. Many clients need the quotes escaped or entered through a
  different command (@ug-data-names).

== Connecting and Logging In <ug-first-login>

#idx("login")
The session starts when the client connects. FTPD sends its banner,
#cmd("220") followed by the text of the #cmd("BANNER") parameter. The client
sends #cmd("USER"), FTPD asks for the password with #cmd("331"), and the
client sends #cmd("PASS"):

#tab(caption: [The first replies of a session])[
  #table(columns: (1.5in, 1fr),
    [The client], [FTPD replies],
    [connects], [#cmd("220 text"), where the text is the value of
      #cmd("BANNER"). The default is #cmd("MVS 3.8j FTPD Server").],
    [#cmd("USER IBMUSER")], [#cmd("331 Send password please.")],
    [#cmd("PASS password")], [#cmd("230 IBMUSER is logged on.  Working directory is \"IBMUSER.\".") The reply shows the user ID in upper case. The
      working directory is that user ID and a period.],
    [#cmd("FEAT")], [A reply of several lines (see below).],
    [#cmd("PWD")], [#cmd("257 \"'IBMUSER.'\" is working directory.")],
    [#cmd("QUIT")], [#cmd("221 Quit command received. Goodbye.") Then FTPD closes
      the connection.],
  )
]

The user ID and the password are folded to upper case before RAKF checks
them. After a successful login FTPD sets the working directory to the user
ID followed by a period (@ug-data-names). Until the login has succeeded, FTPD
answers only #cmd("USER"), #cmd("PASS"), #cmd("QUIT"), #cmd("SYST"),
#cmd("FEAT"), #cmd("HELP"), #cmd("NOOP"), #cmd("STAT"), #cmd("AUTH") (which gets #cmd("504")) and, if the server
runs behind a TLS proxy, #cmd("PBSZ") and #cmd("PROT"). Every other command
gets #cmd("530 Not logged in.").

#idx("login", "attempts")
After three failed attempts FTPD sends #cmd("530 Login incorrect. Too many attempts, disconnecting.") and closes the connection. A correct password
is not enough: the user ID also needs read access to the profile
#cmd("FTPAUTH") (@ug-security-ftpauth). Without it FTPD answers
#cmd("530 Not authorized for FTP access."), and that counts as an attempt
too.

== Reading the Replies <ug-first-replies>

#idx("reply codes")
Every reply starts with a three-digit code. The first digit says how the
command went:

#deflist(width: 0.9in,
  [#cmd("1xx")], [started: the server begins to send or receive data and
    will send another reply when it is done.],
  [#cmd("2xx")], [done.],
  [#cmd("3xx")], [more input is needed, such as the password after
    #cmd("USER").],
  [#cmd("4xx")], [failed for now. The client may try again, but if the
    cause is a lack of space or storage, trying again will not help.],
  [#cmd("5xx")], [refused. The client has to change the command.],
)

A reply of several lines has a hyphen after the code on every line but the
last, as the reply to #cmd("FEAT") shows. It starts with #cmd("211-Features supported"), lists one feature on each line, and ends with #cmd("211 End"). All
codes
FTPD sends are in “Reply Codes” in the _FTPD Reference_.

#note[*Space is permanent.* A lack of space in a data set or in a UFS file
system is answered with #cmd("552"), not with a #cmd("4xx") code, because
the same transfer fails again until the space is raised
(@ug-data-space).]

== Changing and Leaving <ug-first-leave>

#cmd("CWD") changes the prefix for names without quotes, #cmd("PWD") shows
it, and #cmd("QUIT") ends the session with #cmd("221"). A client that
sends nothing for 300 seconds, the default of #cmd("IDLETIMEOUT"), is
disconnected with #cmd("421 Idle timeout, closing connection").
