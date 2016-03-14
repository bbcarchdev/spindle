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

static int spindle_generate_init_(SPINDLEGENERATE *generate);
static int spindle_generate_cleanup_(SPINDLEGENERATE *generate);

static SPINDLE spindle;
static SPINDLEGENERATE generate;

/* Twine plug-in entry-point */
int
twine_plugin_init(void)
{	
	twine_logf(LOG_DEBUG, PLUGIN_NAME " plug-in: initialising\n");
	if(spindle_generate_init_(&generate))
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": initialisation failed\n");
		return -1;
	}
	twine_logf(LOG_INFO, PLUGIN_NAME ": URI prefix is <%s>\n", spindle.root);
	twine_postproc_register(PLUGIN_NAME, spindle_generate_graph, &generate);
	twine_graph_register(PLUGIN_NAME, spindle_generate_graph, &generate);
	twine_update_register("spindle", spindle_generate_update, &generate);
	twine_plugin_register(SPINDLE_URI_MIME, "Spindle proxy URI", spindle_generate_message, &generate);
	spindle_mq_init(NULL);
	return 0;
}

int
twine_plugin_done(void)
{
	twine_logf(LOG_DEBUG, PLUGIN_NAME " plug-in: cleaning up\n");
	spindle_generate_cleanup_(&generate);
	return 0;
}

static int
spindle_generate_init_(SPINDLEGENERATE *generate)
{
	memset(generate, 0, sizeof(SPINDLEGENERATE));
	if(spindle_init(&spindle))
	{
		return -1;
	}
	generate->spindle = &spindle;
	spindle.rules = spindle_rulebase_create(NULL, NULL);
	if(!spindle.rules)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to load rule-base\n");
		return -1;
	}
	generate->rules = spindle.rules;
	if(spindle_cache_init(generate))
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to initialise S3 bucket\n");
		return -1;
	}
	if(spindle_doc_init(generate))
	{
		return -1;
	}
	if(spindle_license_init(generate))
	{
		return -1;
	}
	if(spindle_db_init(&spindle))
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to connect to database\n");
		return -1;
	}	
	generate->db = spindle.db;
	generate->sparql = spindle.sparql;
	generate->graphcache = (struct spindle_graphcache_struct *) calloc(SPINDLE_GRAPHCACHE_SIZE, sizeof(struct spindle_graphcache_struct));
	if(!generate->graphcache)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create graph cache\n");
		return -1;
	}
	if(twine_config_get_bool(PLUGIN_NAME ":dumprules", twine_config_get_bool("spindle:dumprules", 0)))
	{
		spindle_rulebase_dump(generate->rules);
	}
	return 0;
}

static int
spindle_generate_cleanup_(SPINDLEGENERATE *generate)
{
	size_t c;

	if(generate->spindle)
	{
		spindle_cleanup(generate->spindle);
	}
	if(generate->graphcache)
	{
		for(c = 0; generate->graphcache && c < SPINDLE_GRAPHCACHE_SIZE; c++)
		{
			if(!generate->graphcache[c].uri)
			{
				continue;
			}
			twine_rdf_model_destroy(generate->graphcache[c].model);
			free(generate->graphcache[c].uri);
		}
		free(generate->graphcache);
	}
	free(generate->titlepred);
	return 0;
}

#if 0
/* Discard cached information about a graph */
int
spindle_graph_discard(SPINDLE *spindle, const char *uri)
{
	size_t c;

	for(c = 0; c < SPINDLE_GRAPHCACHE_SIZE; c++)
	{
		if(!spindle->graphcache[c].uri)
		{
			continue;
		}
		if(!strcmp(generate->graphcache[c].uri, uri))
		{
			twine_rdf_model_destroy(generate->graphcache[c].model);
			free(generate->graphcache[c].uri);
			memset(&(generate->graphcache[c]), 0, sizeof(struct spindle_graphcache_struct));
			return 0;
		}
	}
	return 0;
}
#endif

/* Fetch information about a graph */
int
spindle_graph_description_node(SPINDLEGENERATE *generate, librdf_model *target, librdf_node *graph)
{
	size_t c;
	librdf_stream *stream;
	librdf_uri *uri;
	librdf_model *temp;
	librdf_statement *query;
	librdf_node *node;
	const char *uristr;

	uri = librdf_node_get_uri(graph);
	uristr = (const char *) librdf_uri_as_string(uri);
	for(c = 0; c < SPINDLE_GRAPHCACHE_SIZE; c++)
	{
		if(!generate->graphcache[c].uri)
		{
			continue;
		}
		if(!strcmp(generate->graphcache[c].uri, uristr))
		{
			stream = librdf_model_context_as_stream(generate->graphcache[c].model, graph);
			twine_rdf_model_add_stream(target, stream, graph);
			librdf_free_stream(stream);
			return 0;
		}
	}
	for(c = 0; c < SPINDLE_GRAPHCACHE_SIZE; c++)
	{
		if(!generate->graphcache[c].uri)
		{
			break;
		}
	}
	if(c == SPINDLE_GRAPHCACHE_SIZE)
	{
		twine_rdf_model_destroy(generate->graphcache[0].model);
		free(generate->graphcache[0].uri);
		memmove(&(generate->graphcache[0]), &(generate->graphcache[1]),
				sizeof(struct spindle_graphcache_struct) * (SPINDLE_GRAPHCACHE_SIZE - 1));
		c = SPINDLE_GRAPHCACHE_SIZE - 1;
		generate->graphcache[c].model = NULL;
		generate->graphcache[c].uri = NULL;
	}
	generate->graphcache[c].model = twine_rdf_model_create();
	generate->graphcache[c].uri = strdup(uristr);
	temp = twine_rdf_model_create();
	if(sparql_queryf_model(generate->sparql, temp,
						   "SELECT DISTINCT ?s ?p ?o\n"
						   " WHERE {\n"
						   "  GRAPH %V {\n"
						   "   ?s ?p ?o .\n"
						   "  }\n"
						   " }", graph))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to fetch a graph description\n");
		return -1;
	}	
/*
	if(sparql_queryf_model(spindle->sparql, generate->graphcache[c].model,
						   "SELECT DISTINCT ?s ?p ?o ?g\n"
						   " WHERE {\n"
						   "  GRAPH ?g {\n"
						   "   ?s ?p ?o .\n"
						   "   FILTER (?g = %V && ?s = ?g)\n"
						   "  }\n"
						   "}",
						   graph))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to fetch a graph description\n");
		return -1;
		} */
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": added graph <%s> to cache\n", uristr);
	query = librdf_new_statement(generate->spindle->world);
	node = librdf_new_node_from_node(graph);
	librdf_statement_set_subject(query, node);

	stream = librdf_model_find_statements(temp, query);
	librdf_model_context_add_statements(generate->graphcache[c].model, graph, stream);
	librdf_free_stream(stream);

	stream = librdf_model_find_statements(temp, query);
	librdf_model_context_add_statements(target, graph, stream);
	librdf_free_stream(stream);

	librdf_free_statement(query);
	librdf_free_model(temp);

	return 0;
}

