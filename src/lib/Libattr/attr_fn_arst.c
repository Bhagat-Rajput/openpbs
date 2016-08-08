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
#include <pbs_config.h>   /* the master config generated by configure */

#include <assert.h>
#include <ctype.h>
#include <memory.h>
#ifndef NDEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "pbs_error.h"


/**
 * @file	attr_fn_arst.c
 * @brief
 * 	This file contains general function for attributes of type
 * 	array of (pointers to) strings
 * @details
 * 	Each set has functions for:
 *		Decoding the value string to the machine representation.
 *		Encoding the internal representation of the attribute to external
 *		Setting the value by =, + or - operators.
 *		Comparing a (decoded) value with the attribute value.
 *		Freeing the space malloc-ed to the attribute value.
 *
 * 	Some or all of the functions for an attribute type may be shared with
 * 	other attribute types.
 *
 * 	The prototypes are declared in "attribute.h"
 *
 * 	Attribute functions for attributes with value type "array of strings":
 *
 * 	The "encoded" or external form of the value is a string with the orginial
 * 	strings separated by commas (or new-lines) and terminated by a null.
 * 	Any embedded commas or back-slashes must be escaped by a prefixed back-
 * 	slash.
 *
 * 	The "decoded" form is a set of strings pointed to by an array_strings struct
 */

/**
 * @brief
 *	decode a comma string into an attribute of type ATR_TYPE_ARST
 *
 * @par Functionality:
 *	1. Call count_substrings to find out the number of sub strings separated by comma
 *	2. Call parse_comma_string function to parse the value of the attribute
 *
 * @see
 *
 * @param[in/out] patr	-	Pointer to attribute structure
 * @param[in]	  val	-	Value of the attribute as comma separated string. This parameter's value
 *				cannot be modified by any of the functions that are called inside this function.
 *
 * @return	int
 * @retval	0  -  Success
 * @retval	>0 -  Failure
 *
 *
 */
static int
decode_arst_direct(struct attribute *patr, char *val)
{
	unsigned long		 bksize;
	int			 j;
	int			 ns;
	char			*pbuf = NULL;
	char			*pc;
	char			*pstr;
	struct array_strings	*stp = NULL;
	int			 rc;
	char			 strbuf[BUF_SIZE];	/* Should handle most values */
	char			*sbufp = NULL;
	size_t			 slen;

	if (!patr || !val)
		return (PBSE_INTERNAL);

	/*
	 * determine number of sub strings, each sub string is terminated
	 * by a non-escaped comma or a new-line, the whole string is terminated
	 * by a null
	 */

	if ((rc = count_substrings(val, &ns)) != 0)
		return (rc);

	slen = strlen(val);

	pbuf  = (char *)malloc(slen + 1);
	if (pbuf == (char *)0)
		return (PBSE_SYSTEM);
	bksize = ((ns-1) * sizeof(char *)) + sizeof(struct array_strings);
	stp = (struct array_strings *)malloc(bksize);
	if (!stp) {
		free(pbuf);
		return (PBSE_SYSTEM);
	}

	/* number of slots (sub strings) */
	stp->as_npointers = ns;
	stp->as_usedptr = 0;
	/* for the strings themselves */
	stp->as_buf     = pbuf;
	stp->as_next    = pbuf;
	stp->as_bufsize = slen + 1;

	/*
	 * determine string storage requirement and copy the string "val"
	 * to a work buffer area
	 */

	if (slen < BUF_SIZE)
		/* buffer on stack */
		sbufp = strbuf;
	else {
		/* buffer on heap */
		if ((sbufp = (char *)malloc(slen + 1)) == NULL) {
			free(pbuf);
			free(stp);
			return (PBSE_SYSTEM);
		}
	}

	strncpy(sbufp, val, slen);
	sbufp[slen] = '\0';

	/* now copy in substrings and set pointers */
	pc = pbuf;
	j = 0;
	pstr = parse_comma_string(sbufp);
	while ((pstr != (char *)0) && (j < ns)) {
		stp->as_string[j] = pc;
		while (*pstr) {
			*pc++ = *pstr++;
		}
		*pc++ = '\0';
		pstr = parse_comma_string((char *)0);
		j++;
	}

	stp->as_usedptr = j;
	stp->as_next = pc;
	patr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY | ATR_VFLAG_MODCACHE;
	patr->at_val.at_arst = stp;

	if (sbufp != strbuf)	/* buffer on heap, not stack */
		free(sbufp);
	return (0);
}

