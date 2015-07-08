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

static int spindle_index_meta_(QUILTREQ *request, const char *abstract, int more);
static int spindle_index_metadata_sparqlres_(QUILTREQ *request, SPARQLRES *res);
static int spindle_index_db_(QUILTREQ *request, const char *qclass);
static int spindle_index_db_rs_(QUILTREQ *request, SQL_STATEMENT *rs);
static int spindle_db_index_row_(QUILTREQ *request, SQL_STATEMENT *rs, const char *id, const char *self, QUILTCANON *item);
static const char *spindle_checklang_(QUILTREQ *request, const char *lang);
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
	int r;

	if(request->offset)
	{
		quilt_canon_set_param_int(request->canonical, "offset", request->offset);
	}
	if(request->limit != request->deflimit)
	{
		quilt_canon_set_param_int(request->canonical, "limit", request->limit);
	}
	if(spindle_db)
	{
		return spindle_index_db_(request, qclass);
	}
	if(request->offset)
	{
		snprintf(limofs, sizeof(limofs) - 1, "OFFSET %d LIMIT %d", request->offset, request->limit + 1);
	}
	else
	{
		snprintf(limofs, sizeof(limofs) - 1, "LIMIT %d", request->limit + 1);
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
	/* Return 200, rather than 0, to auto-serialise the model */
	return 200;
}

/* For all of the things matching a particular query, add the metadata from
 * the related graphs to the model.
 */
static int
spindle_index_metadata_sparqlres_(QUILTREQ *request, SPARQLRES *res)
{
	char *query, *p, *abstract;
	size_t buflen;
	SPARQLROW *row;
	librdf_node *node;
	librdf_uri *uri;
	int subj, more, c;
	const char *uristr;
	librdf_statement *st;

	abstract = quilt_canon_str(request->canonical, QCO_ABSTRACT);
	buflen = 128 + strlen(request->base);
	query = (char *) calloc(1, buflen);
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
	sprintf(query, "SELECT ?s ?p ?o ?g WHERE { GRAPH ?g { ?s ?p ?o . FILTER(?g = <%s>) FILTER(", request->base);
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
	if(sparqlres_next(res))
	{
		more = 1;
	}
	if(subj)
	{
		strcpy(p, "> ) } }");
		if(quilt_sparql_query_rdf(query, request->model))
		{
			quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to create model from query\n");
			free(query);
			free(abstract);
			return 500;
		}
	}	
	free(query);
	spindle_index_meta_(request, abstract, more);
	return 0;
}

