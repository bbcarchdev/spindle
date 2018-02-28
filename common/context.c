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

#include "p_spindle.h"

int
spindle_init(SPINDLE *spindle)
{
	memset(spindle, 0, sizeof(SPINDLE));
	spindle->world = twine_rdf_world();
	if(!spindle->world)
	{
		return -1;
	}
	spindle->multigraph = twine_config_get_bool("spindle:multigraph", 0);
	spindle->root = twine_config_geta("spindle:graph", NULL);
	if(!spindle->root)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to obtain Spindle root graph name\n");
		return -1;
	}
	spindle->sparql = twine_sparql_create();
	if(!spindle->sparql)
	{
		return -1;
	}
	spindle->sameas = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) NS_OWL "sameAs");
	if(!spindle->sameas)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create node for owl:sameAs\n");
		return -1;
	}
	spindle->rdftype = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) NS_RDF "type");
	if(!spindle->rdftype)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create node for rdf:type\n");
		return -1;
	}
	spindle->rootgraph = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) spindle->root);
	if(!spindle->rootgraph)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create node for <%s>\n", spindle->root);
		return -1;
	}	
	spindle->modified = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) NS_DCTERMS "modified");
	if(!spindle->modified)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create node for dct:modified\n");
		return -1;
	}
	spindle->xsd_dateTime = librdf_new_uri(spindle->world, (const unsigned char *) NS_XSD "dateTime");
	if(!spindle->xsd_dateTime)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create URI for xsd:dateTime\n");
		return -1;
	}
	spindle->graphcache = (struct spindle_graphcache_struct *) calloc(SPINDLE_GRAPHCACHE_SIZE, sizeof(struct spindle_graphcache_struct));
	if(!spindle->graphcache)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create graph cache\n");
		return -1;
	}
	return 0;
}

int
spindle_cleanup(SPINDLE *spindle)
{
	size_t c;
	
	if(spindle->sparql)
	{
		sparql_destroy(spindle->sparql);
	}
	if(spindle->root)
	{
		free(spindle->root);
	}
	if(spindle->sameas)
	{
		librdf_free_node(spindle->sameas);
	}
	if(spindle->rdftype)
	{
		librdf_free_node(spindle->rdftype);
	}
	if(spindle->rootgraph)
	{
		librdf_free_node(spindle->rootgraph);
	}
	if(spindle->modified)
	{
		librdf_free_node(spindle->modified);
	}
	if(spindle->xsd_dateTime)
	{
		librdf_free_uri(spindle->xsd_dateTime);
	}
	if(spindle->rules)
	{
		rulebase_destroy(spindle->rules);
	}
	if(spindle->graphcache)
	{
		for(c = 0; spindle->graphcache && c < SPINDLE_GRAPHCACHE_SIZE; c++)
		{
			if(!spindle->graphcache[c].uri)
			{
				continue;
			}
			twine_rdf_model_destroy(spindle->graphcache[c].model);
			free(spindle->graphcache[c].uri);
		}
		free(spindle->graphcache);
	}
	spindle_db_cleanup(spindle);
	return 0;
}