/**
 * @brief
 * 	decode_arst - decode a comma string into an attr of type ATR_TYPE_ARST
 *
 * @param[in] patr - pointer to attribute structure
 * @param[in] name - attribute name 
 * @param[in] rescn - resource name
 * @param[in] val - attribute value
 *
 * @return	int
 * @retval	0	if ok
 * @retval	>0	error number1 if error
 * @retval	patr	members set
 *
 */

int
decode_arst(struct attribute *patr, char *name, char *rescn, char *val)
{
	int	  rc;
	attribute temp;

	if ((val == (char *)0) || (strlen(val) == 0)) {
		free_arst(patr);
		/* _SET cleared in free_arst */
		patr->at_flags |= ATR_VFLAG_MODIFY | ATR_VFLAG_MODCACHE;

		return (0);
	}

	if ((patr->at_flags & ATR_VFLAG_SET) && (patr->at_val.at_arst)) {
		/* already have values, decode new into temp	*/
		/* then use set(incr) to add new to existing	*/

		temp.at_flags   = 0;
		temp.at_type    = ATR_TYPE_ARST;
		temp.at_user_encoded = NULL;
		temp.at_priv_encoded = NULL;
		temp.at_val.at_arst = 0;
		if ((rc = decode_arst_direct(&temp, val)) != 0)
			return (rc);
		rc = set_arst(patr, &temp, SET);
		free_arst(&temp);
		return (rc);

	} else {
		/* decode directly into real attribute */
		return (decode_arst_direct(patr, val));
	}
}

/**
 * @brief
 * 	encode_arst - encode attr of type ATR_TYPE_ARST into attrlist entry
 *
 * Mode ATR_ENCODE_CLIENT - encode strings into single super string
 *			    separated by ','
 *
 * Mode ATR_ENCODE_SVR    - treated as above
 *
 * Mode ATR_ENCODE_MOM    - treated as above
 *
 * Mode ATR_ENCODE_HOOK   - treated as above
 *
 * Mode ATR_ENCODE_SAVE - encode strings into single super string
 *			  separated by '\n'
 *
 * @param[in] attr - ptr to attribute to encode
 * @param[in] phead - ptr to head of attrlist list
 * @param[in] atname - attribute name
 * @param[in] rsname - resource name or null
 * @param[in] mode - encode mode 
 * @param[out] rtnl - ptr to svrattrl 
 *
 * @retval	int
 * @retval	>0	if ok, entry created and linked into list
 * @retval	=0	no value to encode, entry not created
 * @retval	-1	if error
 *
 */

/*ARGSUSED*/

int
encode_arst(attribute *attr, pbs_list_head *phead, char *atname, char *rsname, int mode, svrattrl **rtnl)
{
	char	 *end;
	int	  i;
	int	  j;
	svrattrl *pal;
	char	 *pc;
	char	 *pfrom;
	char	  separator;


	if (!attr)
		return (-2);
	if (((attr->at_flags & ATR_VFLAG_SET) == 0) || !attr->at_val.at_arst ||
		!attr->at_val.at_arst->as_usedptr)
		return (0);	/* no values */


	i = (int)(attr->at_val.at_arst->as_next - attr->at_val.at_arst->as_buf);
	if (mode == ATR_ENCODE_SAVE) {
		separator = '\n';	/* new-line for encode_acl  */
		/* allow one extra byte for final new-line */
		j = i + 1;
	} else {
		separator = ',';	/* normally a comma is the separator */
		j = i;
	}

	/* in the future will need to expand the size of value	*/
	/* if we are escaping special characters		*/

	pal = attrlist_create(atname, rsname, j);
	if (pal == (svrattrl *)0)
		return (-1);

	pal->al_flags = attr->at_flags;

	pc    = pal->al_value;
	pfrom = attr->at_val.at_arst->as_buf;

	/* replace nulls between sub-strings with separater characters */
	/* in the futue we need to escape any embedded special character */

	end = attr->at_val.at_arst->as_next;
	while (pfrom < end) {
		if (*pfrom == '\0') {
			*pc = separator;
		} else {
			*pc = *pfrom;
		}
		pc++;
		pfrom++;
	}

	/* convert the last null to separator only if going to new-lines */

	if (mode == ATR_ENCODE_SAVE)
		*pc = '\0';	/* insure string terminator */
	else
		*(pc-1) = '\0';
	if (phead)
		append_link(phead, &pal->al_link, pal);
	if (rtnl)
		*rtnl = pal;

	return (1);
}

