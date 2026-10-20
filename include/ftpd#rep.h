#ifndef FTPD_REP_H
#define FTPD_REP_H
/*
** FTPD FTP Reply Codes
**
** Free of project and MVS headers on purpose, so that the UFSD return code
** mapper (ftpd#urc.c) can be linked into a host unit test (test/tstuer.c).
*/

/* --- FTP Reply Codes --- */
#define FTP_125             125     /* data conn open, xfer starting */
#define FTP_150             150     /* file ok, opening data conn    */
#define FTP_200             200     /* command okay                  */
#define FTP_202             202     /* command not needed            */
#define FTP_211             211     /* system status / FEAT          */
#define FTP_213             213     /* file status (SIZE)            */
#define FTP_214             214     /* help message                  */
#define FTP_215             215     /* system type                   */
#define FTP_220             220     /* service ready                 */
#define FTP_221             221     /* closing control connection    */
#define FTP_226             226     /* closing data, xfer complete   */
#define FTP_227             227     /* entering passive mode         */
#define FTP_229             229     /* entering extended passive mode */
#define FTP_230             230     /* user logged in                */
#define FTP_250             250     /* file action okay              */
#define FTP_257             257     /* pathname created              */
#define FTP_331             331     /* username ok, need password    */
#define FTP_350             350     /* pending further info          */
#define FTP_421             421     /* service not available         */
#define FTP_425             425     /* can't open data connection    */
#define FTP_426             426     /* connection closed, xfer abort */
#define FTP_450             450     /* file unavailable (busy)       */
#define FTP_451             451     /* local error                   */
#define FTP_452             452     /* insufficient storage          */
#define FTP_503             503     /* bad sequence of commands      */
#define FTP_500             500     /* unrecognized command          */
#define FTP_501             501     /* syntax error in parameters    */
#define FTP_502             502     /* command not implemented       */
#define FTP_504             504     /* not impl for that parameter   */
#define FTP_530             530     /* not logged in                 */
#define FTP_550             550     /* file unavailable              */
#define FTP_552             552     /* exceeded storage allocation   */
#define FTP_522             522     /* network proto not supported   */
#define FTP_553             553     /* file name not allowed         */

#endif /* FTPD_REP_H */
