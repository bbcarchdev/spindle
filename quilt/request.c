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

static int spindle_request_is_query_(QUILTREQ *req);
static const char *spindle_request_is_lookup_(QUILTREQ *req);
static int spindle_request_is_partition_(QUILTREQ *req, char **qclass);
static int spindle_request_is_item_(QUILTREQ *req);
static struct spindle_dynamic_endpoint *spindle_request_is_dynamic_(QUILTREQ *req);
static int spindle_request_audiences_(QUILTREQ *req, struct spindle_dynamic_endpoint *endpoint);

static struct spindle_dynamic_endpoint spindle_endpoints[] =
{
	{ "/audiences", 10, spindle_request_audiences_ },
	{ NULL, 0, NULL }
};

int
spindle_process(QUILTREQ *request)
{
	char *qclass;
	int r;
	const char *uri;
	struct spindle_dynamic_endpoint *endpoint;

	/* Process a request and determine how it should be handled.
	 *
	 * In order of preference:
	 *
	 * - Requests for partitions (look-up against our static list)
	 * - Requests for items (pattern match)
	 * - Requests for endpoints that are generated on the fly
	 *	 (such as /audiences)
	 * - URI lookup queries
	 * - Queries at the index, if no path parameters
	 */

	qclass = NULL;
	if(spindle_request_is_partition_(request, &qclass))
	{
		r = spindle_index(request, qclass);
		free(qclass);
		return r;
	}
	if(spindle_request_is_item_(request))
	{
		return spindle_item(request);
	}
	if((endpoint = spindle_request_is_dynamic_(request)))
	{
		return endpoint->process(request, endpoint);
	}
	uri = spindle_request_is_lookup_(request);
	if(uri)
	{
		return spindle_lookup(request, uri);
	}
	if(spindle_request_is_query_(request))
	{
		return spindle_index(request, NULL);
	}
	if(request->home)
	{
		return spindle_home(request);
	}
	return 404;
}

/* Add information to the model about relationship between the concrete and
 * abstract documents
 */
int
spindle_add_concrete(QUILTREQ *request)
{
	const char *s;
	char *subject, *abstract, *concrete, *typebuf;
	librdf_world *world;
	librdf_statement *st;
	librdf_node *graph;
	int explicit;

	graph = quilt_request_graph(request);
	explicit = (request->ext != NULL);
	abstract = quilt_canon_str(request->canonical, (explicit ? QCO_ABSTRACT : QCO_REQUEST));
	concrete = quilt_canon_str(request->canonical, (explicit ? QCO_REQUEST : QCO_CONCRETE));
	subject = quilt_canon_str(request->canonical, QCO_NOEXT|QCO_FRAGMENT);

	/* abstract foaf:primaryTopic subject */
	if(strchr(subject, '#'))
	{
		st = quilt_st_create_uri(abstract, NS_FOAF "primaryTopic", subject);
		librdf_model_context_add_statement(request->model, graph, st);
		librdf_free_statement(st);
	}

	/* abstract dct:hasFormat concrete */
	st = quilt_st_create_uri(abstract, NS_DCTERMS "hasFormat", concrete);
	librdf_model_context_add_statement(request->model, graph, st);
	librdf_free_statement(st);

	/* concrete rdf:type ... */
	st = quilt_st_create_uri(concrete, NS_RDF "type", NS_DCMITYPE "Text");
	librdf_model_context_add_statement(request->model, graph, st);
	librdf_free_statement(st);	
	s = NULL;
	if(!strcmp(request->type, "text/turtle"))
	{
		s = NS_FORMATS "Turtle";
	}
	else if(!strcmp(request->type, "application/rdf+xml"))
	{
		s = NS_FORMATS "RDF_XML";
	}
	else if(!strcmp(request->type, "text/rdf+n3"))
	{
		s = NS_FORMATS "N3";
	}
	if(s)
	{
		st = quilt_st_create_uri(concrete, NS_RDF "type", s);
		librdf_model_context_add_statement(request->model, graph, st);
		librdf_free_statement(st);
	}

	typebuf = (char *) malloc(strlen(NS_MIME) + strlen(request->type) + 1);
	if(typebuf)
	{
		strcpy(typebuf, NS_MIME);
		strcat(typebuf, request->type);
		st = quilt_st_create_uri(concrete, NS_DCTERMS "format", typebuf);
		librdf_model_context_add_statement(request->model, graph, st);
		librdf_free_statement(st);
		free(typebuf);
	}

	free(abstract);
	free(concrete);

	return 200;
}

