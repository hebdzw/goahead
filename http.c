/*
    http.c -- GoAhead HTTP engine

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/******************************** Description *********************************/

/*
 *  This module implements an embedded HTTP/1.1 web server. It supports
 *  loadable URL handlers that define the nature of URL processing performed.
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/********************************** Defines ***********************************/

#define kLt '<'
#define kLessThan T("&lt;")
#define kGt '>'
#define kGreaterThan T("&gt;")

#define RANDOMKEY   T("onceuponatimeinparadise")
#define NONCE_SIZE  34
#define HASH_SIZE   16

/******************************** Global Data *********************************/

websStatsType   websStats;              /* Web access stats */
webs_t          *webs;                  /* Open connection list head */
sym_fd_t        websMime;               /* Set of mime types */
int             websMax;                /* List size */
int             websPort;               /* Listen port for server */
int             websDebug;              /* Run in debug mode and defeat timeouts */

//  MOB - should allocate
char_t          websHost[64];           /* Host name for the server */
char_t          websIpaddr[64];         /* IP address for the server */
char_t          *websHostUrl = NULL;    /* URL to access server */
char_t          *websIpaddrUrl = NULL;  /* URL to access server */

/*
    htmExt is declared in this way to avoid a Linux and Solaris segmentation
    fault when a constant string is passed to strlower which could change its
    argument.
 */
static char_t  htmExt[] = T(".htm");

/*********************************** Locals ***********************************/
/*
    Addd entries to the MimeList as required for your content
    MOB - compare with appweb
 */
websMimeType websMimeList[] = {
    { T("application/java"), T(".class") },
    { T("application/java"), T(".jar") },
    { T("text/html"), T(".asp") },
    { T("text/html"), T(".htm") },
    { T("text/html"), T(".html") },
    { T("text/xml"), T(".xml") },
    { T("image/gif"), T(".gif") },
    { T("image/jpeg"), T(".jpg") },
    { T("image/png"), T(".png") },
    { T("image/vnd.microsoft.icon"), T(".ico") },
    { T("text/css"), T(".css") },
    { T("text/plain"), T(".txt") },
    { T("application/x-javascript"), T(".js") },
    { T("application/x-shockwave-flash"), T(".swf") },

    { T("application/binary"), T(".exe") },
    { T("application/compress"), T(".z") },
    { T("application/gzip"), T(".gz") },
    { T("application/octet-stream"), T(".bin") },
    { T("application/oda"), T(".oda") },
    { T("application/pdf"), T(".pdf") },
    { T("application/postscript"), T(".ai") },
    { T("application/postscript"), T(".eps") },
    { T("application/postscript"), T(".ps") },
    { T("application/rtf"), T(".rtf") },
    { T("application/x-bcpio"), T(".bcpio") },
    { T("application/x-cpio"), T(".cpio") },
    { T("application/x-csh"), T(".csh") },
    { T("application/x-dvi"), T(".dvi") },
    { T("application/x-gtar"), T(".gtar") },
    { T("application/x-hdf"), T(".hdf") },
    { T("application/x-latex"), T(".latex") },
    { T("application/x-mif"), T(".mif") },
    { T("application/x-netcdf"), T(".nc") },
    { T("application/x-netcdf"), T(".cdf") },
    { T("application/x-ns-proxy-autoconfig"), T(".pac") },
    { T("application/x-patch"), T(".patch") },
    { T("application/x-sh"), T(".sh") },
    { T("application/x-shar"), T(".shar") },
    { T("application/x-sv4cpio"), T(".sv4cpio") },
    { T("application/x-sv4crc"), T(".sv4crc") },
    { T("application/x-tar"), T(".tar") },
    { T("application/x-tgz"), T(".tgz") },
    { T("application/x-tcl"), T(".tcl") },
    { T("application/x-tex"), T(".tex") },
    { T("application/x-texinfo"), T(".texinfo") },
    { T("application/x-texinfo"), T(".texi") },
    { T("application/x-troff"), T(".t") },
    { T("application/x-troff"), T(".tr") },
    { T("application/x-troff"), T(".roff") },
    { T("application/x-troff-man"), T(".man") },
    { T("application/x-troff-me"), T(".me") },
    { T("application/x-troff-ms"), T(".ms") },
    { T("application/x-ustar"), T(".ustar") },
    { T("application/x-wais-source"), T(".src") },
    { T("application/zip"), T(".zip") },
    { T("audio/basic"), T(".au snd") },
    { T("audio/x-aiff"), T(".aif") },
    { T("audio/x-aiff"), T(".aiff") },
    { T("audio/x-aiff"), T(".aifc") },
    { T("audio/x-wav"), T(".wav") },
    { T("audio/x-wav"), T(".ram") },
    { T("image/ief"), T(".ief") },
    { T("image/jpeg"), T(".jpeg") },
    { T("image/jpeg"), T(".jpe") },
    { T("image/tiff"), T(".tiff") },
    { T("image/tiff"), T(".tif") },
    { T("image/x-cmu-raster"), T(".ras") },
    { T("image/x-portable-anymap"), T(".pnm") },
    { T("image/x-portable-bitmap"), T(".pbm") },
    { T("image/x-portable-graymap"), T(".pgm") },
    { T("image/x-portable-pixmap"), T(".ppm") },
    { T("image/x-rgb"), T(".rgb") },
    { T("image/x-xbitmap"), T(".xbm") },
    { T("image/x-xpixmap"), T(".xpm") },
    { T("image/x-xwindowdump"), T(".xwd") },
    { T("text/html"), T(".cfm") },
    { T("text/html"), T(".shtm") },
    { T("text/html"), T(".shtml") },
    { T("text/richtext"), T(".rtx") },
    { T("text/tab-separated-values"), T(".tsv") },
    { T("text/x-setext"), T(".etx") },
    { T("video/mpeg"), T(".mpeg mpg mpe") },
    { T("video/quicktime"), T(".qt") },
    { T("video/quicktime"), T(".mov") },
    { T("video/x-msvideo"), T(".avi") },
    { T("video/x-sgi-movie"), T(".movie") },
    { NULL, NULL},
};

/*
    Standard HTTP error codes
 */

websErrorType websErrors[] = {
    { 200, T("Data follows") },
    { 204, T("No Content") },
    { 301, T("Redirect") },
    { 302, T("Redirect") },
    { 304, T("Use local copy") },
    { 400, T("Page not found") },
    { 401, T("Unauthorized") },
    { 403, T("Forbidden") },
    { 404, T("Site or Page Not Found") },
    { 405, T("Access Denied") },
    { 500, T("Web Error") },
    { 501, T("Not Implemented") },
    { 503, T("Site Temporarily Unavailable. Try again.") },
    { 0, NULL }
};

#if BIT_ACCESS_LOG
static char_t   accessLog[64] = T("access.log");    /* Log filename */
static int      accessFd;                           /* Log file handle */
#endif

//  MOB - review
static char_t   websRealm[64] = T("GoAhead");   /* Realm name */

static int      websListenSock;                 /* Listen socket */

#if BIT_PACK_SSL
static int      sslListenSock;                  /* SSL Listen socket */
#endif

static int      websOpenCount;                  /* count of apps using this module */

/**************************** Forward Declarations ****************************/

static int      setLocalHost();
static int      websGetInput(webs_t wp, char_t **ptext, ssize *nbytes);
static int      websParseFirst(webs_t wp, char_t *text);
static void     websParseRequest(webs_t wp);
static void     websSocketEvent(int sid, int mask, void* data);
static time_t   getTimeSinceMark(webs_t wp);

#if BIT_ACCESS_LOG
static void     websLog(webs_t wp, int code);
#endif
#if BIT_IF_MODIFIED
static time_t   dateParse(time_t tip, char_t *cmd);
#endif

/*********************************** Code *************************************/

int websOpen()
{
    //  MOB - move into an osdepOpen() platformOpen
#if WINDOWS || VXWORKS
    rand();
#else
    random();
#endif

    traceOpen();
    socketOpen();
    if (setLocalHost() < 0) {
        return -1;
    }
#if BIT_PACK_SSL
    websSSLOpen();
#endif 
    return 0;
}


void websClose() 
{
#if BIT_PACK_SSL
    websSSLClose();
#endif
    websCloseServer();
    socketClose();
    traceClose();
}


int websOpenServer(char_t *ip, int port, int sslPort, char_t *documents)
{
    websMimeType    *mt;

    a_assert(port > 0);

    if (++websOpenCount != 1) {
        return websPort;
    }
    webs = NULL;
    websMax = 0;
    if (documents) {
        websSetDefaultDir(documents);
    }
    websDefaultOpen();
#if BIT_ROM
    websRomOpen();
#endif
    /*
        Create a mime type lookup table for quickly determining the content type
     */
    websMime = symOpen(WEBS_SYM_INIT * 4);
    a_assert(websMime >= 0);
    for (mt = websMimeList; mt->type; mt++) {
        symEnter(websMime, mt->ext, valueString(mt->type, 0), 0);
    }
    /*
        Open the URL handler module. The caller should create the required URL handlers after calling this function.
     */
    if (websUrlHandlerOpen() < 0) {
        return -1;
    }
    websFormOpen();

#if BIT_ACCESS_LOG
    accessFd = gopen(accessLog, O_CREAT | O_TRUNC | O_APPEND | O_WRONLY, 0666);
    a_assert(accessFd >= 0);
#endif
    return websOpenListen(ip, port, sslPort);
}


void websCloseServer()
{
    webs_t  wp;
    int     wid;

    if (--websOpenCount > 0) {
        return;
    }
    websCloseListen();

    for (wid = websMax; webs && wid >= 0; wid--) {
        if ((wp = webs[wid]) == NULL) {
            continue;
        }
        socketCloseConnection(wp->sid);
        websFree(wp);
    }

#if BIT_ACCESS_LOG
    if (accessFd >= 0) {
        close(accessFd);
        accessFd = -1;
    }
#endif
#if BIT_ROM
    websRomClose();
#endif
    websDefaultClose();
    symClose(websMime);
    websFormClose();
    websUrlHandlerClose();
}


int websOpenListen(char_t *ip, int port, int sslPort)
{
    if ((websListenSock = socketOpenConnection(ip, port, websAccept, 0)) < 0) {
        error(E_L, E_USER, T("Couldn't open a socket on port %d"), port);
        return -1;
    } 
    /*
        Determine the full URL address to access the home page for this web server
     */
    websPort = port;
    bfree(websHostUrl);
    bfree(websIpaddrUrl);
    websIpaddrUrl = websHostUrl = NULL;
    if (port == 80) {
        websHostUrl = bstrdup(websHost);
        websIpaddrUrl = bstrdup(websIpaddr);
    } else {
        fmtAlloc(&websHostUrl, WEBS_MAX_URL + 80, T("%s:%d"), websHost, port);
        fmtAlloc(&websIpaddrUrl, WEBS_MAX_URL + 80, T("%s:%d"), websIpaddr, port);
    }
    trace(0, T("Started HTTP service on %s:%d\n"), ip ? ip : "*", port);

#if BIT_PACK_SSL
    {
        socket_t    *sptr;

        if ((sslListenSock = socketOpenConnection(ip, sslPort, websAccept, SOCKET_MYOWNBUFFERS)) < 0) {
            trace(0, T("SSL: Unable to open SSL socket on port %d.\n"), port);
            return -1;
        }
        sptr = socketPtr(sslListenSock);
        sptr->secure = 1;
        trace(0, T("Started HTTPS service on %s:%d\n"), ip ? ip : "*", sslPort);
    }
#endif
    return 0;
}


