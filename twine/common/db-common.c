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

static int spindle_db_querylog_(SQL *restrict sql, const char *query);
static int spindle_db_noticelog_(SQL *restrict sql, const char *notice);
static int spindle_db_errorlog_(SQL *restrict sql, const char *sqlstate, const char *message);

/* Initialise a Spindle database connection, if configured to use one */
int
spindle_db_init(SPINDLE *spindle)
{
	char *t;

	t = twine_config_geta("spindle:db", NULL);
	if(!t)
	{
		return 0;
	}
	spindle->db = sql_connect(t);
	if(!spindle->db)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to connect to database <%s>\n", t);
		free(t);
		return -1;
	}
	free(t);
	sql_set_querylog(spindle->db, spindle_db_querylog_);
	sql_set_errorlog(spindle->db, spindle_db_errorlog_);
	sql_set_noticelog(spindle->db, spindle_db_noticelog_);
	if(spindle_db_schema_update_(spindle))
	{
		return -1;
	}
	return 0;	
}

/* Clean up resources used by a Spindle database connection */
int
spindle_db_cleanup(SPINDLE *spindle)
{
	if(spindle->db)
	{
		sql_disconnect(spindle->db);
	}
	spindle->db = NULL;
	return 0;
}

char *
spindle_db_literalset(struct spindle_literalset_struct *set)
{
	size_t c, nbytes;
	char *str, *p;

	nbytes = 1;
	for(c = 0; c < set->nliterals; c++)
	{
		/* "lang"=>"string", */
		nbytes += 7;
		if(set->literals[c].lang[0])
		{
			nbytes += spindle_db_esclen(set->literals[c].lang);
		}
		else
		{
			/* We use an underscore to indicate no language */
			nbytes++;
		}
		nbytes += spindle_db_esclen(set->literals[c].str);
	}
	str = (char *) malloc(nbytes);
	if(!str)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate %lu bytes for literal set\n", (unsigned long) nbytes);
		return NULL;
	}
	p = str;
	for(c = 0; c < set->nliterals; c++)
	{
		if(c)
		{
			*p = ',';
			p++;
		}
		*p = '"';
		p++;
		if(set->literals[c].lang[0])
		{
			p = spindle_db_escstr_lower(p, set->literals[c].lang);
		}
		else
		{
			*p = '_';
			p++;
		}
		strcpy(p, "\"=>\"");
		p += 4;
		p = spindle_db_escstr(p, set->literals[c].str);
		*p = '"';
		p++;
	}
	*p = 0;
	return str;
}

char *
spindle_db_strset(struct spindle_strset_struct *set)
{
	size_t c, nbytes;
	char *str, *p;

	nbytes = 3;
	for(c = 0; c < set->count; c++)
	{
		/* "string", */
		nbytes += 3;
		nbytes += spindle_db_esclen(set->strings[c]);
	}
	str = (char *) malloc(nbytes);
	if(!str)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate %lu bytes for string set\n", (unsigned long) nbytes);
		return NULL;
	}
	p = str;
	*p = '{';
	p++;
	for(c = 0; c < set->count; c++)
	{
		if(c)
		{
			*p = ',';
			p++;
		}
		*p = '"';
		p++;
		p = spindle_db_escstr(p, set->strings[c]);
		*p = '"';
		p++;
	}
	*p = '}';
	p++;
	*p = 0;
	return str;
}

size_t
spindle_db_esclen(const char *src)
{
	size_t len;

	for(len = 0; *src; src++)
	{
		if(*src == '"' || *src == '\\')
		{
			/* Characters which require escaping need an extra byte */
			len++;
		}
		len++;
	}
	return len;
}

char *
spindle_db_escstr(char *dest, const char *src)
{
	for(; *src; src++)
	{
		if(*src == '"' || *src == '\\')
		{
			*dest = '\\';
			dest++;
		}
		*dest = *src;
		dest++;
	}
	return dest;
}

char *
spindle_db_escstr_lower(char *dest, const char *src)
{
	for(; *src; src++)
	{
		if(*src == '"' || *src == '\\')
		{
			*dest = '\\';
			dest++;
		}
		*dest = tolower(*src);
		dest++;
	}
	return dest;
}

int
spindle_db_local(SPINDLE *spindle, const char *localname)
{
	if(strncmp(localname, spindle->root, strlen(spindle->root)))
	{
		return 0;
	}
	return 1;
}

char *
spindle_db_id(const char *localname)
{
	char *id, *p;
	const char *t;

	id = (char *) calloc(1, strlen(localname) + 1);
	if(!id)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for ID\n");
		return NULL;
	}
	if(!(t = strrchr(localname, '/')))
	{
		t = id;
	}
	else
	{
		t++;
	}
	for(p = id; *t; t++)
	{
		if(isxdigit(*t))
		{
			*p = tolower(*t);
			p++;
		}
		else if(*t == '#')
		{
			break;
		}
		else
		{
			/* invalid character - this can't be a local UUID */
			free(id);
			return NULL;
		}
	}
	*p = 0;
	if(strlen(id) != 32)
	{
		/* the extracted UUID is the wrong length to be valid */
		free(id);
		return NULL;
	}
	return id;
}

static int
spindle_db_querylog_(SQL *restrict sql, const char *query)
{
	(void) sql;

	twine_logf(LOG_DEBUG, PLUGIN_NAME ": SQL: %s\n", query);
	return 0;
}

static int
spindle_db_noticelog_(SQL *restrict sql, const char *notice)
{
	(void) sql;

	twine_logf(LOG_NOTICE, PLUGIN_NAME ": %s\n", notice);
	return 0;
}

static int
spindle_db_errorlog_(SQL *restrict sql, const char *sqlstate, const char *message)
{
	(void) sql;

	twine_logf(LOG_ERR, PLUGIN_NAME ": [%s] %s\n", sqlstate, message);
	return 0;
}
