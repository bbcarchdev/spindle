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
static const char *spindle_checklang_(const char *lang);
static int spindle_add_langvector_(librdf_model *model, const char *vector, const char *subject, const char *predicate);
static int spindle_add_array_(librdf_model *model, const char *array, const char *subject, const char *predicate);
static int spindle_add_point_(librdf_model *model, const char *array, const char *subject);
static char *appendf(char *buf, size_t *buflen, const char *fmt, ...);

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
	size_t qbuflen, n;
	char *qbuf, *t;
	SQL_STATEMENT *rs;
	const void *args[8];
	const char *search, *lang;

	memset(args, 0, sizeof(args));
	qbuflen = 511;
	qbuf = (char *) malloc(qbuflen + 1);
	t = qbuf;
	n = 0;
	search = quilt_request_getparam(request, "q");
	lang = spindle_checklang_(quilt_request_getparam(request, "lang"));
	if(!lang)
	{
		return 404;
	}	
	if(!qbuf)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to allocate SQL query buffer\n");
		return 500;
	}
	/* SELECT */
	t = appendf(t, &qbuflen, "SELECT \"id\", \"classes\", \"title\", \"description\", \"coordinates\"");
	if(search)
	{
		/* 4 divides the rank by the mean harmonic distance between extents
		 * 32 divides the rank by itself
		 * See http://www.postgresql.org/docs/9.4/static/textsearch-controls.html
		 */
		t = appendf(t, &qbuflen, ", ts_rank_cd(\"index_%s\", \"query\", 4|32) AS \"rank\"", lang);
	}
	/* FROM */
	t = appendf(t, &qbuflen, " FROM \"index\"");
	if(search)
	{
		t = appendf(t, &qbuflen, ", plainto_tsquery(%Q) \"query\"");
		args[n] = search;
		n++;
	}
	/* WHERE */
	t = appendf(t, &qbuflen, " WHERE \"score\" < 40");
	if(search)
	{
		t = appendf(t, &qbuflen, " AND \"query\" @@ \"index_%s\"", lang);
	}
	if(qclass)
	{
		t = appendf(t, &qbuflen, " AND %%Q = ANY(\"classes\")");
		args[n] = qclass;
		n++;
	}
	/* ORDER BY */
	if(search)
	{
		t = appendf(t, &qbuflen, " ORDER BY \"rank\", \"modified\" DESC");
	}
	else
	{
		t = appendf(t, &qbuflen, " ORDER BY \"modified\" DESC");
	}
	/* LIMIT ... OFFSET ... */
	if(request->offset)
	{
		t = appendf(t, &qbuflen, " LIMIT %d OFFSET %d", request->limit + 1, request->offset);
	}
	else
	{
		t = appendf(t, &qbuflen, " LIMIT %d", request->limit + 1);
	}
	rs = sql_queryf(spindle_db, qbuf, args[0], args[1]);
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
	const char *s;

	(void) id;

	st = quilt_st_create_uri(request->path, "http://www.w3.org/2000/01/rdf-schema#seeAlso", uri);
	librdf_model_add_statement(request->model, st);
	librdf_free_statement(st);

	/* rdfs:label */
	s = sql_stmt_str(rs, 2);
	if(s)
	{
		spindle_add_langvector_(request->model, s, uri, "http://www.w3.org/2000/01/rdf-schema#label");
	}

	/* rdfs:comment */
	s = sql_stmt_str(rs, 3);
	if(s)
	{
		spindle_add_langvector_(request->model, s, uri, "http://www.w3.org/2000/01/rdf-schema#comment");
	}

	/* rdf:type */
	s = sql_stmt_str(rs, 1);
	if(s)
	{
		spindle_add_array_(request->model, s, uri, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
	}

	/* geo:lat, geo:long */
	s = sql_stmt_str(rs, 4);
	if(s)
	{
		spindle_add_point_(request->model, s, uri);
	}
	return 0;
}

