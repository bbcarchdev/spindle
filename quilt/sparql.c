/* This engine processes requests for coreference graphs populated
 * by Twine's "spindle" post-processing module.
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

static int spindle_index_metadata_sparqlres_(QUILTREQ *request, struct query_struct *query, SPARQLRES *res);

/* Peform a query using the SPARQL back-end */
int
spindle_query_sparql(QUILTREQ *request, struct query_struct *query)
{
	SPARQL *sparql;
	SPARQLRES *res;
	char limofs[128];
	int r;

	/* Fail silently if there are parameters specified which aren't supported
	 * by this back-end.
	 */
	if(query->text || query->lang || query->media || query->audience ||
	   query->type)
	{
		return 200;
	}
	if(query->related)
	{
		sparql = quilt_sparql();
	
		if(sparql_queryf_model(sparql, request->model, "SELECT DISTINCT ?s ?p ?o ?g WHERE {\n"
							   "  GRAPH ?g {\n"
							   "    ?s <" NS_FOAF "topic> <%s#id>\n"
							   "  }"
							   "  GRAPH ?g {\n"
							   "    ?s ?p ?o\n"
							   "  }\n"
							   "  FILTER regex(str(?g), \"^%s\", \"i\")\n"
							   "}",
							   request->subject, request->base))
		{
			quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to retrieve related items\n");
			return 500;
		}
		return 200;
	}
	if(query->offset)
	{
		snprintf(limofs, sizeof(limofs) - 1, "OFFSET %d LIMIT %d", query->offset, query->limit + 1);
	}
	else
	{
		snprintf(limofs, sizeof(limofs) - 1, "LIMIT %d", query->limit + 1);
	}
	sparql = quilt_sparql();
	if(!sparql)
	{
		return 500;
	}
	res = sparql_queryf(sparql, "SELECT DISTINCT ?s ?class\n"
						"WHERE {\n"
						" GRAPH <%s> {\n"
						"  ?s <" NS_RDF "type> ?class .\n"
						"  ?ir <" NS_FOAF "primaryTopic> ?s .\n"
						"  ?ir <" NS_DCTERMS "modified> ?modified .\n"
						"  ?ir <" NS_SPINDLE "score> ?score .\n"
						"  %s"
						"  FILTER(?score <= 40)"
						" }\n"
						"}\n"
						"ORDER BY DESC(?modified)\n"
						"%s",
						request->base, ( query->qclass ? query->qclass : "" ), limofs);
	if(!res)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": SPARQL query for index subjects failed\n");
		return 500;
	}
	/* Add information about each of the result items to the model */
	if((r = spindle_index_metadata_sparqlres_(request, query, res)))
	{
		sparqlres_destroy(res);
		return r;
	}
	sparqlres_destroy(res);	
	/* Return 200, rather than 0, to auto-serialise the model */
	return 200;
}

/* Look up an item using the SPARQL back-end */
int
spindle_lookup_sparql(QUILTREQ *request, const char *target)
{
	SPARQL *sparql;
	SPARQLRES *res;
	SPARQLROW *row;
	librdf_node *node;
	librdf_uri *uri;
	const char *uristr;
	size_t l;
	char *buf;

	sparql = quilt_sparql();
	if(!sparql)
	{
		return 500;
	}	
	res = sparql_queryf(sparql, "SELECT ?s\n"
						"WHERE {\n"
						" GRAPH %V {\n"
						"  <%s> <" NS_OWL "#sameAs> ?s .\n"
						" }\n"
						"}\n",
						request->basegraph, target);
	if(!res)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": SPARQL query for coreference failed\n");
		return 500;
	}
	row = sparqlres_next(res);
	if(!row)
	{
		sparqlres_destroy(res);
		return 404;
	}
	node = sparqlrow_binding(row, 0);
	if(!node)
	{
		sparqlres_destroy(res);
		return 404;
	}
	if(!librdf_node_is_resource(node) ||
	   !(uri = librdf_node_get_uri(node)) ||
	   !(uristr = (const char *) librdf_uri_as_string(uri)))
	{
		sparqlres_destroy(res);
		return 404;
	}
	l = strlen(request->base);
	if(!strncmp(uristr, request->base, l))
	{
		buf = (char *) malloc(strlen(uristr) + 32);
		buf[0] = '/';
		strcpy(&(buf[1]), &(uristr[l]));
	}
	else
	{
		buf = strdup(uristr);
	}
	sparqlres_destroy(res);
	quilt_request_headers(request, "Status: 303 See other\n");
	quilt_request_headers(request, "Server: Quilt/" PACKAGE_VERSION "\n");
	quilt_request_headerf(request, "Location: %s\n", buf);
	free(buf);
	/* Return 0 to supress output */
	return 0;
}

