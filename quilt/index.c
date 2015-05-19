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

static int spindle_index_metadata_sparqlres_(QUILTREQ *request, SPARQLRES *res);
static int spindle_index_db_(QUILTREQ *request, const char *qclass);
static int spindle_index_db_rs_(QUILTREQ *request, SQL_STATEMENT *rs);
static int spindle_db_index_row_(QUILTREQ *request, SQL_STATEMENT *rs, const char *id, const char *uri);

int
spindle_index(QUILTREQ *request, const char *qclass)
{
	SPARQL *sparql;
	SPARQLRES *res;
	char limofs[128];
	char *uristr;
	librdf_statement *st;
	int r, i;
	
	if(spindle_db)
	{
		return spindle_index_db_(request, qclass);
	}
	if(request->offset)
	{
		snprintf(limofs, sizeof(limofs) - 1, "OFFSET %d LIMIT %d", request->offset, request->limit);
	}
	else
	{
		snprintf(limofs, sizeof(limofs) - 1, "LIMIT %d", request->limit);
	}	
	sparql = quilt_sparql();
	if(!sparql)
	{
		return 500;
	}
	res = sparql_queryf(sparql, "SELECT DISTINCT ?s ?class\n"
						"WHERE {\n"
						" GRAPH <%s> {\n"
						"  ?s <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> ?class .\n"
						"  ?ir <http://xmlns.com/foaf/0.1/primaryTopic> ?s .\n"
						"  ?ir <http://purl.org/dc/terms/modified> ?modified .\n"
						"  ?ir <http://bbcarchdev.github.io/ns/spindle#score> ?score .\n"
						"  %s"
						"  FILTER(?score <= 40)"
						" }\n"
						"}\n"
						"ORDER BY DESC(?modified)\n"
						"%s",
						request->base, ( qclass ? qclass : "" ), limofs);
	if(!res)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": SPARQL query for index subjects failed\n");
		return 500;
	}
	/* Add information about each of the result items to the model */
	if((r = spindle_index_metadata_sparqlres_(request, res)))
	{
		sparqlres_destroy(res);
		return r;
	}
	sparqlres_destroy(res);	
	/* Add statements about the index itself */
	st = quilt_st_create_literal(request->path, "http://www.w3.org/2000/01/rdf-schema#label", request->indextitle, "en");
	if(!st) return -1;
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);
	st = quilt_st_create_uri(request->path, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type", "http://rdfs.org/ns/void#Dataset");
	if(!st) return -1;
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);
	uristr = (char *) calloc(1, strlen(request->path) + 128);
	if(request->offset)
	{
		i = request->offset - request->limit;
		if(i < 0)
		{
			i = 0;
		}
		if(i)
		{
			sprintf(uristr, "%s?offset=%d", request->path, i);
		}
		else
		{
			strcpy(uristr, request->path);
		}
		st = quilt_st_create_uri(request->path, "http://www.w3.org/1999/xhtml/vocab#prev", uristr);
		librdf_model_context_add_statement(request->model, request->basegraph, st);
		librdf_free_statement(st);
	}
	sprintf(uristr, "%s?offset=%d", request->path, request->offset + request->limit);
	st = quilt_st_create_uri(request->path, "http://www.w3.org/1999/xhtml/vocab#next", uristr);
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);

	free(uristr);
	/* Return 200, rather than 0, to auto-serialise the model */
	return 200;
}

/* For all of the things matching a particular query, add the metadata from
 * the related graphs to the model.
 */
static int
spindle_index_metadata_sparqlres_(QUILTREQ *request, SPARQLRES *res)
{
	char *query, *p;
	size_t buflen;
	SPARQLROW *row;
	librdf_node *node;
	librdf_uri *uri;
	int subj;
	const char *uristr;
	librdf_statement *st;

	buflen = 128 + strlen(request->base);
	query = (char *) calloc(1, buflen);
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
	sprintf(query, "SELECT ?s ?p ?o ?g WHERE { GRAPH ?g { ?s ?p ?o . FILTER(?g = <%s>) FILTER(", request->base);
	subj = 0;
	while((row = sparqlres_next(res)))
	{
		node = sparqlrow_binding(row, 0);
		if(!librdf_node_is_resource(node))
		{
			continue;
		}
		if((uri = librdf_node_get_uri(node)) &&
		   (uristr = (const char *) librdf_uri_as_string(uri)))
		{
			st = quilt_st_create_uri(request->path, "http://www.w3.org/2000/01/rdf-schema#seeAlso", uristr);
			if(!st) return -1;
			librdf_model_context_add_statement(request->model, request->basegraph, st);
			buflen += 8 + (strlen(uristr) * 3);
			p = (char *) realloc(query, buflen);
			if(!p)
			{
				quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to reallocate buffer to %lu bytes\n", (unsigned long) buflen);
				return 500;
			}
			query = p;
			p = strchr(query, 0);
			strcpy(p, "?s = <");
			for(p = strchr(query, 0); *uristr; uristr++)
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
	if(subj)
	{
		strcpy(p, "> ) } }");
		if(quilt_sparql_query_rdf(query, request->model))
		{
			quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to create model from query\n");
			free(query);
			return 500;
		}
	}
	free(query);
	return 0;
}

static int
spindle_index_db_(QUILTREQ *request, const char *qclass)
{
	char limofs[64];
	SQL_STATEMENT *rs;

	if(request->offset)
	{
		snprintf(limofs, sizeof(limofs), "LIMIT %d OFFSET %d", request->limit + 1, request->offset);
	}
	else
	{
		snprintf(limofs, sizeof(limofs), "LIMIT %d", request->limit + 1);
	}
	if(qclass)
	{
		rs = sql_queryf(spindle_db, "SELECT \"id\", \"classes\", \"title\", \"description\", \"coordinates\" FROM \"index\" WHERE \"score\" < 40 AND %Q = ANY(\"classes\") ORDER BY \"modified\" DESC %s", qclass, limofs);
	}
	else
	{
		rs = sql_queryf(spindle_db, "SELECT \"id\", \"classes\", \"title\", \"description\", \"coordinates\" FROM \"index\" WHERE \"score\" < 40 ORDER BY \"modified\" DESC %s", limofs);
	}
	if(!rs)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": query execution failed\n");
		return 500;
	}
	return spindle_index_db_rs_(request, rs);
}

