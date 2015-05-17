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

#if SPINDLE_DB_INDEX

static char *spindle_db_literalset_(struct spindle_literalset_struct *set);
static char *spindle_db_strset_(struct spindle_strset_struct *set);
static size_t spindle_db_esclen_(const char *src);
static char *spindle_db_escstr_(char *dest, const char *src);
static char *spindle_db_escstr_lower_(char *dest, const char *src);

/* Store an index entry in a PostgreSQL (or compatible) database for a proxy.
 *
 * The 'index' table has a core set of fields:-
 *
 *  id       The proxy's UUID
 *  version  The index version (SPINDLE_DB_INDEX_VERSION)
 *  modified The last-modification date
 *  classes  A list of class URIs for this proxy
 *  score    The calculated prominence score for this proxy
 *
 * In addition, the index stores:
 *
 *  title        (v1) An hstore of language/title pairs
 *  description  (v1) An hstore of language/description pairs
 *  coordinates  (v1) A point specifying latitude/longitude
 *  index_en_gb  (v1) The en-gb (British English) full-text index
 *  index_cy_gb  (v1) The cy-gb (Welsh) full-text index
 *  index_gd_gb  (v1) The gd-gb (Scottish Gaelic) full-text index
 *  index_ga_gb  (v1) The ga-gb (Irish Gaelic) full-text index
 */
int
spindle_db_cache_store(SPINDLECACHE *data)
{
	SQL *sql;
	const char *t;
	char *id, *p, *title, *desc, *classes;
	char lbuf[64];

	sql = data->spindle->db;
	id = (char *) calloc(1, strlen(data->localname) + 1);
	if(!id)
	{
		return -1;
	}
	if(!(t = strrchr(data->localname, '/')))
	{
		t = data->localname;
	}
	for(p = id; *t; t++)
	{
		if(isalnum(*t))
		{
			*p = *t;
			p++;
		}
		else if(*t == '#')
		{
			break;
		}
	}
	*p = 0;
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": DB: ID is '%s'\n", id);
	if(sql_executef(sql, "DELETE FROM \"index\" WHERE \"id\" = %Q",
					id))
	{
		free(id);
		return -1;
	}
	title = spindle_db_literalset_(&(data->titleset));
	desc = spindle_db_literalset_(&(data->descset));
	classes = spindle_db_strset_(data->classes);
	if(data->has_geo)
	{
		snprintf(lbuf, sizeof(lbuf), "(%f, %f)", data->lat, data->lon);
		t = lbuf;
	}
	else
	{
		t = NULL;
	}
	if(sql_executef(sql, "INSERT INTO \"index\" (\"id\", \"version\", \"modified\", \"score\", \"title\", \"description\", \"coordinates\", \"classes\") VALUES (%Q, %d, now(), %d, %Q, %Q, %Q, %Q)",
					id, SPINDLE_DB_INDEX_VERSION, data->score, title, desc, t, classes))
	{
		free(id);
		free(title);
		free(desc);
		free(classes);
		return -1;
	}
	free(title);
	free(desc);
	free(classes);
	if(sql_executef(sql, "UPDATE \"index\" SET "
					"\"index_en_gb\" = setweight(to_tsvector(coalesce(\"title\" -> 'en-gb', \"title\" -> 'en', \"title\" -> '_')), 'A') || "
					" setweight(to_tsvector(coalesce(\"description\" -> 'en-gb', \"description\" -> 'en', \"description\" -> '_')), 'B')  "
					"WHERE \"id\" = %Q", id))
	{
		free(id);
		return -1;
	}
	if(sql_executef(sql, "UPDATE \"index\" SET "
					"\"index_cy_gb\" = setweight(to_tsvector(coalesce(\"title\" -> 'cy-gb', \"title\" -> 'cy', \"title\" -> '_')), 'A') || "
					" setweight(to_tsvector(coalesce(\"description\" -> 'cy-gb', \"description\" -> 'cy', \"description\" -> '_')), 'B')  "
					"WHERE \"id\" = %Q", id))
	{
		free(id);
		return -1;
	}
	if(sql_executef(sql, "UPDATE \"index\" SET "
					"\"index_ga_gb\" = setweight(to_tsvector(coalesce(\"title\" -> 'ga-gb', \"title\" -> 'ga', \"title\" -> '_')), 'A') || "
					" setweight(to_tsvector(coalesce(\"description\" -> 'ga-gb', \"description\" -> 'ga', \"description\" -> '_')), 'B')  "
					"WHERE \"id\" = %Q", id))
	{
		free(id);
		return -1;
	}
	if(sql_executef(sql, "UPDATE \"index\" SET "
					"\"index_gd_gb\" = setweight(to_tsvector(coalesce(\"title\" -> 'gd-gb', \"title\" -> 'gd', \"title\" -> '_')), 'A') || "
					" setweight(to_tsvector(coalesce(\"description\" -> 'gd-gb', \"description\" -> 'gd', \"description\" -> '_')), 'B')  "
					"WHERE \"id\" = %Q", id))
	{
		free(id);
		return -1;
	}
	free(id);
	return 0;
}