/* Fetch an item using the SPARQL back-end */
int
spindle_item_sparql(QUILTREQ *request)
{
	char *query;

	quilt_canon_add_path(request->canonical, request->path);
	quilt_canon_set_fragment(request->canonical, "id");
	query = (char *) malloc(strlen(request->subject) + 1024);
	if(!query)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to allocate %u bytes\n", (unsigned) strlen(request->subject) + 128);
		return 500;
	}
	/* Fetch the contents of the graph named in the request */
	sprintf(query, "SELECT DISTINCT * WHERE {\n"
			"GRAPH ?g {\n"
			"  ?s ?p ?o . \n"
			"  FILTER( ?g = <%s> )\n"
			"}\n"
			"}", request->subject);
	if(quilt_sparql_query_rdf(query, request->model))
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to create model from query\n");
		free(query);
		return 500;
	}
	/* If the model is completely empty, consider the graph to be Not Found */
	if(quilt_model_isempty(request->model))
	{
		free(query);
		return 404;
	}
	free(query);
	return 200;
}

/* For all of the things matching a particular query, add the metadata from
 * the related graphs to the model.
 */
static int
spindle_index_metadata_sparqlres_(QUILTREQ *request, struct query_struct *query, SPARQLRES *res)
{
	char *querystr, *p, *abstract;
	size_t buflen;
	SPARQLROW *row;
	librdf_node *node;
	librdf_uri *uri;
	int subj, more, c;
	const char *uristr;
	librdf_statement *st;

	abstract = quilt_canon_str(request->canonical, QCO_ABSTRACT);
	buflen = 128 + strlen(request->base);
	querystr = (char *) calloc(1, buflen);
	more = 0;
	/*
	  PREFIX ...
	  SELECT ?s ?p ?o ?g WHERE {
	  GRAPH ?g {
	  ?s ?p ?o .
	  FILTER( ?g = <root> )
	  FILTER( ...subjects... )
	  }
	  }
	*/
	sprintf(querystr, "SELECT ?s ?p ?o ?g WHERE { GRAPH ?g { ?s ?p ?o . FILTER(?g = <%s>) FILTER(", request->base);
	subj = 0;
	for(c = 0; c < request->limit && (row = sparqlres_next(res)); c++)
	{
		node = sparqlrow_binding(row, 0);
		if(!librdf_node_is_resource(node))
		{
			continue;
		}
		if((uri = librdf_node_get_uri(node)) &&
		   (uristr = (const char *) librdf_uri_as_string(uri)))
		{
			st = quilt_st_create_uri(abstract, NS_RDFS "seeAlso", uristr);
			if(!st) return -1;
			librdf_model_context_add_statement(request->model, request->basegraph, st);
			buflen += 8 + (strlen(uristr) * 3);
			p = (char *) realloc(querystr, buflen);
			if(!p)
			{
				quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to reallocate buffer to %lu bytes\n", (unsigned long) buflen);
				return 500;
			}
			querystr = p;
			p = strchr(querystr, 0);
			strcpy(p, "?s = <");
			for(p = strchr(querystr, 0); *uristr; uristr++)
			{
				if(*uristr == '>')
				{
					*p = '%';
					p++;
					*p = '3';
					p++;
					*p = 'e';
				}
				else
				{
					*p = *uristr;
				}
				p++;
			}
			*p = 0;
			strcpy(p, "> ||");
			subj = 1;
		}
	}
	if(sparqlres_next(res))
	{
		query->more = 1;
	}
	if(subj)
	{
		strcpy(p, "> ) } }");
		if(quilt_sparql_query_rdf(querystr, request->model))
		{
			quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to create model from query\n");
			free(querystr);
			free(abstract);
			return 500;
		}
	}	
	free(querystr);
	free(abstract);
	return 0;
}