//  MOB- naming
int websSSLIsOpen()
{
    return (sslListenSock != -1);
}


void websCloseListen()
{
    if (websListenSock >= 0) {
        socketCloseConnection(websListenSock);
        websListenSock = -1;
    }
    bfree(websHostUrl);
    bfree(websIpaddrUrl);
    websIpaddrUrl = websHostUrl = NULL;
}


int websAccept(int sid, char *ipaddr, int port, int listenSid)
{
    webs_t      wp;
    socket_t    *lp;
    int         wid, len;
    char        *pString;
    struct sockaddr_in ifAddr;

    a_assert(ipaddr && *ipaddr);
    a_assert(sid >= 0);
    a_assert(port >= 0);

    /*
        Allocate a new handle for this accepted connection. This will allocate a webs_t structure in the webs[] list
     */
    if ((wid = websAlloc(sid)) < 0) {
        return -1;
    }
    wp = webs[wid];
    a_assert(wp);
    wp->listenSid = listenSid;
    lp = socketPtr(wp->sid);

    ascToUni(wp->ipaddr, ipaddr, min(sizeof(wp->ipaddr), strlen(ipaddr) + 1));

    /*
        Get the ip address of the interface that accept the connection.
     */
    len = sizeof(struct sockaddr_in);
    if (getsockname(socketList[sid]->sock, (struct sockaddr*) &ifAddr, (socklen_t*) &len) < 0) {
        return -1;
    }
    pString = inet_ntoa(ifAddr.sin_addr);
    gstrncpy(wp->ifaddr, pString, gstrlen(pString));

    /*
        Check if this is a request from a browser on this system. This is useful to know for permitting administrative
        operations only for local access 
     */
    if (gstrcmp(wp->ipaddr, T("127.0.0.1")) == 0 || gstrcmp(wp->ipaddr, websIpaddr) == 0 || 
            gstrcmp(wp->ipaddr, websHost) == 0) {
        wp->flags |= WEBS_LOCAL_REQUEST;
    }
    if (lp->secure) {
        wp->flags |= WEBS_SECURE;
    }
    /*
        Arrange for websSocketEvent to be called when read data is available
     */
    socketCreateHandler(sid, SOCKET_READABLE, (lp->secure) ? websSSLSocketEvent : websSocketEvent, wp);

    /*
        Arrange for a timeout to kill hung requests
     */
    wp->timeout = emfSchedCallback(WEBS_TIMEOUT, websTimeout, (void *) wp);
    trace(8, T("accept request\n"));
    return 0;
}


/*
    The webs socket handler.  Called in response to I/O. We just pass control to the relevant read or write handler. A
    pointer to the webs structure is passed as a (void*) in iwp.  
 */
static void websSocketEvent(int sid, int mask, void* iwp)
{
    webs_t  wp;

    wp = (webs_t) iwp;
    a_assert(wp);

    if (! websValid(wp)) {
        return;
    }
    if (mask & SOCKET_READABLE) {
        websReadEvent(wp);
    } 
    if (mask & SOCKET_WRITABLE) {
        if (websValid(wp) && wp->writeSocket) {
            (*wp->writeSocket)(wp);
        }
    } 
}


/*
    The webs read handler. This is the primary read event loop. It uses a state machine to track progress while parsing
    the HTTP request.  Note: we never block as the socket is always in non-blocking mode.
 */
void websReadEvent(webs_t wp)
{
    char_t  *text;
    ssize   len, nbytes, size;
    int     rc, done, fd;

    a_assert(wp);
    a_assert(websValid(wp));

    text = NULL;
    fd = -1;
    websSetTimeMark(wp);

    /*
        Read as many lines as possible. socketGets is called to read the header and socketRead is called to read posted
        data.  
     */
    for (done = 0; !done; ) {
        bfree(text);
        text = NULL;
        /*
             Get more input into "text". Returns 0, if more data is needed to continue, -1 if finished with the request,
             or 1 if all required data is available for current state.
         */
        while ((rc = websGetInput(wp, &text, &nbytes)) == 0) { }
        if (rc < 0) {
            break;
        }
        /*
            This is the state machine for the web server. 
         */
        switch(wp->state) {
        case WEBS_BEGIN:
            /*
                Parse the first line of the Http header
             */
            if (websParseFirst(wp, text) < 0) {
                done++;
                break;
            }
            wp->state = WEBS_HEADER;
            break;
        
        case WEBS_HEADER:
            /*
                Store more of the HTTP header. As we are doing line reads, we need to separate the lines with '\n'
             */
            if (ringqLen(&wp->header) > 0) {
                ringqPutStr(&wp->header, T("\n"));
            }
            ringqPutStr(&wp->header, text);
            break;

        case WEBS_POST_CLEN:
            /*
                POST request with content specified by a content length.  If this is a CGI request, write the data to
                the cgi stdin.
             */
#if BIT_CGI
            //  MOB - refactor as a handler 
            if (wp->flags & WEBS_CGI_REQUEST) {
                if (fd == -1) {
                    fd = gopen(wp->cgiStdin, O_CREAT | O_WRONLY | O_BINARY, 0666);
                }
                gwrite(fd, text, nbytes);
            } else 
#endif
            if (wp->query) {
                if (wp->query[0] && !(wp->flags & WEBS_POST_DATA)) {
                    /*
                        Special case where the POST request also had query data specified in the URL, ie.
                        url?query_data. In this case the URL query data is separated by a '&' from the posted query
                        data.
                     */
                    len = gstrlen(wp->query);
                    if (text) {
                        size = (len + gstrlen(text) + 2) * sizeof(char_t);
                        wp->query = brealloc(wp->query, size);
                        wp->query[len++] = '&';
                        strcpy(&wp->query[len], text);
                    }

                } else {
                    /*
                        The existing query data came from the POST request so just append it.
                     */
                    if (text != NULL) {
                        len = gstrlen(wp->query);
                        size = (len +  gstrlen(text) + 1) * sizeof(char_t);
                        wp->query = brealloc(wp->query, size);
                        gstrcpy(&wp->query[len], text);
                    }
                }

            } else {
                wp->query = bstrdup(text);
            }
            /*
                Calculate how much more post data is to be read.
             */
            wp->flags |= WEBS_POST_DATA;
            wp->clen -= nbytes;
            if (wp->clen > 0) {
                if (nbytes > 0) {
                    break;
                }
                done++;
                break;
            }
            /*
                No more data so process the request, (but be sure to close the input file first!).
             */
            if (fd != -1) {
                gclose (fd);
                fd = -1;
            }
            websUrlHandlerRequest(wp);
            done++;
            break;

        case WEBS_POST:
            /*
                POST without content-length specification. If this is a CGI request, write the data to the cgi stdin.
                socketGets was used to get the data and it strips \n's so add them back in here.
             */
#if BIT_CGI
            //  MOB - refactor CGI to a handler
            if (wp->flags & WEBS_CGI_REQUEST) {
                if (fd == -1) {
                    fd = gopen(wp->cgiStdin, O_CREAT | O_WRONLY | O_BINARY, 0666);
                }
                gwrite(fd, text, nbytes);
                gwrite(fd, T("\n"), sizeof(char_t));
            } else
#endif
            if (wp->query && *wp->query && !(wp->flags & WEBS_POST_DATA)) {
                len = gstrlen(wp->query);
                size = (len + gstrlen(text) + 2) * sizeof(char_t);
                wp->query = brealloc(wp->query, size);
                if (wp->query) {
                    wp->query[len++] = '&';
                    gstrcpy(&wp->query[len], text);
                }

            } else {
                wp->query = bstrdup(text);
            }
            wp->flags |= WEBS_POST_DATA;
            done++;
            break;

        default:
            websError(wp, 404, T("Bad state"));
            done++;
            break;
        }
    }
    if (fd != -1) {
        fd = gclose (fd);
    }
    bfree(text);
}


/*
    Get input from the browser. Return TRUE (!0) if the request has been handled. Return -1 on errors or if the request
    has been processed, 1 if input read, and 0 to instruct the caller to call again for more input.
  
    Note: socketRead will Return the number of bytes read if successful. This may be less than the requested "bufsize"
    and may be zero. It returns -1 for errors. It returns 0 for EOF. Otherwise it returns the number of bytes read.
    Since this may be zero, callers should use socketEof() to distinguish between this and EOF.
 */
//  MOB - rename because static
static int websGetInput(webs_t wp, char_t **ptext, ssize *pnbytes) 
{
    char_t  *text;
    char    buf[WEBS_SOCKET_BUFSIZ+1];
    ssize   nbytes, len, clen;

    a_assert(websValid(wp));
    a_assert(ptext);
    a_assert(pnbytes);

    *ptext = text = NULL;
    *pnbytes = 0;

    /*
        If this request is a POST with a content length, we know the number
        of bytes to read so we use socketRead().
     */
    if (wp->state == WEBS_POST_CLEN) {
        len = (wp->clen > WEBS_SOCKET_BUFSIZ) ? WEBS_SOCKET_BUFSIZ : wp->clen;
    } else {
        len = 0;
    }
    if (len > 0) {

#if BIT_PACK_SSL
        //  MOB - have wrapper. Call websRead()
        if (wp->flags & WEBS_SECURE) {
            nbytes = websSSLRead(wp, buf, len);
        } else {
            nbytes = socketRead(wp->sid, buf, len);
        }
#else
        nbytes = socketRead(wp->sid, buf, len);
#endif
        if (nbytes < 0) {                       /* Error */
            websDone(wp, 0);
            return -1;

        }  else if (nbytes == 0) {              /* EOF or No data available */
            /*
               Infinite CPU usage if not all post data is sent. This is a side-effect of socketRead whose return value
               does not distinguish between EOF and no-data and we have to explicitly use the socketEof() to test for it.
             */
            if (socketEof(wp->sid)) {
                websDone(wp, 0);
            }
            return -1;

        } else {                                /* Valid data */
            /*
                Convert to UNICODE if necessary.  First be sure the string is NULL terminated.
             */
            buf[nbytes] = '\0';
            if ((text = ballocAscToUni(buf, nbytes)) == NULL) {
                websError(wp, 503, T("Insufficient memory"));
                return -1;
            }
        }

    } else {
#if BIT_PACK_SSL
        if (wp->flags & WEBS_SECURE) {
            nbytes = websSSLGets(wp, &text);
        } else {
            nbytes = socketGets(wp->sid, &text);
        }
#else
        nbytes = socketGets(wp->sid, &text);
#endif

        if (nbytes < 0) {
            int eof;
            /*
                Error, EOF or incomplete
             */
#if BIT_PACK_SSL
            if (wp->flags & WEBS_SECURE) {
                /*
                    If state is WEBS_BEGIN and the request is secure, a -1 will usually indicate SSL negotiation
                 */
                if (wp->state == WEBS_BEGIN) {
                    eof = 1;
                } else {
                    eof = websSSLEof(wp);
                }
            } else {
                eof = socketEof(wp->sid);
            }
#else
            eof = socketEof(wp->sid);
#endif

            if (eof) {
                /*
                    If this is a post request without content length, process the request as we now have all the data.
                    Otherwise just close the connection.
                 */
                if (wp->state == WEBS_POST) {
                    websUrlHandlerRequest(wp);
                } else {
                    websDone(wp, 0);
                    return -1;
                }
            } else if (wp->state == WEBS_HEADER){
                /*
                 If state is WEBS_HEADER and the ringq is empty, then this is a simple request with no additional header
                 fields to process and no empty line terminator. NOTE: this fix for earlier versions of browsers is
                 troublesome because if we don't receive the entire header in the first pass this code assumes we were
                 only expecting a one line header, which is not necessarily the case. So we weren't processing the whole
                 header and weren't fufilling requests properly.
                 */
                return -1;
            } else {
                /*
                    If an error occurred and it wasn't an eof, close the connection
                    MOB - 
                 */
                websDone(wp, 0);
                return -1;
            }

        } else if (nbytes == 0) {
            if (wp->state == WEBS_HEADER) {
                /*
                    Valid empty line, now finished with header
                 */
                websParseRequest(wp);
                if (wp->flags & WEBS_POST_REQUEST) {
                    if (wp->flags & WEBS_CLEN) {
                        wp->state = WEBS_POST_CLEN;
                        clen = wp->clen;
                    } else {
                        wp->state = WEBS_POST;
                        clen = 1;
                    }
                    if (clen > 0) {
                        /*
                            Return 0 to get more data.
                         */
                        return 0;
                    }
                    return 1;
                }
                /*
                    We've read the header so go and handle the request
                 */
                websUrlHandlerRequest(wp);
            }
            return -1;
        }
    }
    a_assert(text);
    a_assert(nbytes > 0);
    *ptext = text;
    *pnbytes = nbytes;
    return 1;
}


