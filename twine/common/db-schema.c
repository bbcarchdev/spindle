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

/* The current version of the database schema: each schema version number in
 * 1..DB_SCHEMA_VERSION must be handled individually in spindle_db_migrate_
 * below.
 */
#define DB_SCHEMA_VERSION               25

static int spindle_db_migrate_(SQL *restrict, const char *identifier, int newversion, void *restrict userdata);

int
spindle_db_schema_update_(SPINDLE *spindle)
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
					   " \"about\" text NOT NULL,"
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
		return 0;
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
	if(newversion == 6)
	{
		if(sql_execute(sql, "ALTER TABLE \"index\" ADD COLUMN \"media\" uuid"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"index_media\" ON \"index\" USING hash (\"media\")"))
		{
			return -1;
		}
		return 0;
	}	
	if(newversion == 7)
	{
		if(sql_execute(sql, "DROP INDEX \"index_media\""))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE TABLE \"index_media\" ("
					   "  \"id\" uuid NOT NULL, "
					   "  \"media\" uuid NOT NULL, "
					   "  PRIMARY KEY(\"id\", \"media\")"
					   ")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"index_media_id\" ON \"index_media\" (\"id\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"index_media_media\" ON \"index_media\" (\"media\")"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 8)
	{
		if(sql_execute(sql, "ALTER TABLE \"index\" DROP COLUMN \"media\""))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 9)
	{
		if(sql_execute(sql, "CREATE TABLE \"index_about\" ("
					   "  \"id\" uuid NOT NULL, "
					   "  \"about\" uuid NOT NULL, "
					   "  PRIMARY KEY(\"id\", \"about\")"
					   ")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"index_about_id\" ON \"index_about\" (\"id\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"index_about_about\" ON \"index_about\" (\"about\")"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 10)
	{
		if(sql_execute(sql, "ALTER TABLE \"about\" ALTER COLUMN \"about\" TYPE uuid USING (\"about\"::uuid)"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 11)
	{
		if(sql_execute(sql, "DROP TABLE \"index_about\""))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 12)
	{
		if(sql_execute(sql, "CREATE TABLE \"membership\" ("
					   "  \"id\" uuid NOT NULL, "
					   "  \"collection\" uuid NOT NULL, "
					   "  \"depth\" integer NOT NULL DEFAULT 0, "
					   "  PRIMARY KEY(\"id\", \"collection\")"
					   ")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"membership_id\" ON \"membership\" (\"id\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"membership_collection\" ON \"membership\" (\"collection\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"membership_depth\" ON \"membership\" (\"depth\")"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 13)
	{
		if(sql_execute(sql, "CREATE TABLE \"moved\" ("
			"  \"from\" uuid NOT NULL, "
			"  \"to\" uuid NOT NULL, "
			"  PRIMARY KEY(\"from\")"
			")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"moved_from\" ON \"moved\" (\"from\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"moved_to\" ON \"moved\" (\"to\")"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 14)
	{
		/* The triggers table is used to express processing dependency
		 * relationships between entities. The "id" is the UUID of
		 * the entity whose update should be triggered, while the "uri"
		 * is the URI whose processing should trigger that update. For
		 * example, a media item's processing should be triggered by
		 * a change in state in the license URI.
		 */
		if(sql_execute(sql, "CREATE TABLE \"triggers\" ("
			"  \"id\" uuid NOT NULL, "
			"  \"uri\" text NOT NULL, "
			"  PRIMARY KEY(\"id\")"
			")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"triggers_id\" ON \"triggers\" (\"id\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"triggers_uri\" ON \"triggers\" (\"uri\")"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 15)
	{
		if(sql_execute(sql, "ALTER TABLE \"triggers\" DROP CONSTRAINT IF EXISTS \"triggers_pkey\""))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 16)
	{
		if(sql_execute(sql, "DROP TYPE IF EXISTS \"state_status\""))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE TYPE \"state_status\" AS ENUM('DIRTY', 'COMPLETE')"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 17)
	{
		if(sql_execute(sql, "CREATE TABLE \"state\" ("
			"\"id\" uuid NOT NULL, "
			"\"shorthash\" BIGINT NOT NULL, "
			"\"tinyhash\" INTEGER NOT NULL, "
			"\"status\" \"state_status\" NOT NULL, "
			"\"modified\" TIMESTAMP NOT NULL, "
			"\"flags\" INTEGER NOT NULL, "
			"PRIMARY KEY (\"id\")"
			")"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 18)
	{
		if(sql_execute(sql, "CREATE TABLE \"audiences\" ("
			"  \"id\" uuid NOT NULL, "
			"  \"uri\" text NOT NULL, "
			"  PRIMARY KEY(\"id\")"
			")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"audiences_id\" ON \"audiences\" (\"id\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"audiences_uri\" ON \"audiences\" (\"uri\")"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 19)
	{
		if(sql_execute(sql, "CREATE INDEX \"state_id\" ON \"state\" (\"id\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"state_shorthash\" ON \"state\" (\"shorthash\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"state_tinyhash\" ON \"state\" (\"tinyhash\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"state_status\" ON \"state\" (\"status\")"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 20)
	{
		if(sql_execute(sql, "ALTER TABLE \"media\" ADD COLUMN \"audienceid\" uuid default NULL"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"media_audienceid\" ON \"media\" (\"audienceid\")"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 21)
	{
		if(sql_execute(sql, "ALTER TABLE \"triggers\" ADD COLUMN \"triggerid\" uuid default NULL"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"triggers_triggerid\" ON \"triggers\" (\"triggerid\")"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 22)
	{
		if(sql_execute(sql, "ALTER TABLE \"triggers\" ADD COLUMN \"flags\" INTEGER NOT NULL default 0"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 23)
	{
		/* This can't be executed within the transaction */
		if(sql_commit(sql))
		{
			return -1;
		}
		if(sql_execute(sql, "ALTER TYPE \"state_status\" ADD VALUE 'REJECTED'"))
		{
			return -1;
		}
		if(sql_begin(sql, SQL_TXN_CONSISTENT))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 24)
	{
		if(sql_execute(sql, "CREATE TABLE \"licenses_audiences\" ("
			"  \"id\" uuid NOT NULL, "
			"  \"uri\" text NOT NULL, "
			"  \"audienceid\" uuid default NULL"
			")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"licenses_audiences_id\" ON \"licenses_audiences\" (\"id\")"))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE INDEX \"licenses_audiences_audienceid\" ON \"licenses_audiences\" (\"audienceid\")"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 25)
	{
		if(sql_execute(sql, "DELETE FROM \"membership\" WHERE \"id\" = \"collection\""))
		{
			return -1;
		}
		return 0;
	}
	twine_logf(LOG_NOTICE, PLUGIN_NAME ": unsupported database schema version %d\n", newversion);
	return -1;
}