/* Add a URI array to the model */
static int
spindle_add_array_(librdf_model *model, const char *array, const char *subject, const char *predicate)
{
	librdf_statement *st;
	char *buf, *p;
	int q, e;

	if(*array != '{')
	{
		return 0;
	}
	array++;
	buf = (char *) malloc(strlen(array) + 1);
	if(!buf)
	{
		quilt_logf(LOG_CRIT, "failed to allocate buffer to process URI array\n");
		return -1;
	}
	q = 0;
	e = 0;
	/* "URI", "URI", "URI", ... } */
	while(*array)
	{
		while(isspace(*array) || *array == ',')
		{
			array++;
		}
		if(!*array || *array == '}')
		{
			break;
		}
		for(p = buf; *array; array++)
		{
			if(e)
			{
				e = 0;
				*p = *array;
				p++;
				continue;
			}
			if(*array == '\\')
			{
				e = 1;
				continue;
			}
			if(q)
			{
				if(*array == q)
				{
					q = 0;
					continue;
				}
				*p = *array;
				p++;
				continue;
			}
			if(*array == '"')
			{
				q = *array;
				continue;
			}
			if(isspace(*array) || *array == '}' || *array == ',')
			{
				break;
			}
			*p = *array;
			p++;
		}
		*p = 0;
		if(*buf)
		{
			st = quilt_st_create_uri(subject, predicate, buf);
			librdf_model_add_statement(model, st);
			librdf_free_statement(st);
		}
	}
	free(buf);
	return 0;
}


/* Add a point to the model */
static int
spindle_add_point_(librdf_model *model, const char *array, const char *subject)
{
	char *buf, *p;
	int q, e;
	librdf_world *world;
	librdf_uri *type;
	librdf_node *coords[2];
	librdf_statement *st;
	size_t n;

	world = quilt_librdf_world();
	type = librdf_new_uri(world, (const unsigned char *) "http://www.w3.org/2001/XMLSchema#decimal");

	if(*array != '(')
	{
		librdf_free_uri(type);
		return 0;
	}
	array++;
	buf = (char *) malloc(strlen(array) + 1);
	if(!buf)
	{
		quilt_logf(LOG_CRIT, "failed to allocate buffer to process coordinates\n");
		librdf_free_uri(type);
		return -1;
	}
	q = 0;
	e = 0;
	n = 0;
	while(*array && n < 2)
	{
		while(isspace(*array) || *array == ',')
		{
			array++;
		}
		if(!*array || *array == ')')
		{
			break;
		}
		for(p = buf; *array; array++)
		{
			if(e)
			{
				e = 0;
				*p = *array;
				p++;
				continue;
			}
			if(*array == '\\')
			{
				e = 1;
				continue;
			}
			if(q)
			{
				if(*array == q)
				{
					q = 0;
					continue;
				}
				*p = *array;
				p++;
				continue;
			}
			if(*array == '"')
			{
				q = *array;
				continue;
			}
			if(isspace(*array) || *array == ')' || *array == ',')
			{
				break;
			}
			*p = *array;
			p++;
		}
		*p = 0;
		if(*buf)
		{
			coords[n] = librdf_new_node_from_typed_literal(world, (const unsigned char *) buf, NULL, type);
			n++;
		}
	}
	free(buf);
	if(n < 2)
	{
		if(n)
		{
			librdf_free_node(coords[0]);
		}
		librdf_free_uri(type);
		return 0;
	}	
	st = quilt_st_create(subject, "http://www.w3.org/2003/01/geo/wgs84_pos#lat");
	librdf_statement_set_object(st, coords[0]);
	librdf_model_add_statement(model, st);
	librdf_free_statement(st);

	st = quilt_st_create(subject, "http://www.w3.org/2003/01/geo/wgs84_pos#long");
	librdf_statement_set_object(st, coords[1]);
	librdf_model_add_statement(model, st);
	librdf_free_statement(st);

	librdf_free_uri(type);
	return 0;
}