/* Is this a request constituting a query for something against the index?
 *
 * Note that this only applies at the root - if we already know it's a
 * non-home index then the query will be performed automatically
 */
static int
spindle_request_is_query_(QUILTREQ *request)
{
	const char *t;

	if(!request->home)
	{
		return 0;
	}
	if(((t = quilt_request_getparam(request, "q")) && t[0]) ||
	   ((t = quilt_request_getparam(request, "media")) && t[0]) ||
	   ((t = quilt_request_getparam(request, "for")) && t[0]) ||
	   ((t = quilt_request_getparam(request, "type")) && t[0]))
	{
		request->index = 1;
		request->home = 0;
		return 1;
	}
	return 0;
}

/* Is this a request for a (potential) item? */
static int
spindle_request_is_item_(QUILTREQ *request)
{
	size_t l;
	const char *t;

	for(t = request->path; *t == '/'; t++);
	if(!*t)
	{
		return 0;
	}
	for(l = 0; isalnum(*t); t++)
	{
		l++;
	}
	if(*t && *t != '/')
	{
		return 0;
	}
	if(l != 32)
	{
		return 0;
	}
	return 1;
}

/* Is this a request for a class partition? If so, return the URI string */
static int
spindle_request_is_partition_(QUILTREQ *request, char **qclass)
{
	const char *t;
	size_t c;

	*qclass = NULL;
	/* First check to determine whether there's a match against the list */
	for(c = 0; spindle_indices[c].uri; c++)
	{
		if(!strcmp(request->path, spindle_indices[c].uri))
		{
			if(spindle_indices[c].qclass)
			{
				*qclass = (char *) calloc(1, 32 + strlen(spindle_indices[c].qclass));
				if(spindle_db)
				{
					strcpy(*qclass, spindle_indices[c].qclass);
				}
				else
				{
					sprintf(*qclass, "FILTER ( ?class = <%s> )", spindle_indices[c].qclass);
				}
			}
			request->indextitle = spindle_indices[c].title;
			request->index = 1;
			request->home = 0;
			quilt_canon_add_path(request->canonical, spindle_indices[c].uri);
			return 1;
		}
	}
	/* Check for an explicit ?class=... parameter at the root */
	t = quilt_request_getparam(request, "class");
	if(t && request->home)
	{
		quilt_canon_set_param(request->canonical, "class", t);
		*qclass = (char *) calloc(1, 32 + strlen(t));
		if(spindle_db)
		{
			strcpy(*qclass, t);
		}
		else
		{
			sprintf(*qclass, "FILTER ( ?class = <%s> )", t);
		}
		if(!request->indextitle)
		{
			request->indextitle = t;
		}
		request->index = 1;
		request->home = 0;
		return 1;
	}
	return 0;
}

static const char *
spindle_request_is_lookup_(QUILTREQ *request)
{
	const char *t;

	if(!request->home)
	{
		return NULL;
	}
	t = quilt_request_getparam(request, "uri");
	if(t && t[0])
	{
		return t;
	}
	return NULL;
}

