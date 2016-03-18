/* Spindle: Co-reference aggregation engine
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2016 BBC
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

struct relate_struct
{
	SPINDLE *spindle;
	const char *id;
	const char *uri;
};

static int spindle_db_perform_proxy_relate_(SQL *restrict db, void *restrict userdata);

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
		sql_stmt_destroy(rs);
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
	sql_stmt_destroy(rs);
	return buf;
}

/* Store a relationship between a proxy and a processed entity */
int
spindle_db_proxy_relate(SPINDLE *spindle, const char *remote, const char *local)
{
	char *id;
	struct relate_struct data;

	id = spindle_db_id(local);
	if(!id)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": DB: adding %s = <%s>\n", id, remote);
	data.spindle = spindle;
	data.id = id;
	data.uri = remote;
	if(spindle_db_perform_proxy_relate_(spindle->db, (void *) &data) < 0)
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

	id = spindle_db_id(uri);
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
	SQL_STATEMENT *rs;
	
	(void) refs;

	oldid = spindle_db_id(from);
	newid = spindle_db_id(to);
	if(!oldid || !newid)
	{
		free(oldid);
		free(newid);
		return -1;
	}
	/* Transaction */
	rs = sql_queryf(spindle->db, "SELECT * FROM \"moved\" WHERE \"from\" = %Q", from);
	if(sql_stmt_eof(rs))
	{
		sql_executef(spindle->db, "INSERT INTO \"moved\" (\"from\", \"to\") VALUES (%Q, %Q)", from, to);
	}
	else
	{
		sql_executef(spindle->db, "UPDATE \"moved\" SET \"to\" = %Q WHERE \"from\" = %Q", to, from);
	}
	sql_stmt_destroy(rs);
	sql_executef(spindle->db, "UPDATE \"proxy\" SET \"sameas\" = \"sameas\" || ( SELECT \"sameas\" FROM \"proxy\" WHERE \"id\" = %Q ) WHERE \"id\" = %Q", oldid, newid); 
	sql_executef(spindle->db, "DELETE FROM \"proxy\" WHERE \"id\" = %Q", oldid);
	sql_executef(spindle->db, "DELETE FROM \"index\" WHERE \"id\" = %Q", oldid);	
	sql_executef(spindle->db, "UPDATE \"triggers\" SET \"triggerid\" = %Q WHERE \"triggerid\" = %Q", newid, oldid);
	sql_executef(spindle->db, "UPDATE \"triggers\" SET \"id\" = %Q WHERE \"id\" = %Q", newid, oldid);
	sql_executef(spindle->db, "UPDATE \"audiences\" SET \"id\" = %Q WHERE \"id\" = %Q", newid, oldid);
	sql_executef(spindle->db, "UPDATE \"licenses_audiences\" SET \"id\" = %Q WHERE \"id\" = %Q", newid, oldid);
	sql_executef(spindle->db, "UPDATE \"licenses_audiences\" SET \"audienceid\" = %Q WHERE \"audienceid\" = %Q", newid, oldid);
	sql_executef(spindle->db, "UPDATE \"media\" SET \"id\" = %Q WHERE \"id\" = %Q", newid, oldid);
	sql_executef(spindle->db, "UPDATE \"membership\" SET \"id\" = %Q WHERE \"id\" = %Q", newid, oldid);
	sql_executef(spindle->db, "UPDATE \"membership\" SET \"collection\" = %Q WHERE \"collection\" = %Q", newid, oldid);
	sql_executef(spindle->db, "UPDATE \"index_media\" SET \"id\" = %Q WHERE \"id\" = %Q", newid, oldid);
	sql_executef(spindle->db, "UPDATE \"index_media\" SET \"media\" = %Q WHERE \"media\" = %Q", newid, oldid);
	sql_executef(spindle->db, "UPDATE \"media\" SET \"id\" = %Q WHERE \"id\" = %Q", newid, oldid);
	sql_executef(spindle->db, "UPDATE \"about\" SET \"id\" = %Q WHERE \"id\" = %Q", newid, oldid);
	sql_executef(spindle->db, "UPDATE \"about\" SET \"about\" = %Q WHERE \"about\" = %Q", newid, oldid);
	spindle_db_proxy_state_(spindle, newid, 1);
	sql_executef(spindle->db, "DELETE FROM \"state\" WHERE \"id\" = %Q", oldid);
	free(oldid);
	free(newid);
	return 0;
}

/* Ensure there's an entry in the state table for this proxy */
int
spindle_db_proxy_state_(SPINDLE *spindle, const char *id, int changed)
{
	SQL_STATEMENT *rs;
	char skey[16], tbuf[20];
	uint32_t shortkey;
	time_t t;
	struct tm tm;
	
	rs = sql_queryf(spindle->db, "SELECT \"id\" FROM \"state\" WHERE \"id\" = %Q", id);
	if(!rs)
	{
		return -2;
	}
	strncpy(skey, id, 8);
	skey[8] = 0;
	shortkey = (uint32_t) strtoul(skey, NULL, 16);
	t = time(NULL);
	gmtime_r(&t, &tm);
	strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm);
	if(sql_stmt_eof(rs))
	{
		if(sql_executef(spindle->db, "INSERT INTO \"state\" (\"id\", \"shorthash\", \"tinyhash\", \"status\", \"modified\", \"flags\") VALUES (%Q, '%lu', '%d', %Q, %Q, 0)",
			id, (unsigned long) shortkey, (int) (shortkey % 256), "DIRTY", tbuf
			))
		{
			sql_stmt_destroy(rs);
			return -2;
		}
		sql_stmt_destroy(rs);
		return 1;
	}
	if(changed)
	{
		if(sql_executef(spindle->db, "UPDATE \"state\" SET \"status\" = %Q, \"flags\" = 0, \"modified\" = %Q WHERE \"id\" = %Q",
			"DIRTY", tbuf, id))
		{
			return -2;
		}
		return 1;
	}
	return 0;
}

static int
spindle_db_perform_proxy_relate_(SQL *restrict db, void *restrict userdata)
{
	struct relate_struct *data;
	SQL_STATEMENT *rs;
	int r;
	
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
	/* Update any indexes which refer to this URI */
	if(sql_executef(db, "UPDATE \"triggers\" SET \"triggerid\" = %Q WHERE \"uri\" = %Q", data->id, data->uri))
	{
		return -2;
	}
	if(sql_executef(db, "UPDATE \"audiences\" SET \"id\" = %Q WHERE \"uri\" = %Q", data->id, data->uri))
	{
		return -2;
	}
	if(sql_executef(db, "UPDATE \"licenses_audiences\" SET \"audienceid\" = %Q WHERE \"uri\" = %Q", data->id, data->uri))
	{
		return -2;
	}
	r = spindle_db_proxy_state_(data->spindle, data->id, 1);
	if(r < 0)
	{
		return -2;
	}
	return 1;
}
