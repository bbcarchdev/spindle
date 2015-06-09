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
static char *spindle_db_id_(const char *localname);
static char *spindle_db_literalset_(struct spindle_literalset_struct *set);
static char *spindle_db_strset_(struct spindle_strset_struct *set);
static size_t spindle_db_esclen_(const char *src);
static char *spindle_db_escstr_(char *dest, const char *src);
static char *spindle_db_escstr_lower_(char *dest, const char *src);
#endif

#if SPINDLE_DB_INDEX
static int spindle_db_remove_(SQL *sql, const char *id);
static int spindle_db_langindex_(SQL *sql, const char *id, const char *target, const char *specific, const char *generic);
#endif

#if SPINDLE_DB_PROXIES
struct relate_struct
{
	SPINDLE *spindle;
	const char *id;
	const char *uri;
};


static int spindle_db_perform_proxy_relate_(SQL *restrict db, void *restrict userdata);
#endif

#if SPINDLE_DB_INDEX

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
 *
 * The 'about' table tracks topicality references:
 *
 *  id       The UUID of an entity being cached
 *  about    The UUID of another reference this entity is about
 *
 * The 'media' table tracks information about media items (digital assets):
 *
 *  id       The UUID of the digital asset
 *  uri      The complete URI of the digital asset
 *  class    The DCMIT member for this asset (:Image, :Sound, etc.)
 *  type     The MIME type of the asset
 *  audience If known to be restricted, the URI of the audience it's
 *           restricted to.
 */

int
spindle_db_cache_store(SPINDLECACHE *data)
{
	SQL *sql;
	const char *t;
	char *id, *title, *desc, *classes;
	char lbuf[64];

	sql = data->spindle->db;
	id = spindle_db_id_(data->localname);
	if(!id)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": DB: ID is '%s'\n", id);
	if(spindle_db_remove_(sql, id))
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
	if(spindle_db_langindex_(sql, id, "en_gb", "en-gb", "en"))
	{
		free(id);
		return -1;
	}
	if(spindle_db_langindex_(sql, id, "cy_gb", "cy-gb", "cy"))
	{
		free(id);
		return -1;
	}
	if(spindle_db_langindex_(sql, id, "ga_gb", "ga-gb", "ga"))
	{
		free(id);
		return -1;
	}
	if(spindle_db_langindex_(sql, id, "gd_gb", "gd-gb", "gd"))
	{
		free(id);
		return -1;
	}
	free(id);
	return 0;
}

static int
spindle_db_remove_(SQL *sql, const char *id)
{
	if(sql_executef(sql, "DELETE FROM \"index\" WHERE \"id\" = %Q",
					id))
	{
		return -1;
	}
	if(sql_executef(sql, "DELETE FROM \"about\" WHERE \"id\" = %Q",
					id))
	{
		return -1;
	}
	if(sql_executef(sql, "DELETE FROM \"media\" WHERE \"id\" = %Q",
					id))
	{
		return -1;
	}
	return 0;
}

static int
spindle_db_langindex_(SQL *sql, const char *id, const char *target, const char *specific, const char *generic)
{
	if(specific)
	{
		if(sql_executef(sql, "UPDATE \"index\" SET "
						"\"index_%s\" = setweight(to_tsvector(coalesce(\"title\" -> '%s', \"title\" -> '%s', \"title\" -> '_', '')), 'A') || "
						" setweight(to_tsvector(coalesce(\"description\" -> '%s', \"description\" -> '%s', \"description\" -> '_', '')), 'B')  "
						"WHERE \"id\" = %Q", target, specific, generic, specific, generic, id))
		{
			return -1;
		}
		return 0;
	}
	if(sql_executef(sql, "UPDATE \"index\" SET "
					"\"index_%s\" = setweight(to_tsvector(coalesce(\"title\" -> '%s', \"title\" -> '_', '')), 'A') || "
					" setweight(to_tsvector(coalesce(\"description\" -> '%s', \"description\" -> '_', '')), 'B')  "
					"WHERE \"id\" = %Q", target, generic, generic, id))
	{
		return -1;
	}
	return 0;
}

#endif


#if SPINDLE_DB_PROXIES

char *
spindle_db_proxy_locate(SPINDLE *spindle, const char *uri)
{
	SQL_STATEMENT *rs;
	char *buf, *p;
	const char *s;
	
	rs = sql_queryf(spindle->db, "SELECT \"id\" FROM \"proxy\" WHERE %Q = ANY(\"sameas\")", uri);
	if(!rs)
	{
		return NULL;
	}
	if(sql_stmt_eof(rs))
	{
		return NULL;
	}
    /* root + '/' + uuid + '#id' + NUL */
	buf = (char *) malloc(strlen(spindle->root) + 1 + 32 + 3 + 1);
	if(!buf)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for proxy URI\n");
		sql_stmt_destroy(rs);
		return NULL;
	}
	strcpy(buf, spindle->root);
	p = strchr(buf, 0);
	if(p > buf)
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
	for(s = sql_stmt_str(rs, 0); *s; s++)
	{
		if(isalnum(*s))
		{
			*p = tolower(*s);
			p++;
		}
	}
	strcpy(p, "#id");
	return buf;
}

