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

struct qbuf_struct
{
	char *buf;
	char *bp;
	size_t size;
	ssize_t remaining;
};

static int process_rs(QUILTREQ *request, struct query_struct *query, SQL_STATEMENT *rs);
static int process_row(QUILTREQ *request, struct query_struct *query, SQL_STATEMENT *rs, const char *id, const char *self, QUILTCANON *item, int index);
static const char *checklang(QUILTREQ *request, const char *lang);
static int add_langvector(librdf_model *model, const char *vector, const char *subject, const char *predicate);
static int add_array(librdf_model *model, const char *array, const char *subject, const char *predicate);
static int add_point(librdf_model *model, const char *array, const char *subject);
static int appendf(struct qbuf_struct *qbuf, const char *fmt, ...);
static int process_membership_row(QUILTREQ *request, SQL_STATEMENT *rs, const char *id, const char *self, QUILTCANON *item);

/* Short names for media classes which can be used for convenience */
struct mediamatch_struct spindle_mediamatch[] = {
	{ "collection", "http://purl.org/dc/dcmitype/Collection" },
	{ "dataset", "http://purl.org/dc/dcmitype/Dataset" },
	{ "video", "http://purl.org/dc/dcmitype/MovingImage" },
	{ "image", "http://purl.org/dc/dcmitype/StillImage" },
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
	struct qbuf_struct qbuf;
	size_t n, c;
	SQL_STATEMENT *rs;
	const void *args[8];
	const char *related, *collection;
	int rankflags;