/*
    Parse the first line of a HTTP request
 */
static int websParseFirst(webs_t wp, char_t *text)
{
    char_t  *op, *proto, *protoVer, *url, *host, *query, *path, *port, *ext;
    char_t  *buf;
    int     testPort;

    a_assert(websValid(wp));
    a_assert(text && *text);

    /*
        Determine the request type: GET, HEAD or POST
     */
    op = gstrtok(text, T(" \t"));
    if (op == NULL || *op == '\0') {
        websError(wp, 400, T("Bad HTTP request"));
        return -1;
    }
    if (gstrcmp(op, T("GET")) != 0) {
        if (gstrcmp(op, T("POST")) == 0) {
            wp->flags |= WEBS_POST_REQUEST;
        } else if (gstrcmp(op, T("HEAD")) == 0) {
            wp->flags |= WEBS_HEAD_REQUEST;
        } else {
            websError(wp, 400, T("Bad request type"));
            return -1;
        }
    }

    /*
        Store result in the form (CGI) variable store
     */
    websSetVar(wp, T("REQUEST_METHOD"), op);

    url = gstrtok(NULL, T(" \t\n"));
    if (url == NULL || *url == '\0') {
        websError(wp, 400, T("Bad HTTP request"));
        return -1;
    }
    protoVer = gstrtok(NULL, T(" \t\n"));

    /*
        Parse the URL and store all the various URL components. websUrlParse returns an allocated buffer in buf which we
        must free. We support both proxied and non-proxied requests. Proxied requests will have http://host/ at the
        start of the URL. Non-proxied will just be local path names.
     */
    host = path = port = proto = query = ext = NULL;
    if (websUrlParse(url, &buf, &host, &path, &port, &query, &proto, NULL, &ext) < 0) {
        websError(wp, 400, T("Bad URL format"));
        return -1;
    }
    wp->url = bstrdup(url);

#if BIT_CGI
    //  MOB - refactor as a handler
    if (gstrstr(url, CGI_BIN) != NULL) {
        wp->flags |= WEBS_CGI_REQUEST;
        if (wp->flags & WEBS_POST_REQUEST) {
            wp->cgiStdin = websGetCgiCommName();
        }
    }
#endif
    wp->query = bstrdup(query);
    wp->host = bstrdup(host);
    wp->path = bstrdup(path);
    wp->protocol = bstrdup(proto);
    wp->protoVersion = bstrdup(protoVer);
    if (strcmp(protoVer, "HTTP/1.1") == 0) {
        wp->flags |= WEBS_KEEP_ALIVE;
    }
    if ((testPort = socketGetPort(wp->listenSid)) >= 0) {
        wp->port = testPort;
    } else {
        wp->port = gatoi(port);
    }
    if (gstrcmp(ext, T(".asp")) == 0) {
        wp->flags |= WEBS_ASP;
    }
    bfree(buf);
    websUrlType(url, wp->type, TSZ(wp->type));
    ringqFlush(&wp->header);
    return 0;
}


/*
    Parse a full request
 */
#define isgoodchar(s) (gisalnum((s)) || ((s) == '/') || ((s) == '_') || ((s) == '.')  || ((s) == '-') )

static void websParseRequest(webs_t wp)
{
    char_t  *authType, *upperKey, *cp, *browser, *lp, *key, *value, *userAuth;

    a_assert(websValid(wp));

    /*
        Define default CGI values
     */
    websSetVar(wp, T("HTTP_AUTHORIZATION"), T(""));

    /* 
        Parse the header and create the Http header keyword variables
        We rewrite the header as we go for non-local requests.  NOTE: this
        modifies the header string directly and tokenizes each line with '\0'.
    */
    browser = NULL;
    for (lp = (char_t*) wp->header.servp; lp && *lp; ) {
        cp = lp;
        if ((lp = gstrchr(lp, '\n')) != NULL) {
            lp++;
        }
        if ((key = gstrtok(cp, T(": \t\n"))) == NULL) {
            continue;
        }
        if ((value = gstrtok(NULL, T("\n"))) == NULL) {
            value = T("");
        }
        while (gisspace(*value)) {
            value++;
        }
        strlower(key);

        /*
            Create a variable (CGI) for each line in the header
         */
        fmtAlloc(&upperKey, (gstrlen(key) + 6), T("HTTP_%s"), key);
        for (cp = upperKey; *cp; cp++) {
            if (*cp == '-') {
                *cp = '_';
            }
        }
        strupper(upperKey);
        websSetVar(wp, upperKey, value);
        bfree(upperKey);

        /*
            Track the requesting agent (browser) type
         */
        if (gstrcmp(key, T("user-agent")) == 0) {
            wp->userAgent = bstrdup(value);

        } else if (gstricmp(key, T("authorization")) == 0) {
            /*
                Determine the type of Authorization Request
             */
            authType = bstrdup (value);
            a_assert (authType);
            /*          
                Truncate authType at the next non-alpha character
             */
            cp = authType;
            while (gisalpha(*cp)) {
                cp++;
            }
            *cp = '\0';

            wp->authType = bstrdup(authType);
            bfree(authType);

            if (gstricmp(wp->authType, T("basic")) == 0) {
                /*
                    The incoming value is username:password (Basic authentication)
                 */
                if ((cp = gstrchr(value, ' ')) != NULL) {
                    *cp = '\0';
                    bfree(wp->authType);
                    wp->authType = bstrdup(value);
                    userAuth = websDecode64(++cp);
                } else {
                    userAuth = websDecode64(value);
                }
                /*
                    Split userAuth into userid and password
                 */
                if ((cp = gstrchr(userAuth, ':')) != NULL) {
                    *cp++ = '\0';
                }
                if (cp) {
                    wp->userName = bstrdup(userAuth);
                    wp->password = bstrdup(cp);
                } else {
                    wp->userName = bstrdup(T(""));
                    wp->password = bstrdup(T(""));
                }
                bfree(userAuth);
                wp->flags |= WEBS_AUTH_BASIC;
            } else {
#if BIT_DIGEST_AUTH
                /*
                    The incoming value is slightly more complicated (Digest)
                 */
                char_t *np;     /* pointer to end of tag name */
                char_t tp;      /* temporary character holding space */
                char_t *vp;     /* pointer to value */
                char_t *npv;    /* pointer to end of value, "next" pointer */
                char_t tpv;     /* temporary character holding space */

                wp->flags |= WEBS_AUTH_DIGEST;
                /*
                    Move cp to Next word beyond "Digest", vp to first char after '='.
                 */
                cp = value;
                while (isgoodchar(*cp)) {
                    cp++;
                }
                while (!isgoodchar(*cp)) {
                    cp++;
                }
                /*
                    Find beginning of value
                 */
                vp = gstrchr(cp, '=');
                while (vp) {
                    /*
                        Zero-terminate tag name
                     */
                    np = cp;
                    while (isgoodchar(*np)) {
                        np++;
                    }
                    tp = *np;
                    *np = 0;
                    /*
                        Advance value pointer to first legit character
                     */
                    vp++;
                    while (!isgoodchar(*vp)) {
                        vp++;
                    }
                    /*
                        Zero-terminate value
                     */
                    npv = vp;
                    while (isgoodchar(*npv)) {
                        npv++;
                    }
                    tpv = *npv;
                    *npv = 0;
                    /*
                        Extract the fields
                     */
                    if (gstricmp(cp, T("username")) == 0) {
                        wp->userName = bstrdup(vp);
                    } else if (gstricmp(cp, T("response")) == 0) {
                        wp->digest = bstrdup(vp);
                    } else if (gstricmp(cp, T("opaque")) == 0) {
                        wp->opaque = bstrdup(vp);
                    } else if (gstricmp(cp, T("uri")) == 0) {
                        wp->uri = bstrdup(vp);
                    } else if (gstricmp(cp, T("realm")) == 0) {
                        wp->realm = bstrdup(vp);
                    } else if (gstricmp(cp, T("nonce")) == 0) {
                        wp->nonce = bstrdup(vp);
                    } else if (gstricmp(cp, T("nc")) == 0) {
                        wp->nc = bstrdup(vp);
                    } else if (gstricmp(cp, T("cnonce")) == 0) {
                        wp->cnonce = bstrdup(vp);
                    } else if (gstricmp(cp, T("qop")) == 0) {
                        wp->qop = bstrdup(vp);
                    }
                    /*
                        Restore tag name and value zero-terminations
                     */
                    *np = tp;
                    *npv = tpv;
                    /*
                        Advance tag name and value pointers
                     */
                    cp = npv;
                    while (*cp && isgoodchar(*cp)) {
                        cp++;
                    }
                    while (*cp && !isgoodchar(*cp)) {
                        cp++;
                    }

                    if (*cp) {
                        vp = gstrchr(cp, '=');
                    } else {
                        vp = NULL;
                    }
                }
#endif /* BIT_DIGEST_AUTH */
            }
        } else if (gstrcmp(key, T("content-length")) == 0) {
            wp->clen = gatoi(value);
            if (wp->clen > 0) {
                wp->flags |= WEBS_CLEN;         
                websSetVar(wp, T("CONTENT_LENGTH"), value);
            } else {
                wp->clen = 0;
            }
        } else if (gstrcmp(key, T("content-type")) == 0) {
            websSetVar(wp, T("CONTENT_TYPE"), value);

#if BIT_KEEP_ALIVE
        } else if (gstrcmp(key, T("connection")) == 0) {
            strlower(value);
            if (gstrcmp(value, T("keep-alive")) == 0) {
                wp->flags |= WEBS_KEEP_ALIVE;
            }
#endif
        } else if (gstrcmp(key, T("cookie")) == 0) {
            wp->flags |= WEBS_COOKIE;
            wp->cookie = bstrdup(value);

#if BIT_IF_MODIFIED
        } else if (gstrcmp(key, T("if-modified-since")) == 0) {
            char_t *cmd;
            time_t tip = 0;

            if ((cp = gstrchr(value, ';')) != NULL) {
                *cp = '\0';
            }

            fmtAlloc(&cmd, 64, T("%s"), value);

            if ((wp->since = dateParse(tip, cmd)) != 0) {
                wp->flags |= WEBS_IF_MODIFIED;
            }
            bfree(cmd);
#endif /* WEBS_IF_MODIFIED_SUPPORT */
        }
    }
}


/*
    Basic event loop. SocketReady returns true when a socket is ready for service. SocketSelect will block until an
    event occurs. SocketProcess will actually do the servicing.
 */
