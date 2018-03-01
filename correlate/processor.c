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

#include "p_spindle-correlate.h"

struct spindle_correlate_data_struct
{
	SPINDLE *spindle;
	twine_graph *graph;
	struct rulebase_corefset_struct *oldset;
	struct rulebase_corefset_struct *newset;
	struct strset_struct *changes;
};

static int spindle_correlate_internal_(struct spindle_correlate_data_struct *cbdata);

/* "spindle-correlate" workflow processor
 *
 * This hook is invoked once (preprocessed) RDF has been pushed into the
 * quad-store and is responsible for correlating references against each
 * other and generating proxy URIs
 */
int
spindle_correlate(twine_graph *graph, void *data)
{
	SPINDLE *spindle;
	struct rulebase_corefset_struct *oldset, *newset;
	struct strset_struct *changes;
	int r;
	struct spindle_correlate_data_struct cbdata;

	spindle = (SPINDLE *) data;
	twine_logf(LOG_INFO, PLUGIN_NAME ": evaluating updated graph <%s>\n", graph->uri);
	spindle_graph_discard(spindle, graph->uri);
	changes = strset_create();
	if(!changes)
	{
		return -1;
	}
	/* find all owl:sameAs refs where either side is same-origin as graph */
	if(graph->old)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": extracting references from existing graph\n");
		oldset = spindle_coref_extract(spindle, graph->old, graph->uri);
		if(!oldset)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to extract co-references from previous graph state\n");
			strset_destroy(changes);
			return -1;
		}
	}
	else
	{
		oldset = spindle_coref_create();
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": extracting references from new graph\n");
	newset = spindle_coref_extract(spindle, graph->store, graph->uri);
	if(!newset)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to extract co-references from new graph state\n");
		strset_destroy(changes);
		spindle_coref_destroy(oldset);
		return -1;
	}
	/* For each co-reference in the new graph, represent that within our
	 * local proxy graph
	 */
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": new graph contains %d coreferences\n", (int) newset->refcount);
	cbdata.spindle = spindle;
	cbdata.graph = graph;
	cbdata.oldset = oldset;
	cbdata.newset = newset;
	cbdata.changes = changes;
	r = 0;
	if(spindle_correlate_internal_(&cbdata) < 0)
	{
		r = -1;
	}
	spindle_coref_destroy(oldset);
	spindle_coref_destroy(newset);
	strset_destroy(changes);
	if(r)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to create proxy entities for graph <%s>\n", graph->uri);
	}
	else
	{
		twine_logf(LOG_INFO, PLUGIN_NAME ": processing complete for graph <%s>\n", graph->uri);
	}
	return r;
}

static int
spindle_correlate_internal_(struct spindle_correlate_data_struct *cbdata)
{
	size_t c;

	for(c = 0; c < cbdata->newset->refcount; c++)
	{
		if(spindle_proxy_create(cbdata->spindle, cbdata->newset->refs[c].left, cbdata->newset->refs[c].right, cbdata->changes))
		{
			return SQL_TXN_FAIL;
		}
	}
	return SQL_TXN_COMMIT;
}