	memset(args, 0, sizeof(args));
	memset(&qbuf, 0, sizeof(struct qbuf_struct));
	n = 0;
	related = NULL;
	collection = NULL;
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
	else if(query->collection)
	{
		quilt_logf(LOG_DEBUG, QUILT_PLUGIN_NAME ": DB: collection: <%s> (base is <%s>)\n", query->collection, request->base);
		c = strlen(request->base);
		if(!strncmp(query->collection, request->base, c))
		{
			collection = query->collection + c;
			while(collection[0] == '/')
			{
				collection++;
			}
			quilt_logf(LOG_DEBUG, QUILT_PLUGIN_NAME ": DB: collection: '%s'\n", collection);
			if(strlen(collection) != 32)
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
			query->audience = "all";
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
			query->audience = "all";
		}
		if(!query->type)
		{
			query->type = "any";
		}
		quilt_logf(LOG_DEBUG, QUILT_PLUGIN_NAME ": spindle_query_db: media='%s', audience='%s', type='%s'\n", query->media, query->audience, query->type);
	}
	/* SELECT */
	appendf(&qbuf, "SELECT \"i\".\"id\", \"i\".\"classes\", \"i\".\"title\", \"i\".\"description\", \"i\".\"coordinates\", \"i\".\"modified\"");
	if(query->text)
	{
		/* Rank flags:
		 *  0 (the default) ignores the document length
		 *  1 divides the rank by 1 + the logarithm of the document length
		 *  2 divides the rank by the document length
		 *  4 divides the rank by the mean harmonic distance between extents
		 *   (this is implemented only by ts_rank_cd)
		 *  8 divides the rank by the number of unique words in document
		 * 16 divides the rank by 1 + the logarithm of the number of unique
		 *    words in document
		 * 32 divides the rank by itself + 1
		 * See http://www.postgresql.org/docs/9.4/static/textsearch-controls.html
		 */
		if(query->mode == QM_AUTOCOMPLETE)
		{
			rankflags = 32;
			appendf(&qbuf, ", ts_rank_cd(ARRAY[0, 0, 0, 1.0], \"i\".\"index_%s\", \"query\", %d) AS \"rank\"", query->lang, rankflags);
		}
		else
		{
			rankflags = 32;
			appendf(&qbuf, ", ts_rank_cd(\"i\".\"index_%s\", \"query\", %d) AS \"rank\"", query->lang, rankflags);
		}
	}
	/* FROM */
	appendf(&qbuf, " FROM \"index\" \"i\"");
	if(query->text)
	{
		appendf(&qbuf, ", plainto_tsquery(%%Q) \"query\"");
		args[n] = query->text;
		n++;
	}
	if(related)
	{
		/* IS related and MAY have media query (no subquery) */
		appendf(&qbuf, ", \"about\" \"a\"");
		if(query->media)
		{
			appendf(&qbuf, ", \"media\" \"m\"");
		}	
	}
	if(collection && !query->media)
	{
		appendf(&qbuf, ", \"membership\" \"cm\"");
	}
	else if(collection && query->media)
	{
		appendf(&qbuf, " LEFT JOIN \"membership\" \"cm1\" ON (\"i\".\"id\" = \"cm1\".\"id\" AND \"cm1\".\"collection\" = %%Q)");
		args[n] = collection;
		n++;
	}
	/* WHERE */
	if(query->score > 0)
	{
		appendf(&qbuf, " WHERE \"i\".\"score\" <= %d", query->score);
	}
	else
	{
		appendf(&qbuf, " WHERE \"i\".\"score\" IS NOT NULL");
	}
	if(query->text)
	{
		appendf(&qbuf, " AND \"query\" @@ \"index_%s\"", query->lang);
	}
	if(query->qclass)
	{
		appendf(&qbuf, " AND %%Q = ANY(\"i\".\"classes\")");
		args[n] = query->qclass;
		n++;
	}
	if(related)
	{	
		/* IS related and MAY have media query (no subquery) */
		appendf(&qbuf, " AND \"a\".\"about\" = %%Q AND \"a\".\"id\" = \"i\".\"id\"");
		args[n] = related;
		n++;
		if(query->media)
		{
			appendf(&qbuf, " AND \"m\".\"id\" = \"i\".\"id\"");
		}
	}
	else if(query->media)
	{
		/* NOT related but HAS media query (subquery) */
		appendf(&qbuf, " AND \"i\".\"id\" IN ( "
					"SELECT \"a\".\"about\" "
					" FROM \"about\" \"a\" "
					" INNER JOIN \"index_media\" \"im\" ON (\"a\".\"id\" = \"im\".\"id\")"
					" INNER JOIN \"media\" \"m\" ON (\"im\".\"media\" = \"m\".\"id\"");
	}
	if(query->media)
	{
		/* Any media query */
		if(!strcmp(query->audience, "all"))
		{
			/* If the audience is 'all' (the default), we only return media
			 * available to the public.
			 */
			appendf(&qbuf, " AND \"m\".\"audience\" IS NULL");
		}
		else if(strcmp(query->audience, "any"))
		{
			/* If the audience is not 'all' and is not 'any', then we filter by
			 * media available to the public, or to the specified audience
			 */
			appendf(&qbuf, " AND (\"m\".\"audience\" IS NULL OR \"m\".\"audience\" = %%Q)");
			args[n] = query->audience;
			n++;
		}
		/* If the media class is not 'any', then filter by the class URI */
		if(strcmp(query->media, "any"))
		{
			appendf(&qbuf, " AND \"m\".\"class\" = %%Q");
			args[n] = query->media;
			n++;
		}
		/* If the MIME type is not 'any', then filter by media type */
		if(strcmp(query->type, "any"))
		{
			appendf(&qbuf, " AND \"m\".\"type\" = %%Q");
			args[n] = query->type;
			n++;
		}
	}
	if(!related && query->media)
	{
		/* NOT related and HAS media query (subquery) */	   
		if(collection)
		{
			appendf(&qbuf, ")"
					   " LEFT JOIN \"membership\" \"cm2\" ON (\"im\".\"id\" = \"cm2\".\"id\" AND \"cm2\".\"collection\" = %%Q)"
					   " WHERE (\"cm1\".\"collection\" IS NOT NULL OR \"cm2\".\"collection\" IS NOT NULL)"
					   ")");
			args[n] = collection;
			n++;
		}
		else
		{
			appendf(&qbuf, ") )");
		}
	}
	/* Collection (but no media): the item must be within the specified collection */
	else if(collection)
	{
		appendf(&qbuf, " AND \"cm\".\"id\" = \"i\".\"id\" AND \"cm\".\"collection\" = %%Q");
		args[n] = collection;
		n++;
	}
	/* ORDER BY */
	if(query->text)
	{
		appendf(&qbuf, " ORDER BY \"rank\" DESC, \"i\".\"score\" ASC, \"modified\" DESC");
	}
	else
	{
		appendf(&qbuf, " ORDER BY \"modified\" DESC");
	}
	/* LIMIT ... OFFSET ... */
	if(request->offset)
	{
		appendf(&qbuf, " LIMIT %d OFFSET %d", request->limit + 1, request->offset);
	}
	else
	{
		appendf(&qbuf, " LIMIT %d", request->limit + 1);
	}
	rs = sql_queryf(spindle_db, qbuf.buf, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
	free(qbuf.buf);
	if(!rs)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": query execution failed\n");
		return 500;
	}
	return process_rs(request, query, rs);	
}