void websServiceEvents(int *finished)
{
    a_assert(finished);
    *finished = 0;
    while (!*finished) {
        if (socketReady(-1) || socketSelect(-1, 1000)) {
            socketProcess(-1);
        }
        websCgiCleanup();
        emfSchedProcess();
    }
}


/*
    Set the variable (CGI) environment for this request. Create variables for all standard CGI variables. Also decode
    the query string and create a variable for each name=value pair.
 */
void websSetEnv(webs_t wp)
{
    char_t  portBuf[8];
    char_t  *keyword, *value, *valCheck, *valNew;

    a_assert(websValid(wp));

    websSetVar(wp, T("QUERY_STRING"), wp->query);
    websSetVar(wp, T("GATEWAY_INTERFACE"), T("CGI/1.1"));
    websSetVar(wp, T("SERVER_HOST"), websHost);
    websSetVar(wp, T("SERVER_NAME"), websHost);
    websSetVar(wp, T("SERVER_URL"), websHostUrl);
    websSetVar(wp, T("REMOTE_HOST"), wp->ipaddr);
    websSetVar(wp, T("REMOTE_ADDR"), wp->ipaddr);
    websSetVar(wp, T("PATH_INFO"), wp->path);
    stritoa(websPort, portBuf, sizeof(portBuf));
    websSetVar(wp, T("SERVER_PORT"), portBuf);
    websSetVar(wp, T("SERVER_ADDR"), wp->ifaddr);
    fmtAlloc(&value, FNAMESIZE, T("GoAhead/%s"), BIT_VERSION);
    websSetVar(wp, T("SERVER_SOFTWARE"), value);
    bfree(value);
    websSetVar(wp, T("SERVER_PROTOCOL"), wp->protoVersion);

    /*
        Decode and create an environment query variable for each query keyword. We split into pairs at each '&', then
        split pairs at the '='.  Note: we rely on wp->decodedQuery preserving the decoded values in the symbol table.
     */
    wp->decodedQuery = bstrdup(wp->query);
    keyword = gstrtok(wp->decodedQuery, T("&"));
    while (keyword != NULL) {
        if ((value = gstrchr(keyword, '=')) != NULL) {
            *value++ = '\0';
            websDecodeUrl(keyword, keyword, gstrlen(keyword));
            websDecodeUrl(value, value, gstrlen(value));
        } else {
            value = T("");
        }
        if (*keyword) {
            /*
                If keyword has already been set, append the new value to what has been stored.
             */
            if ((valCheck = websGetVar(wp, keyword, NULL)) != 0) {
                fmtAlloc(&valNew, 256, T("%s %s"), valCheck, value);
                websSetVar(wp, keyword, valNew);
                bfree(valNew);
            } else {
                websSetVar(wp, keyword, value);
            }
        }
        keyword = gstrtok(NULL, T("&"));
    }
}


/*
    Define a webs (CGI) variable for this connection. Also create in relevant scripting engines. Note: the incoming
    value may be volatile.  
 */
void websSetVar(webs_t wp, char_t *var, char_t *value)
{
    value_t      v;

    a_assert(websValid(wp));

    /*
        value_instring will allocate the string if required.
     */
    if (value) {
        v = valueString(value, VALUE_ALLOCATE);
    } else {
        v = valueString(T(""), VALUE_ALLOCATE);
    }
    symEnter(wp->cgiVars, var, v, 0);
}


/*
 *  Return TRUE if a webs variable exists for this connection.
 */
int websTestVar(webs_t wp, char_t *var)
{
    sym_t       *sp;

    a_assert(websValid(wp));

    if (var == NULL || *var == '\0') {
        return 0;
    }
    if ((sp = symLookup(wp->cgiVars, var)) == NULL) {
        return 0;
    }
    return 1;
}


/*
    Get a webs variable but return a default value if string not found.  Note, defaultGetValue can be NULL to permit
    testing existence.  
 */
char_t *websGetVar(webs_t wp, char_t *var, char_t *defaultGetValue)
{
    sym_t   *sp;

    a_assert(websValid(wp));
    a_assert(var && *var);
 
    if ((sp = symLookup(wp->cgiVars, var)) != NULL) {
        a_assert(sp->content.type == string);
        if (sp->content.value.string) {
            return sp->content.value.string;
        } else {
            return T("");
        }
    }
    return defaultGetValue;
}


/*
    Return TRUE if a webs variable is set to a given value
 */
int websCompareVar(webs_t wp, char_t *var, char_t *value)
{
    a_assert(websValid(wp));
    a_assert(var && *var);
 
    if (gstrcmp(value, websGetVar(wp, var, T(" __UNDEF__ "))) == 0) {
        return 1;
    }
    return 0;
}


/*
    Cancel the request timeout. Note may be called multiple times.
 */
void websTimeoutCancel(webs_t wp)
{
    a_assert(websValid(wp));

    if (wp->timeout >= 0) {
        emfUnschedCallback(wp->timeout);
        wp->timeout = -1;
    }
}


/*
    Output a HTTP response back to the browser. If redirect is set to a 
    URL, the browser will be sent to this location.
 */
void websResponse(webs_t wp, int code, char_t *message, char_t *redirect)
{
    char_t      *date;

    a_assert(websValid(wp));

    /*
        IE3.0 needs no Keep Alive for some return codes.
        MOB - OPT REMOVE
     */
    wp->flags &= ~WEBS_KEEP_ALIVE;

    /*
        Only output the header if a header has not already been output.
     */
    if ( !(wp->flags & WEBS_HEADER_DONE)) {
        wp->flags |= WEBS_HEADER_DONE;
        /*
            Redirect behaves much better when sent with HTTP/1.0
         */
        if (redirect != NULL) {
            websWrite(wp, T("HTTP/1.0 %d %s\r\n"), code, websErrorMsg(code));
        } else {
            websWrite(wp, T("HTTP/1.1 %d %s\r\n"), code, websErrorMsg(code));
        }
        /*
            The Server HTTP header below must not be modified unless explicitly allowed by licensing terms.
         */
        websWrite(wp, T("Server: GoAhead/%s\r\n"), BIT_VERSION);

        if ((date = websGetDateString(NULL)) != NULL) {
            websWrite(wp, T("Date: %s\r\n"), date);
            bfree(date);
        }
        /*
            If authentication is required, send the auth header info
         */
        if (code == 401) {
            if (!(wp->flags & WEBS_AUTH_DIGEST)) {
                websWrite(wp, T("WWW-Authenticate: Basic realm=\"%s\"\r\n"), websGetRealm());
#if BIT_DIGEST_AUTH
            } else {
                char_t *nonce, *opaque;
                nonce = websCalcNonce(wp);
                opaque = websCalcOpaque(wp); 
                websWrite(wp, 
                    T("WWW-Authenticate: Digest realm=\"%s\", domain=\"%s\",")
                    T("qop=\"%s\", nonce=\"%s\", opaque=\"%s\",")
                    T("algorithm=\"%s\", stale=\"%s\"\r\n"), 
                    websGetRealm(),
                    websGetHostUrl(),
                    T("auth"),
                    nonce,
                    opaque, T("MD5"), T("FALSE"));
                bfree(nonce);
                bfree(opaque);
#endif
            }
        }
        if (wp->flags & WEBS_KEEP_ALIVE) {
            websWrite(wp, T("Connection: keep-alive\r\n"));
        }
        websWrite(wp, T("Pragma: no-cache\r\nCache-Control: no-cache\r\n"));
        websWrite(wp, T("Content-Type: text/html\r\n"));
        if (redirect) {
            websWrite(wp, T("Location: %s\r\n"), redirect);
        }
        websWrite(wp, T("\r\n"));
    }

    /*
        If the browser didn't do a HEAD only request, send the message as well.
     */
    if ((wp->flags & WEBS_HEAD_REQUEST) == 0 && message && *message) {
        websWrite(wp, T("%s\r\n"), message);
    }
    websDone(wp, code);
}


/*
    Redirect the user to another webs page
 */
void websRedirect(webs_t wp, char_t *url)
{
    char_t  *msgbuf, *urlbuf, *redirectFmt;

    a_assert(websValid(wp));
    a_assert(url);

    websStats.redirects++;
    msgbuf = urlbuf = NULL;

    /*
        Some browsers require a http://host qualified URL for redirection
     */
    if (gstrstr(url, T("http://")) == NULL) {
        if (*url == '/') {
            url++;
        }
        redirectFmt = T("http://%s/%s");

#if BIT_PACK_SSL
        if (wp->flags & WEBS_SECURE) {
            redirectFmt = T("https://%s/%s");
        }
#endif
        fmtAlloc(&urlbuf, WEBS_MAX_URL + 80, redirectFmt, websGetVar(wp, T("HTTP_HOST"), websHostUrl), url);
        url = urlbuf;
    }

    /*
        Add human readable message for completeness. Should not be required.
     */
    fmtAlloc(&msgbuf, WEBS_MAX_URL + 80, 
        T("<html><head></head><body>\r\n\
        This document has moved to a new <a href=\"%s\">location</a>.\r\n\
        Please update your documents to reflect the new location.\r\n\
        </body></html>\r\n"), url);

    websResponse(wp, 302, msgbuf, url);
    bfree(msgbuf);
    bfree(urlbuf);
}


/*
   websSafeUrl -- utility function to clean up URLs that will be printed by the websError() function, below. To prevent
   problems with the 'cross-site scripting exploit', where attackers request an URL containing embedded JavaScript code,
   we replace all '<' and '>' characters with HTML entities so that the user's browser will not interpret the URL as
   JavaScript.
 */
static int charCount(const char_t* str, char_t ch)
{
   int count = 0;
   char_t* p = (char_t*) str;
   
    if (NULL == str) {
        return 0;
    }

    while (1) {
        p = gstrchr(p, ch);
        if (NULL == p) {
            break;
        }
        /*
            increment the count, and begin looking at the next character
        */
        ++count;
        ++p;
    }
    return count;
}


//  MOB - review vs appweb

static char_t* websSafeUrl(const char_t* url)
{
    //  MOB refactor code style, and review
    int ltCount = charCount(url, kLt);
    int gtCount = charCount(url, kGt);
    ssize safeLen = 0;
    char_t* safeUrl = NULL;
    char_t* src = NULL;
    char_t* dest = NULL;

    if (NULL != url) {
        safeLen = gstrlen(url);
        if (ltCount == 0 && gtCount == 0) {
            safeUrl = bstrdup((char_t*) url);
        } else {
            safeLen += (ltCount * 4);
            safeLen += (gtCount * 4);

            safeUrl = balloc(safeLen);
            if (safeUrl != NULL) {
                src = (char_t*) url;
                dest = safeUrl;
                while (*src) {
                    if (*src == kLt) {
                        gstrcpy(dest, kLessThan);
                        dest += gstrlen(kLessThan);
                    } else if (*src == kGt) {
                        gstrcpy(dest, kGreaterThan);
                        dest += gstrlen(kGreaterThan);
                    } else {
                        *dest++ = *src;
                    }
                    ++src;
                }
                /* don't forget to terminate the string...*/
                *dest = '\0';
            }
        }
    }
    return safeUrl;
}



/*  
    Output an error message and cleanup
 */
