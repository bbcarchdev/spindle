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

static int spindle_index_meta_(QUILTREQ *request, const char *abstract, struct query_struct *query);
static int spindle_index_title_(QUILTREQ *request, const char *abstract, struct query_struct *query);

int
spindle_index(QUILTREQ *request, const char *qclass)
{
	struct query_struct query;
	int r;
	char *abstract;

	memset(&query, 0, sizeof(query));
	query.qclass = qclass;
	query.offset = request->offset;
	if(request->offset)
	{
		quilt_canon_set_param_int(request->canonical, "offset", request->offset);
	}
	query.limit = request->limit;
	if(request->limit != request->deflimit)
	{
		quilt_canon_set_param_int(request->canonical, "limit", request->limit);
	}
	query.text = quilt_request_getparam(request, "q");
	query.lang = quilt_request_getparam(request, "lang");
	query.media = quilt_request_getparam(request, "media");
	if(query.media)
	{
		quilt_canon_set_param(request->canonical, "media", query.media);
	}
	query.audience = quilt_request_getparam(request, "for");
	if(query.audience && strcmp(query.audience, "any"))
	{
		quilt_canon_set_param(request->canonical, "for", query.audience);
	}
	query.type = quilt_request_getparam(request, "type");
	if(query.type && strcmp(query.type, "any"))
	{
		quilt_canon_set_param(request->canonical, "type", query.type);
	}
	r = spindle_query(request, &query);
	if(r == 200)
	{
		abstract = quilt_canon_str(request->canonical, QCO_ABSTRACT);
		spindle_index_meta_(request, abstract, &query);
		free(abstract);
	}
	return r;
}

int
spindle_query(QUILTREQ *request, struct query_struct *query)
{
	int r;
	size_t l;

	if(query->related)
	{
		query->rcanon = quilt_canon_create(NULL);
		l = strlen(request->base);
		if(!strncmp(query->related, request->base, l))
		{
			quilt_canon_set_base(query->rcanon, request->base);
			quilt_canon_add_path(query->rcanon, query->related + l);
		}
		else
		{
			quilt_canon_set_base(query->rcanon, query->related);
		}
		quilt_canon_set_fragment(query->rcanon, "id");	
	}
	if(spindle_db)
	{
	    r = spindle_query_db(request, query);
	}
	else
	{
		r = spindle_query_sparql(request, query);
	}
	if(query->rcanon)
	{
		quilt_canon_destroy(query->rcanon);
	}
	return r;
}

static int
spindle_index_meta_(QUILTREQ *request, const char *abstract, struct query_struct *query)
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
	if(query->more)
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
	spindle_index_title_(request, abstract, query);

	spindle_add_concrete(request); 

	free(root);
	return 0;
}

static int
spindle_index_title_(QUILTREQ *request, const char *abstract, struct query_struct *query)
{
	librdf_statement *st;
	size_t len, c;
	char *buf, *p;
	int sing;

	if(request->indextitle)
	{
		len = strlen(request->indextitle) + 1;
	}
	else if(query->qclass)
	{
		/* Items with class <...> */
		len = strlen(query->qclass) + 22;
	}
	else
	{
		/* "Everything" */
		len = 11;
	}
	if(query->text)
	{
		/* containing "..." */
		len += strlen(query->text) + 16;
	}
	if(query->media || query->type || query->audience)
	{
		/* which have related ... media */
		len += 25;
		if(query->media)
		{			
			len += strlen(query->media) + 2;
		}
		if(query->type)
		{
			/* which is ... */
			len += strlen(query->type) + 10;
		}
		if(query->audience)
		{
			/* available to [everyone | <audience>] */
			len += strlen(query->audience) + 22;
		}
	}
	buf = (char *) malloc(len + 1);
	if(!buf)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to allocate %lu bytes for index title buffer\n", (unsigned long) len + 1);
		return -1;
	}
	p = buf;
	sing = 0;
	if(request->indextitle)
	{
		strcpy(p, request->indextitle);
		p = strchr(p, 0);
		/* We should have a flag indicating collective/singular form */
		if(!strcasecmp(request->indextitle, "everything"))
		{
			sing = 1;
		}
	}
	else if(query->qclass)
	{
		strcpy(p, "Items with class <");
		p = strchr(p, 0);
		strcpy(p, query->qclass);
		p = strchr(p, 0);
		*p = '>';
		p++;
	}
	else
	{
		strcpy(p, "Everything");
		p = strchr(p, 0);
		sing = 1;
	}
	if(query->text)
	{
		strcpy(p, " containing \"");
		p = strchr(p, 0);
		strcpy(p, query->text);
		p = strchr(p, 0);
		*p = '"';
		p++;
	}
	if(query->media || query->type || query->audience)
	{
		/* which have */
		if(sing)
		{
			strcpy(p, " which has related");
		}
		else
		{
			strcpy(p, " which have related");
		}
		p = strchr(p, 0);
		for(c = 0; spindle_mediamatch[c].name; c++)
		{
			if(!strcmp(spindle_mediamatch[c].uri, query->media))
			{
				*p = ' ';
				p++;
				strcpy(p, spindle_mediamatch[c].name);
				p = strchr(p, 0);
				break;
			}
		}
		if(!spindle_mediamatch[c].name)
		{
			if(query->media && strcmp(query->media, "any"))
			{
				*p = ' ';
				p++;
				*p = '<';
				p++;
				strcpy(p, query->media);
				p = strchr(p, 0);
				*p = '>';
				p++;
			}
			strcpy(p, " media");
			p = strchr(p, 0);
		}
		if(query->type && strcmp(query->type, "any"))
		{
			strcpy(p, " which is ");
			p = strchr(p, 0);
			strcpy(p, query->type);
			p = strchr(p, 0);
		}
		if(query->audience && strcmp(query->audience, "any"))
		{
			if(!strcmp(query->audience, "all"))
			{
				strcpy(p, " available to everyone");
				p = strchr(p, 0);
			}
			else
			{
				strcpy(p, " available to <");
				p = strchr(p, 0);
				strcpy(p, query->audience);
				p = strchr(p, 0);
				*p = '>';
				p++;
			}
		}
	}
	*p = 0;
	
	st = quilt_st_create_literal(abstract, NS_RDFS "label", buf, "en-gb");
	librdf_model_add_statement(request->model, st);
	librdf_free_statement(st);
	free(buf);

	return 0;
}
