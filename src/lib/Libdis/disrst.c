/*
 * Copyright (C) 1994-2016 Altair Engineering, Inc.
 * For more information, contact Altair at www.altair.com.
 *  
 * This file is part of the PBS Professional ("PBS Pro") software.
 * 
 * Open Source License Information:
 *  
 * PBS Pro is free software. You can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free 
 * Software Foundation, either version 3 of the License, or (at your option) any 
 * later version.
 *  
 * PBS Pro is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Affero General Public License for more details.
 *  
 * You should have received a copy of the GNU Affero General Public License along 
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 * Commercial License Information: 
 * 
 * The PBS Pro software is licensed under the terms of the GNU Affero General 
 * Public License agreement ("AGPL"), except where a separate commercial license 
 * agreement for PBS Pro version 14 or later has been executed in writing with Altair.
 *  
 * Altair’s dual-license business model allows companies, individuals, and 
 * organizations to create proprietary derivative works of PBS Pro and distribute 
 * them - whether embedded or bundled with other software - under a commercial 
 * license agreement.
 * 
 * Use of Altair’s trademarks, including but not limited to "PBS™", 
 * "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's 
 * trademark licensing policies.
 *
 */
/**
 * @file	disrst.c
 *
 * @par Synopsis:
 *	char *disrst(int stream, int *retval)
 *
 *	Gets a Data-is-Strings character string from <stream> and converts it
 *	into a null-terminated string, and returns a pointer to the result.  The
 *	character string in <stream> consists of an unsigned integer, followed
 *	by a number of characters determined by the unsigned integer.
 *
 *	*<retval> gets DIS_SUCCESS if everything works well.  It gets an error
 *	code otherwise.  In case of an error, the <stream> character pointer is
 *	reset, making it possible to retry with some other conversion strategy.
 *	In case of an error, the return value is NULL.
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#ifndef NDEBUG
#include <string.h>
#endif

#include "dis.h"
#include "dis_.h"

/**
 * @brief
 *      Gets a Data-is-Strings signed integer from <stream>, converts it
 *      into a null-terminated string, and returns a pointer to the result.
 *
 * @param[in] stream - pointer to data stream
 * @param[out] retval - return value
 *
 * @return      string
 * @retval      converted value         success
 * @retval      0                       error
 *
 */

char *
disrst(int stream, int *retval)
{
	int		locret;
	int		negate;
	unsigned	count;
	char		*value = NULL;

	assert(retval != NULL);
	assert(dis_gets != NULL);
	assert(disr_commit != NULL);

	locret = disrsi_(stream, &negate, &count, 1, 0);
	if (locret == DIS_SUCCESS) {
		if (negate)
			locret = DIS_BADSIGN;
		else {
			value = (char *)malloc((size_t)count+1);
			if (value == NULL)
				locret = DIS_NOMALLOC;
			else {
				if ((*dis_gets)(stream, value,
					(size_t)count) != (size_t)count)
					locret = DIS_PROTO;
#ifndef NDEBUG
				else if (memchr(value, 0, (size_t)count))
					locret = DIS_NULLSTR;
#endif
				else
					value[count] = '\0';
			}
		}
	}
	locret = ((*disr_commit)(stream, locret == DIS_SUCCESS) < 0) ?
		DIS_NOCOMMIT : locret;
	if ((*retval = locret) != DIS_SUCCESS && value != NULL) {
		free(value);
		value = NULL;
	}
	return (value);
}