void websError(webs_t wp, int code, char_t *fmt, ...)
{
    va_list     args;
    char_t      *msg, *userMsg, *buf;
   char_t*     safeUrl = NULL;
   char_t*     safeMsg = NULL;

    a_assert(websValid(wp));
    a_assert(fmt);

    websStats.errors++;

    safeUrl = websSafeUrl(wp->url);
    bfree(wp->url);
    wp->url = safeUrl;

    va_start(args, fmt);
    userMsg = NULL;
    fmtValloc(&userMsg, WEBS_BUFSIZE, fmt, args);
    va_end(args);
    safeMsg = websSafeUrl(userMsg);
    bfree(userMsg);
    userMsg = safeMsg;
    safeMsg  = NULL;

    msg = T("<html><head><title>Document Error: %s</title></head>\r\n\
        <body><h2>Access Error: %s</h2>\r\n\
        <p>%s</p></body></html>\r\n");
    buf = NULL;
    fmtAlloc(&buf, WEBS_BUFSIZE, msg, websErrorMsg(code), websErrorMsg(code), userMsg);
    websResponse(wp, code, buf, NULL);
    bfree(buf);
    bfree(userMsg);
}


/*
    Return the error message for a given code
 */
char_t *websErrorMsg(int code)
{
    websErrorType   *ep;

    for (ep = websErrors; ep->code; ep++) {
        if (code == ep->code) {
            return ep->msg;
        }
    }
    a_assert(0);
    return T("");
}


/*
    Do formatted output to the browser. This is the public ASP and form write procedure.
 */
ssize websWrite(webs_t wp, char_t *fmt, ...)
{
    va_list      vargs;
    char_t      *buf;
    ssize        rc;
    
    a_assert(websValid(wp));

    va_start(vargs, fmt);

    buf = NULL;
    rc = 0;
    if (fmtValloc(&buf, WEBS_BUFSIZE, fmt, vargs) >= WEBS_BUFSIZE) {
        trace(0, T("websWrite lost data, buffer overflow\n"));
    }
    va_end(vargs);
    a_assert(buf);
    if (buf) {
        rc = websWriteBlock(wp, buf, gstrlen(buf));
        bfree(buf);
    }
    return rc;
}


/*
    Write a block of data of length "nChars" to the user's browser. Public write block procedure.  If unicode is turned
    on this function expects buf to be a unicode string and it converts it to ASCII before writing.  See
    websWriteDataNonBlock to always write binary or ASCII data with no unicode conversion.  This returns the number of
    char_t's processed.  It spins until nChars are flushed to the socket.  For non-blocking behavior, use
    websWriteDataNonBlock.
 */
ssize websWriteBlock(webs_t wp, char_t *buf, ssize nChars)
{
    char    *asciiBuf, *pBuf;
    ssize   len, done;

    a_assert(wp);
    a_assert(websValid(wp));
    a_assert(buf);
    a_assert(nChars >= 0);

    done = len = 0;

    /*
        ballocUniToAsc will convert Unicode to strings to Ascii.  If Unicode is
        not turned on then ballocUniToAsc will not do the conversion.
     */
    pBuf = asciiBuf = ballocUniToAsc(buf, nChars);

    while (nChars > 0) {  
#if BIT_PACK_SSL
        if (wp->flags & WEBS_SECURE) {
            if ((len = websSSLWrite(wp, pBuf, nChars)) < 0) {
                bfree(asciiBuf);
                return -1;
            }
            websSSLFlush(wp);
        } else {
            //  MOB - refactor duplicate
            if ((len = socketWrite(wp->sid, pBuf, nChars)) < 0) {
                bfree(asciiBuf);
                return -1;
            }
            socketFlush(wp->sid);
        }
#else
        if ((len = socketWrite(wp->sid, pBuf, nChars)) < 0) {
            bfree(asciiBuf);
            return -1;
        }
        socketFlush(wp->sid);
#endif
        nChars -= len;
        pBuf += len;
        done += len;
    }
    bfree(asciiBuf);
    return done;
}


/*
    Write a block of data of length "nChars" to the user's browser. Same as websWriteBlock except that it expects
    straight ASCII or binary and does no unicode conversion before writing the data.  If the socket cannot hold all the
    data, it will return the number of bytes flushed to the socket before it would have blocked.  This returns the
    number of chars processed or -1 if socketWrite fails.
 */
ssize websWriteDataNonBlock(webs_t wp, char *buf, ssize nChars)
{
    //  MOB - naming
    ssize   r;

    a_assert(wp);
    a_assert(websValid(wp));
    a_assert(buf);
    a_assert(nChars >= 0);

#if BIT_PACK_SSL
    if (wp->flags & WEBS_SECURE) {
        r = websSSLWrite(wp, buf, nChars);
        websSSLFlush(wp);
    } else {
        //  MOB - refactor duplicate
        r = socketWrite(wp->sid, buf, nChars);
        socketFlush(wp->sid);
    }
#else
    r = socketWrite(wp->sid, buf, nChars);
    socketFlush(wp->sid);
#endif
    return r;
}


/*
    Decode a URL (or part thereof). Allows insitu decoding.
 */
void websDecodeUrl(char_t *decoded, char_t *token, ssize len)
{
    char_t  *ip,  *op;
    int     num, i, c;
    
    a_assert(decoded);
    a_assert(token);

    op = decoded;
    for (ip = token; *ip && len > 0; ip++, op++) {
        if (*ip == '+') {
            *op = ' ';
        } else if (*ip == '%' && gisxdigit(ip[1]) && gisxdigit(ip[2])) {
            /*
                Convert %nn to a single character
             */
            ip++;
            for (i = 0, num = 0; i < 2; i++, ip++) {
                c = tolower(*ip);
                if (c >= 'a' && c <= 'f') {
                    num = (num * 16) + 10 + c - 'a';
                } else {
                    num = (num * 16) + c - '0';
                }
            }
            *op = (char_t) num;
            ip--;

        } else {
            *op = *ip;
        }
        len--;
    }
    *op = '\0';
}


#if BIT_ACCESS_LOG
/*
    Output a log message in Common Log Format
        http://httpd.apache.org/docs/1.3/logs.html#common
    If WEBS_SIMPLE_TIME is defined, then only the time() external API is used
        and a simple, non-standard log format is used.
    If WEBS_LOG_QUERY is defined, then the query string will be printed to
        the log, in addition to the URL path.  This can be a security issue
        if the query string contains sensitive information that shouldn't
        be hanging around in log files.
    MOB - get rid of LOG_QUERY
 */
static void websLog(webs_t wp, int code)
{
    char_t  *buf;
    char    *abuf;
    ssize   len;
    //  MOB - refactor. Remove simple time
#if !WEBS_SIMPLE_TIME
    time_t timer;
    struct tm localt;
#if WIN
    DWORD   dwRet;
    TIME_ZONE_INFORMATION   tzi;
#endif /* WIN */
    char_t timeStr[28];
    char_t zoneStr[6];
    char_t dataStr[16];
#endif
    a_assert(websValid(wp));
    buf = NULL;
#if !WEBS_SIMPLE_TIME
    time(&timer);
    localtime_r(&timer, &localt);
    strftime(timeStr, sizeof(timeStr), "%d/%b/%Y:%H:%M:%S", &localt); 
    timeStr[sizeof(timeStr) - 1] = '\0';
#if WIN
    dwRet = GetTimeZoneInformation(&tzi);
    snprintf(zoneStr, sizeof(zoneStr), "%+03d00", -(int)(tzi.Bias/60));
#else
    snprintf(zoneStr, sizeof(zoneStr), "%+03d00", (int)(localt.tm_gmtoff/3600));
#endif /* WIN */
    zoneStr[sizeof(zoneStr) - 1] = '\0';
    if (wp->written != 0) {
        snprintf(dataStr, sizeof(dataStr), "%ld", wp->written);
        dataStr[sizeof(dataStr) - 1] = '\0';
    } else {
        dataStr[0] = '-'; dataStr[1] = '\0';
    }
    fmtAlloc(&buf, WEBS_MAX_URL + 80, 
        T("%s - %s [%s %s] \"%s %s %s\" %d %s\n"), 
        wp->ipaddr,
        wp->userName == NULL ? "-" : wp->userName,
        timeStr, zoneStr,
        wp->flags & WEBS_POST_REQUEST ? "POST" : 
            (wp->flags & WEBS_HEAD_REQUEST ? "HEAD" : "GET"),
#if WEBS_LOG_QUERY
        wp->url, /* SECURITY - Printing the query can 'leak' private data */
#else
        wp->path,
#endif /* WEBS_LOG_QUERY */
        wp->protoVersion, code, dataStr);
#else
    //  MOB - reverse conditional
    fmtAlloc(&buf, WEBS_MAX_URL + 80, T("%d %s %d %d\n"), time(0), wp->url, code, wp->written);
#endif
    len = gstrlen(buf);
    abuf = ballocUniToAsc(buf, len+1);
    write(accessFd, abuf, len);
    bfree(buf);
    bfree(abuf);
}
#endif


/*
    Request timeout. The timeout triggers if we have not read any data from the users browser in the last WEBS_TIMEOUT
    period. If we have heard from the browser, simply re-issue the timeout.
 */
void websTimeout(void *arg, int id)
{
    webs_t      wp;
    time_t      tm, delay;

    wp = (webs_t) arg;
    a_assert(websValid(wp));

    tm = getTimeSinceMark(wp) * 1000;
    if (tm >= WEBS_TIMEOUT && !websDebug) {
        websStats.timeouts++;
        emfUnschedCallback(id);
        wp->timeout = -1;
        websDone(wp, 404);
    } else {
        delay = WEBS_TIMEOUT - tm;
        a_assert(delay > 0);
        emfReschedCallback(id, (int) delay);
    }
}


/*
    Called when the request is done.
 */
void websDone(webs_t wp, int code)
{
    a_assert(websValid(wp));

    /*
        Disable socket handler in case keep alive set.
     */
    socketDeleteHandler(wp->sid);
    if (code != 200) {
        wp->flags &= ~WEBS_KEEP_ALIVE;
    }
#if BIT_ACCESS_LOG
    if (! (wp->flags & WEBS_REQUEST_DONE)) {
        websLog(wp, code);
    }
#endif
    websPageClose(wp);

    /*
        Exit if secure
     */
#if BIT_PACK_SSL
    if (wp->flags & WEBS_SECURE) {
        websTimeoutCancel(wp);
        websSSLFlush(wp);
        socketCloseConnection(wp->sid);
        websFree(wp);
        return;
    }
#endif

    /*
        If using Keep Alive (HTTP/1.1) we keep the socket open for a period while waiting for another request on the socket. 
     */
    if (wp->flags & WEBS_KEEP_ALIVE) {
        if (socketFlush(wp->sid) == 0) {
            wp->state = WEBS_BEGIN;
            wp->flags |= WEBS_REQUEST_DONE;
            if (wp->header.buf) {
                ringqFlush(&wp->header);
            }
            socketCreateHandler(wp->sid, SOCKET_READABLE, websSocketEvent, 
                wp);
            websTimeoutCancel(wp);
            wp->timeout = emfSchedCallback(WEBS_TIMEOUT, websTimeout,
                (void *) wp);
            return;
        }
    } else {
        websTimeoutCancel(wp);
        socketSetBlock(wp->sid, 1);
        socketFlush(wp->sid);
        socketCloseConnection(wp->sid);
    }
    websFree(wp);
}


int websAlloc(int sid)
{
    webs_t      wp;
    int         wid;

    if ((wid = hAllocEntry((void***) &webs, &websMax, sizeof(struct websRec))) < 0) {
        return -1;
    }
    wp = webs[wid];
    wp->wid = wid;
    wp->sid = sid;
    wp->state = WEBS_BEGIN;
    wp->docfd = -1;
    wp->timeout = -1;
    ringqOpen(&wp->header, WEBS_HEADER_BUFINC, WEBS_MAX_HEADER);

    /*
        Create storage for the CGI variables. We supply the symbol tables for both the CGI variables and for the global
        functions. The function table is common to all webs instances (ie. all browsers)
     */
    wp->cgiVars = symOpen(WEBS_SYM_INIT);
    return wid;
}


