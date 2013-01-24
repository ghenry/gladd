/*
 * handler.c - some code to handle incoming connections
 *
 * this file is part of GLADD
 *
 * Copyright (c) 2012, 2013 Brett Sheffield <brett@gladserv.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING in the distribution).
 * If not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE

#define BUFSIZE 8096

#include "auth.h"
#include "config.h"
#include "db.h"
#include "handler.h"
#include "http.h"
#include "main.h"
#include "mime.h"
#include "xml.h"

#include <arpa/inet.h>
#include <errno.h>
#include <libxml/parser.h>
#include <limits.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

int sockme;


/*
 * get sockaddr, IPv4 or IPv6:
 */
void *get_in_addr(struct sockaddr *sa)
{
        if (sa->sa_family == AF_INET) {
                return &(((struct sockaddr_in*)sa)->sin_addr);
        }

        return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/*
 * child process where we handle incoming connections
 */
void handle_connection(int sock, struct sockaddr_storage their_addr)
{
        char *basefile;
        char buf[BUFSIZE];
        char *filename;
        char s[INET6_ADDRSTRLEN];
        ssize_t byte_count;
        int state;
        int auth = -1;
        url_t *u;
        char *r;
        char *sql;
        char *xml;
        int hcount = 0;

        inet_ntop(their_addr.ss_family,
                        get_in_addr((struct sockaddr *)&their_addr),
                        s, sizeof s);

        syslog(LOG_INFO, "server: got connection from %s", s);

        /* What are we being asked to do? */
        byte_count = recv(sock, buf, sizeof buf, 0);

        /* read http client headers */
        hcount = http_read_headers(buf, byte_count);
        if (hcount == HTTP_BAD_REQUEST) {
                syslog(LOG_INFO, "Bad Request");
                http_response(sock, 400);
                exit(EXIT_FAILURE);
        }
        if (http_validate_headers(request->headers) != 0) {
                syslog(LOG_INFO, "Bad Request - invalid request headers");
                http_response(sock, 400);
                exit(EXIT_FAILURE);
        }

        syslog(LOG_DEBUG, "Client header count: %i", hcount);
        syslog(LOG_DEBUG, "Method: %s", request->method);
        syslog(LOG_DEBUG, "Resource: %s", request->res);
        syslog(LOG_DEBUG, "HTTP Version: %s", request->httpv);

        /* Return HTTP response */

        /* put a cork in it */
        state = 1;
        setsockopt(sockme, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));

        /* check auth & auth */
        auth = check_auth(request->method, request->res);
        if (auth != 0) {
                http_response(sock, auth);
        }
        else if (strncmp(request->method, "GET", 3) == 0) {

                u = config->urls;
                while (u != NULL) {
                        syslog(LOG_DEBUG, "url matching... Trying %s", u->url);
                        if (strncmp(request->res, u->url, strlen(u->url))==0) {
                                if (strncmp(u->type, "static", 6) == 0) {
                                        /* serve static files */
                                        basefile = strndup(request->res+8,
                                                sizeof(request->res));
                                        asprintf(&filename, "%s%s", u->path,
                                                                    basefile);
                                        free(basefile);
                                        if (send_file(sock, filename) == 1)
                                                http_response(sock, 404);
                                        free(filename);
                                        break;
                                }
                                else if (strcmp(u->type, "sqlview") == 0) {
                                        /* handle sqlview */
                                        db_t *db;
                                        if (!(db = getdb(u->db))) {
                                                syslog(LOG_ERR,
                                                "db '%s' not in config",
                                                u->db);
                                                http_response(sock, 500);
                                        }
                                        if (asprintf(&sql, "%s", 
                                                getsql(u->view)) == -1) 
                                        {
                                                http_response(sock, 500);
                                        }
                                        if (sqltoxml(db, sql, &xml, 1) != 0) {
                                                free(sql);
                                                http_response(sock, 500);
                                        }
                                        free(sql);
                                        if (asprintf(&r, RESPONSE_200,
                                                MIME_XML, xml) == -1)
                                        {
                                                free(xml);
                                                http_response(sock, 500);
                                        }
                                        respond(sock, r);
                                        free(r);
                                        free(xml);
                                        break;
                                }
                                else {
                                        syslog(LOG_ERR, 
                                                "Unknown url type '%s'",
                                                u->type);
                                }
                        }
                        u = u->next;
                }

                if (u == NULL) {
                        /* Not found */
                        http_response(sock, 404);
                }
        }
        else if (strncmp(request->method, "POST", 3) == 0) {
                long len;
                http_status_code_t err;
                FILE *fd;

                len = check_content_length(&err);
                if (err != 0) {
                        http_response(sock, err);
                }
                else {
                        syslog(LOG_DEBUG, "Content-Length: %li", len);
                        
                        /* FIXME: temp */
                        /* write POST data out to file for testing */
                        fd = fopen("/tmp/mypost", "w");
                        fprintf(fd, "Where are my trousers?\n");
                        fprintf(fd, "%s", buf);
                        fclose(fd);
                }
        }
        else {
                /* Method Not Allowed */
                http_response(sock, 405);
        }

        /* close client connection */
        close(sock);

        /* free memory */
        free_request();

        /* child process can exit */
        exit (EXIT_SUCCESS);
}

int send_file(int sock, char *path)
{
        char *r;
        char *mimetype;
        int f;
        int rc;
        off_t offset;
        int state;
        struct stat stat_buf;

        f = open(path, O_RDONLY);
        if (f == -1) {
                syslog(LOG_ERR, "unable to open '%s': %s\n", path,
                                strerror(errno));
                return 1;
        }

        /* get size of file */
        fstat(f, &stat_buf);

        /* ensure file is a regular file */
        if (! S_ISREG(stat_buf.st_mode)) {
                syslog(LOG_ERR, "'%s' is not a regular file\n", path);
                return 1;
        }

        syslog(LOG_DEBUG, "Sending %i bytes", (int)stat_buf.st_size);

        /* send headers */
        mimetype = malloc(strlen(MIME_DEFAULT)+1);
        get_mime_type(mimetype, path);
        syslog(LOG_DEBUG, "Content-Type: %s", mimetype);
        if (asprintf(&r, RESPONSE_200, mimetype, "") == -1) {
                syslog(LOG_ERR, "Malloc failed");
                exit(EXIT_FAILURE);
        }
        free(mimetype);
        respond(sock, r);

        /* send the file */
        errno = 0;
        offset = 0;
        rc = sendfile(sock, f, &offset, stat_buf.st_size);
        if (rc == -1) {
                syslog(LOG_ERR, "error from sendfile: %s\n", strerror(errno));
        }
        syslog(LOG_DEBUG, "Sent %i bytes", rc);

        /* everything sent ? */
        if (rc != stat_buf.st_size) {
                syslog(LOG_ERR,
                        "incomplete transfer from sendfile: %d of %d bytes\n",
                        rc, (int)stat_buf.st_size);
        }

        /* pop my cork */
        state = 0;
        setsockopt(sockme, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));

        close(f);

        return 0;
}

void respond (int fd, char *response)
{
        send(fd, response, strlen(response), 0);
}

