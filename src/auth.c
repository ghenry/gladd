/* 
 * auth.c - authentication functions
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

#include "auth.h"
#include "blowfish.h"
#include "config.h"
#include "gladdb/db.h"
#include "string.h"
#include "xml.h"
#include <fnmatch.h>
#include <grp.h>
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/types.h>
#include <unistd.h>

struct pam_response *r;

int null_conv(int num_msg, const struct pam_message **msg,
struct pam_response **resp, void *appdata_ptr)
{
         *resp = r;
        return PAM_SUCCESS;
}

static struct pam_conv conv = {
        null_conv,
        NULL
};

int check_auth_pam(char *service, char *username, char *password)
{
        int ret;
        pam_handle_t *pamh = NULL;
        char *pass = strdup(password);

        ret = pam_start(service, username, &conv, &pamh);
        if (ret == PAM_SUCCESS) {
                r = (struct pam_response *)malloc(sizeof(struct pam_response));
                r[0].resp = pass;
                r[0].resp_retcode = 0;

                ret = pam_authenticate(pamh, 0);
                if (ret == PAM_SUCCESS) {
                        syslog(LOG_DEBUG,
                               "PAM authentication successful for user %s", username);
                }
                else {
                        if (config->debug) {
                                syslog(LOG_DEBUG, "PAM: %s",
                                        pam_strerror(pamh, ret));
                        }
                        syslog(LOG_ERR,
                                "PAM authentication failure for user %s",
                                username);
                }

        }
        pam_end(pamh, ret);
        return ( ret == PAM_SUCCESS ? 0:HTTP_UNAUTHORIZED );
}

/* 
 * check if authorized for requested method and url
 * default is to return 403 Forbidden
 * a return value of 0 is allow.
 */
int check_auth(http_request_t *r)
{
        acl_t *a;
        int i;
        int pass = 0;
        http_status_code_t res = HTTP_FORBIDDEN;

        a = config->acls;
        while (a != NULL) {
                syslog(LOG_DEBUG, 
                        "Checking acls for %s %s ... trying %s %s", 
                        r->method, r->res, a->method, a->url);
                /* ensure method and url matches */
                if ((fnmatch(a->url, r->res, 0) == 0) &&
                    (strncmp(r->method, a->method, strlen(r->method)) == 0))
                {
                        syslog(LOG_DEBUG,
                                "Found matching acl - checking credentials");

                        if (strcmp(a->type, "deny") == 0) {
                                syslog(LOG_DEBUG, "acl deny");
                                return HTTP_FORBIDDEN;
                        }
                        else if (strcmp(a->type, "params") == 0) {
                                if (strcmp(a->auth, "cookie:session") == 0) {
                                        syslog(LOG_DEBUG, "cookie");
                                        r->cookie = 1;
                                }
                                if (strcmp(a->auth, "nocache") == 0) {
                                        syslog(LOG_DEBUG, "nocache");
                                        r->nocache = 1;
                                }
                        }
                        else if (strcmp(a->type, "allow") == 0) {
                                /* TODO: check for ip address */
                                syslog(LOG_DEBUG, "acl allow");
                                return 0;
                        }
                        else if (strcmp(a->type, "sufficient") == 0) {
                                syslog(LOG_DEBUG, "acl sufficient...");
                                /* if this is successful, no further checks */
                                i = check_auth_sufficient(a->auth, r);
                                if (i == 0) {
                                        syslog(LOG_DEBUG, "auth sufficient");
                                        return i;
                                }
                                /* if we fail later, code is 401, not 403 */
                                res = HTTP_UNAUTHORIZED;
                        }
                        else if (strcmp(a->type, "require") == 0) {
                                syslog(LOG_DEBUG, "acl require ...");
                                /* this MUST pass, but do further checks */
                                i = check_auth_require(a->auth, r);
                                if (i != 0) {
                                        syslog(LOG_DEBUG, "auth require fail");
                                        return i;
                                }
                                pass++;
                        }
                        else {
                                syslog(LOG_DEBUG,
                                "acl auth type '%s' not understood", a->type);
                                return HTTP_INTERNAL_SERVER_ERROR;
                        }
                }
                a = a->next;
        }
        if (a != NULL) {
                if (strcmp(a->type, "sufficient") == 0) {
                        return 0;
                }
        }
        if (pass > 0) return 0;
        syslog(LOG_DEBUG, "no acl matched");
        return res;
}

