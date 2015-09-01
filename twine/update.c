/* Spindle: Co-reference aggregation engine
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2015 BBC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_spindle.h"

#if SPINDLE_DB_INDEX || SPINDLE_DB_PROXIES
static int spindle_update_all_(SPINDLE *spindle);
#endif

/* Process a message containing Spindle proxy URIs by passing them to the
 * update handler.
 */
int
spindle_process_uri(const char *mime, const unsigned char *buf, size_t buflen, void *data)
{
	SPINDLE *spindle;
	char *str, *t;
	int r;

	(void) mime;

	spindle = (SPINDLE *) data;

	/* Impose a hard limit on URL lengths */
	if(buflen > 1024)
	{
		buflen = 1024;
	}
	str = (char *) calloc(1, buflen + 1);
	if(!str)
	{
		return -1;
	}
	memcpy((void *) str, (void *) buf, buflen);
	str[buflen] = 0;
	t = strchr(str, '\n');
	if(t)
	{
		*t = 0;
	}
	if(!strcasecmp(str, "all"))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": special value 'all' is not valid as part of an update message\n");
		free(str);
		return -1;
	}
	r = spindle_update(PLUGIN_NAME, str, data);
	free(str);
	return r;
}

/* Update handler: re-build the cached contents of the item with the supplied
 * identifier (which may be a UUID or a complete URI)
 *
 * When invoked using twine -u SPINDLE <ID>, and using an RDBMS index, the
 * special value 'all' is valid to trigger a re-build of all known proxies.
 */
int
spindle_update(const char *name, const char *identifier, void *data)
{
	SPINDLE *spindle;
	char uuid[40];
	char *idbuf, *p;
	int c, r;
	const char *t;

	(void) name;

	spindle = (SPINDLE *) data;
	idbuf = NULL;

	if(!strcasecmp(identifier, "all"))
	{
#if SPINDLE_DB_INDEX || SPINDLE_DB_PROXIES
		if(spindle->db)
		{			
			return spindle_update_all_(spindle);
		}
#endif
		twine_logf(LOG_CRIT, PLUGIN_NAME ": can only update all items when using the a relational database index\n");
		return -1;
	}
	/* If identifier is a string of 32 hex-digits, possibly including hyphens,
	 * then we can prefix it with the root and suffix it with #id to form
	 * a real identifer.
	 */
	c = 0;
	for(t = identifier; *t && c < 32; t++)
	{
		if(isxdigit(*t))
		{
			uuid[c] = tolower(*t);
			c++;
		}
		else if(*t == '-')
		{
			continue;
		}
		else
		{
			break;
		}
	}
	uuid[c] = 0;
	if(!*t && c == 32)
	{
		/* XXX the fragment should be configurable */
		idbuf = (char *) malloc(strlen(spindle->root) + 1 + 32 + 3 + 1);
		if(!idbuf)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for local identifier\n");
			return -1;
		}
		strcpy(idbuf, spindle->root);
		p = strchr(idbuf, 0);
		if(p > idbuf)
		{
			p--;
			if(*p == '/')
			{
				p++;
			}
			else
			{
				p++;
				*p = '/';
				p++;
			}
		}
		else
		{
			*p = '/';
			p++;
		}
		strcpy(p, uuid);
		p += 32;
		*p = '#';
		p++;
		*p = 'i';
		p++;
		*p = 'd';
		p++;
		*p = 0;
		identifier = idbuf;
	}
	r = spindle_cache_update(spindle, identifier, NULL);
	if(r)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": update failed for <%s>\n", identifier);
	}
	else
	{
		twine_logf(LOG_NOTICE, PLUGIN_NAME ": update complete for <%s>\n", identifier);
	}
	free(idbuf);
	return r;
}

#if SPINDLE_DB_INDEX || SPINDLE_DB_PROXIES
static int
spindle_update_all_(SPINDLE *spindle)
{
	SQL_STATEMENT *rs;
	size_t l, c;
	int r;
	char *buf, *p;
	const char *t;

	l = strlen(spindle->root);
	buf = (char *) malloc(l + 1 + 32 + 4);
	if(!buf)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for URIs\n");
		return -1;
	}
	strcpy(buf, spindle->root);
	p = buf + l;
	/* Ensure there's a trailing slash */
	if(l)
	{
		p--;
		if(*p == '/')
		{
			p++;
		}
		else
		{
			p++;
			*p = '/';
			p++;
			l++;
		}
	}
	else
	{
		*p = '/';
		p++;
		l++;
	}
#if SPINDLE_DB_PROXIES
	rs = sql_query(spindle->db, "SELECT \"id\" FROM \"proxy\"");
#else
	twine_logf(LOG_WARN, PLUGIN_NAME ": only existing cached entries can be updated because database-based proxies are not available\n");
	rs = sql_query(spindle->db, "SELECT \"id\" FROM \"index\"");
#endif
	if(!rs)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to query for item UUIDs\n");
		free(buf);
		return -1;
	}
	r = 0;
	for(; !sql_stmt_eof(rs); sql_stmt_next(rs))
	{
		p = buf + l;
		t = sql_stmt_str(rs, 0);
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": will update {%s}\n", t);
		if(!t)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to obtain value from column\n");
			r = -1;
			break;
		}
		if(strlen(t) > 36)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": invalid UUID '%s' in database column\n", t);
			r = -1;
			break;
		}
		c = 0;
		for(; *t && c < 32; t++)
		{
			if(isxdigit(*t))
			{
				*p = tolower(*t);
				c++;
				p++;
			}
		}
		*p = '#';
		p++;
		*p = 'i';
		p++;
		*p = 'd';
		p++;
		*p = 0;
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": URI is <%s>\n", buf);
		r = spindle_cache_update(spindle, buf, NULL);
		if(r)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": update failed for <%s>\n", buf);
			break;
		}
		else
		{
			twine_logf(LOG_NOTICE, PLUGIN_NAME ": update complete for <%s>\n", buf);
		}		
	}
	sql_stmt_destroy(rs);
	free(buf);
	return r;
}

#endif /* SPINDLE_DB_INDEX */
