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

#include "p_spindle-generate.h"

static int spindle_source_fetch_db_(SPINDLEENTRY *data);
static int spindle_source_fetch_sparql_(SPINDLEENTRY *data);
static int spindle_source_sameas_(SPINDLEENTRY *data);
static int spindle_source_clean_(SPINDLEENTRY *data);

/* Obtain cached source data for processing */
int
spindle_source_fetch_entry(SPINDLEENTRY *data)
{
	if(data->db)
	{
		if(spindle_source_fetch_db_(data))
		{
			return -1;
		}
	}
	else
	{
		if(spindle_source_fetch_sparql_(data))
		{
			return -1;
		}
	}
	if(spindle_source_clean_(data))
	{
		return -1;
	}
	return 0;
}

/* Fetch all of the source data about the entities that relate to a particular
 * proxy using the database
 */
static int
spindle_source_fetch_db_(SPINDLEENTRY *data)
{
	int r;
	size_t c;
	librdf_statement *st;

	if(!data->refs)
	{
		data->refs = spindle_proxy_refs(data->spindle, data->localname);
		if(!data->refs)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain co-references for <%s>\n", data->localname);
			return -1;
		}
	}
	r = 0;
	for(c = 0; data->refs[c]; c++)
	{
		/* Add <ref> owl:sameAs <localname> triples to the proxy model */
		st = twine_rdf_st_create();
		librdf_statement_set_subject(st, twine_rdf_node_createuri(data->refs[c]));
		librdf_statement_set_predicate(st, twine_rdf_node_clone(data->spindle->sameas));
		librdf_statement_set_object(st, twine_rdf_node_createuri(data->localname));
		librdf_model_context_add_statement(data->proxydata, data->graph, st);
		twine_rdf_st_destroy(st);
		/* Fetch the source data into the source model */
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": DB: fetching source data for <%s>\n", data->refs[c]);
		/* XXX this query is very inefficient */
		if(sparql_queryf_model(data->sparql, data->sourcedata,
							   "SELECT DISTINCT ?s ?p ?o ?g\n"
							   " WHERE {\n"
							   "  GRAPH ?g {\n"
							   "   ?s ?p ?o .\n"
							   "   FILTER(?s = <%s> || ?o = <%s>)\n"
							   "  }\n"
							   "}",
							   data->refs[c], data->refs[c]))
		{
			r = -1;
			break;
		}
	}
	return r;
}

/* Fetch all of the source data about the entities that relate to a particular
 * proxy using SPARQL
 */
static int
spindle_source_fetch_sparql_(SPINDLEENTRY *data)
{
	/* Find all of the triples related to all of the subjects linked to the
	 * proxy.
	 *
	 * Note that this includes data in both the root and proxy graphs,
	 * but they will be removed by spindle_cache_source_clean_().
	 */
	if(sparql_queryf_model(data->sparql, data->sourcedata,
						   "SELECT DISTINCT ?s ?p ?o ?g\n"
						   " WHERE {\n"
						   "  GRAPH %V {\n"
						   "   ?s %V %V .\n"
						   "  }\n"
						   "  GRAPH ?g {\n"
						   "   ?s ?p ?o .\n"
						   "  }\n"
						   "}",
						   data->spindle->rootgraph, data->sameas, data->self))
	{
		return -1;
	}
	if(spindle_source_sameas_(data))
	{
		return -1;
	}
	return 0;
}

/* Copy any owl:sameAs references into the proxy graph from the root
 * graph.
 */
static int
spindle_source_sameas_(SPINDLEENTRY *data)
{
	librdf_statement *query, *st;
	librdf_node *node;
	librdf_stream *stream;
	
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": **** caching sameAs triples ****\n");
	query = twine_rdf_st_create();
	if(!query)
	{
		return -1;
	}
	node = twine_rdf_node_clone(data->sameas);
	if(!node)
	{
		twine_rdf_st_destroy(query);
		return -1;
	}
	librdf_statement_set_predicate(query, node);
	node = twine_rdf_node_clone(data->self);
	if(!node)
	{
		twine_rdf_st_destroy(query);
		return -1;
	}
	librdf_statement_set_object(query, node);
	/* Create a stream querying for (?s owl:sameAs <self>) in the root graph
	 * from the source data
	 */
	stream = librdf_model_find_statements_with_options(data->sourcedata, query, data->spindle->rootgraph, NULL);
	if(!stream)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to query model\n");
		twine_rdf_st_destroy(query);
		return -1;
	}
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		/* Add the statement to the proxy graph */
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": *** adding sameAs statement to proxy graph\n");
		if(twine_rdf_model_add_st(data->proxydata, st, data->graph))
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to add statement to proxy model\n");
			librdf_free_stream(stream);
			twine_rdf_st_destroy(query);
			return -1;
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	twine_rdf_st_destroy(query);	
	return 0;
}

/* Remove any proxy data from the source model */
static int
spindle_source_clean_(SPINDLEENTRY *data)
{
	librdf_iterator *iterator;
	librdf_node *node;
	librdf_uri *uri;
	const char *uristr;

	iterator = librdf_model_get_contexts(data->sourcedata);
	while(!librdf_iterator_end(iterator))
	{
		node = librdf_iterator_get_object(iterator);
		uri = librdf_node_get_uri(node);
		uristr = (const char *) librdf_uri_as_string(uri);
		if(!strncmp(uristr, data->spindle->root, strlen(data->spindle->root)))
		{
			librdf_model_context_remove_statements(data->sourcedata, node);
		}
		librdf_iterator_next(iterator);
	}
	librdf_free_iterator(iterator);
	return 0;
}