/* For a given item, determine what collections (if any) this item is part
 * of.
 */
int
spindle_membership_db(QUILTREQ *request)
{
	size_t c;
	const char *id, *t;
	SQL_STATEMENT *rs;
	QUILTCANON *item;
	char idbuf[36];
	char *self, *p;

	c = strlen(request->base);
	if(!strncmp(request->subject, request->base, c))
	{
		id = request->subject + c;
		while(id[0] == '/')
		{
			id++;
		}
		quilt_logf(LOG_DEBUG, QUILT_PLUGIN_NAME ": DB: membership: '%s'\n", id);
		if(strlen(id) != 32)
		{
			return 404;
		}
	}
	else
	{
		return 404;
	}
	if(request->index)
	{
		self = quilt_canon_str(request->canonical, (request->ext ? QCO_ABSTRACT : QCO_REQUEST));
	}
	else
	{
		self = quilt_canon_str(request->canonical, QCO_NOEXT|QCO_FRAGMENT);
	}
	rs = sql_queryf(spindle_db, "SELECT \"collection\" FROM \"membership\" WHERE \"id\" = %Q", id);
	if(!rs)
	{
		free(self);
		return 500;
	}
	for(; !sql_stmt_eof(rs); sql_stmt_next(rs))
	{
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
		process_membership_row(request, rs, idbuf, self, item);
		quilt_canon_destroy(item);
	}
	sql_stmt_destroy(rs);
	free(self);
	return 200;
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

/* Retrieve the list of audiences */
int
spindle_audiences_db(QUILTREQ *request, struct query_struct *query)
{
	SQL_STATEMENT *rs;
	int limit, offset;
	QUILTCANON *dest;
	char *self, *deststr;
	const char *audience, *id;
	librdf_statement *st;
	const char *title;

	limit = request->limit;
	offset = request->offset;
	
	/* We synthesise an entry at the start of the list, so adjust our
	 * limits and offsets accordingly
	 */
	if(!offset)
	{
		limit--;
	}
	else
	{
		offset--;
	}
	if(!limit)
	{
		return 200;
	}
	if(offset)
	{
		rs = sql_queryf(spindle_db, "SELECT \"a\".\"uri\", \"a\".\"id\", \"i\".\"title\" FROM \"audiences\" \"a\" LEFT JOIN \"index\" \"i\" ON \"i\".\"id\" = \"a\".\"id\" LIMIT %d OFFSET %d", limit + 1, offset);
	}
	else
	{
		rs = sql_queryf(spindle_db, "SELECT \"a\".\"uri\", \"a\".\"id\", \"i\".\"title\" FROM \"audiences\" \"a\" LEFT JOIN \"index\" \"i\" ON \"i\".\"id\" = \"a\".\"id\" LIMIT %d", limit + 1);
	}
	if(!rs)
	{
		return 500;
	}
	self = quilt_canon_str(request->canonical, (request->ext ? QCO_ABSTRACT : QCO_REQUEST));
	dest = quilt_canon_create(request->canonical);
	quilt_canon_reset_path(dest);
	quilt_canon_reset_params(dest);
	quilt_canon_set_fragment(dest, NULL);
	for(; limit && !sql_stmt_eof(rs); sql_stmt_next(rs))
	{
		limit--;
		audience = sql_stmt_str(rs, 0);
		id = sql_stmt_str(rs, 1);
		title = sql_stmt_str(rs, 2);
		quilt_canon_set_param(dest, "for", audience);
		deststr = quilt_canon_str(dest, QCO_DEFAULT);
		st = quilt_st_create_uri(audience, NS_RDF "type", NS_ODRL "Group");
		librdf_model_add_statement(request->model, st);
		librdf_free_statement(st);
		st = quilt_st_create_uri(audience, NS_RDFS "seeAlso", deststr);
		librdf_model_add_statement(request->model, st);
		librdf_free_statement(st);
		st = quilt_st_create_uri(self, NS_RDFS "seeAlso", audience);
		librdf_model_add_statement(request->model, st);
		librdf_free_statement(st);
		if(title)
		{
			add_langvector(request->model, title, audience, NS_RDFS "label");
		}
		free(deststr);
	}
	if(!limit && !sql_stmt_eof(rs))
	{
		query->more = 1;
	}
	quilt_canon_destroy(dest);
	sql_stmt_destroy(rs);
	free(self);
	return 200;
}

static int
process_rs(QUILTREQ *request, struct query_struct *query, SQL_STATEMENT *rs)
{
	QUILTCANON *item;
	int c;
	const char *t;
	char idbuf[36], *p, *self;	

	if(request->index)
	{
		self = quilt_canon_str(request->canonical, (request->ext ? QCO_ABSTRACT : QCO_REQUEST));
	}
	else
	{
		self = quilt_canon_str(request->canonical, QCO_NOEXT|QCO_FRAGMENT);
	}
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
		process_row(request, query, rs, idbuf, self, item, query->offset + c);
		quilt_canon_destroy(item);
	}
	if(!sql_stmt_eof(rs))
	{
		query->more = 1;
	}
	sql_stmt_destroy(rs);
	free(self);
	return 200;
}

