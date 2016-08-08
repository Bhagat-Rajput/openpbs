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
 * @file	parse_at.c
 */
#include <pbs_config.h>   /* the master config generated by configure */

#include "cmds.h"
#include "pbs_ifl.h"



#ifdef WIN32	/* we're including the space character under windows */
#define ISNAMECHAR(x) ((isprint(x)) && ((x) != '#') && ((x) != '@'))
#else
#define ISNAMECHAR(x) ((isgraph(x)) && ((x) != '#') && ((x) != '@'))
#endif

struct hostlist {
	char host[PBS_MAXHOSTNAME+1];
	struct hostlist *next;
};

/** @fn int parse_at_item(char *at_item, char *at_name, char *host_name)
 * @brief	parse a single name[@host] item and return name and host
 *
 * @param[in]	at_item
 * @param[out]	at_name
 * @param[out]	host_name
 *
 * @return	int
 * @retval	1	success
 * @retval	0	parsing failure

 * @par MT-Safe:	yes
 * @par Side Effects:
 *	None
 *
 * @par Note:
 *	This function requires caller to provide output parameters with
 *	required memory allocated.  Checks in this function are removed
 *	for speed.
 */
int
parse_at_item(char *at_item, char *at_name, char *host_name)
{
	char *c;
	int a_pos = 0;
	int h_pos = 0;

	/* Begin the parse */
	c = at_item;
	while (isspace(*c))
		c++;

	/* Looking for something before the @ sign */
	while (*c != '\0') {
		if (ISNAMECHAR(*c)) {
			if (a_pos >= MAXPATHLEN)
				return 1;
			at_name[a_pos++]=*c;
		} else
			break;
		c++;
	}
	if (a_pos == 0)
		return 1;

	/* Looking for a server */
	if (*c == '@') {
		c++;
		while (*c != '\0') {
			if (ISNAMECHAR(*c)) {
				if (h_pos >= PBS_MAXSERVERNAME)
					return 1;
				host_name[h_pos++]=*c;
			} else
				break;
			c++;
		}
		if (h_pos == 0)
			return 1;
	}

	if (*c != '\0')
		return 1;

	/* set null chars at the end of the string */
	at_name[a_pos] = '\0';
	host_name[h_pos] = '\0';

	return (0);
}

/** @fn int parse_at_list(char *list, int use_count, int abs_path)
 * @brief	parse a comma-separated list of name[@host] items
 *
 * @param[in]	list
 * @param[out]	use_count	if true, make sure no host is repeated
 *				in the list, and host is defaulted only
 *				once
 * @param[out]	abs_path	if true, make sure the item appears to
 *				begin with an absolute path name
 *
 * @return	int
 * @retval	1	parsing failure
 * @retval	0	success

 * @par MT-Safe:	no
 * @par Side Effects:
 *	exits with return code 1 on memory allocation failure
 */
int
parse_at_list(char *list, int use_count, int abs_path)
{
	char *b, *c, *s, *l;
	int rc = 0;
	char user[MAXPATHLEN+1];
	char host[PBS_MAXSERVERNAME];
	struct hostlist *ph, *nh, *hostlist = (struct hostlist *)0;

	if ((list == NULL) || (*list == '\0'))
		return 1;

#ifdef WIN32
	back2forward_slash(list);        /* "\" translate to "/" for path */
#endif

	if ((l = strdup(list)) == NULL) {
		fprintf(stderr, "Out of memory.\n");
		exit(1);
	}

	for (c = l; *c != '\0'; rc = 0) {
		rc = 1;

		/* Drop leading white space */
		while (isspace(*c))
			c++;

		/* If requested, is this an absolute path */
		if (abs_path && !is_full_path(c))
			break;

		/* Find the next comma */
		for (s = c; *c && *c != ','; c++)
			;

		/* Drop any trailing blanks */
		for (b = c - 1; isspace(*b); b--)
			*b = '\0';

		/* Make sure the list does not end with a comma */
		if (*c == ',') {
			*c++ = '\0';
			if (*c == '\0')
				break;
		}

		/* Parse the individual list item */
		if (parse_at_item(s, user, host))
			break;

		/* The user part must be given */
		if (*user == '\0')
			break;

		/* If requested, make sure the host name is not repeated */
		if (use_count) {
			ph = hostlist;
			while (ph) {
				if (strcmp(ph->host, host) == 0)
					goto duplicate;
				ph = ph->next;
			}
			nh = (struct hostlist *) malloc(sizeof(struct hostlist));
			if (nh == NULL) {
				fprintf(stderr, "Out of memory\n");
				exit(1);
			}
			nh->next = hostlist;
			strcpy(nh->host, host);
			hostlist = nh;
		}
	}
duplicate:

	/* Release memory for hostlist and argument list */
	ph = hostlist;
	while (ph) {
		nh = ph->next;
		free(ph);
		ph = nh;
	}
	free(l);

	return rc;
}