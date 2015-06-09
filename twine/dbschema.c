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

/* The current version of the database schema: each schema version number in
 * 1..DB_SCHEMA_VERSION must be handled individually in spindle_db_migrate_
 * below.
 */
#define DB_SCHEMA_VERSION               5

#if SPINDLE_DB_INDEX || SPINDLE_DB_PROXIES

static int spindle_db_migrate_(SQL *restrict, const char *identifier, int newversion, void *restrict userdata);

int
spindle_db_schema_update(SPINDLE *spindle)
{
	if(sql_migrate(spindle->db, "com.github.bbcarchdev.spindle.twine", spindle_db_migrate_, NULL))
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to update database schema to latest version\n");
		return -1;
	}
	return 0;
}

static int
spindle_db_migrate_(SQL *restrict sql, const char *identifier, int newversion, void *restrict userdata)
{
	SQL_LANG lang;
	SQL_VARIANT variant;

	(void) identifier;
	(void) userdata;

	lang = sql_lang(sql);
	variant = sql_variant(sql);
	if(lang != SQL_LANG_SQL)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": only SQL databases can be used as back-ends for the Spindle index\n");
		return -1;
	}
	if(variant != SQL_VARIANT_POSTGRES)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": only PostgreSQL databases can be used as back-ends for the Spindle index\n");
		return -1;
	}
	if(newversion == 0)
	{
		/* Return target version */
		return DB_SCHEMA_VERSION;
	}
	twine_logf(LOG_NOTICE, PLUGIN_NAME ": updating database schema to version %d\n", newversion);
	if(newversion == 1)
	{
		if(sql_execute(sql, "CREATE TABLE \"index\" ("
					   "\"id\" uuid NOT NULL,"
					   "\"version\" smallint NOT NULL,"
					   "\"modified\" timestamp without time zone NOT NULL,"
					   "\"classes\" text[],\n"
					   "\"score\" smallint,\n"
					   "\"title\" hstore,\n"
					   "\"description\" hstore,\n"
					   "\"coordinates\" point,\n"
					   "\"index_en_gb\" tsvector,\n"
					   "\"index_cy_gb\" tsvector,\n"
					   "\"index_ga_gb\" tsvector,\n"
					   "\"index_gd_gb\" tsvector,\n"
					   "PRIMARY KEY (\"id\")\n"
					   ")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE UNIQUE INDEX \"index_id\" ON \"index\" (\"id\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"index_version\" ON \"index\" (\"version\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"index_classes\" ON \"index\" USING hash (\"classes\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"index_cy_gb\" ON \"index\" USING gin (\"index_cy_gb\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"index_en_gb\" ON \"index\" USING gin (\"index_en_gb\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"index_ga_gb\" ON \"index\" USING gin (\"index_ga_gb\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"index_gd_gb\" ON \"index\" USING gin (\"index_gd_gb\")"))
		{
			return -1;
		}
		return 0;		
	}
	if(newversion == 2)
	{
		if(sql_execute(sql, "CREATE TABLE \"proxy\" ("
					   " \"id\" uuid NOT NULL,"
					   " \"sameas\" text[],"
					   " PRIMARY KEY(\"id\")"
					   ")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE UNIQUE INDEX \"proxy_id\" ON \"proxy\" (\"id\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"proxy_sameas\" ON \"proxy\" USING HASH (\"sameas\")"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 3)
	{
		if(sql_execute(sql, "ALTER TABLE \"index\" ADD COLUMN \"parent\" uuid"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"index_parent\" ON \"index\" (\"parent\")"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 4)
	{
		if(sql_execute(sql, "CREATE TABLE \"about\" ("
					   " \"id\" uuid NOT NULL,"
					   " \"class\" text NOT NULL,"
					   " PRIMARY KEY(\"id\", \"about\")"
					   ")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"about_id\" ON \"about\" (\"id\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"about_about\" ON \"about\" (\"about\")"))
		{
			return -1;
		}
	}
	if(newversion == 5)
	{
		if(sql_execute(sql, "CREATE TABLE \"media\" ("
					   " \"id\" uuid NOT NULL,"
					   " \"uri\" text,"
					   " \"class\" text,"
					   " \"type\" varchar(64), "
					   " \"audience\" text "
					   ")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"media_id\" ON \"media\" (\"id\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"media_class\" ON \"media\" USING hash (\"class\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"media_type\" ON \"media\" (\"type\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"media_audience\" ON \"media\" USING hash (\"audience\")"))
		{
			return -1;
		}
		return 0;
	}
	twine_logf(LOG_NOTICE, PLUGIN_NAME ": unsupported database schema version %d\n", newversion);
	return -1;
}

#endif /*SPINDLE_DB_INDEX*/
