/* This engine processes requests for coreference graphs populated
 * by Twine's "spindle" post-processing module.
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

AWSS3BUCKET *spindle_bucket;
char *spindle_cachepath;
SQL *spindle_db;
int spindle_s3_verbose;
long spindle_s3_fetch_limit;

static int spindle_cache_init_(void);
static int spindle_cache_init_s3_(const char *bucket);
static int spindle_cache_init_file_(const char *path);
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
	spindle_cachepath = NULL;
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
	if(spindle_cache_init_())
	{
		return -1;
	}
	return 0;
}


/* spindle_array_contains(array, string);
 * Returns 1 if the array contains the string (via case-sensitive comprison)
 * Otherwise 0
 */
int
spindle_array_contains(const char *const *array, const char *string)
{
	size_t i=0;
	while(array && array[i] != NULL) {
		if(!strcmp(array[i++], string))
		{
			quilt_logf(LOG_DEBUG, QUILT_PLUGIN_NAME ": array_contains %s TRUE\n", string);
			return 1;
		}
	}
	quilt_logf(LOG_DEBUG, QUILT_PLUGIN_NAME ": array_contains %s FALSE\n", string);
	return 0;
}

static int
spindle_cache_init_(void)
{
	URI *base, *uri;
	URI_INFO *info;
	char *t;
	int r;
	
	if((t = quilt_config_geta(QUILT_PLUGIN_NAME ":cache", NULL)))
	{
		base = uri_create_cwd();
		uri = uri_create_str(t, base);
		info = uri_info(uri);
		uri_destroy(uri);
		uri_destroy(base);
		if(!strcmp(info->scheme, "s3"))
		{
			r = spindle_cache_init_s3_(info->host);
		}
		else if(!strcmp(info->scheme, "file"))
		{
			r = spindle_cache_init_file_(info->path);
		}
		else
		{
			quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": cache scheme '%s' is not supported in URI <%s>\n", info->scheme, t);
			r = -1;
		}
		uri_info_destroy(info);
		free(t);
		
	}
	else if((t = quilt_config_geta(QUILT_PLUGIN_NAME ":bucket", NULL)))
	{
		quilt_logf(LOG_WARNING, QUILT_PLUGIN_NAME ": the 'bucket' configuration option is deprecated; you should specify an S3 bucket URI as the value of the 'cache' option instead\n");
		r = spindle_cache_init_s3_(t);
		free(t);
	}
	else
	{
		r = 0;
	}
	return r;
}

static int
spindle_cache_init_s3_(const char *bucket)
{
	char *t;
	
	spindle_bucket = aws_s3_create(bucket);
	if(!spindle_bucket)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to initialise S3 bucket '%s'\n", bucket);
		return -1;
	}
	if((t = quilt_config_geta("s3:endpoint", NULL)))
	{
		aws_s3_set_endpoint(spindle_bucket, t);
		free(t);
	}
	if((t = quilt_config_geta("s3:access", NULL)))
	{
		aws_s3_set_access(spindle_bucket, t);
		free(t);
	}
	if((t = quilt_config_geta("s3:secret", NULL)))
	{
		aws_s3_set_secret(spindle_bucket, t);
		free(t);
	}

	// As its in terms of kbs
	spindle_s3_fetch_limit = 1024 * quilt_config_get_int("s3:fetch_limit", DEFAULT_SPINDLE_FETCH_LIMIT);

	spindle_s3_verbose = quilt_config_get_bool("s3:verbose", 0);
	return 0;
}

static int
spindle_cache_init_file_(const char *path)
{
	char *t;
	
	if(!path || !path[0])
	{
		return 0;
	}
	spindle_cachepath = (char *) calloc(1, strlen(path) + 2);
	if(!spindle_cachepath)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to allocate memory for cache path buffer\n");
		return -1;
	}
	strcpy(spindle_cachepath, path);
	if(path[0])
	{
		t = strchr(spindle_cachepath, 0);
		t--;
		if(*t != '/')
		{
			t++;
			*t = '/';
			t++;
			*t = 0;
		}
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


