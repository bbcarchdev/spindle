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

#include "p_spindle.h"

/* Fetch the contents of a graph identified by @graph and return a pointer to
 * it.
 *
 * NOTE:
 * 1. The resulting model MUST NOT be modified.
 * 2. The returned pointer is only valid until the next call to a graphcache
 *    API.
 */
librdf_model *
spindle_graphcache_fetch_node(SPINDLE *spindle, librdf_node *graph)
{
	size_t c;
	librdf_uri *uri;
	librdf_model *temp;
	const char *uristr;
	
	uri = librdf_node_get_uri(graph);
	uristr = (const char *) librdf_uri_as_string(uri);
	for(c = 0; c < SPINDLE_GRAPHCACHE_SIZE; c++)
	{
		if(!spindle->graphcache[c].uri)
		{
			continue;
		}
		if(!strcmp(spindle->graphcache[c].uri, uristr))
		{
			twine_logf(LOG_DEBUG, PLUGIN_NAME ": graphcache: graph <%s> already present in graph cache\n", uristr);
			return spindle->graphcache[c].model;
		}
	}
	for(c = 0; c < SPINDLE_GRAPHCACHE_SIZE; c++)
	{
		if(!spindle->graphcache[c].uri)
		{
			break;
		}
	}
	if(c == SPINDLE_GRAPHCACHE_SIZE)
	{
		twine_rdf_model_destroy(spindle->graphcache[0].model);
		free(spindle->graphcache[0].uri);
		memmove(&(spindle->graphcache[0]), &(spindle->graphcache[1]),
				sizeof(struct spindle_graphcache_struct) * (SPINDLE_GRAPHCACHE_SIZE - 1));
		c = SPINDLE_GRAPHCACHE_SIZE - 1;
		spindle->graphcache[c].model = NULL;
		spindle->graphcache[c].uri = NULL;
	}
	temp = twine_rdf_model_create();
	if(twine_cache_fetch_graph_(temp, uristr))
//		sparql_queryf_model(spindle->sparql, temp,
//		"SELECT DISTINCT ?s ?p ?o"
//		" WHERE {"
//		"  GRAPH %V {"
//		"   ?s ?p ?o ."
//		"  }"
//		" }", graph)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": graphcache: failed to fetch a graph description for <%s>\n", uristr);
		twine_rdf_model_destroy(temp);
		return NULL;
	}
	spindle->graphcache[c].model = temp;
	spindle->graphcache[c].uri = strdup(uristr);
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": graphcache: added graph <%s> to cache\n", uristr);
	return spindle->graphcache[c].model;
}

int
spindle_graphcache_discard(SPINDLE *spindle, const char *uri)
{
	size_t c;

	for(c = 0; c < SPINDLE_GRAPHCACHE_SIZE; c++)
	{
		if(!spindle->graphcache[c].uri)
		{
			continue;
		}
		if(!strcmp(spindle->graphcache[c].uri, uri))
		{
			twine_rdf_model_destroy(spindle->graphcache[c].model);
			free(spindle->graphcache[c].uri);
			memset(&(spindle->graphcache[c]), 0, sizeof(struct spindle_graphcache_struct));
			return 0;
		}
	}
	return 0;
}

/* Fetch a description of a graph identified by @graph and store it in @target */
int
spindle_graphcache_description_node(SPINDLE *spindle, librdf_model *target, librdf_node *graph)
{
	librdf_stream *stream;
	librdf_model *model;
	librdf_statement *query;
	librdf_node *node;
	
	model = spindle_graphcache_fetch_node(spindle, graph);
	if(!model)
	{
		return -1;
	}

	query = librdf_new_statement(spindle->world);
	node = librdf_new_node_from_node(graph);
	librdf_statement_set_subject(query, node);

	stream = librdf_model_find_statements(model, query);
	librdf_model_context_add_statements(target, graph, stream);
	librdf_free_stream(stream);

	librdf_free_statement(query);

	return 0;
}