/* Add a language=>literal PostgreSQL vector to the model */
static int
spindle_add_langvector_(librdf_model *model, const char *vector, const char *subject, const char *predicate)
{
	librdf_statement *st;
	char *buf, *lang, *value, *p;
	int q, e;

	buf = (char *) malloc(strlen(vector) + 1);
	if(!buf)
	{
		quilt_logf(LOG_CRIT, "failed to allocate buffer to process multilingual literal vector\n");
		return -1;
	}
	q = 0;
	e = 0;
	/* "lang" => "literal value", "lang" => "literal value", ... */
	while(*vector)
	{
		/* First retrieve the language */
		while(isspace(*vector) || *vector == ',')
		{
			vector++;
		}
		if(!*vector)
		{
			break;
		}
		lang = buf;
		for(p = buf; *vector; vector++)
		{
			if(e)
			{
				e = 0;
				*p = *vector;
				p++;
				continue;
			}
			if(*vector == '\\')
			{
				e = 1;
				continue;
			}
			if(q)
			{
				if(*vector == q)
				{
					q = 0;
					continue;
				}
				*p = *vector;
				p++;
				continue;
			}
			if(*vector == '"')
			{
				q = *vector;
				continue;
			}
			if(isspace(*vector) || *vector == '=' || *vector == ',')
			{
				break;
			}
			*p = *vector;
			p++;
		}
		*p = 0;
		p++;
		while(isspace(*vector))
		{
			vector++;
		}
		if(*vector == ',')
		{
			/* Unexpectedly-formed; skip to the next one */
			vector++;
			continue;
		}
		if(!*vector || vector[0] != '=' || vector[1] != '>')
		{
			break;
		}	   
		vector += 2;
		while(isspace(*vector))
		{
			vector++;
		}
		if(!*vector)
		{
			break;
		}
		/* Now process the value */
		for(value = p; *vector; vector++)
		{
			if(e)
			{
				e = 0;
				*p = *vector;
				p++;
				continue;
			}
			if(*vector == '\\')
			{
				e = 1;
				continue;
			}
			if(q)
			{
				if(*vector == q)
				{
					q = 0;
					continue;
				}
				*p = *vector;
				p++;
				continue;
			}
			if(*vector == '"')
			{
				q = *vector;
				continue;
			}
			if(isspace(*vector) || *vector == ',')
			{
				break;
			}
			*p = *vector;
			p++;
		}
		*p = 0;

		if(!lang[0] || lang[0] == '_')
		{
			lang = NULL;
		}
		else
		{
			for(p = lang; *p; p++)
			{
				*p = *p == '_' ? '-' : tolower(*p);
			}
		}
		st = quilt_st_create_literal(subject, predicate, value, lang);
		librdf_model_add_statement(model, st);
		librdf_free_statement(st);
	}
	free(buf);
	return 0;
}

static char *
appendf(char *buf, size_t *buflen, const char *fmt, ...)
{
	va_list ap;
	int l;
	
	va_start(ap, fmt);
	l = vsnprintf(buf, *buflen, fmt, ap);
	va_end(ap);
	if(l == -1)
	{
		return NULL;
	}
	buf += l;
	*buflen -= l;
	return buf;
}

static const char *
spindle_checklang_(const char *lang)
{
	if(!lang || !lang[0])
	{
		return "en_gb";
	}
	if(!strcasecmp(lang, "en") ||
	   !strcasecmp(lang, "en-gb") ||
	   !strcasecmp(lang, "en_gb") ||
	   !strcasemp(lang, "en-us"))
	{
		return "en_gb";
	}
	if(!strcasecmp(lang, "ga") ||
	   !strcasecmp(lang, "ga-gb") ||
	   !strcasecmp(lang, "ga_gb"))
	{
		return "ga_gb";
	}
	if(!strcasecmp(lang, "cy") ||
	   !strcasecmp(lang, "en-cy") ||
	   !strcasecmp(lang, "en_cy"))
	{
		return "cy_gb";
	}
	if(!strcasecmp(lang, "gd") ||
	   !strcasecmp(lang, "gd-gb") ||
	   !strcasecmp(lang, "gd_gb"))
	{
		return "gd_gb";
	}
	return NULL;
}
