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
static int spindle_generate_entry_(SPINDLEENTRY *entry);

#if 0
static int spindle_cache_init_(SPINDLEENTRY *data, SPINDLE *spindle, const char *localname);
static int spindle_cache_cleanup_(SPINDLEENTRY *data);
static int spindle_cache_cleanup_literalset_(struct spindle_literalset_struct *set);
static int spindle_cache_store_(SPINDLEENTRY *data);
static int spindle_cache_source_(SPINDLEENTRY *data);
static int spindle_cache_source_sameas_(SPINDLEENTRY *data);
static int spindle_cache_source_clean_(SPINDLEENTRY *data);
static int spindle_cache_strset_refs_(SPINDLEENTRY *data, struct spindle_strset_struct *set);
static int spindle_cache_describedby_(SPINDLEENTRY *data);
static int spindle_cache_extra_(SPINDLEENTRY *data);
#endif

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
	if(spindle_entry_init(&data, generate, idbuf))
	{
		r = -1;
	}
	else
	{
		r = spindle_generate_entry_(&data);
	}
	/* TODO: once we've updated an entry, turn triggers referring to this
	 * proxy into updates of the state table
	 */
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

/* Re-build the data for the proxy entity identified by cache->localname;
 * if no references exist any more, the cached data will be removed.
 */
static int
spindle_generate_entry_(SPINDLEENTRY *entry)
{
	unsigned long long start;

	twine_logf(LOG_INFO, PLUGIN_NAME ": updating <%s>\n", entry->localname);
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
	if(entry->flags & TK_PROXY)
	{
		/* Update proxy classes */
		if(spindle_class_update_entry(entry) < 0)
		{
			return -1;
		}
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": [%dms] update classes\n", gettimediffms(&start));
		/* Update proxy properties */
		if(spindle_prop_update_entry(entry) < 0)
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
	}
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
	if(spindle_generate_state_update_(entry) < 0)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": [%dms] update entry state\n", gettimediffms(&start));
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": generation complete for <%s>\n", entry->localname);
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
		twine_logf(LOG_WARNING, PLUGIN_NAME ": no state entry exists for <%s>\n", cache->localname);
		cache->flags = -1;
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
	return 0;
}

static int
spindle_generate_state_update_(SPINDLEENTRY *cache)
{
	return sql_executef(cache->db, "UPDATE \"state\" SET \"status\" = %Q, \"flags\" = '%d' WHERE \"id\" = %Q", "COMPLETE", 0, cache->id);
}

/* Add a trigger URI */
int
spindle_cache_trigger(SPINDLEENTRY *cache, const char *uri, unsigned int kind)
{
	size_t c;
	struct spindle_trigger_struct *p;

	for(c = 0; c < cache->ntriggers; c++)
	{
		if(!strcmp(cache->triggers[c].uri, uri))
		{
			cache->triggers[c].kind |= kind;
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
	p->kind = kind;
	cache->ntriggers++;
	return 0;
}

#if 0
/* For anything which is the subject of one of the triples in the source
 * dataset, find any triples whose object is that thing and add the
 * subjects to the set if they're not already present.
 *
 * This a trigger mechanism: by adding the local URI of the entity referring to
 * this one to the set, it will be updated as part of this run of the
 * generation process.
 */
static int
spindle_cache_strset_refs_(SPINDLEENTRY *data, struct spindle_strset_struct *set)
{
	struct spindle_strset_struct *subjects;
	librdf_stream *stream;
	librdf_statement *st;
	librdf_node *node;
	librdf_uri *uri;
	const char *uristr;
	size_t c;
	SPARQLRES *res;
	SPARQLROW *row;

	if(!set)
	{
		return 0;
	}
	/* Build the subject set */
	subjects = spindle_strset_create();
	stream = librdf_model_as_stream(data->sourcedata);
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		node = librdf_statement_get_subject(st);
		if(librdf_node_is_resource(node) &&
		   (uri = librdf_node_get_uri(node)) &&
		   (uristr = (const char *) librdf_uri_as_string(uri)))
		{
			spindle_strset_add(subjects, uristr);
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
		
	/* Query for references to each of the subjects */
	for(c = 0; c < subjects->count; c++)
	{
		res = sparql_queryf(data->spindle->sparql,
							"SELECT ?local, ?s WHERE {\n"
							" GRAPH %V {\n"
							"  ?s <" NS_OWL "sameAs> ?local .\n"
							" }\n"
							" GRAPH ?g {\n"
							"   ?s ?p <%s> .\n"
							" }\n"
							"}",
							data->spindle->rootgraph, subjects->strings[c]);
		if(!res)
		{
			twine_logf(LOG_ERR, "SPARQL query for inbound references failed\n");
			spindle_strset_destroy(subjects);
			return -1;
		}
		/* Add each result to the changeset */
		while((row = sparqlres_next(res)))
		{
			node = sparqlrow_binding(row, 0);
			if(node && librdf_node_is_resource(node) &&
			   (uri = librdf_node_get_uri(node)) &&
			   (uristr = (const char *) librdf_uri_as_string(uri)))
			{
				spindle_strset_add(set, uristr);
			}				
		}
		sparqlres_destroy(res);
	}
	spindle_strset_destroy(subjects);
	return 0;
}
#endif

#if 0
/* Re-build the cached data for a set of proxies */
int
spindle_cache_update_set(SPINDLE *spindle, struct spindle_strset_struct *set)
{ 
	size_t c, origcount;

	twine_logf(LOG_DEBUG, PLUGIN_NAME ": updating proxies:\n");
	for(c = 0; c < set->count; c++)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": <%s>\n", set->strings[c]);
	}
	/* Keep track of how many things were in the original set, so that we
	 * don't recursively re-cache a huge amount
	 */
	origcount = set->count;
	for(c = 0; c < set->count; c++)
	{
		if(set->flags[c] & SF_DONE)
		{
			continue;
		}
		if(c < origcount && (set->flags[c] & SF_MOVED))
		{
			spindle_cache_update(spindle, set->strings[c], set);
		}
		else
		{
			spindle_cache_update(spindle, set->strings[c], NULL);
		}
		set->flags[c] |= SF_DONE;
	}
	return 0;
}
#endif

