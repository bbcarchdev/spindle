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

/* Cache information about the digital objects describing the entity */
int
spindle_describe_entry(SPINDLEENTRY *data)
{
	librdf_model *model;
	librdf_iterator *iter;
	librdf_stream *stream;
	librdf_node *node, *subject;
	librdf_statement *st, *statement;
	const char *uri;
	int r;

	model = twine_rdf_model_create();
	if(data->flags & TK_PROXY)
	{
		/* Force a refresh of the cached data */
		r = 0;
	}
	else
	{
		r = spindle_cache_fetch(data, "graphs", model);
	}
	if(r < 0)
	{
		twine_rdf_model_destroy(model);
		return -1;
	}
	if(r == 0)
	{
		/* Find all of the triples related to all of the graphs describing
		 * source data.
		 */
		iter = librdf_model_get_contexts(data->sourcedata);
		while(!librdf_iterator_end(iter))
		{
			node = librdf_iterator_get_object(iter);
			uri = (const char *) librdf_uri_as_string(librdf_node_get_uri(node));
			if(!strncmp(uri, data->spindle->root, strlen(data->spindle->root)))
			{
				librdf_iterator_next(iter);
				continue;
			}
			twine_logf(LOG_DEBUG, PLUGIN_NAME ": fetching information about graph <%s>\n", uri);
			/* Fetch triples from graph G where the subject of each
			 * triple is also graph G
			 */
			if(spindle_graphcache_description_node(data->spindle, data->sourcedata, node))
			{
				return -1;
			}
			/* Add triples, in our graph, stating that:
			 *   ex:graphuri rdf:type foaf:Document .
			 */
			st = twine_rdf_st_create();
			librdf_statement_set_subject(st, librdf_new_node_from_node(node));
			librdf_statement_set_predicate(st, twine_rdf_node_createuri(NS_RDF "type"));
			librdf_statement_set_object(st, twine_rdf_node_createuri(NS_FOAF "Document"));
			twine_rdf_model_add_st(model, st, data->graph);
			librdf_free_statement(st);
		
			/* For each subject in the graph, add triples stating that:
			 *   ex:subject wdrs:describedby ex:graphuri .
			 */
			stream = librdf_model_context_as_stream(data->sourcedata, node);
			for(; !librdf_stream_end(stream); librdf_stream_next(stream))
			{
				statement = librdf_stream_get_object(stream);
				subject = librdf_statement_get_subject(statement);
				if(!librdf_node_is_resource(subject))
				{
					continue;
				}
				if(librdf_node_equals(node, subject))
				{
					continue;
				}
				st = twine_rdf_st_create();
				librdf_statement_set_subject(st, librdf_new_node_from_node(subject));
				librdf_statement_set_predicate(st, twine_rdf_node_createuri(NS_POWDER "describedby"));
				librdf_statement_set_object(st, librdf_new_node_from_node(node));
				twine_rdf_model_add_st(model, st, data->graph);
				librdf_free_statement(st);

				/* Add <doc> rdfs:seeAlso <source> */
				st = twine_rdf_st_create();
				librdf_statement_set_subject(st, librdf_new_node_from_node(data->doc));
				librdf_statement_set_predicate(st, twine_rdf_node_createuri(NS_RDFS "seeAlso"));
				librdf_statement_set_object(st, librdf_new_node_from_node(node));
				twine_rdf_model_add_st(model, st, data->graph);
				librdf_free_statement(st);
			}
			librdf_free_stream(stream);

			librdf_iterator_next(iter);
		}
		librdf_free_iterator(iter);
		spindle_cache_store(data, "graphs", model);
	}
	if(data->generate->describedby)
	{
		stream = librdf_model_as_stream(model);
		librdf_model_add_statements(data->proxydata, stream);
		librdf_free_stream(stream);
	}
	twine_rdf_model_destroy(model);
	return 0;
}


