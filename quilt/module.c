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
int spindle_threshold;

struct index_struct *spindle_indices = NULL;

static int spindle_cache_init_(void);
static int spindle_cache_init_s3_(const char *bucket);
static int spindle_cache_init_file_(const char *path);
static int spindle_db_querylog_(SQL *restrict sql, const char *query);
static int spindle_db_noticelog_(SQL *restrict sql, const char *notice);
static int spindle_db_errorlog_(SQL *restrict sql, const char *sqlstate, const char *message);
static struct index_struct *spindle_partition_(const char *resource);
static int spindle_partition_cb_(const char *key, const char *value, void *data);

int
quilt_plugin_init(void)
{
	char *t;
	struct index_struct *everything;

	if(quilt_plugin_register_engine(QUILT_PLUGIN_NAME, spindle_process))
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to register engine\n");
		return -1;
	}
	spindle_bucket = NULL;
	spindle_db = NULL;
	spindle_cachepath = NULL;
	spindle_threshold = quilt_config_get_int(QUILT_PLUGIN_NAME ":score", SPINDLE_THRESHOLD);
	quilt_logf(LOG_INFO, QUILT_PLUGIN_NAME ": default score threshold set to %d\n", spindle_threshold);
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
	everything = spindle_partition_("/everything");
	everything->title = strdup("Everything");
	quilt_config_get_all(NULL, NULL, spindle_partition_cb_, NULL);
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

/* quilt_config_getall() callback */
static int
spindle_partition_cb_(const char *key, const char *value, void *data)
{
	char buf[64], *s;
	const char *prop;
	struct index_struct *ind;

	(void) data;

	if(!key || !value || strncmp(key, "partition:", 10))
	{
		return 0;
	}
	key += 10;
	prop = strchr(key, ':');
	if(!prop)
	{
		return 0;
	}
	if(prop - key >= 63)
	{
		return 0;
	}
	buf[0] = '/';
	strncpy(&(buf[1]), key, prop - key);
	buf[prop - key + 1] = 0;
	prop++;
	quilt_logf(LOG_DEBUG, "partition=[%s], prop=[%s], value=[%s]\n", buf, prop, value);
	ind = spindle_partition_(buf);
	if(!ind)
	{
		return 0;
	}
	if(!strcmp(prop, "class"))
	{
		s = strdup(value);
		free(ind->qclass);
		ind->qclass = s;
		return 0;
	}
	if(!strcmp(prop, "title"))
	{
		s = strdup(value);
		free(ind->title);
		ind->title = s;
	}	
	return 0;
}

static struct index_struct *
spindle_partition_(const char *resource)
{
	size_t c;
	struct index_struct *p;

	for(c = 0; spindle_indices && spindle_indices[c].uri; c++)
	{
		if(!strcmp(spindle_indices[c].uri, resource))
		{
			return &(spindle_indices[c]);
		}
	}
	p = (struct index_struct *) realloc(spindle_indices, sizeof(struct index_struct) * (c + 2));
	if(!p)
	{
		return NULL;
	}
	spindle_indices = p;
	memset(&(p[c]), 0, sizeof(struct index_struct));
	memset(&(p[c + 1]), 0, sizeof(struct index_struct));
	p[c].uri = strdup(resource);
	if(!p[c].uri)
	{
		return NULL;
	}
	return &(p[c]);
}
