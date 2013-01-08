/* 
 * config_test.h
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

#ifndef __GLADD_CONFIG_TEST_H__
#define __GLADD_CONFIG_TEST_H__ 1

char *test_config_skip_comment();
char *test_config_skip_blank();
char *test_config_invalid_line();
char *test_config_open_success();
char *test_config_open_fail();
char *test_config_default_debug_value();
char *test_config_default_port_value();
char *test_config_set_debug_value();
char *test_config_set_port_value();
char *test_config_read_url_static();
char *test_config_read_url_static_next();
char *test_config_add_acl_invalid();
char *test_config_acl_allow_all();

#endif /* __GLADD_CONFIG_TEST_H__ */