static int
spindle_index_db_(QUILTREQ *request, const char *qclass)
{
	size_t qbuflen, n, c;
	char *qbuf, *t;
	SQL_STATEMENT *rs;
	const void *args[8];
	const char *search, *lang, *media, *audience, *mtype;

	struct mediamatch {
		const char *name;
		const char *uri;
	} mediamatch[] = {
		{ "collection", "http://purl.org/dc/dcmitype/Collection" },
		{ "dataset", "http://purl.org/dc/dcmitype/Dataset" },
		{ "video", "http://purl.org/dc/dcmitype/MovingImage" },
		{ "interactive", "http://purl.org/dc/dcmitype/InteractiveResource" },
		{ "software", "http://purl.org/dc/dcmitype/Software" },
		{ "audio", "http://purl.org/dc/dcmitype/Sound" },
		{ "text", "http://purl.org/dc/dcmitype/Text" },
		{ NULL, NULL }
	};

	memset(args, 0, sizeof(args));
	qbuflen = 511;
	n = 0;
	search = quilt_request_getparam(request, "q");
	media = quilt_request_getparam(request, "media");
	audience = quilt_request_getparam(request, "for");
	mtype = quilt_request_getparam(request, "type");
	if(!strcmp(search, "*"))
	{
		search = NULL;
	}
	lang = spindle_checklang_(request, quilt_request_getparam(request, "lang"));
	if(!lang)
	{
		return 404;
	}
	if(media)
	{
		quilt_canon_set_param(request->canonical, "media", media);
		if(!audience)
		{
			audience = "any";
		}
		if(strcmp(audience, "any"))
		{
			quilt_canon_set_param(request->canonical, "for", audience);
		}
		if(!mtype)
		{
			mtype = "any";
		}
		if(strcmp(mtype, "any"))
		{
			quilt_canon_set_param(request->canonical, "type", mtype);
		}
	}
	qbuf = (char *) malloc(qbuflen + 1);
	t = qbuf;
	if(!qbuf)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to allocate SQL query buffer\n");
		return 500;
	}
	/* SELECT */
	t = appendf(t, &qbuflen, "SELECT \"i\".\"id\", \"i\".\"classes\", \"i\".\"title\", \"i\".\"description\", \"i\".\"coordinates\"");
	if(search)
	{
		/* 4 divides the rank by the mean harmonic distance between extents
		 * 32 divides the rank by itself
		 * See http://www.postgresql.org/docs/9.4/static/textsearch-controls.html
		 */
		t = appendf(t, &qbuflen, ", ts_rank_cd(\"i\".\"index_%s\", \"query\", 4|32) AS \"rank\"", lang);
	}
	/* FROM */
	t = appendf(t, &qbuflen, " FROM \"index\" \"i\"");
	if(search)
	{
		t = appendf(t, &qbuflen, ", plainto_tsquery(%Q) \"query\"");
		args[n] = search;
		n++;
	}
	if(media)
	{
		t = appendf(t, &qbuflen, ", \"about\" \"a\", \"index_media\" \"im\", \"media\" \"m\"");
	}
	/* WHERE */
	t = appendf(t, &qbuflen, " WHERE \"i\".\"score\" < 40");
	if(search)
	{
		t = appendf(t, &qbuflen, " AND \"query\" @@ \"index_%s\"", lang);
	}
	if(qclass)
	{
		t = appendf(t, &qbuflen, " AND %%Q = ANY(\"i\".\"classes\")");
		args[n] = qclass;
		n++;
	}
	if((audience || mtype) && !media)
	{
		media = "any";
	}
	if(media)
	{
		t = appendf(t, &qbuflen, " AND \"a\".\"id\" = \"im\".\"id\"");
		t = appendf(t, &qbuflen, " AND \"im\".\"media\" = \"m\".\"id\"");
		if(!strcmp(audience, "all"))
		{
			t = appendf(t, &qbuflen, " AND \"m\".\"audience\" IS NULL");
		}
		else if(strcmp(audience, "any"))
		{
			t = appendf(t, &qbuflen, " AND (\"m\".\"audience\" IS NULL OR \"m\".\"audience\" = %Q)");
			args[n] = audience;
			n++;
		}
		if(strcmp(media, "any"))
		{
			for(c = 0; mediamatch[c].name; c++)
			{
				if(!strcmp(mediamatch[c].name, media))
				{
					t = appendf(t, &qbuflen, " AND \"m\".\"class\" = %Q");
					args[n] = mediamatch[c].uri;
					n++;
					break;
				}
			}
			if(!mediamatch[c].name)
			{
				t = appendf(t, &qbuflen, " AND \"m\".\"class\" = %Q");
				args[n] = media;
				n++;
			}
		}
		if(strcmp(mtype, "any"))
		{
			t = appendf(t, &qbuflen, " AND \"m\".\"type\" = %Q");
			args[n] = mtype;
			n++;
		}
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
	rs = sql_queryf(spindle_db, qbuf, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
	if(!rs)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": query execution failed\n");
		return 500;
	}
	return spindle_index_db_rs_(request, rs);
}

static int
spindle_index_meta_(QUILTREQ *request, const char *abstract, int more)
{
	QUILTCANON *link;
	char *root, *linkstr;
	int c;
	librdf_statement *st;

	root = quilt_canon_str(request->canonical, QCO_ABSTRACT|QCO_NOPARAMS);
	if(request->offset)
	{
		/* If the request had an offset, link to the previous page */
		link = quilt_canon_create(request->canonical);
		c = request->offset - request->limit;
		if(c < 0)
		{
			c = 0;
		}
		if(c)
		{
			quilt_canon_set_param_int(link, "offset", c);
		}
		else
		{
			quilt_canon_set_param(link, "offset", NULL);
		}
		linkstr = quilt_canon_str(link, QCO_ABSTRACT);
		st = quilt_st_create_uri(abstract, NS_XHTML "prev", linkstr);
		librdf_model_add_statement(request->model, st);
		librdf_free_statement(st);
		free(linkstr);
		quilt_canon_destroy(link);
	}
	if(more)
	{
		/* ... xhv:next </?offset=...> */
		link = quilt_canon_create(request->canonical);
		quilt_canon_set_param_int(link, "offset", request->offset + request->limit);
		linkstr = quilt_canon_str(link, QCO_ABSTRACT);
		st = quilt_st_create_uri(abstract, NS_XHTML "next", linkstr);
		librdf_model_add_statement(request->model, st);
		librdf_free_statement(st);
		free(linkstr);
		quilt_canon_destroy(link);
	}
	if(strcmp(root, abstract))
	{		
		st = quilt_st_create_uri(abstract, NS_DCTERMS "isPartOf", root);
		librdf_model_add_statement(request->model, st);
		librdf_free_statement(st);

		st = quilt_st_create_uri(root, NS_RDF "type", NS_VOID "Dataset");
		librdf_model_add_statement(request->model, st);
		librdf_free_statement(st);

		if(request->indextitle)
		{
			st = quilt_st_create_literal(root, NS_RDFS "label", request->indextitle, "en-gb");
			librdf_model_add_statement(request->model, st);
			librdf_free_statement(st);
		}
	}
	/* ... rdf:type void:Dataset */
	st = quilt_st_create_uri(abstract, NS_RDF "type", NS_VOID "Dataset");
	librdf_model_add_statement(request->model, st);
	librdf_free_statement(st);

	/* ... rdf:label */
	if(request->indextitle)
	{
		st = quilt_st_create_literal(abstract, NS_RDFS "label", request->indextitle, "en-gb");
		librdf_model_add_statement(request->model, st);
		librdf_free_statement(st);
	}

	spindle_add_concrete(request); 

	free(root);
	return 0;
}

