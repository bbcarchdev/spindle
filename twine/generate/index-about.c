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

#include "p_spindle-generate.h"

/*
 * The 'about' table tracks topicality references:
 *
 *  id       The UUID of an entity being cached
 *  about    The UUID of another reference this entity is about
 */

/* Add information about foaf:topic relationships from this entity to
 * others.
 */
int
spindle_index_about(SQL *sql, const char *id, SPINDLEENTRY *data)
{
	librdf_statement *query;
	librdf_statement *st;
	librdf_stream *stream;
	librdf_node *node;
	librdf_uri *uri;
	const char *uristr;
	char *tid;
	int r;
	size_t c;

	const char *predlist[] = 
	{
		NS_FOAF "topic",
		NS_EVENT "factor",
		NULL
	};
	
#if SPINDLE_ENABLE_ABOUT_SELF
	/* Force an entity to always 'about' itself, so that queries match both
	 * topics and the things about those topics
	 */
	if(sql_executef(sql, "INSERT INTO \"about\" (\"id\", \"about\") VALUES (%Q, %Q)", id, id))
	{
		return -1;
	}
#endif
	if(!(query = librdf_new_statement(data->spindle->world)))
	{
		return -1;
	}
	if(!(node = librdf_new_node_from_node(data->self)))
	{
		librdf_free_statement(query);
		return -1;
	}
	librdf_statement_set_subject(query, node);
	if(!(node = librdf_new_node_from_uri_string(data->spindle->world, (const unsigned char *) NS_FOAF "topic")))
	{
		librdf_free_statement(query);
		return -1;
	}
	librdf_statement_set_predicate(query, node);
	r = 0;
	for(stream = librdf_model_find_statements_in_context(data->proxydata, query, data->graph); !librdf_stream_end(stream); librdf_stream_next(stream))
	{
		st = librdf_stream_get_object(stream);
		if(!(node = librdf_statement_get_predicate(st)) ||
		   !(uri = librdf_node_get_uri(node)) ||
		   !(uristr = (const char *) librdf_uri_as_string(uri)))
		{
			continue;
		}
		/* XXX use rulebase */
		for(c = 0; predlist[c]; c++)
		{
			if(!strcmp(predlist[c], uristr))
			{
				break;
			}
		}
		if(!predlist[c])
		{
			continue;
		}			 
		if((node = librdf_statement_get_object(st)) &&
		   librdf_node_is_resource(node) &&
		   (uri = librdf_node_get_uri(node)) &&
		   (uristr = (const char *) librdf_uri_as_string(uri)))
		{
			tid = spindle_db_id(uristr);
			if(!tid)
			{
				continue;
			}
			if(!strcasecmp(id, tid))
			{
				free(tid);
				continue;
			}
			if(sql_executef(sql, "INSERT INTO \"about\" (\"id\", \"about\") VALUES (%Q, %Q)", id, tid))
			{
				free(tid);
				r = -1;
				break;
			}
			free(tid);
		}
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	return r;
}
