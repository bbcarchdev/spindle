/* This engine processes requests for coreference graphs populated
 * by Twine's "spindle" post-processing module.
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

S3BUCKET *spindle_bucket;
SQL *spindle_db;
int spindle_s3_verbose;

static int spindle_db_querylog_(SQL *restrict sql, const char *query);
static int spindle_db_noticelog_(SQL *restrict sql, const char *notice);
static int spindle_db_errorlog_(SQL *restrict sql, const char *sqlstate, const char *message);

int
quilt_plugin_init(void)
{
	char *t;

	if(quilt_plugin_register_engine(QUILT_PLUGIN_NAME, spindle_process))
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to register engine\n");
		return -1;
	}
	spindle_bucket = NULL;
	spindle_db = NULL;
	if((t = quilt_config_geta(QUILT_PLUGIN_NAME ":db", NULL)))
	{
		spindle_db = sql_connect(t);
		if(!spindle_db)
		{
			quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to connect to database <%s>\n", t);
			free(t);
			return -1;
		}
		free(t);
		sql_set_querylog(spindle_db, spindle_db_querylog_);
		sql_set_errorlog(spindle_db, spindle_db_errorlog_);
		sql_set_noticelog(spindle_db, spindle_db_noticelog_);
	}
	if((t = quilt_config_geta(QUILT_PLUGIN_NAME ":bucket", NULL)))
	{
		spindle_bucket = s3_create(t);
		if(!spindle_bucket)
		{
			quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to initialise S3 bucket '%s'\n", t);
			free(t);
			return -1;
		}
		free(t);
		if((t = quilt_config_geta("s3:endpoint", NULL)))
		{
			s3_set_endpoint(spindle_bucket, t);
			free(t);
		}
		if((t = quilt_config_geta("s3:access", NULL)))
		{
			s3_set_access(spindle_bucket, t);
			free(t);
		}
		if((t = quilt_config_geta("s3:secret", NULL)))
		{
			s3_set_secret(spindle_bucket, t);
			free(t);
		}
		spindle_s3_verbose = quilt_config_get_bool("s3:verbose", 0);
	}
	return 0;
}

static int
spindle_db_querylog_(SQL *restrict sql, const char *query)
{
	(void) sql;

	quilt_logf(LOG_DEBUG, QUILT_PLUGIN_NAME ": SQL: %s\n", query);
	return 0;
}

static int
spindle_db_noticelog_(SQL *restrict sql, const char *notice)
{
	(void) sql;

	quilt_logf(LOG_NOTICE, QUILT_PLUGIN_NAME ": %s\n", notice);
	return 0;
}

static int
spindle_db_errorlog_(SQL *restrict sql, const char *sqlstate, const char *message)
{
	(void) sql;

	quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": [%s] %s\n", sqlstate, message);
	return 0;
}


