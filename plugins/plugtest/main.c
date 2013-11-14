/* 
 * Example plugin code
 *
 * main.c
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "main.h"
#include "plugin.h"

/* meat of the plugin */
enum xmlstatus action()
{
        return xml_parse(buf);
}

int main()
{
        int err = 3;
        read_input(stdin);
        enum xmlstatus result = action();
        write_output(stdout, result);
        switch (result) {
        case XML_STATUS_OK:
                err = HTTP_OK;
        case XML_STATUS_INVALID:
                err = HTTP_BAD_REQUEST;
        case XML_STATUS_UNKNOWN:
                err = HTTP_INTERNAL_SERVER_ERROR;
        default:
                err = HTTP_INTERNAL_SERVER_ERROR;
        }
        return err;
}