/* any auth will do */
int check_auth_sufficient(char *alias, http_request_t *r)
{
        auth_t *a;
        int i;

        if (strcmp(alias, "*") == 0) {
                a = config->auth;
                while (a != NULL) {
                        i = check_auth_alias(a->alias, r);
                        if (i == 0) {
                                return i; /* That'll do nicely */
                        }
                        a = a->next;
                }
        }
        else {
                return check_auth_alias(alias, r);
        }

        return HTTP_FORBIDDEN;
}

/* all auth MUST pass */
int check_auth_require(char *alias, http_request_t *r)
{
        auth_t *a;
        int i;
        
        if (strcmp(alias, "*") == 0) {
                /* all auth MUST succeed */
                a = config->auth;
                while (a != NULL) {
                        i = check_auth_alias(a->alias, r);
                        if (i != 0) {
                                return i; /* one failure is all it takes */
                        }
                        a = a->next;
                }
        }
        else {
                return check_auth_alias(alias, r);
        }

        return 0;
}

int check_auth_alias(char *alias, http_request_t *r)
{
        auth_t *a;
        int res;

        if (! (a = getauth(alias))) {
                syslog(LOG_ERR, 
                        "Invalid alias '%s' supplied to check_auth_alias()",
                        alias);
                return HTTP_INTERNAL_SERVER_ERROR;
        }

        syslog(LOG_DEBUG, "checking alias %s", alias);

        if (strcmp(a->type, "cookie") == 0) {
                return check_auth_cookie(r, a);
        }
        else if (r->authuser == NULL || r->authpass == NULL) {
                /* don't allow auth to proceed with blank credentials */
                syslog(LOG_DEBUG, "auth attempted with blank credentials");
                return HTTP_UNAUTHORIZED;
        }
        else if (strcmp(a->type, "group") == 0) {
                char *vgroup = strdup(a->db);
                char *url = strdup(r->res);
                sqlvars(&vgroup, url);
                if (check_auth_group(r->authuser, vgroup)) {
                        syslog(LOG_DEBUG, "user %s in group %s",
                                r->authuser, vgroup);
                        free(url);
                        free(vgroup);
                        return 0;
                }
                else {
                        syslog(LOG_DEBUG, "user %s NOT in group %s",
                                r->authuser, vgroup);
                        free(url);
                        free(vgroup);
                        return HTTP_UNAUTHORIZED;
                }
        }
        else if (strcmp(a->type, "ldap") == 0) {
                /* test credentials against ldap */
                syslog(LOG_DEBUG, "checking ldap users");
                res = db_test_bind(getdb(a->db),
                        getsql(a->sql), a->bind,
                        r->authuser, r->authpass);
                if (res == -1) return HTTP_INTERNAL_SERVER_ERROR;
                if (res == -2) return HTTP_UNAUTHORIZED;
                return res;
        }
        else if (strcmp(a->type, "user") == 0) {
                /* test credentials against users */
                syslog(LOG_DEBUG, "checking static users");
                user_t *u;
                u = getuser(r->authuser);
                if (u == NULL) {
                        /* user not found */
                        syslog(LOG_DEBUG, "no static user match");
                        return HTTP_UNAUTHORIZED;
                }
                syslog(LOG_DEBUG, "matched static user");
                if ((strncmp(r->authpass, u->password,
                strlen(u->password)) != 0)
                || (strlen(r->authpass) != strlen(u->password)))
                {
                        /* password incorrect */
                        syslog(LOG_DEBUG, "password incorrect");
                        return HTTP_UNAUTHORIZED;
                }
        }
        else if (strcmp(a->type, "pam") == 0) {
                /* use pam authentication */
                syslog(LOG_DEBUG, "checking PAM authentication");
                return check_auth_pam(a->db, r->authuser, r->authpass);
        }
        else {
                syslog(LOG_ERR,
                        "Invalid auth type '%s' in check_auth_require()",
                        a->type);
                return HTTP_INTERNAL_SERVER_ERROR;
        }

        return 0;
}

/* return 1 if user in config-defined group, 0 if not */
int ingroup(char *user, char *group)
{
        group_t *g;
        user_t *m;

        g = getgroup(group);
        if (!g) {
                fprintf(stderr, "group not found\n");
                return -1;
        }

        m = g->members;
        while (m != NULL) {
                if (strcmp(m->username, user) == 0) return 1;
                m = m->next;
        }

        return 0;
}

/* check is user is in specified group
 * searches, static config-defined groups, then NSS
 * returns 1 if in group, 0 if not, or -1 on error */
