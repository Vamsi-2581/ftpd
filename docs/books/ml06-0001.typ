#import "bookmaster/bookmaster.typ": *

#show: book.with(
  title: "FTPD for MVS 3.8j",
  subtitle: "User's Guide",
  short-title: "FTPD User's Guide",
  product: "FTPD",
  number: "ML06-0001-0",
  date: "October 2026",
  authors: ("Mike Großmann",),
  edition: [
    #text(font: head-font, weight: "bold", size: 11pt)[First Edition (October 2026)]

    This edition applies to Version 1 Release 3 of FTPD, the FTP server for
    MVS 3.8j (ftpd 1.3.0), and to all subsequent releases and modifications
    until otherwise indicated in new editions.

    *Draft.* FTPD 1.3 is still in development, and so is this book: chapters
    still to be written say so.

    Comments on this book may be addressed to the issue tracker of the
    mvslovers/ftpd repository on GitHub.

    © Copyright Mike Großmann 2026. All rights reserved.
  ],
)

#part("index.html", title: [FTPD User's Guide])[
#titlepage()
#contents()
#figures()

#heading(numbering: none)[About This Book] <about>

This book shows how to use FTPD (ftpd), the FTP server for MVS 3.8j: how to
log in, how data sets are named, how to store, append to and fetch them,
how to describe a new data set with SITE, how FTPD uses the security system,
and how an operator runs the started task. Every command, reply code,
parameter and message in full is the subject of the companion volume, the
_FTPD Reference_.

The book describes FTPD 1.3. Installing FTPD, upgrading it and removing it
are described in the installation guide in the `docs/` directory of the
repository, not here.

== How This Book Is Organized

#deflist(width: 1.35in,
  [Chapter 1], [“Introducing FTPD”.],
  [Chapter 2], [“A First Session”.],
  [Chapter 3], [“Working with Data Sets”.],
  [Chapter 4], [“Describing New Data Sets with SITE”.],
  [Chapter 5], [“Security and Identity”.],
  [Chapter 6], [“Running FTPD”.],
  [Chapter 7], [“JES and UFS”.],
  [Appendix A], [“Glossary”.],
)

== Related Publications

#deflist(width: 1.35in,
  [ML06-0002], [_FTPD Reference_: the FTP commands and reply codes, the
    SITE keywords, the FTPDPRM parameters, the console commands and the
    messages of FTPD.],
  [ML02-0001], [_MBT User's Guide_: the build tool that builds FTPD and
    packages its releases.],
)

#mainmatter()
]
#set page(numbering: "1")

#part("ug-intro.html", include "guide/ug-intro.typ")
#part("ug-first.html", include "guide/ug-first.typ")
#part("ug-data.html", include "guide/ug-data.typ")
#part("ug-site.html", include "guide/ug-site.typ")
#part("ug-security.html", include "guide/ug-security.typ")
#part("ug-ops.html", include "guide/ug-ops.typ")
#part("ug-other.html", include "guide/ug-other.typ")

#show: appendices
#part("ug-glossary.html", include "guide/ug-glossary.typ")

#part("index-terms.html", title: [Index])[
#heading(numbering: none)[Index]
#make-index()
]
