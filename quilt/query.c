/* This engine processes requests for coreference graphs populated
 * by Twine's "spindle" post-processing module.
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

#include "p_spindle.h"

static char *spindle_query_subjtitle_(QUILTREQ *request, const char *primary, const char *secondary);
static int spindle_query_title_(QUILTREQ *request, const char *abstract, struct query_struct *query);


/* Initialise a query_struct */
int
spindle_query_init(struct query_struct *dest)
{
	memset(dest, 0, sizeof(struct query_struct));
	dest->score = -1;
	return 0;
}

/* Populate an empty query_struct from a QUILTREQ */
int
spindle_query_request(struct query_struct *dest, QUILTREQ *request, const char *qclass)
{
	const char *t;

	t = quilt_request_getparam(request, "q");
	if(t && t[0])
	{
		dest->explicit = 1;
		quilt_canon_set_param(request->canonical, "q", t);
	}
	t = quilt_request_getparam(request, "class");
	if(t && t[0])
	{
		dest->explicit = 1;
		quilt_canon_set_param(request->canonical, "class", t);
		dest->qclass = t;
	}
	else if(qclass && qclass[0])
	{
		dest->qclass = qclass;
	}
	dest->offset = request->offset;
	if(request->offset)
	{
		quilt_canon_set_param_int(request->canonical, "offset", request->offset);
	}
	dest->limit = request->limit;
	if(request->limit != request->deflimit)
	{
		quilt_canon_set_param_int(request->canonical, "limit", request->limit);
	}
	dest->text = quilt_request_getparam(request, "q");
	dest->lang = quilt_request_getparam(request, "lang");
	dest->media = quilt_request_getparam(request, "media");
	if((dest->text && dest->text[0]) || (dest->lang && dest->lang[0]) || (dest->media && dest->media[0]))
	{
		dest->explicit = 1;
	}
	if(dest->media)
	{
		quilt_canon_set_param(request->canonical, "media", dest->media);
	}

	// Process for= parameters
	dest->audience = quilt_request_getparam_multi(request, "for");
	if(dest->audience)
	{
		// Put the array of for= params in the request
		quilt_canon_set_param_multi(request->canonical, "for", dest->audience);
		dest->explicit = 1;
	}

	dest->type = quilt_request_getparam(request, "type");
	if(dest->type && dest->type[0])
	{
		dest->explicit = 1;
	}
	if(dest->type && strcmp(dest->type, "any"))
	{
		quilt_canon_set_param(request->canonical, "type", dest->type);
	}
	t = quilt_request_getparam(request, "mode");
	if(t && t[0])
	{
		if(!strcmp(t, "autocomplete"))
		{
			dest->mode = QM_AUTOCOMPLETE;
		}
		else
		{
			t = NULL;
		}
		if(t)
		{
			quilt_canon_set_param(request->canonical, "mode", t);
		}
	}
	t = quilt_request_getparam(request, "score");
	if(t && t[0])
	{
		dest->score = atoi(t);
		quilt_canon_set_param(request->canonical, "score", t);
	}
	if(dest->score == -1)
	{
		dest->score = spindle_threshold;
	}
	return 200;
}

/* Perform a query (using either a database or SPARQL back-ends) */
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

/* Generate information about the query, such as navigational links */
int
spindle_query_meta(QUILTREQ *request, struct query_struct *query)
{
	QUILTCANON *link;
	char *root, *linkstr, *abstract;
	int c;
	librdf_statement *st;

	if(request->index)
	{
		/* If this is an index, then the dataset is the abstract document URI */
		abstract = quilt_canon_str(request->canonical, (request->ext ? QCO_ABSTRACT : QCO_REQUEST));
		root = quilt_canon_str(request->canonical, QCO_ABSTRACT|QCO_NOPARAMS);
	}
	else
	{
		/* If this is an item, then the dataset is the actual item */
		abstract = quilt_canon_str(request->canonical, QCO_NOEXT|QCO_FRAGMENT);
		root = quilt_canon_str(request->canonical, QCO_SUBJECT|QCO_NOPARAMS);
	}
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
		if(request->index)
		{
			linkstr = quilt_canon_str(link, QCO_ABSTRACT);
		}
		else
		{
			linkstr = quilt_canon_str(link, QCO_NOEXT|QCO_FRAGMENT);
		}
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
		if(request->index)
		{
			linkstr = quilt_canon_str(link, QCO_ABSTRACT);
		}
		else
		{
			linkstr = quilt_canon_str(link, QCO_NOEXT|QCO_FRAGMENT);
		}
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

	if(request->index || query->explicit || query->offset || query->limit != request->deflimit)
	{
		/* ... rdf:label */
		spindle_query_title_(request, abstract, query);
	}
	
	free(root);
	free(abstract);
	return 200;
}