static int
spindle_index_db_rs_(QUILTREQ *request, SQL_STATEMENT *rs)
{
	QUILTCANON *item;
	int c, more;
	const char *t;
	char idbuf[36], *p, *abstract;	

	abstract = quilt_canon_str(request->canonical, QCO_ABSTRACT);
	more = 0;
	for(c = 0; !sql_stmt_eof(rs) && c < request->limit; sql_stmt_next(rs))
	{
		c++;
		item = quilt_canon_create(request->canonical);
		quilt_canon_reset_path(item);
		quilt_canon_reset_params(item);
		quilt_canon_set_fragment(item, "id");		
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
		quilt_canon_add_path(item, idbuf);
		spindle_db_index_row_(request, rs, idbuf, abstract, item);
		quilt_canon_destroy(item);
	}
	if(!sql_stmt_eof(rs))
	{
		more = 1;
	}
	sql_stmt_destroy(rs);
	spindle_index_meta_(request, abstract, more);
	free(abstract);
	return 200;
}

static int
spindle_db_index_row_(QUILTREQ *request, SQL_STATEMENT *rs, const char *id, const char *self, QUILTCANON *item)
{
	librdf_statement *st;
	const char *s;
	char *uri;

	(void) id;

	uri = quilt_canon_str(item, QCO_SUBJECT);
	st = quilt_st_create_uri(self, NS_RDFS "seeAlso", uri);
	librdf_model_add_statement(request->model, st);
	librdf_free_statement(st);

	/* rdfs:label */
	s = sql_stmt_str(rs, 2);
	if(s)
	{
		spindle_add_langvector_(request->model, s, uri, NS_RDFS "label");
	}

	/* rdfs:comment */
	s = sql_stmt_str(rs, 3);
	if(s)
	{
		spindle_add_langvector_(request->model, s, uri, NS_RDFS "comment");
	}

	/* rdf:type */
	s = sql_stmt_str(rs, 1);
	if(s)
	{
		spindle_add_array_(request->model, s, uri, NS_RDF "type");
	}

	/* geo:lat, geo:long */
	s = sql_stmt_str(rs, 4);
	if(s)
	{
		spindle_add_point_(request->model, s, uri);
	}
	free(uri);
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
	type = librdf_new_uri(world, (const unsigned char *) NS_XSD "decimal");

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
	st = quilt_st_create(subject, NS_GEO "lat");
	librdf_statement_set_object(st, coords[0]);
	librdf_model_add_statement(model, st);
	librdf_free_statement(st);

	st = quilt_st_create(subject, NS_GEO "long");
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
spindle_checklang_(QUILTREQ *req, const char *lang)
{
	if(!lang || !lang[0])
	{
		return "en_gb";
	}
	if(!strcasecmp(lang, "en") ||
	   !strcasecmp(lang, "en-gb") ||
	   !strcasecmp(lang, "en_gb") ||
	   !strcasecmp(lang, "en-us"))
	{
		quilt_canon_set_param(req->canonical, "lang", "en_gb");
		return "en_gb";
	}
	if(!strcasecmp(lang, "ga") ||
	   !strcasecmp(lang, "ga-gb") ||
	   !strcasecmp(lang, "ga_gb"))
	{
		quilt_canon_set_param(req->canonical, "lang", "ga_gb");
		return "ga_gb";
	}
	if(!strcasecmp(lang, "cy") ||
	   !strcasecmp(lang, "en-cy") ||
	   !strcasecmp(lang, "en_cy"))
	{
		quilt_canon_set_param(req->canonical, "lang", "cy_gb");
		return "cy_gb";
	}
	if(!strcasecmp(lang, "gd") ||
	   !strcasecmp(lang, "gd-gb") ||
	   !strcasecmp(lang, "gd_gb"))
	{
		quilt_canon_set_param(req->canonical, "lang", "gd_gb");
		return "gd_gb";
	}
	return NULL;
}