/**
 * @brief
 * 	set_arst - set value of attribute of type ATR_TYPE_ARST to another
 *
 *	A=B --> set of strings in A replaced by set of strings in B
 *	A+B --> set of strings in B appended to set of strings in A
 *	A-B --> any string in B found is A is removed from A
 *
 * @param[in]   attr - pointer to new attribute to be set (A)
 * @param[in]   new  - pointer to attribute (B)
 * @param[in]   op   - operator
 *
 * @return      int
 * @retval      0       if ok
 * @retval     >0       if error
 *
 */

int
set_arst(struct attribute *attr, struct attribute *new, enum batch_op op)
{
	int	 i;
	int	 j;
	unsigned long nsize;
	unsigned long need;
	long	 offset;
	char	*pc;
	long	 used;
	struct array_strings *newpas;
	struct array_strings *pas;
	struct array_strings *xpasx;
	void free_arst(attribute *);

	assert(attr && new && (new->at_flags & ATR_VFLAG_SET));

	pas = attr->at_val.at_arst;
	xpasx = new->at_val.at_arst;
	if (!xpasx)
		return (PBSE_INTERNAL);

	if (!pas) {

		/* not array_strings control structure, make one */

		j = xpasx->as_npointers;
		if (j < 1)
			return (PBSE_INTERNAL);
		need = sizeof(struct array_strings) + (j-1) * sizeof(char *);
		pas=(struct array_strings *)malloc(need);
		if (!pas)
			return (PBSE_SYSTEM);
		pas->as_npointers = j;
		pas->as_usedptr = 0;
		pas->as_bufsize = 0;
		pas->as_buf     = (char *)0;
		pas->as_next    = (char *)0;
		attr->at_val.at_arst = pas;
	}
	if ((op == INCR) && !pas->as_buf)
		op = SET;	/* no current strings, change op to SET */

	/*
	 * At this point we know we have a array_strings struct initialized
	 */

	switch (op) {
		case SET:

			/*
			 * Replace old array of strings with new array, this is
			 * simply done by deleting old strings and appending the
			 * new (to the null set).
			 */

			for (i=0; i< pas->as_usedptr; i++)
				pas->as_string[i] = (char *)0;	/* clear all pointers */
			pas->as_usedptr = 0;
			pas->as_next   = pas->as_buf;

			if (new->at_val.at_arst == (struct array_strings *)0)
				break;	/* none to set */

			nsize = xpasx->as_next - xpasx->as_buf; /* space needed */
			if (nsize > pas->as_bufsize) {		 /* new wont fit */
				if (pas->as_buf)
					free(pas->as_buf);
				nsize += nsize / 2;			/* alloc extra space */
				if (!(pas->as_buf = malloc(nsize))) {
					pas->as_bufsize = 0;
					return (PBSE_SYSTEM);
				}
				pas->as_bufsize = nsize;

			} else {			/* str does fit, clear buf */
				(void)memset(pas->as_buf, 0, pas->as_bufsize);
			}

			pas->as_next = pas->as_buf;

			/* No break, "SET" falls into "INCR" to add strings */

		case INCR:

			nsize = xpasx->as_next - xpasx->as_buf;   /* space needed */
			used = pas->as_next - pas->as_buf;

			if (nsize > (pas->as_bufsize - used)) {
				need = pas->as_bufsize + 2 * nsize;  /* alloc new buf */
				if (pas->as_buf)
					pc = realloc(pas->as_buf, need);
				else
					pc = malloc(need);
				if (pc == (char *)0)
					return (PBSE_SYSTEM);
				offset = pc - pas->as_buf;
				pas->as_buf   = pc;
				pas->as_next = pc + used;
				pas->as_bufsize = need;

				for (j=0; j<pas->as_usedptr; j++)	/* adjust points */
					pas->as_string[j] += offset;
			}

			j = pas->as_usedptr + xpasx->as_usedptr;
			if (j > pas->as_npointers) {	/* need more pointers */
				j = 3 * j / 2;		/* allocate extra     */
				need = sizeof(struct array_strings) + (j-1) * sizeof(char *);
				newpas=(struct array_strings *)realloc((char *)pas, need);
				if (newpas == (struct array_strings *)0)
					return (PBSE_SYSTEM);
				newpas->as_npointers = j;
				pas = newpas;
				attr->at_val.at_arst = pas;
			}

			/* now append new strings */

			for (i=0; i<xpasx->as_usedptr; i++) {
				(void)strcpy(pas->as_next, xpasx->as_string[i]);
				pas->as_string[pas->as_usedptr++] = pas->as_next;
				pas->as_next += strlen(pas->as_next) + 1;
			}
			break;

		case DECR:	/* decrement (remove) string from array */
			for (j=0; j<xpasx->as_usedptr; j++) {
				for (i=0; i<pas->as_usedptr; i++) {
					if (!strcmp(pas->as_string[i], xpasx->as_string[j])) {
						/* compact buffer */
						nsize = strlen(pas->as_string[i]) + 1;
						pc = pas->as_string[i] + nsize;
						need = pas->as_next - pc;
						(void)memcpy(pas->as_string[i], pc, (int)need);
						pas->as_next -= nsize;
						/* compact pointers */
						for (++i; i < pas->as_npointers; i++)
							pas->as_string[i-1] = pas->as_string[i] - nsize;
						pas->as_string[i-1] = (char *)0;
						pas->as_usedptr--;
						break;
					}
				}
			}
			break;

		default:	return (PBSE_INTERNAL);
	}
	attr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY | ATR_VFLAG_MODCACHE;
	return (0);
}

