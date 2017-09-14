/* Spindle: Co-reference aggregation engine
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2017 BBC
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

struct spindle_create_struct
{
	SPINDLE *spindle;
	const char *uri1;
	const char *uri2;
	struct spindle_strset_struct *changeset;
	char id[36];
};

struct relate_struct
{
	SPINDLE *spindle;
	const char *id;
	const char *uri;
};

static int spindle_db_perform_proxy_create_(SQL *restrict db, void *restrict userdata);
static int spindle_db_perform_proxy_relate_(SQL *restrict db, void *restrict userdata);

int
spindle_db_proxy_create(SPINDLE *spindle, const char *uri1, const char *uri2, struct spindle_strset_struct *changeset)
{
	struct spindle_create_struct data;
	
	/* Perform the actual correlation within a transaction,
	 * and THEN update state if needed
	 */
	data.spindle = spindle;
	data.uri1 = uri1;
	data.uri2 = uri2;
	data.changeset = changeset;
	data.id[0] = 0;
	if(sql_perform(spindle->db, spindle_db_perform_proxy_create_, (void *) &data, -1, SQL_TXN_CONSISTENT) < 0)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": DB: failed to create proxy\n");
		return -1;
	}
	if(data.id[0])
	{
		/* Now update the index state if required */
		if(spindle_db_proxy_state_(spindle, data.id, 1))
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": DB: failed to update proxy state\n");
			return -1;
		}
	}
	return 0;
}

char *
spindle_db_proxy_locate(SPINDLE *spindle, const char *uri)
{
	SQL_STATEMENT *rs;
	char *buf, *p;
	const char *s;
	
	if(!uri)
	{
		return NULL;
	}
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
	char id[36];
	struct relate_struct data;

	if(spindle_db_id_copy(id, local))
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
		return -1;
	}
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
	rs = sql_queryf(spindle->db, "SELECT * FROM \"moved\" WHERE \"from\" = %Q", oldid);
	if(!rs)
	{
		free(oldid);
		free(newid);
		return -1;
	}
	if(sql_stmt_eof(rs))
	{
		sql_executef(spindle->db, "INSERT INTO \"moved\" (\"from\", \"to\") VALUES (%Q, %Q)", oldid, newid);
	}
	else
	{
		sql_executef(spindle->db, "UPDATE \"moved\" SET \"to\" = %Q WHERE \"from\" = %Q", oldid, newid);
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
spindle_db_perform_proxy_create_(SQL *restrict db, void *restrict userdata)
{
	SPINDLE *spindle;
	struct spindle_create_struct *data;
	char *u1, *u2, *uu;
	unsigned flags = SF_REFRESHED;

	(void) db;

	data = (struct spindle_create_struct *) userdata;
	spindle = data->spindle;
	data->id[0] = 0;
	/* Locate the existing identifiers for our URIs, if they exist */
	u1 = spindle_db_proxy_locate(spindle, data->uri1);
	u2 = spindle_db_proxy_locate(spindle, data->uri2);
	if(u1 && u2 && !strcmp(u1, u2))
	{
		/* The coreference already exists */
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": DB: <%s> <=> <%s> already exists\n", data->uri1, data->uri2);
		if(data->changeset)
		{
			spindle_strset_add_flags(data->changeset, u1, flags);
		}
		spindle_db_id_copy(data->id, u1);
		/* No database changes needed */
		free(u1);
		free(u2);
		return SQL_TXN_ROLLBACK;
	}
	else if(!data->uri2 && u1)
	{
		/* The lone subject already exists */
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": DB: <%s> already exists\n", data->uri1);
		if(data->changeset)
		{
			spindle_strset_add_flags(data->changeset, u1, flags);
		}
		spindle_db_id_copy(data->id, u1);
		free(u1);
		free(u2);
		return SQL_TXN_ROLLBACK;
	}
	/* If both entities already have local proxies, we just pick the first
	 * to use as the new unified proxy.
	 *
	 * If only one entity has a proxy, just use that and attach the new
	 * referencing triples to it.
	 *
	 * If neither does, generate a new local name and populate it.
	 */
	uu = (u1 ? u1 : (u2 ? u2 : NULL));
	if(!uu)
	{
		/* Generate a new URI for uri1 */
		uu = spindle_proxy_generate(spindle, data->uri1);
		if(!uu)
		{
			free(u1);
			free(u2);
			return SQL_TXN_FAIL;
		}
		flags |= SF_MOVED;
	}
	/* Store the proxy UUID in data->id so that its state will be updated */
	spindle_db_id_copy(data->id, uu);
	if(data->uri2)
	{
		twine_logf(LOG_INFO, PLUGIN_NAME ": DB: <%s> => (<%s>, <%s>)\n", uu, data->uri1, data->uri2);
	}
	else
	{
		twine_logf(LOG_INFO, PLUGIN_NAME ": <%s> => (<%s>)\n", uu, data->uri1);
	}
	/* If the first entity didn't previously have a local proxy, attach it */
	if(!u1)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": DB: relating %s to %s\n", data->uri1, uu);
		if(spindle_db_proxy_relate(spindle, data->uri1, uu) < 0)
		{
			free(u1);
			free(u2);
			if(uu != u1 && uu != u2)
			{
				free(uu);
			}
			return SQL_TXN_FAIL;
		}
		flags |= SF_MOVED;
	}
	/* If the second entity didn't previously have a local proxy, attach it */
	if(!u2)
	{
		if(data->uri2)
		{
			twine_logf(LOG_DEBUG, PLUGIN_NAME ": relating %s to %s\n", data->uri2, uu);
			if(spindle_db_proxy_relate(spindle, data->uri2, uu))
			{
				free(u1);
				free(u2);
				if(uu != u1 && uu != u2)
				{
					free(uu);
				}
				return SQL_TXN_FAIL;
			}
		}
	}
	else if(strcmp(u2, uu))
	{
		/* However, if it did have a local proxy and it was different to
		 * the one we've chosen, migrate its references over, leaving a single
		 * unified proxy.
		 */
		twine_logf(LOG_INFO, PLUGIN_NAME ": DB: relocating references from <%s> to <%s>\n", u2, uu);
		if(spindle_db_proxy_migrate(spindle, u2, uu, NULL))
		{
			free(u1);
			free(u2);
			if(uu != u1 && uu != u2)
			{
				free(uu);
			}
			return -1;
		}
		flags |= SF_MOVED;
		if(data->changeset)
		{
			spindle_strset_add_flags(data->changeset, u2, flags);
		}
	}
	if(data->changeset)
	{
		spindle_strset_add_flags(data->changeset, uu, flags);
	}
	free(u1);
	free(u2);
	if(uu != u1 && uu != u2)
	{
		free(uu);
	}
	return SQL_TXN_COMMIT;
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
	return 1;
}