static int
spindle_index_db_rs_(QUILTREQ *request, SQL_STATEMENT *rs)
{
	int c, more;
	const char *t;
	char idbuf[36], *uribuf, *p, *tbuf, *buf;
	size_t l;
	librdf_statement *st;

	if(request->base)
	{
		l = strlen(request->base);
	}
	else
	{
		l = 0;
	}
	more = 0;
	uribuf = (char *) malloc(l + 48);
	if(l)
	{
		strcpy(uribuf, request->base);
		if(isalnum(uribuf[l - 1]))
		{
			uribuf[l] = '/';
			l++;
		}
	}
	else
	{
		uribuf[0] = '/';
		l = 1;
	}
	for(c = 0; !sql_stmt_eof(rs) && c < request->limit; sql_stmt_next(rs))
	{
		c++;
		t = sql_stmt_str(rs, 0);
		for(p = idbuf; p - idbuf < 32; t++)
		{
			if(isalnum(*t))
			{
				*p = tolower(*t);
				p++;
			}
		}
		*p = 0;
		strcpy(&(uribuf[l]), idbuf);
		strcat(uribuf, "#id");
		spindle_db_index_row_(request, rs, idbuf, uribuf);
	}
	if(!sql_stmt_eof(rs))
	{
		more = 1;
	}
	sql_stmt_destroy(rs);

	l = strlen(request->path);
	tbuf = (char *) malloc(l + 64);
	buf = (char *) malloc(l + 64);

	strcpy(tbuf, request->path);
	strcpy(buf, request->path);
	if(request->offset)
	{
		snprintf(&(tbuf[l]), 64, "?offset=%d", request->offset);
		/* ... xhv:prev </?offset=...> */
		c = request->offset - request->limit;
		if(c < 0)
		{
			c = 0;
		}
		if(c)
		{
			snprintf(buf, l + 64, "%s?offset=%d", request->path, c);
		}
		else
		{
			strcpy(buf, request->path);
		}
		st = quilt_st_create_uri(tbuf, "http://www.w3.org/1999/xhtml/vocab#prev", buf);
		librdf_model_add_statement(request->model, st);
		librdf_free_statement(st);
	}
	if(tbuf[l])
	{
		st = quilt_st_create_uri(tbuf, "http://purl.org/dc/terms/isPartOf", request->path);
		librdf_model_add_statement(request->model, st);
		librdf_free_statement(st);

		st = quilt_st_create_uri(request->path, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type", "http://rdfs.org/ns/void#Dataset");
		librdf_model_add_statement(request->model, st);
		librdf_free_statement(st);
	}
	if(more)
	{
		/* ... xhv:next </?offset=...> */
		snprintf(&(buf[l]), 64, "?offset=%d", request->offset + request->limit);
		st = quilt_st_create_uri(tbuf, "http://www.w3.org/1999/xhtml/vocab#next", buf);
		librdf_model_add_statement(request->model, st);
		librdf_free_statement(st);
	}
	/* ... rdf:type void:Dataset */
	st = quilt_st_create_uri(tbuf, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type", "http://rdfs.org/ns/void#Dataset");
	librdf_model_add_statement(request->model, st);
	librdf_free_statement(st);

	/* ... rdf:label */
	if(request->indextitle)
	{
		st = quilt_st_create_literal(tbuf, "http://www.w3.org/2000/01/rdf-schema#label", request->indextitle, "en-gb");
		librdf_model_add_statement(request->model, st);
		librdf_free_statement(st);
	}
	free(tbuf);
	free(buf);
	return 200;
}

static int
spindle_db_index_row_(QUILTREQ *request, SQL_STATEMENT *rs, const char *id, const char *uri)
{
	librdf_statement *st;

	st = quilt_st_create_uri(request->path, "http://www.w3.org/2000/01/rdf-schema#seeAlso", uri);
	librdf_model_add_statement(request->model, st);
	librdf_free_statement(st);

	return 0;
}