static struct spindle_dynamic_endpoint *
spindle_request_is_dynamic_(QUILTREQ *req)
{
	size_t c;

	for(c = 0; spindle_endpoints[c].path; c++)
	{
		if(!strncmp(req->path, spindle_endpoints[c].path, spindle_endpoints[c].pathlen))
		{
			if(!req->path[spindle_endpoints[c].pathlen] ||
				 req->path[spindle_endpoints[c].pathlen] == '/')
			{
				return &(spindle_endpoints[c]);
			}
		}
	}
	return NULL;
}

static int
spindle_request_audiences_(QUILTREQ *req, struct spindle_dynamic_endpoint *endpoint)
{
	librdf_statement *st;
	QUILTCANON *entry, *dest;
	char *entrystr, *deststr, *self;
	int r;
	struct query_struct query;
	librdf_node *graph;

	graph = quilt_request_graph(req);
	spindle_query_init(&query);
	req->index = 1;
	req->home = 0;
	/* There are no subsidiary resources, only accept the actual endpoint as
	 * a request-URI
	 */
	quilt_canon_add_path(req->canonical, req->path);
	if(strcmp(req->path, endpoint->path))
	{
		return 404;
	}
	if(req->offset)
	{
		quilt_canon_set_param_int(req->canonical, "offset", req->offset);
	}
	if(req->limit != req->deflimit)
	{
		quilt_canon_set_param_int(req->canonical, "limit", req->limit);
	}
	req->indextitle = "Audiences";
	self = quilt_canon_str(req->canonical, (req->ext ? QCO_ABSTRACT : QCO_REQUEST));
/*	st = quilt_st_create_literal(self, NS_RDFS "label", "Audiences", "en-gb");
	librdf_model_add_statement(req->model, st);
	librdf_free_statement(st);	
	st = quilt_st_create_uri(self, NS_RDF "type", NS_VOID "Dataset");
	librdf_model_add_statement(req->model, st);
	librdf_free_statement(st);	 */

	if(!req->offset)
	{		
		/* Generate a magic entry for 'any' */
		entry = quilt_canon_create(req->canonical);
		quilt_canon_reset_path(entry);
		quilt_canon_reset_params(entry);
		quilt_canon_add_path(entry, endpoint->path);
		quilt_canon_set_fragment(entry, "all");
		entrystr = quilt_canon_str(entry, QCO_SUBJECT);
		dest = quilt_canon_create(req->canonical);
		quilt_canon_reset_path(dest);
		quilt_canon_reset_params(dest);
		quilt_canon_add_path(dest, "everything");
		quilt_canon_set_fragment(dest, NULL);
		deststr = quilt_canon_str(dest, QCO_FRAGMENT);
		st = quilt_st_create_uri(entrystr, NS_RDF "type", NS_ODRL "Group");
		librdf_model_context_add_statement(req->model, graph, st);
		librdf_free_statement(st);
		st = quilt_st_create_literal(entrystr, NS_RDFS "label", "Everyone", "en-gb");
		librdf_model_context_add_statement(req->model, graph, st);
		librdf_free_statement(st);
		st = quilt_st_create_literal(entrystr, NS_RDFS "comment", "Resources which are generally-accessible to the public", "en-gb");
		librdf_model_context_add_statement(req->model, graph, st);
		librdf_free_statement(st);
		st = quilt_st_create_uri(entrystr, NS_RDFS "seeAlso", deststr);
		librdf_model_context_add_statement(req->model, graph, st);
		librdf_free_statement(st);
		st = quilt_st_create_uri(self, NS_RDFS "seeAlso", entrystr);
		librdf_model_context_add_statement(req->model, graph, st);
		librdf_free_statement(st);
		free(entrystr);
		free(deststr);
		quilt_canon_destroy(entry);
		quilt_canon_destroy(dest);
	}
	if(spindle_db)
	{
		if((r = spindle_audiences_db(req, &query)) != 200)
		{
			return r;
		}
	}
	if((r = spindle_add_concrete(req)) != 200)
	{
		return r;
	}
	if((r = spindle_query_meta(req, &query)) != 200)
	{
		return r;
	}
	free(self);
	return 200;
}
