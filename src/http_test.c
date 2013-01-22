/* 
 * http_test.c - unit tests for http.c
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
#include "http_test.h"
#include "minunit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *test_http_read_headers()
{
        int hcount = 0;
        char *headers;
        char *httpv = NULL;
        char *method = NULL;
        char *res = NULL;

        http_header_t *h;

        asprintf(&headers, "GET /index.html HTTP/1.1\nAuthorization: Basic YmV0dHk6bm9iYnk=\nUser-Agent: curl/7.25.0 (x86_64-pc-linux-gnu) libcurl/7.25.0 OpenSSL/1.0.0j zlib/1.2.5.1 libidn/1.25\nHost: localhost:3000\nAccept: */*\n");

        hcount = http_read_headers(headers, &method, &res, &httpv, &h);
        mu_assert("Check request method = GET", strcmp(method, "GET") == 0);
        mu_assert("Check HTTP version = 1.1", strcmp(httpv, "1.1") == 0);
        mu_assert("Check request url = /index.html",
                strcmp(res, "/index.html") == 0);
        mu_assert("Check client header count", hcount == 4);
        fprintf(stderr, "Headers: %i\n", hcount);
        fprintf(stderr, "%s\n", h->key);
        fprintf(stderr, "%s\n", h->value);
        mu_assert("Test 1st header key",
                (strcmp(h->key, "Authorization") == 0));
        mu_assert("http_get_header()",
                strcmp(http_get_header(h, "Authorization"),
                "Basic YmV0dHk6bm9iYnk=") == 0);
        mu_assert("Decode base64", 
                strcmp(decode64("YmV0dHk6bm9iYnk="), "betty:nobby") == 0);
        mu_assert("Fetch next header", h = h->next);
        mu_assert("Test 2nd header key", strcmp(h->key, "User-Agent") == 0);
        mu_assert("Fetch next header", h = h->next);
        mu_assert("Test 3rd header key", strcmp(h->key, "Host") == 0);
        mu_assert("Fetch next header", h = h->next);
        mu_assert("Test 4th header key", strcmp(h->key, "Accept") == 0);
        mu_assert("Ensure final header->next is NULL", !(h = h->next));

        free(headers);
        free(method);
        free(httpv);
        free(res);
        free(h);

        return 0;
}