static char *
spindle_db_literalset_(struct spindle_literalset_struct *set)
{
	size_t c, nbytes;
	char *str, *p;

	nbytes = 1;
	for(c = 0; c < set->nliterals; c++)
	{
		/* "lang"=>"string", */
		nbytes += 7;
		if(set->literals[c].lang)
		{
			nbytes += spindle_db_esclen_(set->literals[c].lang);
		}
		else
		{
			/* We use an underscore to indicate no language */
			nbytes++;
		}
		nbytes += spindle_db_esclen_(set->literals[c].str);
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
		if(set->literals[c].lang)
		{
			p = spindle_db_escstr_lower_(p, set->literals[c].lang);
		}
		else
		{
			*p = '_';
			p++;
		}
		strcpy(p, "\"=>\"");
		p += 4;
		p = spindle_db_escstr_(p, set->literals[c].str);
		*p = '"';
		p++;
	}
	*p = 0;
	return str;
}

static char *
spindle_db_strset_(struct spindle_strset_struct *set)
{
	size_t c, nbytes;
	char *str, *p;

	nbytes = 3;
	for(c = 0; c < set->count; c++)
	{
		/* "string", */
		nbytes += 3;
		nbytes += spindle_db_esclen_(set->strings[c]);
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
		p = spindle_db_escstr_(p, set->strings[c]);
		*p = '"';
		p++;
	}
	*p = '}';
	p++;
	*p = 0;
	return str;
}

static size_t
spindle_db_esclen_(const char *src)
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

static char *
spindle_db_escstr_(char *dest, const char *src)
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

static char *
spindle_db_escstr_lower_(char *dest, const char *src)
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

#endif


#if SPINDLE_DB_PROXIES
/* Not currently used or implemented */

char *
spindle_db_proxy_locate(SPINDLE *spindle, const char *uri)
{
	SQL_STATEMENT *rs;

	rs = sql_queryf(spindle->db, "SELECT \"id\" FROM \"proxy\" WHERE %V = ANY(\"sameas\")", uri);
	/* ... */
}

/* Store a relationship between a proxy and a processed entity */
int
spindle_db_proxy_relate(SPINDLE *spindle, const char *remote, const char *local)
{
	
	/* Ensure proxy exists */
	sql_executef(spindle->db, "UPDATE \"proxy\" SET \"sameas\" = array_append(\"sameas\", %V) WHERE \"id\" = %V", remote, uuid);
	
}

/* Obtain all of the outbound references from a proxy */
char **
spindle_db_proxy_refs(SPINDLE *spindle, const char *uri)
{
	SQL_STATEMENT *rs;

	rs = sql_queryf(spindle->db, "SELECT unnest(\"sameas\") AS \"uri\" FROM \"proxy\" WHERE \"id\" = %V", uuid);
}

/* Move a set of references from one proxy to another */
int
spindle_db_proxy_migrate(SPINDLE *spindle, const char *from, const char *to, char **refs)
{
	/* Ensure new proxy exists */
	sql_executef("UPDATE \"proxy\" SET \"sameas\" = \"sameas\" || ( SELECT \"sameas\" FROM \"proxy\" WHERE \"id\" = %V ) WHERE \"id\" = %V", olduuid, newuuid); 
	sql_executef("DELETE FROM \"proxy\" WHERE \"id\" = %V", olduuid);
	return 0;
}

#endif /* SPINDLE_DB_PROXIES */

