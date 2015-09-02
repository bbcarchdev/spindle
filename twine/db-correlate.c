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

#if SPINDLE_DB_PROXIES
struct relate_struct
{
	SPINDLE *spindle;
	const char *id;
	const char *uri;
};

static int spindle_db_perform_proxy_relate_(SQL *restrict db, void *restrict userdata);
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
	/* XXX the fragment should be configurable */
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

/* Fetch all of the source data about the entities that relate to this
 * proxy
 */
int
spindle_db_cache_source(SPINDLECACHE *data)
{
	char **refs;
	int r;
	size_t c;

	refs = spindle_db_proxy_refs(data->spindle, data->localname);
	if(!refs)
	{
		return -1;
	}
	r = 0;
	for(c = 0; refs[c]; c++)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": DB: fetching data for <%s>\n", refs[c]);
		if(sparql_queryf_model(data->spindle->sparql, data->sourcedata,
							   "SELECT DISTINCT ?s ?p ?o ?g\n"
							   " WHERE {\n"
							   "  GRAPH ?g {\n"
							   "   ?s ?p ?o .\n"
							   "   FILTER(?s = <%s>)\n"
							   "  }\n"
							   "}",
							   refs[c]))
		{
			r = -1;
			break;
		}
	}
	
	for(c = 0; refs[c]; c++)
	{
		free(refs[c]);
	}
	free(refs);
	return r;
}

#endif /* SPINDLE_DB_PROXIES */

