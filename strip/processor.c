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

#include "p_spindle-strip.h"

typedef int (*SPINDLESTRIPFN)(RULEBASE *restrict rules, librdf_statement *restrict statement, const char *restrict uristr);

static int spindle_strip_is_cachepred_(RULEBASE *restrict rules, librdf_statement *restrict st, const char *restrict uristr);

static SPINDLESTRIPFN spindle_strip_rules_[] = {
	spindle_strip_is_cachepred_,
	NULL
};

/* Process a graph, stripping out triples using predicates which don't appear
 * in the rule-base
 */
int
spindle_strip(twine_graph *graph, void *data)
{
	librdf_stream *st;
	librdf_statement *statement;
	librdf_node *predicate;
	librdf_uri *uri;
	const char *uristr;
	int r, n, keep;
	size_t c;
	RULEBASE *rules;
	librdf_model *filtered_model;

	rules = (RULEBASE *) data;

	filtered_model = twine_rdf_model_create();

	r = 0;
	for(st = librdf_model_as_stream(graph->store); !librdf_stream_end(st); librdf_stream_next(st))
	{
		statement = librdf_stream_get_object(st);
		if(!statement)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain statement from model stream\n");
			r = -1;
			break;
		}

		predicate = librdf_statement_get_predicate(statement);
		if(!predicate)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain predicate from statement\n");
			r = -1;
			break;
		}
		if(!librdf_node_is_resource(predicate))
		{
			twine_logf(LOG_DEBUG, PLUGIN_NAME ": ignoring statement with non-resource predicate\n");
			continue;
		}
		uri = librdf_node_get_uri(predicate);
		if(!uri)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain URI from predicate\n");
			r = -1;
			break;
		}
		uristr = (const char *) librdf_uri_as_string(uri);
		if(!uristr)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain string for URI\n");
			r = -1;
			break;
		}

		/* Loop each of the callback functions in spindle_strip_rules_[] in
		 * turn, passing the rulebase, the librdf statement, and the
		 * predicate URI string.
		 *
		 * If the callback function returns 1, we mark the triple as being
		 * left un-stripped and stop processing.
		 *
		 * If the callback function returns -1, we abort with an error.
		 *
		 * If all of the functions return 0, the triple is stripped
		 * from the graph.
		 */
		keep = 0;
		for(c = 0; spindle_strip_rules_[c]; c++)
		{
			n = spindle_strip_rules_[c](rules, statement, uristr);
			if(n < 0)
			{
				r = -1;
				break;
			}
			if(n > 0)
			{
				keep = 1;
				break;
			}
		}
		/* If an error occurred in the loop above, break out of this
		 * one
		 */
		if(r < 0)
		{
			break;
		}

		/* If keep is nonzero, add the triple to the filtered model that will
		 * replace the input model
		 */
		if(keep)
		{
			twine_logf(LOG_DEBUG, PLUGIN_NAME ": keeping a triple with predicate <%s>\n", uristr);
			librdf_model_add_statement(filtered_model, statement);
		}
		else
		{
			twine_logf(LOG_DEBUG, PLUGIN_NAME ": stripping a triple with predicate <%s>\n", uristr);
		}
	}
	librdf_free_stream(st);
	if(!r)
	{
		/* If no error occurred during the loop above, replace graph->store
		 * with our new filtered model.
		 */
		twine_rdf_model_destroy(graph->store);
		graph->store = filtered_model;
	}
	return r;
}

/* Determine whether a triple should be kept because its predicate is
 * in the 'cachepred' list from the rulebase
 */
static int
spindle_strip_is_cachepred_(RULEBASE *rules, librdf_statement *st, const char *uristr)
{
	size_t c;
	int r;

	(void) st;

	for(c = 0; c < rules->cpcount; c++)
	{
		r = strcmp(rules->cachepreds[c], uristr);
		if(!r)
		{
			return 1;
		}
		if(r > 0)
		{
			/* The cachepreds list is lexicographically sorted */
			break;
		}
	}
	return 0;
}
