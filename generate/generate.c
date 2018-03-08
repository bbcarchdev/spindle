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

struct s3_upload_struct
{
	char *buf;
	size_t bufsize;
	size_t pos;
};

static char *spindle_generate_uri_(SPINDLEGENERATE *generate, const char *identifier);
static int spindle_generate_state_fetch_(SPINDLEENTRY *cache);
static int spindle_generate_state_update_(SPINDLEENTRY *cache);
static int spindle_generate_proxy_node_locate_(librdf_node **node, struct proxy_node_locator_struct *locator);
static int spindle_generate_txn_(SQL *restrict sql, void *restrict userdata);
static int spindle_generate_entry_(SPINDLEENTRY *entry);

static unsigned long long gettimems(void);
static int gettimediffms(unsigned long long *start);

/* Generate the data for a single entity, given an identifier
 *
 * The identifier may be in the form of:
 *
 * - A local URI
 * - A UUID
 * - An external URI, which needs to be looked up
 */
int
spindle_generate(SPINDLEGENERATE *generate, const char *identifier, int mode)
{
	SPINDLEENTRY data;
	char *idbuf;
	int r;

	(void) mode;

	idbuf = spindle_generate_uri_(generate, identifier);
	if(!idbuf)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to determine identifier for <%s>\n", identifier);
		return -1;
	}
	r = 0;
	if(spindle_entry_init(&data, generate, idbuf))
	{
		r = -1;
	}
	else if(data.db)
	{
		if(sql_perform(data.db, spindle_generate_txn_, (void *) &data, -1, SQL_TXN_CONSISTENT))
		{
			r = -1;
		}
	}
	else
	{
		r = spindle_generate_entry_(&data);
	}
	spindle_entry_cleanup(&data);
	if(r)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": update failed for <%s>\n", idbuf);
	}
	else
	{
		twine_logf(LOG_NOTICE, PLUGIN_NAME ": update complete for <%s>\n", idbuf);
	}
	free(idbuf);
	return r;
}

static unsigned long long
gettimems(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

static int
gettimediffms(unsigned long long *start)
{
	struct timeval tv;
	int r;

	gettimeofday(&tv, NULL);
	r = (int) (((tv.tv_sec * 1000) + (tv.tv_usec / 1000)) - *start);
/*	*start = (tv.tv_sec * 1000) + (tv.tv_usec / 1000); */
	return r;
}


static char *
spindle_generate_uri_(SPINDLEGENERATE *generate, const char *identifier)
{
	size_t c;
	const char *t;
	char uuid[40];
	char *idbuf, *p;

	/* If identifier is a string of 32 hex-digits, possibly including hyphens,
	 * then we can prefix it with the root and suffix it with #id to form
	 * a real identifer.
	 */
	idbuf = NULL;
	c = 0;
	for(t = identifier; *t && c < 32; t++)
	{
		if(isxdigit(*t))
		{
			uuid[c] = tolower(*t);
			c++;
		}
		else if(*t == '-')
		{
			continue;
		}
		else
		{
			break;
		}
	}
	uuid[c] = 0;
	if(!*t && c == 32)
	{
		/* It was a UUID, transform it into a URI */
		/* XXX the fragment should be configurable */
		idbuf = (char *) malloc(strlen(generate->spindle->root) + 1 + 32 + 3 + 1);
		if(!idbuf)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for local identifier\n");
			return NULL;
		}
		strcpy(idbuf, generate->spindle->root);
		p = strchr(idbuf, 0);
		if(p > idbuf)
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
		strcpy(p, uuid);
		p += 32;
		*p = '#';
		p++;
		*p = 'i';
		p++;
		*p = 'd';
		p++;
		*p = 0;
		return idbuf;
	}
	if(strncmp(identifier, generate->spindle->root, strlen(generate->spindle->root)))
	{
		return spindle_proxy_locate(generate->spindle, identifier);
	}
	idbuf = strdup(identifier);
	if(!idbuf)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for local identifier\n");
		return NULL;
	}
	return idbuf;
}

static int
spindle_generate_txn_(SQL *restrict sql, void *restrict userdata)
{
	SPINDLEENTRY *entry;

	(void) sql;

	entry = (SPINDLEENTRY *) userdata;
	if(spindle_generate_entry_(entry))
	{
		if(sql_deadlocked(sql))
		{
			/* Reset the models before retrying */
			if (spindle_entry_reset(entry))
			{
				return -2;
			}
			/* Retry in the event of a deadlock */
			return -1;
		}
		return -2;
	}
	return 1;
}

/* Re-build the data for the proxy entity identified by cache->localname;
 * if no references exist any more, the cached data will be removed.
 *
 * Note: this method can be called twice with the same entry data!
 */
