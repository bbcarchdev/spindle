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

static int spindle_entry_cleanup_literalset_(struct spindle_literalset_struct *set);

/* Initialise a data structure used to hold state while an individual proxy
 * entity is updated to reflect modified source data.
 */
int
spindle_entry_init(SPINDLEENTRY *data, SPINDLEGENERATE *generate, const char *localname)
{	
	char *t;

	twine_logf(LOG_DEBUG, PLUGIN_NAME " ---------------------------------------\n");
	memset(data, 0, sizeof(SPINDLEENTRY));
	data->generate = generate;
	data->spindle = generate->spindle;
	data->rules = generate->rules;
	data->sparql = generate->sparql;
	data->db = generate->db;
	data->localname = localname;
	data->score = 50;
	data->sameas = generate->spindle->sameas;
	if(data->db)
	{
		data->id = spindle_db_id(localname);
		if(!data->id)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain UUID for <%s>\n", localname);
			return -1;
		}
	}
	/* Create a node representing the full URI of the proxy entity */
	data->self = librdf_new_node_from_uri_string(generate->spindle->world, (const unsigned char *) localname);
	if(!data->self)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create node for <%s>\n", localname);
		return -1;
	}
	/* Create a node representing the full URI of the proxy resource */
	data->docname = strdup(localname);		
	t = strchr(data->docname, '#');
	if(t)
	{
		*t = 0;
	}
	data->doc = librdf_new_node_from_uri_string(generate->spindle->world, (unsigned const char *) data->docname);
	/* Set data->graphname to the URI of the named graph which will contain the proxy data, and
	 * data->graph to the corresponding node
	 */
	if(generate->spindle->multigraph)
	{
		data->graphname = strdup(data->docname);
		data->graph = data->doc;
	}
	else
	{
		data->graphname = strdup(generate->spindle->root);
		data->graph = generate->spindle->rootgraph;
	}

	/* The rootdata model holds proxy data which is stored in the root
	 * graph for convenience
	 */
	if(!(data->rootdata = twine_rdf_model_create()))
	{
		return -1;
	}
	/* The sourcedata model holds data from the external data sources */
	if(!(data->sourcedata = twine_rdf_model_create()))
	{
		return -1;
	}
	/* The proxydata model holds the contents of the proxy graph */
	if(!(data->proxydata = twine_rdf_model_create()))
	{
		return -1;
	}
	/* The extradata model holds graphs which are related to this subject,
	 * and are fetched and cached in an S3 bucket if available.
	 */
	if(!(data->extradata = twine_rdf_model_create()))
	{
		return -1;
	}
	if(!(data->sources = spindle_strset_create()))
	{
		return -1;
	}
	return 0;
}

/* Clean up the proxy entity update state data structure */
int
spindle_entry_cleanup(SPINDLEENTRY *data)
{
	size_t c;

	twine_logf(LOG_DEBUG, PLUGIN_NAME " ---------------------------------------\n");
	spindle_entry_cleanup_literalset_(&(data->titleset));
	spindle_entry_cleanup_literalset_(&(data->descset));
	spindle_strset_destroy(data->sources);
	if(data->refs)
	{
		spindle_proxy_refs_destroy(data->refs);
	}
	if(data->doc)
	{
		librdf_free_node(data->doc);
	}
	if(data->self)
	{
		librdf_free_node(data->self);
	}
	if(data->rootdata)
	{
		twine_rdf_model_destroy(data->rootdata);
	}
	if(data->proxydata)
	{
		twine_rdf_model_destroy(data->proxydata);
	}
	if(data->sourcedata)
	{
		twine_rdf_model_destroy(data->sourcedata);
	}
	if(data->extradata)
	{
		twine_rdf_model_destroy(data->extradata);
	}
	if(data->classes)
	{
		spindle_strset_destroy(data->classes);
	}
	for(c = 0; c < data->ntriggers; c++)
	{
		free(data->triggers[c].uri);
	}
	free(data->triggers);
	/* Never free data->graph - it is a pointer to data->doc or spindle->rootgraph */
	free(data->id);
	free(data->title);
	free(data->title_en);
	free(data->docname);
	free(data->graphname);
	return 0;
}

/* Free resources used by a literal set */
static int
spindle_entry_cleanup_literalset_(struct spindle_literalset_struct *set)
{
	size_t c;

	for(c = 0; c < set->nliterals; c++)
	{
		free(set->literals[c].str);
	}
	free(set->literals);
	return 0;
}