/* Store a relationship between a proxy and a processed entity */
int
spindle_db_proxy_relate(SPINDLE *spindle, const char *remote, const char *local)
{
	char *id;
	struct relate_struct data;

	id = spindle_db_id_(local);
	if(!id)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": DB: adding %s = <%s>\n", id, remote);
	data.spindle = spindle;
	data.id = id;
	data.uri = remote;
	if(sql_perform(spindle->db, spindle_db_perform_proxy_relate_, (void *) &data, -1, SQL_TXN_CONSISTENT))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to relate <%s> to %s\n", remote, id);
		free(id);
		return -1;
	}
	free(id);
	return 0;
}

/* Obtain all of the outbound references from a proxy */
char **
spindle_db_proxy_refs(SPINDLE *spindle, const char *uri)
{
	SQL_STATEMENT *rs;
	char *id;
	char **refset;
	size_t count;

	id = spindle_db_id_(uri);
	if(!id)
	{
		return NULL;
	}
	
	rs = sql_queryf(spindle->db, "SELECT unnest(\"sameas\") AS \"uri\" FROM \"proxy\" WHERE \"id\" = %Q", id);
	free(id);
	if(!rs)
	{
		return NULL;
	}
	for(count = 0; !sql_stmt_eof(rs); sql_stmt_next(rs))
	{
		count++;
	}
	refset = (char **) calloc(count + 1, sizeof(char *));
	if(!refset)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for reference-set\n");		
		sql_stmt_destroy(rs);
		return NULL;
	}
	sql_stmt_rewind(rs);
	for(count = 0; !sql_stmt_eof(rs); sql_stmt_next(rs))
	{
		refset[count] = strdup(sql_stmt_str(rs, 0));
		if(!refset[count])
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate reference <%s>\n", sql_stmt_str(rs, 0));
			for(count--; ; count--)
			{
				free(refset[count]);
				if(!count)
				{
					break;
				}
			}
			free(refset);
			sql_stmt_destroy(rs);
			return NULL;
		}
		count++;
	}
	sql_stmt_destroy(rs);
	return refset;
}

/* Move a set of references from one proxy to another */
int
spindle_db_proxy_migrate(SPINDLE *spindle, const char *from, const char *to, char **refs)
{
	char *oldid, *newid;

	(void) refs;

	oldid = spindle_db_id_(from);
	newid = spindle_db_id_(to);
	if(!oldid || !newid)
	{
		free(oldid);
		free(newid);
		return -1;
	}
	/* Transaction */
	sql_executef(spindle->db, "UPDATE \"proxy\" SET \"sameas\" = \"sameas\" || ( SELECT \"sameas\" FROM \"proxy\" WHERE \"id\" = %Q ) WHERE \"id\" = %Q", oldid, newid); 
	sql_executef(spindle->db, "DELETE FROM \"proxy\" WHERE \"id\" = %Q", oldid);
	free(oldid);
	free(newid);
	return 0;
}

static int
spindle_db_perform_proxy_relate_(SQL *restrict db, void *restrict userdata)
{
	struct relate_struct *data;
	SQL_STATEMENT *rs;

	data = (struct relate_struct *) userdata;
	rs = sql_queryf(db, "SELECT \"id\" FROM \"proxy\" WHERE \"id\" = %Q", data->id);
	if(!rs)
	{
		return -2;
	}
	if(sql_stmt_eof(rs))
	{
		if(sql_executef(db, "INSERT INTO \"proxy\" (\"id\", \"sameas\") VALUES (%Q, ARRAY[]::text[])", data->id))
		{
			sql_stmt_destroy(rs);
			return -2;
		}
	}
	sql_stmt_destroy(rs);
	if(sql_executef(db, "UPDATE \"proxy\" SET \"sameas\" = array_append(\"sameas\", %Q) WHERE \"id\" = %Q", data->uri, data->id))
	{
		return -2;
	}
	return 1;
}

#endif /* SPINDLE_DB_PROXIES */

#if SPINDLE_DB_INDEX || SPINDLE_DB_PROXIES

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
		if(set->literals[c].lang[0])
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
		if(set->literals[c].lang[0])
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

static char *
spindle_db_id_(const char *localname)
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
	return id;
}

#endif /* SPINDLE_DB_INDEX || SPINDLE_DB_PROXIES */
