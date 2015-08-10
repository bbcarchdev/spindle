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

int
spindle_process(QUILTREQ *request)
{
	char *qclass;
	const char *t;
	size_t c;
	int r;
		
	qclass = NULL;
	t = quilt_request_getparam(request, "q");
	if(t && t[0])
	{
		quilt_canon_set_param(request->canonical, "q", t);
		if(!request->indextitle)
		{
			request->indextitle = t;
		}
		request->index = 1;
		request->home = 0;
	}
	t = quilt_request_getparam(request, "class");
	if(t && t[0])
	{
		quilt_canon_set_param(request->canonical, "class", t);
		qclass = (char *) calloc(1, 32 + strlen(t));
		if(spindle_db)
		{
			strcpy(qclass, t);
		}
		else
		{
			sprintf(qclass, "FILTER ( ?class = <%s> )", t);
		}
		if(!request->indextitle)
		{
			request->indextitle = t;
		}
		request->index = 1;
		request->home = 0;
	}
	else
	{
		for(c = 0; spindle_indices[c].uri; c++)
		{
			if(!strcmp(request->path, spindle_indices[c].uri))
			{
				if(spindle_indices[c].qclass)
				{
					qclass = (char *) calloc(1, 32 + strlen(spindle_indices[c].qclass));
					if(spindle_db)
					{
						strcpy(qclass, spindle_indices[c].qclass);
					}
					else
					{
						sprintf(qclass, "FILTER ( ?class = <%s> )", spindle_indices[c].qclass);
					}
				}
				request->indextitle = spindle_indices[c].title;
				request->index = 1;
				quilt_canon_add_path(request->canonical, spindle_indices[c].uri);			   
				break;
			}
		}
	}
	if(request->home &&
	   (quilt_request_getparam(request, "media") ||
		quilt_request_getparam(request, "type") ||
		quilt_request_getparam(request, "for")))
	{
		request->index = 1;
		request->home = 0;
		request->indextitle = "Everything";
		for(c = 0; spindle_indices[c].uri; c++)
		{
			if(!spindle_indices[c].qclass)
			{
				request->indextitle = spindle_indices[c].title;
				break;
			}
		}
	}
	if(request->home)
	{
		r = spindle_home(request);
	}
	else if(request->index)
	{
		r = spindle_index(request, qclass);
	}
	else
	{
		r = spindle_item(request);
	}
	free(qclass);
	return r;
}

int
spindle_add_concrete(QUILTREQ *request)
{
	const char *s;
	char *abstract, *concrete, *typebuf;
	librdf_statement *st;

	abstract = quilt_canon_str(request->canonical, QCO_ABSTRACT);
	concrete = quilt_canon_str(request->canonical, QCO_CONCRETE);
	
	st = quilt_st_create_uri(abstract, NS_DCTERMS "hasFormat", concrete);
	librdf_model_add_statement(request->model, st);
	librdf_free_statement(st);

	/* concrete rdf:type ... */
	st = quilt_st_create_uri(concrete, NS_RDF "type", NS_DCMITYPE "Text");
	librdf_model_add_statement(request->model, st);
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
		librdf_model_add_statement(request->model, st);
		librdf_free_statement(st);
	}

	typebuf = (char *) malloc(strlen(NS_MIME) + strlen(request->type) + 1);
	if(typebuf)
	{
		strcpy(typebuf, NS_MIME);
		strcat(typebuf, request->type);
		st = quilt_st_create_uri(concrete, NS_DCTERMS "format", typebuf);
		librdf_model_add_statement(request->model, st);
		librdf_free_statement(st);
		free(typebuf);
	}

	free(abstract);
	free(concrete);
	return 0;
}
