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

static int process_rs(QUILTREQ *request, struct query_struct *query, SQL_STATEMENT *rs);
static int process_row(QUILTREQ *request, struct query_struct *query, SQL_STATEMENT *rs, const char *id, const char *self, QUILTCANON *item);
static const char *checklang(QUILTREQ *request, const char *lang);
static int add_langvector(librdf_model *model, const char *vector, const char *subject, const char *predicate);
static int add_array(librdf_model *model, const char *array, const char *subject, const char *predicate);
static int add_point(librdf_model *model, const char *array, const char *subject);
static char *appendf(char *buf, size_t *buflen, const char *fmt, ...);

/* Short names for media classes which can be used for convenience */
struct mediamatch_struct spindle_mediamatch[] = {
	{ "collection", "http://purl.org/dc/dcmitype/Collection" },
	{ "dataset", "http://purl.org/dc/dcmitype/Dataset" },
	{ "video", "http://purl.org/dc/dcmitype/MovingImage" },
	{ "interactive", "http://purl.org/dc/dcmitype/InteractiveResource" },
	{ "software", "http://purl.org/dc/dcmitype/Software" },
	{ "audio", "http://purl.org/dc/dcmitype/Sound" },
	{ "text", "http://purl.org/dc/dcmitype/Text" },
	{ NULL, NULL }
};

