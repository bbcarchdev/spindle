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

#include "p_spindle-generate.h"

static int spindle_index_remove_(SQL *sql, const char *id, SPINDLEENTRY *data);
static int spindle_index_triggers_(SQL *sql, const char *id, SPINDLEENTRY *data);

/* Store an index entry in a PostgreSQL (or compatible) database for a proxy */
int
spindle_index_entry(SPINDLEENTRY *data)
{
	SQL *sql;
	char *id;

	if(!data->spindle->db)
	{
		/* No SQL database configured, nothing to do */
		return 0;
	}
	sql = data->spindle->db;
	id = spindle_db_id(data->localname);
	if(!id)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": DB: ID is '%s'\n", id);
	if(spindle_index_remove_(sql, id, data))
	{
		free(id);
		return -1;
	}
	if((data->flags & TK_PROXY) && spindle_index_core(sql, id, data))
	{
		free(id);
		return -1;
	}
	if((data->flags & TK_TOPICS) && spindle_index_about(sql, id, data))
	{
		free(id);
		return -1;
	}
	if((data->flags & TK_MEDIA) && spindle_index_media(sql, id, data))
	{
		free(id);
		return -1;
	}
	if((data->flags & TK_MEMBERSHIP) && spindle_index_membership(sql, id, data))
	{
		free(id);
		return -1;
	}
	if((data->flags == -1) && spindle_index_triggers_(sql, id, data))
	{
		free(id);
		return -1;
	}
	free(id);
	return 0;
}

static int
spindle_index_remove_(SQL *sql, const char *id, SPINDLEENTRY *data)
{
	if(data->flags & TK_PROXY)
	{
		if(sql_executef(sql, "DELETE FROM \"index\" WHERE \"id\" = %Q",
						id))
		{
			return -1;
		}
	}
	if(data->flags & TK_TOPICS)
	{
		if(sql_executef(sql, "DELETE FROM \"about\" WHERE \"id\" = %Q",
						id))
		{
			return -1;
		}
	}
	if(data->flags & TK_MEDIA)
	{
		if(sql_executef(sql, "DELETE FROM \"media\" WHERE \"id\" = %Q",
						id))
		{
			return -1;
		}
		if(sql_executef(sql, "DELETE FROM \"index_media\" WHERE \"id\" = %Q",
						id))
		{
			return -1;
		}
	}
	if(data->flags & TK_MEMBERSHIP)
	{
		if(sql_executef(sql, "DELETE FROM \"membership\" WHERE \"id\" = %Q",
						id))
		{
			return -1;
		}
	}
	if(data->flags == -1)
	{
		if(sql_executef(sql, "DELETE FROM \"triggers\" WHERE \"id\" = %Q",
						id))
		{
			return -1;
		}
	}
	return 0;
}

/* Add the set of trigger URIs to the database */
static int
spindle_index_triggers_(SQL *sql, const char *id, SPINDLEENTRY *data)
{
	size_t c;

	for(c = 0; c < data->ntriggers; c++)
	{
		if(sql_executef(sql, "INSERT INTO \"triggers\" (\"id\", \"uri\", \"flags\") VALUES (%Q, %Q, '%d')", id, data->triggers[c].uri, data->triggers[c].kind))
		{
			return -1;
		}
	}
	return 0;
}
