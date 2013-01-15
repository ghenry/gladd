/* 
 * db_test.c - unit tests for config.c
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

#include "db_test.h"
#include "minunit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *test_db_connect()
{
        db_t *db;
        db = config->dbs;

        mu_assert("Ensure db_connect() fails when db doesn't exist",
                db_connect(db) == 1);
        mu_assert("db_disconnect()", db_disconnect(db) == 0);

        /* TODO: create database automatically with db_create() */
        mu_assert("Get test database from config", db = db->next);

        mu_assert("db_connect()", db_connect(db) == 0);
        mu_assert("Test database connection", db->conn != NULL);

        mu_assert("db_exec_sql() invalid sql returns failure",
                db_exec_sql(db, "invalidsql") != 0);
        mu_assert("db_exec_sql() BEGIN",
                db_exec_sql(db, "BEGIN;") == 0);
        mu_assert("db_exec_sql() CREATE TABLE",
                db_exec_sql(db,
                "CREATE TABLE IF NOT EXISTS test(id integer, name char(32));")
                == 0);
        mu_assert("db_exec_sql() INSERT",
                db_exec_sql(db,
                "INSERT INTO test(id, name) VALUES (0, 'boris');") == 0);
        mu_assert("db_exec_sql() COMMIT",
                db_exec_sql(db, "COMMIT;") == 0);
        mu_assert("db_exec_sql() BEGIN",
                db_exec_sql(db, "BEGIN;") == 0);
        mu_assert("db_exec_sql() DECLARE CURSOR",
                db_exec_sql(db,
                "DECLARE testcursor CURSOR FOR SELECT * FROM test;") == 0);

        mu_assert("db_fetch_all() FETCH ALL",
                db_fetch_all(db, "testcursor") == 0);

        mu_assert("db_exec_sql() ROLLBACK",
                db_exec_sql(db, "ROLLBACK;") == 0);

        mu_assert("db_disconnect()", db_disconnect(db) == 0);

        return 0;
}
