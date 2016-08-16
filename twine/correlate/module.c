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

#include "p_spindle-correlate.h"

static SPINDLE spindle;

static struct coref_match_struct coref_match_types[] = 
{
	{ NS_SPINDLE "resourceMatch", spindle_match_sameas },
	{ NS_SPINDLE "wikipediaMatch", spindle_match_wikipedia },
	{ NULL, NULL }
};

/* Twine plug-in entry-point */
int
twine_plugin_init(void)
{	
	twine_logf(LOG_DEBUG, PLUGIN_NAME " plug-in: initialising\n");
	if(spindle_init(&spindle))
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": initialisation failed\n");
		return -1;
	}
	spindle.rules = spindle_rulebase_create(NULL, coref_match_types);
	if(!spindle.rules)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to load rule-base\n");
		return -1;
	}
	if(spindle_db_init(&spindle))
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to connect to database\n");
		return -1;
	}
	twine_logf(LOG_INFO, PLUGIN_NAME ": URI prefix is <%s>\n", spindle.root);
	twine_postproc_register("spindle", spindle_correlate, &spindle);
	twine_graph_register(PLUGIN_NAME, spindle_correlate, &spindle);
	if(twine_config_get_bool(PLUGIN_NAME ":dumprules", twine_config_get_bool("spindle:dumprules", 0)))
	{
		spindle_rulebase_dump(spindle.rules);
	}
	return 0;
}

int
twine_plugin_done(void)
{
	twine_logf(LOG_DEBUG, PLUGIN_NAME " plug-in: cleaning up\n");
	spindle_cleanup(&spindle);
	return 0;
}

/* Discard cached information about a graph */
int
spindle_graph_discard(SPINDLE *spindle, const twine_graph *graph)
{
	/* This is a no-op in the stand-alone post-processor; it can be
	 * removed when we no longer have a monolithic module.
	 */
	(void) spindle;

	size_t c;
	librdf_stream *st;
	librdf_statement *statement;
	librdf_node *subject;
	librdf_uri *subj;
	const char *subjuri;

	// This will old the list of URIs to remove from the cache
	struct spindle_strset_struct *subjects;
	subjects = spindle_strset_create();
	if(!subjects)
	{
		return -1;
	}

	// Find all the subjects in the graph
	st = librdf_model_as_stream(graph->store);
	while(!librdf_stream_end(st))
	{
		statement = librdf_stream_get_object(st);
		subject = librdf_statement_get_subject(statement);
		if(librdf_node_is_resource(subject) &&
		   (subj = librdf_node_get_uri(subject)) &&
		   (subjuri = (const char *) librdf_uri_as_string(subj)))
		{
			spindle_strset_add(subjects, subjuri);
		}
		librdf_stream_next(st);
	}
	librdf_free_stream(st);

	// Delete all the subjects
	for(c = 0; c < subjects->count; c++)
	{
		subjuri = subjects->strings[c];
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": removing cached data about <%s>\n", subjuri);
	}

	spindle_strset_destroy(subjects);
	return 0;
}