/**
 * @brief
 * 	comp_arst - compare two attributes of type ATR_TYPE_ARST
 *
 * @param[in] attr - pointer to attribute structure
 * @param[in] with - pointer to attribute structure
 *
 * @return	int
 * @retval	0	if the set of strings in "with" is a subset of "attr"
 * @retval	1	otherwise
 *
 */

int
comp_arst(struct attribute *attr, struct attribute *with)
{
	int	i;
	int	j;
	int	match = 0;
	struct array_strings *apa;
	struct array_strings *bpb;

	if (!attr || !with || !attr->at_val.at_arst || !with->at_val.at_arst)
		return (1);
	if ((attr->at_type != ATR_TYPE_ARST) ||
		(with->at_type != ATR_TYPE_ARST))
		return (1);
	apa = attr->at_val.at_arst;
	bpb = with->at_val.at_arst;

	for (j=0; j<bpb->as_usedptr; j++) {
		for (i=0; i<apa->as_usedptr; i++) {
			if (strcmp(bpb->as_string[j], apa->as_string[i]) == 0) {
				match++;
				break;
			}
		}
	}

	if (match == bpb->as_usedptr)
		return (0);	/* all in "with" are in "attr" */
	else
		return (1);
}
 
/**
 * @brief
 *	frees arst attribute.
 *
 * @param[in] attr - pointer to attribute
 *
 * @return	Void
 *
 */

void
free_arst(struct attribute *attr)
{
	if ((attr->at_flags & ATR_VFLAG_SET) && (attr->at_val.at_arst)) {
		(void)free(attr->at_val.at_arst->as_buf);
		(void)free((char *)attr->at_val.at_arst);
	}
	free_null(attr);
}


/**
 * @brief
 * 	arst_string - see if a string occurs as a prefix in an arst attribute entry
 *	Search each entry in the value of an arst attribute for a sub-string
 *	that begins with the passed string
 *
 * @param[in] pattr - pointer to attribute structure
 * @param[in] str - string which is prefix to be searched in an attribute
 *
 * @return	string
 * @retval	arst attribute		Success
 * @retval	NULL			Failure
 *
 */

char *
arst_string(char *str, attribute *pattr)
{
	int    i;
	size_t len;
	struct array_strings *parst;

	if ((pattr->at_type != ATR_TYPE_ARST) || !(pattr->at_flags & ATR_VFLAG_SET))
		return ((char *)0);

	len = strlen(str);
	parst = pattr->at_val.at_arst;
	for (i = 0; i < parst->as_usedptr; i++) {
		if (strncmp(str, parst->as_string[i], len) == 0)
			return (parst->as_string[i]);
	}
	return ((char *)0);
}

