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

#include "p_spindle-strip.h"

/* Process a graph, stripping out triples using predicates which don't appear
 * in the rule-base
 */
int
spindle_strip(twine_graph *graph, void *data)
{
	librdf_stream *st;
	librdf_statement *statement;
	librdf_node *predicate, *object;
	librdf_uri *pred;
	const char *preduri, *lang;
	int match, r, literal_ok;
	size_t c;
	SPINDLERULES *rules;
	librdf_model *filtered_model;

	rules = (SPINDLERULES *) data;

	filtered_model = twine_rdf_model_create();

	st = librdf_model_as_stream(graph->store);
	while(!librdf_stream_end(st))
	{
		// Get the current statement from the stream
		statement = librdf_stream_get_object(st);

		// Skip all statements that do not use a URI for predicate
		predicate = librdf_statement_get_predicate(statement);
		if(!librdf_node_is_resource(predicate))
			continue;

		// Get the char* version of the URI, if that fails exit with an error
		pred = librdf_node_get_uri(predicate);
		if (!pred)
		{
			librdf_free_stream(st);
			return -1;
		}
		preduri = (const char *) librdf_uri_as_string(pred);
		if (!preduri)
		{
			librdf_free_stream(st);
			return -1;
		}

		// Check all the predicates in the rulebase to see if that one is one
		// we want to keep
		match = 0;
		for(c = 0; c < rules->cpcount; c++)
		{
			r = strcmp(rules->cachepreds[c], preduri);
			if(!r)
			{
				match = 1;
				break;
			}
			if(r > 0)
			{
				/* The cachepreds list is lexigraphically sorted */
				break;
			}
		}

		// If the object is a literal we need to ensure it is using either
		// no language or one that we use for the indexing
		literal_ok = 1;
		object = librdf_statement_get_object(statement);
		if(librdf_node_is_literal(object))
		{
			lang = librdf_node_get_literal_value_language(object);
			if (lang)
			{
				literal_ok = !strncmp(lang, "en", 2) ||
							 !strncmp(lang, "cy", 2) ||
							 !strncmp(lang, "ga", 2) ||
							 !strncmp(lang, "gd", 2);
			}
		}

		// If the predicate is acceptable and the eventual literal is too
		// we add that statement to the filtered model
		if(match && literal_ok)
		{
			twine_logf(LOG_DEBUG, PLUGIN_NAME ": keeping a triple with predicate <%s>\n", preduri);
			librdf_model_add_statement(filtered_model, statement);
		}

		// Move to the next statement
		librdf_stream_next(st);
	}
	librdf_free_stream(st);

	// Swap the non filtered and filtered models
	twine_rdf_model_destroy(graph->store);
	graph->store = filtered_model;

	return 0;
}

