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

static int spindle_store_sparql_(SPINDLEENTRY *entry);
static int spindle_store_cache_(SPINDLEENTRY *entry);

/* Depending upon configuration, store the generated data either in a SPARQL
 * store, or as serialised N-Quads in a cache location.
 */
int
spindle_store_entry(SPINDLEENTRY *entry)
{
	if(entry->generate->bucket || entry->generate->cachepath)
	{
		return spindle_store_cache_(entry);
	}
	return spindle_store_sparql_(entry);
}

static int
spindle_store_sparql_(SPINDLEENTRY *entry)
{
	char *triples;
	size_t triplen;

	/* First update the root graph - only if we're not using a SQL-based
	 * index
	 */
	
	/* Note that our owl:sameAs statements take the form
	 * <external> owl:sameAs <proxy>, so we can delete <proxy> ?p ?o with
	 * impunity.
	 */
	if(sparql_updatef(entry->sparql,
					  "WITH %V\n"
					  " DELETE { %V ?p ?o }\n"
					  " WHERE { %V ?p ?o }",
					  entry->spindle->rootgraph, entry->self, entry->self))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to delete previously-cached triples\n");
		return -1;
	}
	if(sparql_updatef(entry->sparql,
					  "WITH %V\n"
					  " DELETE { %V ?p ?o }\n"
					  " WHERE { %V ?p ?o }",
					  entry->spindle->rootgraph, entry->doc, entry->doc))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to delete previously-cached triples\n");
		return -1;
	}
	if(sparql_insert_model(entry->sparql, entry->rootdata))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to push new proxy data into the root graph of the store\n");
		return -1;
	}
	
	/* Now update the proxy data */
	if(entry->spindle->multigraph)
	{
		triples = twine_rdf_model_ntriples(entry->proxydata, &triplen);
		if(sparql_put(entry->sparql, entry->graphname, triples, triplen))
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to push new proxy data into the store\n");
			librdf_free_memory(triples);
			return -1;
		}
		librdf_free_memory(triples);
	}
	else
	{
		if(sparql_updatef(entry->sparql,
						  "WITH %V\n"
						  " DELETE { %V ?p ?o }\n"
						  " WHERE { %V ?p ?o }",
						  entry->graph, entry->self, entry->self))
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to delete previously-cached triples\n");
			return -1;
		}
		/* Insert the new proxy triples, if any */
		if(sparql_insert_model(entry->sparql, entry->proxydata))
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to push new proxy data into the store\n");
			return -1;
		}
	}
	return 0;
}

/* Generate a set of pre-composed N-Quads representing an entity we have
 * indexed and write it to a location for the Quilt module (or indeed anything
 * else) to read.
 */
static int
spindle_store_cache_(SPINDLEENTRY *data)
{
	char *proxy, *source, *extra, *buf;
	size_t bufsize, proxylen, sourcelen, extralen, l;
	int r;
	
	/* If there's no S3 bucket nor cache-path, this is a no-op */
	if(!data->generate->bucket &&
		!data->generate->cachepath)
	{
		return 0;
	}
	if(data->spindle->multigraph)
	{
		/* Remove the root graph from the proxy data model, if it's present */
		librdf_model_context_remove_statements(data->proxydata, data->spindle->rootgraph);
	}
	proxy = twine_rdf_model_nquads(data->proxydata, &proxylen);
	if(!proxy)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to serialise proxy model as N-Quads\n");
		return -1;
	}
	source = twine_rdf_model_nquads(data->sourcedata, &sourcelen);
	if(!source)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to serialise source model as N-Quads\n");
		librdf_free_memory(proxy);
		return -1;
	}

	extra = twine_rdf_model_nquads(data->extradata, &extralen);
	if(!extra)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to serialise extra model as N-Quads\n");
		librdf_free_memory(proxy);
		librdf_free_memory(source);
		return -1;
	}
	bufsize = proxylen + sourcelen + extralen + 3 + 128;
	buf = (char *) calloc(1, bufsize + 1);
	if(!buf)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for consolidated N-Quads\n");
		librdf_free_memory(proxy);
		librdf_free_memory(source);
		librdf_free_memory(extra);
		return -1;
	}
	strcpy(buf, "## Proxy:\n");
	l = strlen(buf);

	if(proxylen)
	{
		memcpy(&(buf[l]), proxy, proxylen);
	}
	strcpy(&(buf[l + proxylen]), "\n## Source:\n");
	l = strlen(buf);

	if(sourcelen)
	{
		memcpy(&(buf[l]), source, sourcelen);
	}
	strcpy(&(buf[l + sourcelen]), "\n## Extra:\n");
	l = strlen(buf);
	
	if(extralen)
	{
		memcpy(&(buf[l]), extra, extralen);
	}
	strcpy(&(buf[l + extralen]), "\n## End\n");
	bufsize = strlen(buf);

	librdf_free_memory(proxy);
	librdf_free_memory(source);
	librdf_free_memory(extra);

	r = spindle_cache_store_buf(data, NULL, buf, bufsize);

	free(buf);
	return r;
}