/**
 * @brief
 * 	parse_comma_string_bs() - parse a string of the form:
 *		value1 [, value2 ...]
 *
 *	For use by decode_arst_direct_bs(), the old 8.0 version.
 *	On the first call, start is non null, a pointer to the first value
 *	element upto a comma, new-line, or end of string is returned.
 *
 *	Commas escaped by a back-slash '\' are ignored.
 *
 *	Newlines (\n) are allowed because they could be present in
 *	environment variables.
 *
 *	On any following calls with start set to a null pointer (char *)0,
 *	the next value element is returned...
 *
 * @param[in] start - string to be parsed
 *
 * @return 	string
 * @retval	start address for string	Success
 * @retval	NULL				Failure
 *
 */	

static char *
parse_comma_string_bs(char *start)
{
	static char *pc = NULL;	/* if start is null, restart from here */
	char	    *dest;
	char	    *back;
	char	    *rv;

	if (start != (char *)0)
		pc = start;

	/* skip over leading white space */
	while (pc && *pc && isspace((int)*pc))
		pc++;

	if (!pc || !*pc)
		return ((char *)0);	/* already at end, no strings */

	rv = dest = pc;	/* the start point which will be returned */

	/* find comma */
	while (*pc) {
		if (*pc == '\\') {
			/*
			 * Both copy_env_value() and encode_arst_bs() escape certain
			 * characters. Unescape them here.
			 */
			pc++;
			if (*pc == '\0') {
				/* should not happen, but handle it */
				break;
			} else if ((*pc == '"') || (*pc == '\'') || (*pc == ',') || (*pc == '\\')) {
				/* omit the backslash preceding these characters */
				*dest = *pc;
			} else {
				/* unrecognized escape sequence, just copy it */
				*dest++ = '\\';
				*dest = *pc;
			}
		} else if (*pc == ',') {
			break;
		} else {
			*dest = *pc;
		}
		++pc;
		++dest;
	}

	if (*pc)
		*pc++ = '\0';	/* if not end, terminate this and adv past */

	*dest = '\0';
	back = dest;
	while (isspace((int)*--back))	/* strip trailing spaces */
		*back = '\0';

	return (rv);
}

/**
 * @brief
 * 	count_substrings_bs - counts number of substrings in a comma separated
 * 	string.
 *
 * 	Newlines (\n) are allowed because they could be present in environment
 * 	variables.
 *
 *	See also count_substrings
 */
int
count_substrings_bs(val, pcnt)
char	*val;		/*comma separated string of substrings*/
int	*pcnt;		/*where to return the value*/

{
	int	rc = 0;
	int	ns;
	char   *pc;

	if (val == (char *)0)
		return  (PBSE_INTERNAL);

	/*
	 * determine number of substrings, each sub string is terminated
	 * by a non-escaped comma or a new-line, the whole string is terminated
	 * by a null
	 */

	ns = 1;
	for (pc = val; *pc; pc++) {
		if (*pc == '\\') {
			if (*(pc+1))
				pc++;
		} else {
			if (*pc == ',')
				++ns;
		}
	}
	pc--;
	if (*pc == ',') {
		ns--;		/* strip any trailing null string */
		*pc = '\0';
	}

	*pcnt = ns;
	return rc;
}


/**
 * @brief
 *	decode_arst_direct_bs - this version of the decode routine treats
 *	back-slashes (hence the "_bs" on the name) as escape characters
 *	It is needed to deal with environment variables that contain a
 *	comma and was taken from the 8.0 version.
 *
 * @param[in] patr - pointer to attribute structure
 * @param[in] val - string holding value for attribute structure
 *
 * @retuan	int
 * @retval	0	Success
 * @retval	>0	PBSE error code
 *
 */