static int
process_row(QUILTREQ *request, struct query_struct *query, SQL_STATEMENT *rs, const char *id, const char *self, QUILTCANON *item, int index)
{
	librdf_statement *st;
	const char *s;
	QUILTCANON *slot;
	char *slotstr, *uri, *related;
	char nbuf[64];
	librdf_node *node;
	
	(void) id;

	slot = quilt_canon_create(request->canonical);
	quilt_canon_set_fragment(slot, id);
	slotstr = quilt_canon_str(slot, QCO_FRAGMENT);
	
	uri = quilt_canon_str(item, QCO_SUBJECT);
	quilt_logf(LOG_DEBUG, "adding row <%s>\n", uri);
	
	/* rdfs:seeAlso */
	st = quilt_st_create_uri(self, NS_RDFS "seeAlso", uri);
	librdf_model_add_statement(request->model, st);
	librdf_free_statement(st);

	/* olo:slot */
	st = quilt_st_create_uri(self, NS_OLO "slot", slotstr);
	librdf_model_add_statement(request->model, st);
	librdf_free_statement(st);

	/* <slot> rdf:type olo:Slot */
	st = quilt_st_create_uri(slotstr, NS_RDF "type", NS_OLO "Slot");
	librdf_model_add_statement(request->model, st);
	librdf_free_statement(st);

	/* <slot> olo:item <item> */
	st = quilt_st_create_uri(slotstr, NS_OLO "item", uri);
	librdf_model_add_statement(request->model, st);
	librdf_free_statement(st);

	/* <slot> rdfs:label "Result item %d" */
	snprintf(nbuf, sizeof(nbuf) - 1, "Result #%d", index);
	st = quilt_st_create_literal(slotstr, NS_RDFS "label", nbuf, "en-gb");
	librdf_model_add_statement(request->model, st);
	librdf_free_statement(st);

	/* <slot> olo:index nn */
	st = quilt_st_create(slotstr, NS_OLO "index");
	node = quilt_node_create_int(index);
	librdf_statement_set_object(st, node);
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
	quilt_canon_destroy(slot);
	free(slotstr);
	return 0;
}

static int
process_membership_row(QUILTREQ *request, SQL_STATEMENT *rs, const char *id, const char *self, QUILTCANON *item)
{
	char *uri;
	librdf_statement *st;

	(void) rs;
	(void) id;

	/* <item> dct:isPartOf <self> */
	uri = quilt_canon_str(item, QCO_SUBJECT);
	if(!strcasecmp(self, uri))
	{
		free(uri);
		return 0;
	}
	st = quilt_st_create_uri(self, NS_DCTERMS "isPartOf", uri);
	librdf_model_add_statement(request->model, st);
	librdf_free_statement(st);
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

static int
appendf(struct qbuf_struct *qbuf, const char *fmt, ...)
{
	char *p;
	va_list ap;
	int l;
	
	va_start(ap, fmt);
	l = vsnprintf(qbuf->bp, qbuf->remaining, fmt, ap);
	va_end(ap);
	if(l == -1)
	{
		return -1;
	}
	if(!l)
	{
		return 0;
	}
	if(l >= qbuf->remaining)
	{
		p = (char *) realloc(qbuf->buf, qbuf->size + l + 1);
		if(!p)
		{
			return -1;
		}
		if(qbuf->buf)
		{
			qbuf->bp = p + (qbuf->bp - qbuf->buf);
			qbuf->buf = p;
		}
		else
		{
			qbuf->buf = p;
			qbuf->bp = p;
		}
		qbuf->size += l + 1;
		qbuf->remaining += l + 1;
		va_start(ap, fmt);
		l = vsnprintf(qbuf->bp, qbuf->remaining, fmt, ap);
		va_end(ap);
	}
	qbuf->bp += l;
	qbuf->remaining -= l;
	return 0;
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