void websFree(webs_t wp)
{
    a_assert(websValid(wp));

    //  MOB - OPT reduce allocations
    bfree(wp->path);
    bfree(wp->url);
    bfree(wp->host);
    bfree(wp->lpath);
    bfree(wp->query);
    bfree(wp->decodedQuery);
    bfree(wp->authType);
    bfree(wp->password);
    bfree(wp->userName);
    bfree(wp->cookie);
    bfree(wp->userAgent);
    bfree(wp->dir);
    bfree(wp->protocol);
    bfree(wp->protoVersion);
    bfree(wp->cgiStdin);

#if BIT_DIGEST_AUTH
    bfree(wp->realm);
    //  MOB - is URI just for AUTH?
    bfree(wp->uri);
    bfree(wp->digest);
    bfree(wp->opaque);
    bfree(wp->nonce);
    bfree(wp->nc);
    bfree(wp->cnonce);
    bfree(wp->qop);
#endif
#if BIT_PACK_SSL
    websSSLFree(wp);
#endif
    symClose(wp->cgiVars);

    if (wp->header.buf) {
        ringqClose(&wp->header);
    }

    websMax = hFree((void***) &webs, wp->wid);
    bfree(wp);
    a_assert(websMax >= 0);
}


char_t *websGetHost()
{
    return websHost;
}


char_t *websGetIpaddrUrl()
{
    return websIpaddrUrl;
}


char_t *websGetHostUrl()
{
    return websHostUrl;
}


int websGetPort()
{
    return websPort;
}


ssize websGetRequestBytes(webs_t wp)
{
    a_assert(websValid(wp));
    return wp->numbytes;
}


char_t *websGetRequestDir(webs_t wp)
{
    a_assert(websValid(wp));

    if (wp->dir == NULL) {
        return T("");
    }
    return wp->dir;
}


int websGetRequestFlags(webs_t wp)
{
    a_assert(websValid(wp));

    return wp->flags;
}


char_t *websGetRequestIpaddr(webs_t wp)
{
    a_assert(websValid(wp));

    return wp->ipaddr;
}


char_t *websGetRequestLpath(webs_t wp)
{
    a_assert(websValid(wp));

    //  MOB - unify
#if BIT_ROM
    return wp->path;
#else
    return wp->lpath;
#endif
}


char_t *websGetRequestPath(webs_t wp)
{
    a_assert(websValid(wp));

    if (wp->path == NULL) {
        return T("");
    }
    return wp->path;
}


char_t *websGetRequestPassword(webs_t wp)
{
    a_assert(websValid(wp));
    return wp->password;
}


char_t *websGetRequestType(webs_t wp)
{
    a_assert(websValid(wp));
    return wp->type;
}


char_t *websGetRequestUserName(webs_t wp)
{
    a_assert(websValid(wp));
    return wp->userName;
}


ssize websGetRequestWritten(webs_t wp)
{
    a_assert(websValid(wp));

    return wp->written;
}


static int setLocalHost()
{
    struct hostent  *hp;
    struct in_addr  intaddr;
    char            host[128], *cp;
    char_t          wbuf[128];

    if (gethostname(host, sizeof(host)) < 0) {
        error(E_L, E_LOG, T("Can't get hostname"));
        return -1;
    }
#if VXWORKS
    intaddr.s_addr = (ulong) hostGetByName(host);
    cp = inet_ntoa(intaddr);
#elif ECOS
    cp = inet_ntoa(eth0_bootp_data.bp_yiaddr);
#else
    if ((hp = gethostbyname(host)) == NULL) {
        error(E_L, E_LOG, T("Can't get host address"));
        return -1;
    }
    memcpy((char *) &intaddr, (char *) hp->h_addr_list[0], (size_t) hp->h_length);
    cp = inet_ntoa(intaddr);
#endif
    //  MOB - OPT so don't copy if not unicode
    ascToUni(wbuf, cp, min(strlen(cp) + 1, sizeof(wbuf)));
    free(cp);
    websSetIpaddr(wbuf);
    websSetHost(wbuf);
    return 0;
}


void websSetHost(char_t *host)
{
    gstrncpy(websHost, host, TSZ(websHost));
}


void websSetHostUrl(char_t *url)
{
    a_assert(url && *url);

    bfree(websHostUrl);
    websHostUrl = gstrdup(url);
}


void websSetIpaddr(char_t *ipaddr)
{
    a_assert(ipaddr && *ipaddr);
    gstrncpy(websIpaddr, ipaddr, TSZ(websIpaddr));
}


void websSetRequestBytes(webs_t wp, ssize bytes)
{
    a_assert(websValid(wp));
    a_assert(bytes >= 0);
    wp->numbytes = bytes;
}


void websSetRequestFlags(webs_t wp, int flags)
{
    a_assert(websValid(wp));
    wp->flags = flags;
}


void websSetRequestLpath(webs_t wp, char_t *lpath)
{
    a_assert(websValid(wp));
    a_assert(lpath && *lpath);

    if (wp->lpath) {
        bfree(wp->lpath);
    }
    wp->lpath = bstrdup(lpath);
    websSetVar(wp, T("PATH_TRANSLATED"), wp->lpath);
}


/*
    Update the URL path and the directory containing the web page
 */
void websSetRequestPath(webs_t wp, char_t *dir, char_t *path)
{
    char_t  *tmp;

    a_assert(websValid(wp));

    if (dir) { 
        tmp = wp->dir;
        wp->dir = bstrdup(dir);
        if (tmp) {
            bfree(tmp);
        }
    }
    if (path) {
        tmp = wp->path;
        wp->path = bstrdup(path);
        websSetVar(wp, T("PATH_INFO"), wp->path);
        if (tmp) {
            bfree(tmp);
        }
    }
}


/*
    Set the Write handler for this socket
 */
void websSetRequestSocketHandler(webs_t wp, int mask, void (*fn)(webs_t wp))
{
    a_assert(websValid(wp));
    wp->writeSocket = fn;
    socketCreateHandler(wp->sid, SOCKET_WRITABLE, websSocketEvent, wp);
}


void websSetRequestWritten(webs_t wp, ssize written)
{
    a_assert(websValid(wp));
    wp->written = written;
}


int websValid(webs_t wp)
{
    int     wid;

    for (wid = 0; wid < websMax; wid++) {
        if (wp == webs[wid]) {
            return 1;
        }
    }
    return 0;
}


/*
    Build an ASCII time string.  If sbuf is NULL we use the current time, else we use the last modified time of sbuf;
 */
char_t *websGetDateString(websStatType *sbuf)
{
    char_t* cp, *r;
    time_t  now;

    if (sbuf == NULL) {
        time(&now);
    } else {
        now = sbuf->mtime;
    }
    if ((cp = gctime(&now)) != NULL) {
        cp[gstrlen(cp) - 1] = '\0';
        r = bstrdup(cp);
        return r;
    }
    return NULL;
}


/*
    Mark time. Set a timestamp so that, later, we can return the number of seconds since we made the mark. Note that the
    mark my not be a "real" time, but rather a relative marker.
 */
void websSetTimeMark(webs_t wp)
{
    wp->timestamp = time(0);
}


/*
    Get the number of seconds since the last mark.
 */
static time_t getTimeSinceMark(webs_t wp)
{
    return time(0) - wp->timestamp;
}


/*
    Store the new realm name
 */
void websSetRealm(char_t *realmName)
{
    a_assert(realmName);

    gstrncpy(websRealm, realmName, TSZ(websRealm));
}


/*
    Return the realm name (used for authorization)
 */
char_t *websGetRealm()
{
    return websRealm;
}


#if BIT_IF_MODIFIED
//  MOB - move all into a date.c
/*  
    These functions are intended to closely mirror the syntax for HTTP-date 
    from RFC 2616 (HTTP/1.1 spec).  This code was submitted by Pete Bergstrom.
    
    RFC1123Date = wkday "," SP date1 SP time SP "GMT"
    RFC850Date  = weekday "," SP date2 SP time SP "GMT"
    ASCTimeDate = wkday SP date3 SP time SP 4DIGIT
  
    Each of these functions tries to parse the value and update the index to 
    the point it leaves off parsing.
 */

typedef enum { JAN, FEB, MAR, APR, MAY, JUN, JUL, AUG, SEP, OCT, NOV, DEC } MonthEnumeration;
typedef enum { SUN, MON, TUE, WED, THU, FRI, SAT } WeekdayEnumeration;

/*  
    Parse an N-digit value
 */

static int parseNDIGIT(char_t *buf, int digits, int *index) 
{
    int tmpIndex, returnValue;

    returnValue = 0;
    for (tmpIndex = *index; tmpIndex < *index+digits; tmpIndex++) {
        if (gisdigit(buf[tmpIndex])) {
            returnValue = returnValue * 10 + (buf[tmpIndex] - T('0'));
        }
    }
    *index = tmpIndex;
    return returnValue;
}


/*
    Return an index into the month array
 */

static int parseMonth(char_t *buf, int *index) 
{
    /*  
        "Jan" | "Feb" | "Mar" | "Apr" | "May" | "Jun" | 
        "Jul" | "Aug" | "Sep" | "Oct" | "Nov" | "Dec"
     */
    int tmpIndex, returnValue;
    returnValue = -1;
    tmpIndex = *index;

    switch (buf[tmpIndex]) {
        case 'A':
            switch (buf[tmpIndex+1]) {
                case 'p':
                    returnValue = APR;
                    break;
                case 'u':
                    returnValue = AUG;
                    break;
            }
            break;
        case 'D':
            returnValue = DEC;
            break;
        case 'F':
            returnValue = FEB;
            break;
        case 'J':
            switch (buf[tmpIndex+1]) {
                case 'a':
                    returnValue = JAN;
                    break;
                case 'u':
                    switch (buf[tmpIndex+2]) {
                        case 'l':
                            returnValue = JUL;
                            break;
                        case 'n':
                            returnValue = JUN;
                            break;
                    }
                    break;
            }
            break;
        case 'M':
            switch (buf[tmpIndex+1]) {
                case 'a':
                    switch (buf[tmpIndex+2]) {
                        case 'r':
                            returnValue = MAR;
                            break;
                        case 'y':
                            returnValue = MAY;
                            break;
                    }
                    break;
            }
            break;
        case 'N':
            returnValue = NOV;
            break;
        case 'O':
            returnValue = OCT;
            break;
        case 'S':
            returnValue = SEP;
            break;
    }
    if (returnValue >= 0) {
        *index += 3;
    }
    return returnValue;
}


/* 
    Parse a year value (either 2 or 4 digits)
 */
static int parseYear(char_t *buf, int *index) 
{
    int tmpIndex, returnValue;

    tmpIndex = *index;
    returnValue = parseNDIGIT(buf, 4, &tmpIndex);

    if (returnValue >= 0) {
        *index = tmpIndex;
    } else {
        returnValue = parseNDIGIT(buf, 2, &tmpIndex);
        if (returnValue >= 0) {
            /*
                Assume that any year earlier than the start of the epoch for time_t (1970) specifies 20xx
             */
            if (returnValue < 70) {
                returnValue += 2000;
            } else {
                returnValue += 1900;
            }
            *index = tmpIndex;
        }
    }
    return returnValue;
}


/* 
    The formulas used to build these functions are from "Calendrical Calculations", by Nachum Dershowitz, Edward M.
    Reingold, Cambridge University Press, 1997.  
 */

//  MOB - move to header
#include <math.h>

//  MOB - static
const int GregorianEpoch = 1;