static int
decode_arst_direct_bs(struct attribute *patr, char *val)
{
	unsigned long		 bksize;
	int			 j;
	int			 ns;
	int			 rc;
	size_t			 slen;
	char			*pbuf = NULL;
	char			*pc;
	char			*pstr;
	char			*sbufp = NULL;
	struct array_strings	*stp = NULL;
	char			 strbuf[BUF_SIZE];	/* Should handle most values */

	if (!patr || !val)
		return (PBSE_INTERNAL);

	/*
	 * determine number of sub strings, each sub string is terminated
	 * by a non-escaped comma, the whole string is terminated by a null
	 */

	if ((rc = count_substrings_bs(val, &ns)) != 0)
		return (rc);

	slen = strlen(val);

	pbuf  = (char *)malloc(slen + 1);
	if (pbuf == (char *)0)
		return (PBSE_SYSTEM);
	bksize = (ns-1) * sizeof(char *) + sizeof(struct array_strings);
	stp = (struct array_strings *)malloc(bksize);
	if (!stp) {
		free(pbuf);
		return (PBSE_SYSTEM);
	}
	/* number of slots (sub strings) */
	stp->as_npointers = ns;
	stp->as_usedptr = 0;
	/* for the strings themselves */
	stp->as_buf     = pbuf;
	stp->as_next    = pbuf;
	stp->as_bufsize = slen + 1;

	/*
	 * determine string storage requirement and copy the string "val"
	 * to a work buffer area
	 */

	if (slen < BUF_SIZE)
		/* buffer on stack */
		sbufp = strbuf;
	else {
		/* buffer on heap */
		if ((sbufp = (char *)malloc(slen + 1)) == NULL) {
			free(pbuf);
			free(stp);
			return (PBSE_SYSTEM);
		}
	}

	strncpy(sbufp, val, slen);
	sbufp[slen] = '\0';

	/* now copy in substrings and set pointers */
	pc = pbuf;
	j  = 0;
	pstr = parse_comma_string_bs(sbufp);
	while ((pstr != (char *)0) && (j < ns)) {
		stp->as_string[j] = pc;
		while (*pstr) {
			*pc++ = *pstr++;
		}
		*pc++ = '\0';
		pstr = parse_comma_string_bs((char *)0);
		j++;
	}

	stp->as_usedptr = j;
	stp->as_next = pc;
	patr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY | ATR_VFLAG_MODCACHE;
	patr->at_val.at_arst = stp;

	if (sbufp != strbuf)	/* buffer on heap, not stack */
		free(sbufp);
	return (0);
}

/**
 * @brief
 * 	decode_arst_bs - decode a comma string into an attr of type ATR_TYPE_ARST
 *	Calls decode_arst_direct_bs() instead of decode_arst_direct() for
 *	environment variables that may contain commas
 *
 *
 * @param[in] patr - ptr to attribute to decode
 * @param[in] name - attribute name
 * @param[in] rescn - resource name or null
 * @param[out] val - string holding values for attribute structure 
 *
 * @retval      int
 * @retval      0	if ok
 * @retval      >0	error number1 if error,
 * @retval      *patr 	members set
 *
 */

int
decode_arst_bs(struct attribute *patr, char *name, char *rescn, char *val)
{
	int	  rc;
	attribute temp;

	if ((val == (char *)0) || (strlen(val) == 0)) {
		free_arst(patr);
		/* _SET cleared in free_arst */
		patr->at_flags |= ATR_VFLAG_MODIFY | ATR_VFLAG_MODCACHE;

		return (0);
	}

	if ((patr->at_flags & ATR_VFLAG_SET) && (patr->at_val.at_arst)) {
		/* already have values, decode new into temp	*/
		/* then use set(incr) to add new to existing	*/

		temp.at_flags   = 0;
		temp.at_type    = ATR_TYPE_ARST;
		temp.at_user_encoded = NULL;
		temp.at_priv_encoded = NULL;
		temp.at_val.at_arst = 0;
		if ((rc = decode_arst_direct_bs(&temp, val)) != 0)
			return (rc);
		rc = set_arst(patr, &temp, SET);
		free_arst(&temp);
		return (rc);

	} else {
		/* decode directly into real attribute */
		return (decode_arst_direct_bs(patr, val));
	}
}

