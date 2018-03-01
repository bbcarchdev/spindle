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

/* Determine the class of something */
int
spindle_class_match(SPINDLEENTRY *cache, struct strset_struct *classes)
{
	librdf_statement *query, *st;
	librdf_node *node;
	librdf_stream *stream;
	librdf_uri *uri;
	unsigned char *uristr;
	size_t c, d, n;
	struct rulebase_classmap_struct *mapentry;
	struct spindle_classmatch_struct *match;
	int score;

	mapentry = NULL;
	match = NULL;
	score = 1000;
	node = librdf_new_node_from_uri_string(cache->spindle->world, (const unsigned char *) NS_RDF "type");
	query = librdf_new_statement(cache->spindle->world);
	librdf_statement_set_predicate(query, node);
	stream = librdf_model_find_statements(cache->sourcedata, query);
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		node = librdf_statement_get_object(st);
		if(librdf_node_is_resource(node) &&
		   (uri = librdf_node_get_uri(node)) &&
		   (uristr = librdf_uri_as_string(uri)))
		{
			if(classes)
			{
				strset_add(classes, (const char *) uristr);
			}
			for(c = 0; c < cache->rules->classcount; c++)
			{
				if(cache->rules->classes[c].score > score)
				{
					continue;
				}
				for(d = 0; d < cache->rules->classes[c].matchcount; d++)
				{
					if(!strcmp((const char *) uristr, cache->rules->classes[c].match[d].uri))
					{
						mapentry = &(cache->rules->classes[c]);
						match = &(cache->rules->classes[c].match[d]);
						score = cache->rules->classes[c].score;
						if(classes)
						{
							strset_add(classes, mapentry->uri);
						}
						break;
					}
				}
			}
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	if(!match)
	{
		/* Attempt to infer the class using the coreferences of the entity */
		for(c = 0; c < cache->rules->classcount; c++)
		{
			if(!cache->rules->classes[c].roots)
			{
				/* This class has no roots */
				continue;
			}
			if(cache->rules->classes[c].score > score)
			{
				/* We've previously matched a higher-prominence class */
				continue;
			}
			for(d = 0; d < cache->rules->classes[c].roots->count; d++)
			{
				for(n = 0; cache->refs[n]; n++)
				{
					/* Check the entry's coreference URIs against the
					 * class root URIs from the rulebase
					 */
					if(!strncmp(cache->refs[n], cache->rules->classes[c].roots->strings[d],
								strlen(cache->rules->classes[c].roots->strings[d])))
					{
						mapentry = &(cache->rules->classes[c]);
						match = &(cache->rules->classes[c].match[d]);
						score = cache->rules->classes[c].score;
						if(classes)
						{
							spindle_strset_add(classes, mapentry->uri);
						}
						break;
					}
				}
			}
		}
	}
	if(!match)
	{
		twine_logf(LOG_WARNING, PLUGIN_NAME ": no class match for object <%s>\n", cache->localname);
		for(c = 0; c < classes->count; c++)
		{
			twine_logf(LOG_INFO, PLUGIN_NAME ": <%s>\n", classes->strings[c]);
		}
		cache->classname = NULL;
		return 0;
	}
	twine_logf(LOG_DEBUG, "==> Class is <%s>\n", mapentry->uri);
	if(match->prominence)
	{
		cache->score -= match->prominence;
	}
	else
	{
		cache->score -= mapentry->prominence;
	}
	cache->classname = mapentry->uri;
	return 1;
}

/* Update the classes of a proxy */
int
spindle_class_update_entry(SPINDLEENTRY *cache)
{
	struct strset_struct *classes;
	size_t c;
	librdf_node *node;
	librdf_statement *base, *st;

	classes = strset_create();
	if(!classes)
	{
		return -1;
	}
	if(spindle_class_match(cache, classes) < 0)
	{
		return -1;
	}
	base = librdf_new_statement(cache->spindle->world);
	node = librdf_new_node_from_uri_string(cache->spindle->world, (const unsigned char *) cache->localname);
	librdf_statement_set_subject(base, node);
	node = librdf_new_node_from_node(cache->spindle->rdftype);
	librdf_statement_set_predicate(base, node);
	for(c = 0; c < classes->count; c++)
	{
		twine_logf(LOG_DEBUG, "--> Adding class <%s>\n", classes->strings[c]);
		st = librdf_new_statement_from_statement(base);
		node = librdf_new_node_from_uri_string(cache->spindle->world, (const unsigned char *) classes->strings[c]);
		librdf_statement_set_object(st, node);
		if(librdf_model_context_add_statement(cache->proxydata, cache->graph, st))
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to add rdf:type <%s> statement to model\n", classes->strings[c]);
			librdf_free_statement(st);
			librdf_free_statement(base);
			strset_destroy(classes);
			return -1;
		}
		if(cache->spindle->multigraph && cache->classname && !strcmp(cache->classname, classes->strings[c]))
		{
			librdf_model_context_add_statement(cache->rootdata, cache->spindle->rootgraph, st);
		}
		librdf_free_statement(st);
	}
	librdf_free_statement(base);
	cache->classes = classes;
	return 0;
}