/*
    Determine if year is a leap year
 */
int GregorianLeapYearP(long year) 
{
    long    tmp;
    
    tmp = year % 400;
    return (year % 4 == 0) && (tmp != 100) && (tmp != 200) && (tmp != 300);
}


/*
    Return the fixed date from the gregorian date
 */
long FixedFromGregorian(long month, long day, long year) 
{
    long fixedDate;

    fixedDate = (long)(GregorianEpoch - 1 + 365 * (year - 1) + 
        floor((year - 1) / 4.0) -
        floor((double)(year - 1) / 100.0) + 
        floor((double)(year - 1) / 400.0) + 
        floor((367.0 * ((double)month) - 362.0) / 12.0));

    if (month <= 2) {
        fixedDate += 0;
    } else if (GregorianLeapYearP(year)) {
        fixedDate += -1;
    } else {
        fixedDate += -2;
    }
    fixedDate += day;
    return fixedDate;
}


/*
    Return the gregorian year from a fixed date
 */
long GregorianYearFromFixed(long fixedDate) 
{
    long result, d0, n400, d1, n100, d2, n4, d3, n1, d4, year;

    d0 =    fixedDate - GregorianEpoch;
    n400 =  (long)(floor((double)d0 / (double)146097));
    d1 =    d0 % 146097;
    n100 =  (long)(floor((double)d1 / (double)36524));
    d2 =    d1 % 36524;
    n4 =    (long)(floor((double)d2 / (double)1461));
    d3 =    d2 % 1461;
    n1 =    (long)(floor((double)d3 / (double)365));
    d4 =    (d3 % 365) + 1;
    year =  400 * n400 + 100 * n100 + 4 * n4 + n1;

    if ((n100 == 4) || (n1 == 4)) {
        result = year;
    } else {
        result = year + 1;
    }
    return result;
}


/* 
    Returns the Gregorian date from a fixed date (not needed for this use, but included for completeness)
 */
#if UNUSED && KEEP
GregorianFromFixed(long fixedDate, long *month, long *day, long *year) 
{
    long priorDays, correction;

    *year =         GregorianYearFromFixed(fixedDate);
    priorDays =     fixedDate - FixedFromGregorian(1, 1, *year);

    if (fixedDate < FixedFromGregorian(3,1,*year)) {
        correction = 0;
    } else if (true == GregorianLeapYearP(*year)) {
        correction = 1;
    } else {
        correction = 2;
    }
    *month = (long)(floor((12.0 * (double)(priorDays + correction) + 373.0) / 367.0));
    *day = fixedDate - FixedFromGregorian(*month, 1, *year);
}
#endif


/* 
    Returns the difference between two Gregorian dates
 */
long GregorianDateDifferenc(long month1, long day1, long year1,
                            long month2, long day2, long year2) 
{
    return FixedFromGregorian(month2, day2, year2) - FixedFromGregorian(month1, day1, year1);
}


/*
    Return the number of seconds into the current day
 */
#define SECONDS_PER_DAY 24*60*60

static int parseTime(char_t *buf, int *index) 
{
    /*  
        Format of buf is - 2DIGIT ":" 2DIGIT ":" 2DIGIT
     */
    int returnValue, tmpIndex, hourValue, minuteValue, secondValue;

    hourValue = minuteValue = secondValue = -1;
    returnValue = -1;
    tmpIndex = *index;

    hourValue = parseNDIGIT(buf, 2, &tmpIndex);

    if (hourValue >= 0) {
        tmpIndex++;
        minuteValue = parseNDIGIT(buf, 2, &tmpIndex);
        if (minuteValue >= 0) {
            tmpIndex++;
            secondValue = parseNDIGIT(buf, 2, &tmpIndex);
        }
    }
    if ((hourValue >= 0) && (minuteValue >= 0) && (secondValue >= 0)) {
        returnValue = (((hourValue * 60) + minuteValue) * 60) + secondValue;
        *index = tmpIndex;
    }
    return returnValue;
}


/*
    Return the equivalent of time() given a gregorian date
 */
static time_t dateToTimet(int year, int month, int day) 
{
    long dayDifference;

    dayDifference = FixedFromGregorian(month + 1, day, year) - FixedFromGregorian(1, 1, 1970);
    return dayDifference * SECONDS_PER_DAY;
}


/*
    Return the number of seconds between Jan 1, 1970 and the parsed date (corresponds to documentation for time() function)
 */
static time_t parseDate1or2(char_t *buf, int *index) 
{
    /*  
        Format of buf is either
        2DIGIT SP month SP 4DIGIT
        or
        2DIGIT "-" month "-" 2DIGIT
     */
    int     dayValue, monthValue, yearValue, tmpIndex;
    time_t  returnValue;

    returnValue = (time_t) -1;
    tmpIndex = *index;

    dayValue = monthValue = yearValue = -1;

    if (buf[tmpIndex] == T(',')) {
        /* 
            Skip over the ", " 
         */
        tmpIndex += 2; 

        dayValue = parseNDIGIT(buf, 2, &tmpIndex);
        if (dayValue >= 0) {
            /*
                Skip over the space or hyphen
             */
            tmpIndex++; 
            monthValue = parseMonth(buf, &tmpIndex);
            if (monthValue >= 0) {
                /*
                    Skip over the space or hyphen
                 */
                tmpIndex++; 
                yearValue = parseYear(buf, &tmpIndex);
            }
        }

        if ((dayValue >= 0) &&
            (monthValue >= 0) &&
            (yearValue >= 0)) {
            if (yearValue < 1970) {
                /*              
                    Allow for Microsoft IE's year 1601 dates 
                 */
                returnValue = 0; 
            } else {
                returnValue = dateToTimet(yearValue, monthValue, dayValue);
            }
            *index = tmpIndex;
        }
    }
    
    return returnValue;
}


/*
    Return the number of seconds between Jan 1, 1970 and the parsed date
 */
static time_t parseDate3Time(char_t *buf, int *index) 
{
    /*
        Format of buf is month SP ( 2DIGIT | ( SP 1DIGIT ))
     */
    int     dayValue, monthValue, yearValue, timeValue, tmpIndex;
    time_t  returnValue;

    returnValue = (time_t) -1;
    tmpIndex = *index;

    dayValue = monthValue = yearValue = timeValue = -1;

    monthValue = parseMonth(buf, &tmpIndex);
    if (monthValue >= 0) {
        /*      
            Skip over the space 
         */
        tmpIndex++; 
        if (buf[tmpIndex] == T(' ')) {
            /*
                Skip over this space too 
             */
            tmpIndex++; 
            dayValue = parseNDIGIT(buf, 1, &tmpIndex);
        } else {
            dayValue = parseNDIGIT(buf, 2, &tmpIndex);
        }
        /*      
            Now get the time and time SP 4DIGIT
         */
        timeValue = parseTime(buf, &tmpIndex);
        if (timeValue >= 0) {
            /*          
                Now grab the 4DIGIT year value
             */
            yearValue = parseYear(buf, &tmpIndex);
        }
    }

    if ((dayValue >= 0) && (monthValue >= 0) && (yearValue >= 0)) {
        returnValue = dateToTimet(yearValue, monthValue, dayValue);
        returnValue += timeValue;
        *index = tmpIndex;
    }
    return returnValue;
}


//  MOB - macro and rename
static int bufferIndexIncrementGivenNTest(char_t *buf, int testIndex, char_t testChar, int foundIncrement, int notfoundIncrement) 
{
    if (buf[testIndex] == testChar) {
        return foundIncrement;
    }
    return notfoundIncrement;
}


/*
    Return an index into a logical weekday array
 */
static int parseWeekday(char_t *buf, int *index) 
{
    /*  
        Format of buf is either
            "Mon" | "Tue" | "Wed" | "Thu" | "Fri" | "Sat" | "Sun"
        or
            "Monday" | "Tuesday" | "Wednesday" | "Thursday" | "Friday" | "Saturday" | "Sunday"
     */
    int tmpIndex, returnValue;

    returnValue = -1;
    tmpIndex = *index;

    switch (buf[tmpIndex]) {
        case 'F':
            returnValue = FRI;
            *index += bufferIndexIncrementGivenNTest(buf, tmpIndex+3, 'd', sizeof("Friday"), 3);
            break;
        case 'M':
            returnValue = MON;
            *index += bufferIndexIncrementGivenNTest(buf, tmpIndex+3, 'd', sizeof("Monday"), 3);
            break;
        case 'S':
            switch (buf[tmpIndex+1]) {
                case 'a':
                    returnValue = SAT;
                    *index += bufferIndexIncrementGivenNTest(buf, tmpIndex+3, 'u', sizeof("Saturday"), 3);
                    break;
                case 'u':
                    returnValue = SUN;
                    *index += bufferIndexIncrementGivenNTest(buf, tmpIndex+3, 'd', sizeof("Sunday"), 3);
                    break;
            }
            break;
        case 'T':
            switch (buf[tmpIndex+1]) {
                case 'h':
                    returnValue = THU;
                    *index += bufferIndexIncrementGivenNTest(buf, tmpIndex+3, 'r', sizeof("Thursday"), 3);
                    break;
                case 'u':
                    returnValue = TUE;
                    *index += bufferIndexIncrementGivenNTest(buf, tmpIndex+3, 's', sizeof("Tuesday"), 3);
                    break;
            }
            break;
        case 'W':
            returnValue = WED;
            *index += bufferIndexIncrementGivenNTest(buf, tmpIndex+3, 'n', sizeof("Wednesday"), 3);
            break;
    }
    return returnValue;
}


/*
        Parse the date and time string.
 */
static time_t dateParse(time_t tip, char_t *cmd)
{
    int index, tmpIndex, weekday, timeValue;
    time_t parsedValue, dateValue;

    parsedValue = (time_t) 0;
    index = timeValue = 0;
    weekday = parseWeekday(cmd, &index);

    if (weekday >= 0) {
        tmpIndex = index;
        dateValue = parseDate1or2(cmd, &tmpIndex);
        if (dateValue >= 0) {
            index = tmpIndex + 1;
            /*
                One of these two forms is being used
                wkday "," SP date1 SP time SP "GMT"
                weekday "," SP date2 SP time SP "GMT"
             */
            timeValue = parseTime(cmd, &index);
            if (timeValue >= 0) {
                /*              
                    Now match up that "GMT" string for completeness
                    Compute the final value if there were no problems in the parse
                 */
                if ((weekday >= 0) &&
                    (dateValue >= 0) &&
                    (timeValue >= 0)) {
                    parsedValue = dateValue + timeValue;
                }
            }
        } else {
            /* 
                Try the other form - wkday SP date3 SP time SP 4DIGIT
             */
            tmpIndex = index;
            parsedValue = parseDate3Time(cmd, &tmpIndex);
        }
    }

    return parsedValue;
}

#endif /* WEBS_IF_MODIFIED_SUPPORT */


/*
    Return the mime type for the given URL given a URL. The caller supplies the buffer to hold the result.
    charCnt is the number of characters the buffer will hold, ascii or UNICODE.
 */
char_t *websUrlType(char_t *url, char_t *buf, int charCnt)
{
    sym_t   *sp;
    char_t  *ext, *parsebuf;

    a_assert(url && *url);
    a_assert(buf && charCnt > 0);

    if (url == NULL || *url == '\0') {
        gstrcpy(buf, T("text/plain"));
        return buf;
    }
    if (websUrlParse(url, &parsebuf, NULL, NULL, NULL, NULL, NULL, 
            NULL, &ext) < 0) {
        gstrcpy(buf, T("text/plain"));
        return buf;
    }
    strlower(ext);

    /*
        Lookup the mime type symbol table to find the relevant content type
     */
    if ((sp = symLookup(websMime, ext)) != NULL) {
        gstrncpy(buf, sp->content.value.string, charCnt);
    } else {
        gstrcpy(buf, T("text/plain"));
    }
    bfree(parsebuf);
    return buf;
}