/* Peform a query using the SQL database back-end */
int
spindle_query_db(QUILTREQ *request, struct query_struct *query)
{
	size_t qbuflen, n, c;
	char *qbuf, *t;
	SQL_STATEMENT *rs;
	const void *args[8];
	const char *related;

	memset(args, 0, sizeof(args));
	qbuflen = 560;
	n = 0;
	related = NULL;
	if(query->related)
	{
		quilt_logf(LOG_DEBUG, QUILT_PLUGIN_NAME ": DB: related: <%s> (base is <%s>)\n", query->related, request->base);
		c = strlen(request->base);
		if(!strncmp(query->related, request->base, c))
		{
			related = query->related + c;
			while(related[0] == '/')
			{
				related++;
			}
			quilt_logf(LOG_DEBUG, QUILT_PLUGIN_NAME ": DB: related: '%s'\n", related);
			if(strlen(related) != 32)
			{
				return 404;
			}
		}
		else
		{
			return 404;
		}
	}
	if(query->text && !strcmp(query->text, "*"))
	{
		query->text = NULL;
	}	
	query->lang = checklang(request, query->lang);
	if(!query->lang)
	{
		return 404;
	}
	if(query->media)
	{
		if(!query->audience)
		{
			query->audience = "any";
		}
		if(!query->type)
		{
			query->type = "any";
		}
		if(strcmp(query->media, "any"))
		{
			for(c = 0; spindle_mediamatch[c].name; c++)
			{
				if(!strcmp(spindle_mediamatch[c].name, query->media))
				{
					query->media = spindle_mediamatch[c].uri;
					break;
				}
			}
		}
	}
	/* If we're performing any kind of media query, ensure the parameters
	 * have defaults
	 */
	if(query->media || query->audience || query->type)
	{
		if(!query->media)
		{
			query->media = "any";
		}
		if(!query->audience)
		{
			query->audience = "any";
		}
		if(!query->type)
		{
			query->type = "any";
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
	t = appendf(t, &qbuflen, "SELECT \"i\".\"id\", \"i\".\"classes\", \"i\".\"title\", \"i\".\"description\", \"i\".\"coordinates\", \"i\".\"modified\"");
	if(query->text)
	{
		/* 4 divides the rank by the mean harmonic distance between extents
		 * 32 divides the rank by itself
		 * See http://www.postgresql.org/docs/9.4/static/textsearch-controls.html
		 */
		t = appendf(t, &qbuflen, ", ts_rank_cd(\"i\".\"index_%s\", \"query\", 4|32) AS \"rank\"", query->lang);
	}
	/* FROM */
	t = appendf(t, &qbuflen, " FROM \"index\" \"i\"");
	if(query->text)
	{
		t = appendf(t, &qbuflen, ", plainto_tsquery(%Q) \"query\"");
		args[n] = query->text;
		n++;
	}
	if(related)
	{
		/* IS related and MAY have media query (no subquery) */
		t = appendf(t, &qbuflen, ", \"about\" \"a\"");
		if(query->media)
		{
			t = appendf(t, &qbuflen, ", \"media\" \"m\"");
		}	
	}
	/* WHERE */
	t = appendf(t, &qbuflen, " WHERE \"i\".\"score\" < 40");
	if(query->text)
	{
		t = appendf(t, &qbuflen, " AND \"query\" @@ \"index_%s\"", query->lang);
	}
	if(query->qclass)
	{
		t = appendf(t, &qbuflen, " AND %%Q = ANY(\"i\".\"classes\")");
		args[n] = query->qclass;
		n++;
	}
	if(related)
	{	
		/* IS related and MAY have media query (no subquery) */
		t = appendf(t, &qbuflen, " AND \"a\".\"about\" = %Q AND \"a\".\"id\" = \"i\".\"id\"");
		args[n] = related;
		n++;
		if(query->media)
		{
			t = appendf(t, &qbuflen, " AND \"m\".\"id\" = \"i\".\"id\"");
		}
	}
	else if(query->media)
	{
		/* NOT related but HAS media query (subquery) */
		t = appendf(t, &qbuflen, " AND \"i\".\"id\" IN ( "
					"SELECT \"a\".\"about\" FROM "
					" \"about\" \"a\", "
					" \"index_media\" \"im\", "
					" \"media\" \"m\" "
					"WHERE "
					" \"im\".\"id\" = \"a\".\"id\" AND "
					" \"m\".\"id\" = \"im\".\"media\" ");
	}
	if(query->media)
	{
		/* Any media query */
		if(!strcmp(query->audience, "all"))
		{
			t = appendf(t, &qbuflen, " AND \"m\".\"audience\" IS NULL");
		}
		else if(strcmp(query->audience, "any"))
		{
			t = appendf(t, &qbuflen, " AND (\"m\".\"audience\" IS NULL OR \"m\".\"audience\" = %Q)");
			args[n] = query->audience;
			n++;
		}
		if(strcmp(query->media, "any"))
		{
			t = appendf(t, &qbuflen, " AND \"m\".\"class\" = %Q");
			args[n] = query->media;
			n++;
		}
		if(strcmp(query->type, "any"))
		{
			t = appendf(t, &qbuflen, " AND \"m\".\"type\" = %Q");
			args[n] = query->type;
			n++;
		}
	}
	if(!related && query->media)
	{
		/* NOT related and HAS media query (subquery) */
		t = appendf(t, &qbuflen, ") ");
	}
	/* ORDER BY */
	if(query->text)
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
	quilt_logf(LOG_DEBUG, "query: [[%s]]\n", qbuf);
	rs = sql_queryf(spindle_db, qbuf, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
	if(!rs)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": query execution failed\n");
		return 500;
	}
	return process_rs(request, query, rs);	
}

/* Look up an item using the SQL database back-end */
int
spindle_lookup_db(QUILTREQ *request, const char *target)
{
	SQL_STATEMENT *rs;
	const char *t;
	char *buf, *p;

	rs = sql_queryf(spindle_db, "SELECT \"id\" FROM \"proxy\" WHERE %Q = ANY(\"sameas\")", target);
	if(!rs)
	{
		return 500;
	}
	if(sql_stmt_eof(rs))
	{
		sql_stmt_destroy(rs);
		return 404;
	}
	buf = (char *) calloc(1, 1 + 32 + 4 + 1);
	if(!buf)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to allocate buffer for redirect URL\n");
		sql_stmt_destroy(rs);
		return 500;
	}
	buf[0] = '/';
	p = &(buf[1]);
	for(t = sql_stmt_str(rs, 0); *t; t++)
	{
		if(isalnum(*t))
		{
			*p = *t;
			p++;
		}
	}
	strcpy(p, "#id");
	sql_stmt_destroy(rs);
	quilt_request_headers(request, "Status: 303 See other\n");
	quilt_request_headers(request, "Server: Quilt/" PACKAGE_VERSION "\n");
	quilt_request_headerf(request, "Location: %s\n", buf);
	free(buf);
	return 0;
}