static int
spindle_generate_entry_(SPINDLEENTRY *entry)
{
	struct proxy_node_locator_struct locator;
	unsigned long long start;

	twine_logf(LOG_INFO, PLUGIN_NAME ": updating <%s>\n", entry->proxy->localname);
	/* Obtain cached source data */
	start = gettimems();
	/* TODO: when performing a partial update, what sort of data do we need to retrieve in order for indexing to work correctly? */
	/* TODO: can we use the cache? */
	if(spindle_generate_state_fetch_(entry))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to retrieve entity state\n");
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": [%dms] retrieve state\n", gettimediffms(&start));
	if(spindle_source_fetch_entry(entry))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain cached data from store\n");
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": [%dms] obtain cached data\n", gettimediffms(&start));
	/* Update any triggers */
	if(spindle_triggers_update(entry) < 0)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": [%dms] update triggers\n", gettimediffms(&start));
	/* Update proxy classes */
	if(rulebase_class_update_entry(entry->proxy, entry->spindle->world) < 0)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": [%dms] update classes\n", gettimediffms(&start));
	/* Update proxy properties */
	locator.localname = entry->proxy->localname;
	locator.extra_data = entry->spindle;
	if(rulebase_prop_update_entry(entry->proxy, entry->spindle->world, entry->generate->titlepred, spindle_generate_proxy_node_locate_, &locator) < 0)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": [%dms] update properties\n", gettimediffms(&start));
	/* Fetch information about the documents describing the entities */
	if(spindle_describe_entry(entry) < 0)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": [%dms] update describedBy\n", gettimediffms(&start));
	/* Describe the document itself */
	if(spindle_doc_apply(entry) < 0)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": [%dms] add information resource\n", gettimediffms(&start));
	/* Describing licensing information */
	if(spindle_license_apply(entry) < 0)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": [%dms] add licensing information\n", gettimediffms(&start));
	/* Fetch data about related resources */
	if(spindle_related_fetch_entry(entry) < 0)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": [%dms] fetch related data\n", gettimediffms(&start));
	/* Index the resulting model */
	if(spindle_index_entry(entry) < 0)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": [%dms] index generated entry\n", gettimediffms(&start));
	/* Store the resulting model */
	if(spindle_store_entry(entry) < 0)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": [%dms] store generated entry\n", gettimediffms(&start));
	/* Update the state of the entry */
	if(spindle_generate_state_update_(entry) < 0)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": [%dms] update entry state\n", gettimediffms(&start));
	/* Apply the triggers to update the state of target entries */
	if(spindle_trigger_apply(entry) < 0)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": [%dms] apply triggers\n", gettimediffms(&start));
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": generation complete for <%s>\n", entry->proxy->localname);
	return 0;
}

static int
spindle_generate_proxy_node_locate_(librdf_node **node, struct proxy_node_locator_struct *locator)
{
	librdf_uri *uri;

	*node = NULL;
	if(!locator
	->match
	->map
	->proxyonly)
	{
		/* tell caller to continue with NULL node */
		return 0;
	}
	uri = spindle_proxy_locate((SPINDLE *) locator->extra_data, (const char *) librdf_uri_as_string(librdf_node_get_uri(locator->obj)));
	if(!uri || !strcmp(uri, locator->localname))
	{
		/* tell caller to abort successfully */
		free(uri);
		return 1;
	}
	*node = rdf_node_createuri_(uri);
	free(uri);
	if(!*node)
	{
		/* tell caller to abort unsuccessfully */
		return -1;
	}
	/* tell caller to continue with non-NULL node */
	return 0;
}

static int
spindle_generate_state_fetch_(SPINDLEENTRY *cache)
{
	SQL_STATEMENT *rs;
	const char *modified, *state;
	int flags;
	struct tm tm;

	if(!cache->id)
	{
		/* No database, nothing to do */
		cache->flags = -1;
		return 0;
	}
	rs = sql_queryf(cache->db, "SELECT \"status\", \"modified\", \"flags\" FROM \"state\" WHERE \"id\" = %Q", cache->id);
	if(!rs)
	{
		return -1;
	}
	if(sql_stmt_eof(rs))
	{
		twine_logf(LOG_WARNING, PLUGIN_NAME ": no state entry exists for <%s>\n", cache->proxy->localname);
		cache->flags = -1;
		sql_stmt_destroy(rs);
		return 0;
	}
	state = sql_stmt_str(rs, 0);
	modified = sql_stmt_str(rs, 1);
	flags = (int) sql_stmt_long(rs, 2);
	if(!state || !strcmp(state, "CLEAN") || !flags)
	{
		flags = -1;
	}
	memset(&tm, 0, sizeof(struct tm));
	if(strptime(modified, "%Y-%m-%d %H:%M:%S", &tm))
	{
		cache->modified = mktime(&tm);
	}
	else
	{
		modified = NULL;
	}
	cache->flags = flags;
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": {%s} has state %s, last modified %s, flags %ld\n",
		cache->id, state, modified, flags);
	sql_stmt_destroy(rs);
	return 0;
}

static int
spindle_generate_state_update_(SPINDLEENTRY *cache)
{
	return sql_executef(cache->db, "UPDATE \"state\" SET \"status\" = %Q, \"flags\" = '%d' WHERE \"id\" = %Q", "COMPLETE", 0, cache->id);
}
