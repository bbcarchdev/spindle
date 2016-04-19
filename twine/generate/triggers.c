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

#include "p_spindle-generate.h"

/* Add a trigger URI */
int
spindle_trigger_add(SPINDLEENTRY *cache, const char *uri, unsigned int kind, const char *id)
{
	size_t c;
	struct spindle_trigger_struct *p;

	for(c = 0; c < cache->ntriggers; c++)
	{
		if(!strcmp(cache->triggers[c].uri, uri))
		{
			cache->triggers[c].kind |= kind;
			if(!cache->triggers[c].id && id)
			{
				cache->triggers[c].id = strdup(id);
			}
			return 0;
		}
	}
	p = (struct spindle_trigger_struct *) realloc(cache->triggers, (cache->ntriggers + 1) * (sizeof(struct spindle_trigger_struct)));
	if(!p)
	{
		return -1;
	}
	cache->triggers = p;
	p = &(cache->triggers[cache->ntriggers]);
	memset(p, 0, sizeof(struct spindle_trigger_struct));
	p->uri = strdup(uri);
	if(!p->uri)
	{
		return -1;
	}
	if(id)
	{
		p->id = strdup(id);
		if(!p->id)
		{
			free(p->uri);
			return -1;
		}
	}
	p->kind = kind;
	cache->ntriggers++;
	return 0;
}

int
spindle_trigger_apply(SPINDLEENTRY *entry)
{
	SQL_STATEMENT *rs;
	int flags;
	
	if(!entry->generate->db)
	{
		return 0;
	}
	rs = sql_queryf(entry->generate->db, "SELECT \"id\", \"flags\" FROM \"triggers\" WHERE \"triggerid\" = %Q", entry->id);
	if(!rs)
	{
		return -1;
	}
	for(; !sql_stmt_eof(rs); sql_stmt_next(rs))
	{
		flags = (int) sql_stmt_long(rs, 1);
		sql_executef(entry->generate->db, "UPDATE \"state\" SET \"status\" = %Q, \"flags\" = \"flags\" | %d WHERE \"id\" = %Q", "DIRTY", flags, sql_stmt_str(rs, 0));
	}
	
	sql_stmt_destroy(rs);

	return 0;
}

/* Update any triggers which have one of our URIs */
int
spindle_triggers_update(SPINDLEENTRY *data)
{
	size_t c;
	
	if(!data->generate->spindle->db)
	{
		return 0;
	}
	if(sql_executef(data->generate->spindle->db, "UPDATE \"triggers\" SET \"triggerid\" = %Q WHERE \"uri\" = %Q",
		data->id, data->localname))
	{
		return -1;
	}
	for(c = 0; c < data->refcount; c++)
	{
		if(sql_executef(data->generate->spindle->db, "UPDATE \"triggers\" SET \"triggerid\" = %Q WHERE \"uri\" = %Q",
			data->id, data->refs[c]))
		{
			return -1;
		}
	}
	return 0;
}

/* Add the set of trigger URIs to the database */
int
spindle_triggers_index(SQL *sql, const char *id, SPINDLEENTRY *data)
{
	size_t c;

	for(c = 0; c < data->ntriggers; c++)
	{
		if(sql_executef(sql, "INSERT INTO \"triggers\" (\"id\", \"uri\", \"flags\", \"triggerid\""") VALUES (%Q, %Q, '%d', %Q)",
			id, data->triggers[c].uri, data->triggers[c].kind, data->triggers[c].id))
		{
			return -1;
		}
	}
	return 0;
}