/**
 * @brief
 * 	encode_arst_bs - encode attr of type ATR_TYPE_ARST into attrlist entry
 *	Used in conjunction with decode_arst_bs() and decode_arst_direct_bs()
 *	for environment variables that may contain commas.
 *
 * Mode ATR_ENCODE_CLIENT - encode strings into single super string
 *			    separated by ','
 *
 * Mode ATR_ENCODE_SVR    - treated as above
 *
 * Mode ATR_ENCODE_MOM    - treated as above
 *
 * Mode ATR_ENCODE_HOOK   - treated as above
 *
 * Mode ATR_ENCODE_SAVE - encode strings into single super string
 *			  separated by '\n'
 *
 * @param[in] attr - ptr to attribute to encode
 * @param[in] phead - ptr to head of attrlist list
 * @param[in] atname - attribute name
 * @param[in] rsname - resource name or null
 * @param[in] mode - encode mode
 * @param[out] rtnl - ptr to svrattrl
 *
 * @retval      int
 * @retval      >0      if ok, entry created and linked into list
 * @retval      =0      no value to encode, entry not created
 * @retval      -1      if error
 *
 */
/*ARGSUSED*/

int
encode_arst_bs(attribute *attr, pbs_list_head *phead, char *atname, char *rsname, int mode, svrattrl **rtnl)
{
	char	 *end;
	int	  i;
	int	  j;
	svrattrl *pal;
	char	 *pc;
	char	 *pfrom;
	char	  separator;


	if (!attr)
		return (-2);
	if (((attr->at_flags & ATR_VFLAG_SET) == 0) || !attr->at_val.at_arst ||
		!attr->at_val.at_arst->as_usedptr)
		return (0);	/* no values */


	i = (int)(attr->at_val.at_arst->as_next - attr->at_val.at_arst->as_buf);
	if (mode == ATR_ENCODE_SAVE) {
		separator = '\n';	/* new-line for encode_acl  */
		/* allow one extra byte for final new-line */
		j = i + 1;
	} else {
		separator = ',';	/* normally a comma is the separator */
		j = i;
	}

	/* how many back-slashes are required */

	for (pc = attr->at_val.at_arst->as_buf; pc < attr->at_val.at_arst->as_next; ++pc) {
		if ((*pc == '"') || (*pc == '\'') || (*pc == ',') || (*pc == '\\'))
			++j;
	}
	pal = attrlist_create(atname, rsname, j);
	if (pal == (svrattrl *)0)
		return (-1);

	pal->al_flags = attr->at_flags;

	pc    = pal->al_value;
	pfrom = attr->at_val.at_arst->as_buf;

	/*
	 * replace nulls between sub-strings with separater characters
	 * escape any embedded special character
	 *
	 * Keep the list of special characters consistent with copy_env_value()
	 * and parse_comma_string_bs().
	 */

	end = attr->at_val.at_arst->as_next;
	while (pfrom < end) {
		if ((*pfrom == '"') || (*pfrom == '\'') || (*pfrom == ',') || (*pfrom == '\\')) {
			*pc++ = '\\';
			*pc   = *pfrom;
		} else if (*pfrom == '\0') {
			*pc = separator;
		} else {
			*pc = *pfrom;
		}
		pc++;
		pfrom++;
	}

	/* convert the last null to separator only if going to new-lines */

	if (mode == ATR_ENCODE_SAVE)
		*pc = '\0';	/* terminate string */
	else
		*(pc-1) = '\0';
	append_link(phead, &pal->al_link, pal);
	if (rtnl)
		*rtnl = pal;

	return (1);
}

/**
 * @brief
 * set_arst_uniq - set value of attribute of type ATR_TYPE_ARST to another
 * discarding duplicate entries on the INCR operation.
 *
 * @par Functionality:
 * Set value of one attribute of type ATR_TYPE_ARST to another discarding
 * duplicate entries on the INCR operator. For example:
 *	(A B C) + (D B E) = (A B C D E)
 *
 * The operators are:
 *   SET   A=B --> set of strings in A replaced by set of strings in B
 *	Done by clearing A and then setting A = A + B
 *   INCR  A+B --> set of strings in B appended to set of strings in A
 *	except no duplicates are appended
 *   DECR  A-B --> any string in B found in A is removed from A
 *	Done via basic set_arst() function
 *
 * @param[in]	attr - pointer to new attribute to be set (A)
 * @param[in]	new  - pointer to attribute (B)
 * @param[in]	op   - operator
 *
 * @return	int
 * @retval	 PBSE_NONE on success
 * @retval	 PBSE_* on error
 *
 * @par Side Effects: None
 *
 * @par MT-safe: unknown
 *
 */