/*
    Parse the URL. A buffer is allocated to store the parsed URL in *pbuf. This must be freed by the caller. NOTE: tag
    is not yet fully supported.  
 */
int websUrlParse(char_t *url, char_t **pbuf, char_t **phost, char_t **ppath, char_t **pport, char_t **pquery, 
        char_t **pproto, char_t **ptag, char_t **pext)
{
    char_t      *tok, *cp, *host, *path, *port, *proto, *tag, *query, *ext;
    char_t      *hostbuf, *portbuf, *buf;
    ssize       len, ulen;
    int         c;

    a_assert(url);
    a_assert(pbuf);

    ulen = gstrlen(url);
    /*
        We allocate enough to store separate hostname and port number fields.  As there are 3 strings in the one buffer,
        we need room for 3 null chars.  We allocate MAX_PORT_LEN char_t's for the port number.  
     */
    len = ulen * 2 + MAX_PORT_LEN + 3;
    if ((buf = balloc(len * sizeof(char_t))) == NULL) {
        return -1;
    }
    portbuf = &buf[len - MAX_PORT_LEN - 1];
    hostbuf = &buf[ulen+1];
    websDecodeUrl(buf, url, ulen);

    url = buf;

    /*
        Convert the current listen port to a string. We use this if the URL has no explicit port setting
     */
    stritoa(websGetPort(), portbuf, MAX_PORT_LEN);
    port = portbuf;
    path = T("/");
    proto = T("http");
    host = T("localhost");
    query = T("");
    ext = htmExt;
    tag = T("");

    if (gstrncmp(url, T("http://"), 7) == 0) {
        tok = &url[7];
        tok[-3] = '\0';
        proto = url;
        host = tok;
        for (cp = tok; *cp; cp++) {
            if (*cp == '/') {
                break;
            }
            if (*cp == ':') {
                *cp++ = '\0';
                port = cp;
                tok = cp;
            }
        }
        if ((cp = gstrchr(tok, '/')) != NULL) {
            /*
                If a full URL is supplied, we need to copy the host and port portions into static buffers.
             */
            c = *cp;
            *cp = '\0';
            gstrncpy(hostbuf, host, ulen);
            gstrncpy(portbuf, port, MAX_PORT_LEN);
            *cp = c;
            host = hostbuf;
            port = portbuf;
            path = cp;
            tok = cp;
        }
    } else {
        path = url;
        tok = url;
    }
    /*
        Parse the query string
     */
    if ((cp = gstrchr(tok, '?')) != NULL) {
        *cp++ = '\0';
        query = cp;
        path = tok;
        tok = query;
    } 

    /*
        Parse the fragment identifier
     */
    if ((cp = gstrchr(tok, '#')) != NULL) {
        *cp++ = '\0';
        if (*query == 0) {
            path = tok;
        }
    }

    /*
        Only do the following if asked for the extension
     */
    if (pext) {
        /*
            Later the path will be cleaned up for trailing slashes and so on.  To be ready, we need to clean up here,
            much as in websValidateUrl.  Otherwise a URL ending in "asp/" or "asP" sends Ejscript source to the browser.
        */
        if ((cp = gstrrchr(path, '.')) != NULL) {
            const char_t* garbage = T("/\\");
            ssize length = gstrcspn(cp, garbage);
            ssize garbageLength = gstrspn(cp + length, garbage);
            ssize ok = (length + garbageLength == (int) gstrlen(cp));
            if (ok) {
                cp[length] = '\0';
#if BIT_WIN_LIKE
                strlower(cp);            
#endif
                ext = cp;
            }
        }
    }
    /*
        Pass back the fields requested (if not NULL)
     */
    if (phost)
        *phost = host;
    if (ppath)
        *ppath = path;
    if (pport)
        *pport = port;
    if (pproto)
        *pproto = proto;
    if (pquery)
        *pquery = query;
    if (ptag)
        *ptag = tag;
    if (pext)
        *pext = ext;
    *pbuf = buf;
    return 0;
}


#if BIT_DIGEST_AUTH
/*
    Get a Nonce value for passing along to the client.  This function composes the string "RANDOMKEY:timestamp:myrealm"
    and calculates the MD5 digest placing it in output. 
 */
char_t *websCalcNonce(webs_t wp)
{
    char_t      *nonce, *prenonce;
    time_t      longTime;
#if WIN
    char_t      buf[26];
    errno_t     error;
    struct tm   newtime;
#else
    struct tm   *newtime;
#endif

    a_assert(wp);
    time(&longTime);
#if !WIN
    newtime = localtime(&longTime);
#else
    error = localtime_s(&newtime, &longTime);
#endif

#if !WIN
    prenonce = NULL;
    fmtAlloc(&prenonce, 256, T("%s:%s:%s"), RANDOMKEY, gasctime(newtime), wp->realm); 
#else
    asctime_s(buf, elementsof(buf), &newtime);
    fmtAlloc(&prenonce, 256, T("%s:%s:%s"), RANDOMKEY, buf, RANDOMKEY); 
#endif

    a_assert(prenonce);
    nonce = websMD5(prenonce);
    bfree(prenonce);
    return nonce;
}


/*
    Get an Opaque value for passing along to the client
 */
char_t *websCalcOpaque(webs_t wp)
{
    char_t *opaque;
    a_assert(wp);
    /*
        MOB ??? Temporary stub!
     */
    opaque = bstrdup(T("5ccc069c403ebaf9f0171e9517f40e41"));
    return opaque;
}


/*
    Get a Digest value using the MD5 algorithm
 */
char_t *websCalcDigest(webs_t wp)
{
    char_t  *digest, *a1, *a1prime, *a2, *a2prime, *preDigest, *method;

    a_assert(wp);
    digest = NULL;

    /*
        Calculate first portion of digest H(A1)
     */
    a1 = NULL;
    fmtAlloc(&a1, 255, T("%s:%s:%s"), wp->userName, wp->realm, wp->password);
    a_assert(a1);
    a1prime = websMD5(a1);
    bfree(a1);

    /*
        Calculate second portion of digest H(A2)
     */
    method = websGetVar(wp, T("REQUEST_METHOD"), NULL);
    a_assert(method);
    a2 = NULL;
    fmtAlloc(&a2, 255, T("%s:%s"), method, wp->uri); 
    a_assert(a2);
    a2prime = websMD5(a2);
    bfree(a2);

    /*
        Construct final digest KD(H(A1):nonce:H(A2))
     */
    a_assert(a1prime);
    a_assert(a2prime);
    a_assert(wp->nonce);

    preDigest = NULL;
    if (!wp->qop) {
        fmtAlloc(&preDigest, 255, T("%s:%s:%s"), a1prime, wp->nonce, a2prime);
    } else {
        fmtAlloc(&preDigest, 255, T("%s:%s:%s:%s:%s:%s"), 
            a1prime, wp->nonce, wp->nc, wp->cnonce, wp->qop, a2prime);
    }

    a_assert(preDigest);
    digest = websMD5(preDigest);
    bfree(a1prime);
    bfree(a2prime);
    bfree(preDigest);
    return digest;
}


/*
    Get a Digest value using the MD5 algorithm. Digest is based on the full URL rather than the URI as above This
    alternate way of calculating is what most browsers use.
 */
char_t *websCalcUrlDigest(webs_t wp)
{
    char_t  *digest, *a1, *a1prime, *a2, *a2prime, *preDigest, *method;

    a_assert(wp);
    digest = NULL;

    /*
        Calculate first portion of digest H(A1)
     */
    a1 = NULL;
    fmtAlloc(&a1, 255, T("%s:%s:%s"), wp->userName, wp->realm, wp->password);
    a_assert(a1);
    a1prime = websMD5(a1);
    bfree(a1);

    /*
        Calculate second portion of digest H(A2)
     */
    method = websGetVar(wp, T("REQUEST_METHOD"), NULL);
    a_assert(method);
    a2 = balloc((gstrlen(method) +2 + gstrlen(wp->url) ) * sizeof(char_t));
    a_assert(a2);
    gsprintf(a2, T("%s:%s"), method, wp->url);
    a2prime = websMD5(a2);
    bfree(a2);

    /*
        Construct final digest KD(H(A1):nonce:H(A2))
     */
    a_assert(a1prime);
    a_assert(a2prime);
    a_assert(wp->nonce);

    preDigest = NULL;
    if (!wp->qop) {
        fmtAlloc(&preDigest, 255, T("%s:%s:%s"), a1prime, wp->nonce, a2prime);
    } else {
        fmtAlloc(&preDigest, 255, T("%s:%s:%s:%s:%s:%s"), 
            a1prime, wp->nonce, wp->nc, wp->cnonce, wp->qop, a2prime);
    }
    a_assert(preDigest);
    digest = websMD5(preDigest);
    /*
        Now clean up
     */
    bfree(a1prime);
    bfree(a2prime);
    bfree(preDigest);
    return digest;
}

#endif /* BIT_DIGEST_AUTH */

/*
    Open a web page. lpath is the local filename. path is the URL path name.
 */
int websPageOpen(webs_t wp, char_t *lpath, char_t *path, int mode, int perm)
{
    a_assert(websValid(wp));
#if BIT_ROM
    return websRomPageOpen(wp, path, mode, perm);
#else
    return (wp->docfd = gopen(lpath, mode, perm));
#endif
}


void websPageClose(webs_t wp)
{
    a_assert(websValid(wp));

#if BIT_ROM
    websRomPageClose(wp->docfd);
#else
    if (wp->docfd >= 0) {
        close(wp->docfd);
        wp->docfd = -1;
    }
#endif
}


/*
    Stat a web page lpath is the local filename. path is the URL path name.
 */
int websPageStat(webs_t wp, char_t *lpath, char_t *path, websStatType* sbuf)
{
#if BIT_ROM
    return websRomPageStat(path, sbuf);
#else
    gstat_t s;

    if (gstat(lpath, &s) < 0) {
        return -1;
    }
    sbuf->size = (ssize) s.st_size;
    sbuf->mtime = s.st_mtime;
    sbuf->isDir = s.st_mode & S_IFDIR;
    return 0;
#endif
}


int websPageIsDirectory(char_t *lpath)
{
#if BIT_ROM
    websStatType    sbuf;

    if (websRomPageStat(lpath, &sbuf) >= 0) {
        return(sbuf.isDir);
    } else {
        return 0;
    }
#else
    gstat_t sbuf;

    if (gstat(lpath, &sbuf) >= 0) {
        return(sbuf.st_mode & S_IFDIR);
    } else {
        return 0;
    }
#endif
}


/*
    Read a web page. Returns the number of _bytes_ read. len is the size of buf, in bytes.
 */
ssize websPageReadData(webs_t wp, char *buf, ssize nBytes)
{

#if BIT_ROM
    a_assert(websValid(wp));
    return websRomPageReadData(wp, buf, nBytes);
#else
    a_assert(websValid(wp));
    return read(wp->docfd, buf, nBytes);
#endif
}


/*
    Move file pointer offset bytes.
 */
void websPageSeek(webs_t wp, filepos offset)
{
    a_assert(websValid(wp));

#if BIT_ROM
    websRomPageSeek(wp, offset, SEEK_CUR);
#else
    lseek(wp->docfd, offset, SEEK_CUR);
#endif
}

/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis GoAhead open source license or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */