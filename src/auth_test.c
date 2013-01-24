/* 
 * auth_test.c - unit tests for auth.c
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

#include "auth.h"
#include "auth_test.h"
#include "minunit.h"

/* ensure check_auth() returns 403 by default */
char *test_auth_default()
{
        mu_assert("ensure check_auth() returns 403 Forbidden by default", 
                check_auth("POST", "/blah/") == 403);
        return 0;
}

char *test_auth_deny()
{
        mu_assert("ensure GET /static/secret.html returns 401 Unauthorized", 
                check_auth("GET", "/static/secret.html") == 401);

        mu_assert("ensure GET /denyme.html returns 403 Forbidden", 
                check_auth("GET", "/denyme.html") == 403);

        mu_assert("ensure POST /static/index.html returns 403 Forbidden", 
                check_auth("POST", "/static/index.html") == 403);
        mu_assert("ensure DELETE /sqlview/ returns 403 Forbidden", 
                check_auth("DELETE", "/sqlview/") == 403);
        return 0;
}

char *test_auth_allow()
{
        mu_assert("ensure GET /static/index.html allowed", 
                check_auth("GET", "/static/index.html") == 0);
        mu_assert("ensure GET /sqlview/ allowed", 
                check_auth("GET", "/sqlview/") == 0);
        mu_assert("ensure POST /sqlview/ allowed", 
                check_auth("POST", "/sqlview/") == 0);
        return 0;
}

char *test_auth_require()
{
        mu_assert("check_auth_require() - fail on invalid alias",
                check_auth_require("invalid", NULL, NULL) != 0);
        mu_assert("check_auth_require() - fail with invalid credentials",
                check_auth_require("ldap", "betty", "false") != 0);
        mu_assert("check_auth_require() - successful ldap bind",
                check_auth_require("ldap", "betty", "ie5a8P40") == 0);

        return 0;
}