/* Add OpenSearch descriptive metadata and friends to a dataset or subset */
int
spindle_query_osd(QUILTREQ *request)
{
	char *self, *linkstr;
	QUILTCANON *link;
	librdf_statement *st;
	librdf_node *graph;

	graph = quilt_request_graph(request);
	/* Add OpenSearch information to the index */
	if(request->index)
	{
		self = quilt_canon_str(request->canonical, (request->ext ? QCO_ABSTRACT : QCO_REQUEST));
	}
	else
	{
		self = quilt_canon_str(request->canonical, QCO_NOEXT|QCO_FRAGMENT);
	}
	link = quilt_canon_create(request->canonical);
	quilt_canon_reset_params(link);
	quilt_canon_add_param(link, "q", "{searchTerms?}");
	quilt_canon_add_param(link, "lang", "{language?}");
	quilt_canon_add_param(link, "limit", "{count?}");
	quilt_canon_add_param(link, "offset", "{startIndex?}");
	if(request->home || !request->index)
	{
		quilt_canon_add_param(link, "class", "{rdfs:Class?}");
	}
	quilt_canon_add_param(link, "for", "{odrl:Party?}");
	quilt_canon_add_param(link, "media", "{dct:DCMIType?}");
	quilt_canon_add_param(link, "type", "{dct:IMT?}");
	quilt_canon_set_ext(link, NULL);
	linkstr = quilt_canon_str(link, QCO_ABSTRACT|QCO_FRAGMENT);
	st = quilt_st_create_literal(self, NS_OSD "template", linkstr, NULL);
	librdf_model_context_add_statement(request->model, graph, st);
	librdf_free_statement(st);
	free(linkstr);
	quilt_canon_destroy(link);

	st = quilt_st_create_literal(self, NS_OSD "Language", "en-gb", NULL);
	librdf_model_context_add_statement(request->model, graph, st);
	librdf_free_statement(st);

	st = quilt_st_create_literal(self, NS_OSD "Language", "cy-gb", NULL);
	librdf_model_context_add_statement(request->model, graph, st);
	librdf_free_statement(st);

	st = quilt_st_create_literal(self, NS_OSD "Language", "gd-gb", NULL);
	librdf_model_context_add_statement(request->model, graph, st);
	librdf_free_statement(st);

	st = quilt_st_create_literal(self, NS_OSD "Language", "ga-gb", NULL);
	librdf_model_context_add_statement(request->model, graph, st);
	librdf_free_statement(st);

	if(request->home)
	{
		/* Add VoID descriptive metadata */	
		st = quilt_st_create_uri(self, NS_RDF "type", NS_VOID "Dataset");
		librdf_model_context_add_statement(request->model, graph, st);
		librdf_free_statement(st);
		
		link = quilt_canon_create(request->canonical);
		quilt_canon_reset_params(link);
		quilt_canon_add_param(link, "uri", "");
		linkstr = quilt_canon_str(link, QCO_ABSTRACT);
		st = quilt_st_create_uri(self, NS_VOID "uriLookupEndpoint", linkstr);
		librdf_model_context_add_statement(request->model, graph, st);
		librdf_free_statement(st);	
		free(linkstr);
		quilt_canon_destroy(link);
	}
	
	if(request->home)
	{
		/* ... void:openSearchDescription </xxx.osd> */
		link = quilt_canon_create(request->canonical);
		quilt_canon_reset_params(link);
		quilt_canon_set_explicitext(link, NULL);
		quilt_canon_set_ext(link, "osd");
		linkstr = quilt_canon_str(link, QCO_CONCRETE);
		st = quilt_st_create_uri(self, NS_VOID "openSearchDescription", linkstr);	
		librdf_model_context_add_statement(request->model, graph, st);
		librdf_free_statement(st);	
		free(linkstr);
		quilt_canon_destroy(link);
	}

	return 200;
}