int
set_arst_uniq(struct attribute *attr, struct attribute *new, enum batch_op op)
{
	int	 i;
	int	 j;
	unsigned long nsize;
	unsigned long need;
	long	 offset;
	char	*pc;
	long	 used;
	struct array_strings *newpas;
	struct array_strings *pas;
	struct array_strings *xpasx;
	void free_arst(attribute *);

	assert(attr && new && (new->at_flags & ATR_VFLAG_SET));

	/* if the operation is DECR, just use the normal set_arst() function */
	if (op == DECR)
		return (set_arst(attr, new, op));

	pas = attr->at_val.at_arst;	/* old attribute, A */
	xpasx = new->at_val.at_arst;	/* new attribute, B */
	if (!xpasx)
		return (PBSE_INTERNAL);

	/* if the operation is SET, free the existing and then INCR (add) new */
	if (op == SET) {
		free_arst(attr);  /* clear old and use INCR to set w/o dups */
		pas = NULL;	  /* just freed what it was point to */
		op = INCR;
	}

	if (!pas) {

		/* not array_strings control structure, make one */

		j = xpasx->as_npointers;
		if (j < 1)
			return (PBSE_INTERNAL);
		need = sizeof(struct array_strings) + (j-1) * sizeof(char *);
		pas=(struct array_strings *)malloc(need);
		if (!pas)
			return (PBSE_SYSTEM);
		pas->as_npointers = j;
		pas->as_usedptr = 0;
		pas->as_bufsize = 0;
		pas->as_buf     = (char *)0;
		pas->as_next   = (char *)0;
		attr->at_val.at_arst = pas;
	}

	/*
	 * At this point we know we have a array_strings struct initialized
	 * and we are doing the equivalent of the INCR operation
	 */

	nsize = xpasx->as_next - xpasx->as_buf;   /* space needed */
	used = pas->as_next - pas->as_buf;

	if (nsize > (pas->as_bufsize - used)) {
		need = pas->as_bufsize + 2 * nsize;  /* alloc new buf */
		if (pas->as_buf)
			pc = realloc(pas->as_buf, need);
		else
			pc = malloc(need);
		if (pc == (char *)0)
			return (PBSE_SYSTEM);
		offset = pc - pas->as_buf;
		pas->as_buf   = pc;
		pas->as_next = pc + used;
		pas->as_bufsize = need;

		if (offset != 0) {
			for (j=0; j<pas->as_usedptr; j++)  /* adjust points */
				pas->as_string[j] += offset;
		}
	}

	j = pas->as_usedptr + xpasx->as_usedptr;
	if (j > pas->as_npointers) {	/* need more pointers */
		j = 3 * j / 2;		/* allocate extra     */
		need = sizeof(struct array_strings) + (j-1) * sizeof(char *);
		newpas=(struct array_strings *)realloc((char *)pas, need);
		if (newpas == (struct array_strings *)0)
			return (PBSE_SYSTEM);
		newpas->as_npointers = j;
		pas = newpas;
		attr->at_val.at_arst = pas;
	}

	/* now append new strings ingoring enties already present  */

	for (i=0; i<xpasx->as_usedptr; i++) {
		for (j=0; j<pas->as_usedptr; ++j) {
			if (strcasecmp(xpasx->as_string[i], pas->as_string[j]) == 0)
				break;
		}
		if (j == pas->as_usedptr) {
			/* didn't find this there already, so copy it in */

			(void)strcpy(pas->as_next, xpasx->as_string[i]);
			pas->as_string[pas->as_usedptr++] = pas->as_next;
			pas->as_next += strlen(pas->as_next) + 1;
		}
	}

	attr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY | ATR_VFLAG_MODCACHE;
	return (0);
}

/**
 * @brief 
 *	check for duplicate entries in a string array
 *
 * @param[in] strarr - the string array to check for duplicates
 *
 * @retval 0 - no duplicate entries found
 * @retval 1 - duplicate entries found
 *
 */

int
check_duplicates(struct array_strings *strarr)
{
	int i, j;

	if (strarr == NULL)
		return 0;

	for (i=0; i < strarr->as_usedptr; i++) {
		for (j=i+1; j < strarr->as_usedptr; j++) {
			if (strcmp(strarr->as_string[i],
				strarr->as_string[j]) == 0)
				return 1;
		}
	}
	return 0;
}