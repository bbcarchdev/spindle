/* Perform one-off migrations
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <inttypes.h>
#include <time.h>
#include <ctype.h>
#include <libsql.h>

static int schema_version(SQL *sql);
static int migrate_proxies_queue(SQL *sql);
static int migrate_proxy_txn(SQL *restrict sql, void *restrict userdata);

static const char *progname;

int
main(int argc, char **argv)
{
	SQL *sql;
	int ver;
	
	if((progname = strrchr(argv[0], '/')))
	{
		progname++;
	}
	else
	{
		progname = argv[0];
	}
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s DATABASE-URI\n", progname);
		exit(EXIT_FAILURE);
	}
	sql = sql_connect(argv[1]);
	if(!sql)
	{
		fprintf(stderr, "%s: failed to connect to database\n", progname);
		exit(EXIT_FAILURE);
	}
	ver = schema_version(sql);
	if(ver < 0)
	{
		fprintf(stderr, "%s: an error occurred while obtaining the current schema version from the target database\n", progname);
		exit(EXIT_FAILURE);
	}
	if(ver < 24)
	{
		fprintf(stderr, "%s: schema version must be 24 or higher to run this tool; current version in the target database is %d\n", progname, ver);
		exit(EXIT_FAILURE);
	}
	fprintf(stderr, "%s: target database schema version is %d\n", progname, ver);
	if(migrate_proxies_queue(sql) < 0)
	{
		fprintf(stderr, "%s: failed to migrate proxies into processing queue\n", progname);
		exit(EXIT_FAILURE);
	}
	sql_disconnect(sql);
	return 0;
}

static int
schema_version(SQL *sql)
{
	SQL_STATEMENT *rs;
	int r;
	
	rs = sql_queryf(sql, "SELECT \"version\" FROM \"_version\" WHERE \"ident\" = %Q", "com.github.bbcarchdev.spindle.twine");
	if(!rs)
	{
		return -1;
	}
	if(sql_stmt_eof(rs))
	{
		sql_stmt_destroy(rs);
		return 0;
	}
	r = (int) sql_stmt_ulong(rs, 0);
	sql_stmt_destroy(rs);
	return r;
}

static int
migrate_proxies_queue(SQL *sql)
{
	unsigned long total, done;
	SQL_STATEMENT *rs;
	char idbuf[36], *p;
	const char *t;

	rs = sql_queryf(sql, "SELECT COUNT(*) FROM \"proxy\"");
	if(!rs)
	{
		return -1;
	}
	if(sql_stmt_eof(rs))
	{
		sql_stmt_destroy(rs);
		return 0;
	}
	total = sql_stmt_ulong(rs, 0);
	sql_stmt_destroy(rs);
	fprintf(stderr, "%s: %lu proxies found\n", progname, total);
	done = 0;
	while(1)
	{
		if(done)
		{
			rs = sql_queryf(sql, "SELECT \"id\" FROM \"proxy\" ORDER BY \"id\" DESC LIMIT %d OFFSET %lu", 1000, done);
		}
		else
		{
			rs = sql_queryf(sql, "SELECT \"id\" FROM \"proxy\" ORDER BY \"id\" DESC LIMIT %d", 1000);
		}
		if(sql_stmt_eof(rs))
		{
			break;
		}
		for(; !sql_stmt_eof(rs); sql_stmt_next(rs))
		{
			p = idbuf;
			for(t = sql_stmt_str(rs, 0); *t; t++)
			{
				if(isalnum(*t))
				{
					*p = tolower(*t);
					p++;
				}
			}
			*p = 0;
			sql_perform(sql, migrate_proxy_txn, idbuf, -1, SQL_TXN_CONSISTENT);
			done++;
		}		
		sql_stmt_destroy(rs);
		fprintf(stderr, "%s: migrated %lu/%lu\n", progname, done, total);
	}
	return 0;
}

static int
migrate_proxy_txn(SQL *restrict sql, void *restrict userdata)
{
	const char *id;
	char skey[16];
	uint32_t shortkey;
	time_t t;
	struct tm tm;
	char tbuf[32];
	SQL_STATEMENT *rs;
	
	id = (const char *) userdata;
	
	strncpy(skey, id, 8);
	skey[8] = 0;
	shortkey = (uint32_t) strtoul(skey, NULL, 16);
	t = time(NULL);
	gmtime_r(&t, &tm);
	strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm);
	rs = sql_queryf(sql, "SELECT \"id\" FROM \"state\" WHERE \"id\" = %Q", id);
	if(!rs)
	{
		return -2;
	}
	if(!sql_stmt_eof(rs))
	{
		sql_stmt_destroy(rs);
		return 0;
	}
	if(sql_executef(sql, "INSERT INTO \"state\" (\"id\", \"shorthash\", \"tinyhash\", \"status\", \"modified\", \"flags\") VALUES (%Q, '%lu', '%d', %Q, %Q, '%d')",
		id, (unsigned long) shortkey, (int) (shortkey % 256), "DIRTY", tbuf, 0))
	{
		return -2;
	}
	return 1;
}