static int
process_rs(QUILTREQ *request, struct query_struct *query, SQL_STATEMENT *rs)
{
	QUILTCANON *item;
	int c;
	const char *t;
	char idbuf[36], *p, *abstract;	

	abstract = quilt_canon_str(request->canonical, (request->ext ? QCO_ABSTRACT : QCO_REQUEST));
	for(c = 0; !sql_stmt_eof(rs) && c < request->limit; sql_stmt_next(rs))
	{
		c++;
		item = quilt_canon_create(request->canonical);
		quilt_canon_reset_path(item);
		quilt_canon_reset_params(item);
		quilt_canon_set_fragment(item, "id");
		t = sql_stmt_str(rs, 0);
		quilt_logf(LOG_DEBUG, "t=<%s>\n", t);
		for(p = idbuf; p - idbuf < 32; t++)
		{
			if(isalnum(*t))
			{
				*p = tolower(*t);
				p++;
			}
		}
		*p = 0;
		quilt_logf(LOG_DEBUG, "idbuf=<%s>\n", idbuf);
		quilt_canon_add_path(item, idbuf);
		process_row(request, query, rs, idbuf, abstract, item);
		quilt_canon_destroy(item);
	}
	if(!sql_stmt_eof(rs))
	{
		query->more = 1;
	}
	sql_stmt_destroy(rs);
	free(abstract);
	return 200;
}

static int
process_row(QUILTREQ *request, struct query_struct *query, SQL_STATEMENT *rs, const char *id, const char *self, QUILTCANON *item)
{
	librdf_statement *st;
	const char *s;
	char *uri, *related;

	(void) id;

	uri = quilt_canon_str(item, QCO_SUBJECT);
	quilt_logf(LOG_DEBUG, "adding row <%s>\n", uri);
	st = quilt_st_create_uri(self, NS_RDFS "seeAlso", uri);
	librdf_model_add_statement(request->model, st);
	librdf_free_statement(st);

	if(query->rcanon)
	{
		related = quilt_canon_str(query->rcanon, QCO_SUBJECT);
		/* foaf:topic */
		st = quilt_st_create_uri(uri, NS_FOAF "topic", related);
		librdf_model_add_statement(request->model, st);
		librdf_free_statement(st);
		free(related);
	}

	/* rdfs:label */
	s = sql_stmt_str(rs, 2);
	if(s)
	{
		add_langvector(request->model, s, uri, NS_RDFS "label");
	}

	/* rdfs:comment */
	s = sql_stmt_str(rs, 3);
	if(s)
	{
		add_langvector(request->model, s, uri, NS_RDFS "comment");
	}

	/* rdf:type */
	s = sql_stmt_str(rs, 1);
	if(s)
	{
		add_array(request->model, s, uri, NS_RDF "type");
	}

	/* geo:lat, geo:long */
	s = sql_stmt_str(rs, 4);
	if(s)
	{
		add_point(request->model, s, uri);
	}
	free(uri);
	return 0;
}

/* Add a URI array to the model */
static int
add_array(librdf_model *model, const char *array, const char *subject, const char *predicate)
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
add_point(librdf_model *model, const char *array, const char *subject)
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
add_langvector(librdf_model *model, const char *vector, const char *subject, const char *predicate)
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
checklang(QUILTREQ *req, const char *lang)
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
