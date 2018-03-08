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

/* Initialise a data structure used to hold state while an individual proxy
 * entity is updated to reflect modified source data.
 * If this function fails, you need to call spindle_entry_cleanup to free resources.
 */
int
spindle_entry_init(SPINDLEENTRY *data, SPINDLEGENERATE *generate, const char *localname)
{
	char *t;
	librdf_node *graph;

	twine_logf(LOG_DEBUG, PLUGIN_NAME " ---------------------------------------\n");
	memset(data, 0, sizeof(SPINDLEENTRY));
	data->generate = generate;
	data->spindle = generate->spindle;
	data->sparql = generate->sparql;
	data->db = generate->db;
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
		graph = data->doc;
	}
	else
	{
		data->graphname = strdup(generate->spindle->root);
		graph = generate->spindle->rootgraph;
	}
	data->proxy = calloc(1, sizeof(PROXYENTRY));
	if(!data->proxy)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate proxy entry object\n");
		return -1;
	}
	if(proxy_entry_init(data->proxy, generate->rules, localname, graph))
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to initialise proxy entry object\n");
		return -1;
	}
	if(!(data->sources = strset_create()))
	{
		return -1;
	}
	return 0;
}

/* Reset the proxy entity update state data structure, so that it can be used
 * again without initialising it (for example, when a db transaction fails) */
int
spindle_entry_reset(SPINDLEENTRY *data)
{
	return proxy_entry_reset(data->proxy);
}

/* Clean up the proxy entity update state data structure */
int
spindle_entry_cleanup(SPINDLEENTRY *data)
{
	size_t c;

	twine_logf(LOG_DEBUG, PLUGIN_NAME " ---------------------------------------\n");
	proxy_entry_cleanup(data->proxy);
	if(data->sources)
	{
		strset_destroy(data->sources);
	}
	if(data->doc)
	{
		librdf_free_node(data->doc);
	}
	for(c = 0; c < data->ntriggers; c++)
	{
		free(data->triggers[c].uri);
	}
	free(data->triggers);
	free(data->id);
	free(data->docname);
	free(data->proxy);
	return 0;
}
