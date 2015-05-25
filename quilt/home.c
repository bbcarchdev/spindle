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

/* XXX replace with config */
struct index_struct spindle_indices[] = {
	{ "/everything", "Everything", NULL },
	{ "/people", "People", "http://xmlns.com/foaf/0.1/Person" },
	{ "/groups", "Groups", "http://xmlns.com/foaf/0.1/Group" },
	{ "/agents", "Agents", "http://xmlns.com/foaf/0.1/Agent" },
	{ "/places", "Places", "http://www.w3.org/2003/01/geo/wgs84_pos#SpatialThing" },
	{ "/events", "Events", "http://purl.org/NET/c4dm/event.owl#Event" },
	{ "/things", "Physical things", "http://www.cidoc-crm.org/cidoc-crm/E18_Physical_Thing" },
	{ "/collections", "Collections", "http://purl.org/dc/dcmitype/Collection" },
	{ "/works", "Creative works", "http://purl.org/vocab/frbr/core#Work" },
	{ "/assets", "Digital assets", "http://xmlns.com/foaf/0.1/Document" },
	{ "/concepts", "Concepts", "http://www.w3.org/2004/02/skos/core#Concept" },
	{ NULL, NULL, NULL }
};

int
spindle_home(QUILTREQ *request)
{
	librdf_statement *st;
	size_t c;

	const char *uri;

	uri = quilt_request_getparam(request, "uri");
	if(uri && uri[0])
	{
		return spindle_lookup(request, uri);
	}

	/* Add OpenSearch information to the index */
	st = quilt_st_create_literal(request->path, "http://a9.com/-/spec/opensearch/1.1/template", "/?q={searchTerms?}&language={language?}&limit={count?}&offset={startIndex?}&class={rdfs:Class?}", NULL);
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);

	st = quilt_st_create_literal(request->path, "http://a9.com/-/spec/opensearch/1.1/Language", "en-gb", NULL);
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);
	st = quilt_st_create_literal(request->path, "http://a9.com/-/spec/opensearch/1.1/Language", "cy-gb", NULL);
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);
	st = quilt_st_create_literal(request->path, "http://a9.com/-/spec/opensearch/1.1/Language", "gd-gb", NULL);
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);
	st = quilt_st_create_literal(request->path, "http://a9.com/-/spec/opensearch/1.1/Language", "ga-gb", NULL);
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);

	/* Add VoID descriptive metadata */
	st = quilt_st_create_uri(request->path, "http://rdfs.org/ns/void#uriLookupEndpoint", "/?uri=");
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);	

	st = quilt_st_create_uri(request->path, "http://rdfs.org/ns/void#openSearchDescription", "/index.osd");
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);	
	
	/* Add all of the indices as void:Datasets */
	for(c = 0; spindle_indices[c].uri; c++)
	{
		if(spindle_indices[c].qclass)
		{
			st = quilt_st_create_uri(request->path, "http://rdfs.org/ns/void#classPartition", spindle_indices[c].uri);
		}
		else
		{
			st = quilt_st_create_uri(request->path, "http://rdfs.org/ns/void#rootResource", spindle_indices[c].uri);
		}
		if(!st) return -1;
		librdf_model_context_add_statement(request->model, request->basegraph, st);
		librdf_free_statement(st);
	
		st = quilt_st_create_literal(spindle_indices[c].uri, "http://www.w3.org/2000/01/rdf-schema#label", spindle_indices[c].title, "en");
		if(!st) return -1;
		librdf_model_context_add_statement(request->model, request->basegraph, st);
		librdf_free_statement(st);

		st = quilt_st_create_uri(spindle_indices[c].uri, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type", "http://rdfs.org/ns/void#Dataset");
		if(!st) return -1;
		librdf_model_context_add_statement(request->model, request->basegraph, st);
		librdf_free_statement(st);

		if(spindle_indices[c].qclass)
		{
			st = quilt_st_create_uri(spindle_indices[c].uri, "http://rdfs.org/ns/void#class", spindle_indices[c].qclass);
			if(!st) return -1;
			librdf_model_context_add_statement(request->model, request->basegraph, st);
			librdf_free_statement(st);
		}
	}
	/* Return 200, rather than 0, to auto-serialise the model */
	return 200;
}