int check_auth_group(char *username, char *groupname)
{
        /* check config groups first */
        if (ingroup(username, groupname) == 1) {
                return 1;
        }

        /* now search nsswitch groups */
        int ret = 0;
        char *member;
        int i = 0;
        struct group *g;
        if ((g = getgrnam(groupname))) {
                while ((member = g->gr_mem[i++])) {
                        if (strcmp(username, member) == 0) {
                                ret = 1;
                                break;
                        }
                }
        }
        endgrent();
        return ret;
}

/* set session cookie */
int auth_set_cookie(char **r, http_cookie_type_t type)
{
        /* we create the session cookie by concatenating:
         * <username> <timestamp> <nonce>
         * and encrypting this using the blowfish cipher and our key */
        char *nonce = randstring(64);
        time_t timestamp = time(NULL);
        char *dough;
        char *cookie;

        asprintf(&dough, "%s %li %s", request->authuser, (long)timestamp,
                nonce);
        free(nonce);
        cookie = encipher(dough);
        free(dough);

        syslog(LOG_DEBUG, "setting session cookie %s", cookie);
        http_insert_header(r, "Set-Cookie: SID=%s; Path=/; Secure; HttpOnly",
                cookie);
        free(cookie);

        return 0;
}

/* invalidate session cookie */
int auth_unset_cookie(char **r)
{
        syslog(LOG_DEBUG, "logout requested - invalidating session cookie");
        http_insert_header(r,
                "Set-Cookie: SID=logout; Path=/; Max-Age=0; Secure; HttpOnly");
        return 0;
}

int check_auth_cookie(http_request_t *r, auth_t *a)
{
        /* get the Cookie header (we only expect one) */
        char *cookie = http_get_header(r, "Cookie");
        if (!cookie) {
                syslog(LOG_DEBUG, "No Cookie header, skipping cookie auth");
                return HTTP_UNAUTHORIZED;
        }
        syslog(LOG_DEBUG, "attempting cookie auth");

        /* find the first cookie called SID, ignore any others */
        char *tmp = malloc(strlen(cookie));
        if (sscanf(cookie, "SID=%[^;]", tmp) != 1) {
                syslog(LOG_DEBUG, "Session cookie not found");
                free(tmp);
                return HTTP_UNAUTHORIZED;
        }
        /* decrypt cookie */
        char *clearcookie = decipher(tmp);
        free(tmp);
        syslog(LOG_DEBUG, "Decrypted cookie: %s", clearcookie);

        /* check cookie is valid */
        char username[64];
        char nonce[64];
        long timestamp;
        if (sscanf(clearcookie, "%s %li %s",
        username, &timestamp, nonce) != 3)
        {
                syslog(LOG_DEBUG, "Invalid cookie");
                return HTTP_UNAUTHORIZED;
        }

        /* check cookie freshness */
        long now = (long)time(NULL);
        if ((timestamp + config->sessiontimeout) < now) {
                syslog(LOG_DEBUG, "Stale cookie (older than %lis)",
                        config->sessiontimeout);
                return HTTP_UNAUTHORIZED;
        }

        /* set session info from cookie */
        syslog(LOG_INFO, "Valid cookie obtained for %s", username);
        syslog(LOG_DEBUG, "Setting username %s from session cookie", username);
        request->authuser = strdup(username);

        free(cookie);
        free(clearcookie);

        return 0;
}


/* Return blowfish-encrypted ciphertext using our secretkey 
 * NB: string must be exactly 64 bytes long or it will be truncated
 */
char *encipher(char *cleartext)
{
        char *ciphertext = strndup(cleartext, 64);
        BLOWFISH_CTX ctx;
        unsigned long L = (unsigned long)ciphertext;
        unsigned long R = (unsigned long)ciphertext+32;

        Blowfish_Init(&ctx, (unsigned char*)config->secretkey,
                strlen(config->secretkey));
        Blowfish_Encrypt(&ctx, &L, &R);

        /* blowfish may contain embedded nuls, so run it through base64 */
        char *base64 = encode64(ciphertext, 64);
        free(ciphertext);

        return base64;
}

/* Return decrypted blowfish ciphertext */
char *decipher(char *base64)
{
        /* first, strip base64 encoding */
        char *ciphertext = decode64(base64);

        char *cleartext = strndup(ciphertext, 64);
        BLOWFISH_CTX ctx;
        unsigned long L = (unsigned long)cleartext;
        unsigned long R = (unsigned long)cleartext+32;

        Blowfish_Init(&ctx, (unsigned char*)config->secretkey,
                strlen(config->secretkey));
        Blowfish_Decrypt(&ctx, &L, &R);

        return cleartext;
}