static char *
spindle_query_subjtitle_(QUILTREQ *request, const char *primary, const char *secondary)
{
	char *subj, *pri, *sec, *none;
	const char *lang, *value;
	librdf_statement *query, *st;
	librdf_stream *stream;
	librdf_node *obj;
	
	pri = NULL;
	sec = NULL;
	none = NULL;
	subj = quilt_canon_str(request->canonical, QCO_SUBJECT|QCO_NOPARAMS);
	query = quilt_st_create(subj, NS_RDFS "label");
	free(subj);
	for(stream = librdf_model_find_statements(request->model, query);
		!librdf_stream_end(stream);
		librdf_stream_next(stream))
	{
		st = librdf_stream_get_object(stream);
		obj = librdf_statement_get_object(st);
		if(!librdf_node_is_literal(obj))
		{
			continue;
		}
		value = (const char *) librdf_node_get_literal_value(obj);
		if(!value)
		{
			continue;
		}
		lang = librdf_node_get_literal_value_language(obj);
		if(lang)
		{
			if(!pri && primary && !strcasecmp(lang, primary))
			{
				pri = strdup(value);
			}
			if(!sec && secondary && !strcasecmp(lang, secondary))
			{
				sec = strdup(value);
			}
		}
		else if(!none)
		{
			none = strdup(value);
		}
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	if(pri)
	{
		free(sec);
		free(none);
		return pri;
	}
	if(sec)
	{
		free(none);
		return sec;
	}
	return none;
}

static int
spindle_query_title_(QUILTREQ *request, const char *abstract, struct query_struct *query)
{
	librdf_statement *st;
	size_t len, c;
	char *buf, *p, *title_en_gb;
	int sing;

	title_en_gb = NULL;
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
	if(query->collection)
	{
		/* within <...> */
		title_en_gb = spindle_query_subjtitle_(request, "en-gb", "en");
		if(title_en_gb)
		{
			len += strlen(title_en_gb) + 16;
		}
		else
		{
			len += strlen(query->collection) + 16;
		}
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
			size_t i=0;
			while(query->audience && (query->audience[i] != NULL)) {
				len += strlen(query->audience[i]) + 22;
				i++;
			}
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
	if(query->collection)
	{
		if(title_en_gb)
		{
			strcpy(p, " within “");
			p = strchr(p, 0);
			strcpy(p, title_en_gb);
			p = strchr(p, 0);
			strcpy(p, "”");
			p = strchr(p, 0);
		}
		else
		{
			strcpy(p, " within <");
			p = strchr(p, 0);
			strcpy(p, query->collection);
			p = strchr(p, 0);
			*p = '>';
			p++;
		}
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
			if(query->media && !strcmp(spindle_mediamatch[c].uri, query->media))
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
		if(query->audience && !spindle_array_contains(query->audience, "any"))
		{
			if(spindle_array_contains(query->audience, "all"))
			{
				strcpy(p, " available to everyone");
				p = strchr(p, 0);
			}
			else
			{
				strcpy(p, " available to <");
				size_t i=0;
				while(query->audience[i] != NULL)
				{
					strcat(p, query->audience[i++]);
					if (query->audience[i] != NULL)
					{
						strcat(p, ", ");
					}
				}
				strcat(p, ">");
			}
		}
	}
	*p = 0;

	st = quilt_st_create_literal(abstract, NS_RDFS "label", buf, "en-gb");
	librdf_model_add_statement(request->model, st);
	librdf_free_statement(st);
	free(buf);
	free(title_en_gb);
	return 0;
}

int
spindle_membership(QUILTREQ *request)
{
	if(spindle_db)
	{
		return spindle_membership_db(request);
	}
	return 200;
}
