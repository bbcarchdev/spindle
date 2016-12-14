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

#include "p_spindle-generate.h"

static int spindle_source_fetch_sparql_(SPINDLEENTRY *data);
static int spindle_source_clean_(SPINDLEENTRY *data);
static int spindle_source_refs_(SPINDLEENTRY *data);
static int spindle_source_graphs_(SPINDLEENTRY *data);

/* Obtain cached source data for processing */
int
spindle_source_fetch_entry(SPINDLEENTRY *data)
{
	int r;
	
	if(!(data->flags & TK_PROXY))
	{
		/* If the co-references haven't changed, we can use cached
		 * source data if it's available.
		 */
		r = spindle_cache_fetch(data, "source", data->sourcedata);
		if(r < 0)
		{
			return -1;
		}
		if(r > 0)
		{
			/* N-Quads were retrieved from the cache */
			if(spindle_source_refs_(data))
			{
				return -1;
			}
			if (spindle_source_graphs_(data))
			{
				return -1;
			}
			return 0;
		}
	}
	if(spindle_source_refs_(data))
	{
		return -1;
	}
	if(spindle_source_fetch_sparql_(data))
	{
		return -1;
	}
	if(spindle_source_clean_(data))
	{
		return -1;
	}
	if (spindle_source_graphs_(data))
	{
		return -1;
	}
	if(spindle_cache_store(data, "source", data->sourcedata))
	{
		return -1;
	}
	return 0;
}

static int
spindle_source_refs_(SPINDLEENTRY *data)
{
	size_t c;
	librdf_statement *st;

	if(!data->refs)
	{
		data->refs = spindle_proxy_refs(data->spindle, data->localname);
		if(!data->refs)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain co-references for <%s>\n", data->localname);
			return -1;
		}
	}
	data->refcount = 0;
	for(c = 0; data->refs[c]; c++)
	{
		data->refcount++;
		/* Add <ref> owl:sameAs <localname> triples to the proxy model */
		st = twine_rdf_st_create();
		librdf_statement_set_subject(st, twine_rdf_node_createuri(data->refs[c]));
		librdf_statement_set_predicate(st, twine_rdf_node_clone(data->spindle->sameas));
		librdf_statement_set_object(st, twine_rdf_node_createuri(data->localname));
		librdf_model_context_add_statement(data->proxydata, data->graph, st);
		twine_rdf_st_destroy(st);
	}
	return 0;
}

/* Add the graph URIs in the source data to the sources list, assuming there
 * are no internal graph URIs in the source data (so spindle_source_clean_() was
 * called first)
 */
static int
spindle_source_graphs_(SPINDLEENTRY *data)
{
	librdf_iterator *iter;
	librdf_node *node;
	librdf_uri *nodeuri;
	const char *nodeuristr;

	iter = librdf_model_get_contexts(data->sourcedata);
	while(!librdf_iterator_end(iter))
	{
		node = librdf_iterator_get_object(iter);
		nodeuri = librdf_node_get_uri(node);
		if(!nodeuri)
		{
			continue;
		}
		nodeuristr = (const char *) librdf_uri_as_string(nodeuri);
		if(!nodeuristr)
		{
			continue;
		}
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": adding graph <%s> to sources list\n", nodeuristr);
		spindle_strset_add(data->sources, nodeuristr);
		librdf_iterator_next(iter);
	}
	librdf_free_iterator(iter);
	return 0;
}

/* Fetch all of the source data about the entities that relate to a particular
 * proxy using the database
 */
static int
spindle_source_fetch_sparql_(SPINDLEENTRY *data)
{
	int r;
	size_t c;

	r = 0;
	for(c = 0; data->refs[c]; c++)
	{
		/* Add <ref> owl:sameAs <localname> triples to the proxy model */
//		if(sparql_queryf_model(data->sparql, data->sourcedata,
//			"SELECT DISTINCT ?s ?p ?o ?g"
//			" WHERE {"
//			"  GRAPH ?g {"
//			"  { <%s> ?p ?o ."
//			"   BIND(<%s> as ?s)"
//			"  }"
//			"  UNION"
//			"  { ?s ?p <%s> ."
//			"   BIND(<%s> as ?o)"
//			"  }"
//			" }"
//			"}",
//			data->refs[c], data->refs[c], data->refs[c], data->refs[c]))
		if (twine_cache_fetch_about_(data->sourcedata, data->refs[c]))
		{
			r = -1;
			break;
		}
	}
	return r;
}



/* Remove any proxy data from the source model */
static int
spindle_source_clean_(SPINDLEENTRY *data)
{
	librdf_iterator *iterator;
	librdf_node *node;
	librdf_uri *uri;
	const char *uristr;

	iterator = librdf_model_get_contexts(data->sourcedata);
	while(!librdf_iterator_end(iterator))
	{
		node = librdf_iterator_get_object(iterator);
		uri = librdf_node_get_uri(node);
		uristr = (const char *) librdf_uri_as_string(uri);
		if(!strncmp(uristr, data->spindle->root, strlen(data->spindle->root)))
		{
			librdf_model_context_remove_statements(data->sourcedata, node);
		}
		librdf_iterator_next(iterator);
	}
	librdf_free_iterator(iterator);
	return 0;
}
