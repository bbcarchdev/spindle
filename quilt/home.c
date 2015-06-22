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
	{ "/people", "People", NS_FOAF "Person" },
	{ "/groups", "Groups", NS_FOAF "Group" },
	{ "/agents", "Agents", NS_FOAF "Agent" },
	{ "/places", "Places", NS_GEO "SpatialThing" },
	{ "/events", "Events", NS_EVENT "Event" },

	{ "/things", "Physical things", NS_CRM "E18_Physical_Thing" },
	{ "/collections", "Collections", NS_DCMITYPE "Collection" },
	{ "/works", "Creative works", NS_FRBR "Work" },
	{ "/assets", "Digital assets", NS_FOAF "Document" },
	{ "/concepts", "Concepts", NS_SKOS "Concept" },
	{ NULL, NULL, NULL }
};

int
spindle_home(QUILTREQ *request)
{
	librdf_statement *st;
	size_t c;
	QUILTCANON *partcanon, *link;
	char *abstract, *partstr, *linkstr;
	int r;
	const char *uri;

	uri = quilt_request_getparam(request, "uri");
	if(uri && uri[0])
	{
		quilt_canon_set_param(request->canonical, "uri", uri);
		return spindle_lookup(request, uri);
	}

	/* Add OpenSearch information to the index */
	abstract = quilt_canon_str(request->canonical, QCO_ABSTRACT);
	link = quilt_canon_create(request->canonical);
	quilt_canon_reset_params(link);
	quilt_canon_add_param(link, "q", "{searchTerms?}");
	quilt_canon_add_param(link, "lang", "{language?}");
	quilt_canon_add_param(link, "limit", "{count?}");
	quilt_canon_add_param(link, "offset", "{startIndex?}");
	quilt_canon_add_param(link, "class", "{rdfs:Class?}");
	quilt_canon_set_ext(link, NULL);						
	linkstr = quilt_canon_str(link, QCO_ABSTRACT);
	st = quilt_st_create_literal(abstract, NS_OSD "template", linkstr, NULL);
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);
	free(linkstr);
	quilt_canon_destroy(link);

	st = quilt_st_create_literal(abstract, NS_OSD "Language", "en-gb", NULL);
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);

	st = quilt_st_create_literal(abstract, NS_OSD "Language", "cy-gb", NULL);
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);

	st = quilt_st_create_literal(abstract, NS_OSD "Language", "gd-gb", NULL);
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);

	st = quilt_st_create_literal(abstract, NS_OSD "Language", "ga-gb", NULL);
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);

	/* Add VoID descriptive metadata */
	abstract = quilt_canon_str(request->canonical, QCO_ABSTRACT);
	link = quilt_canon_create(request->canonical);
	quilt_canon_reset_params(link);
	quilt_canon_add_param(link, "uri", "");
	linkstr = quilt_canon_str(link, QCO_ABSTRACT);
	st = quilt_st_create_uri(abstract, NS_VOID "uriLookupEndpoint", linkstr);
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);	
	free(linkstr);
	quilt_canon_destroy(link);

	link = quilt_canon_create(request->canonical);
	quilt_canon_reset_params(link);
	quilt_canon_set_explicitext(link, NULL);
	quilt_canon_set_ext(link, "osd");
	linkstr = quilt_canon_str(link, QCO_CONCRETE);
	st = quilt_st_create_uri(abstract, NS_VOID "openSearchDescription", linkstr);	
	librdf_model_context_add_statement(request->model, request->basegraph, st);
	librdf_free_statement(st);	
	free(linkstr);
	quilt_canon_destroy(link);
	
	/* Add all of the indices as void:Datasets */
	r = 0;
	partcanon = NULL;
	partstr = NULL;
	for(c = 0; spindle_indices[c].uri; c++)
	{
		partcanon = quilt_canon_create(request->canonical);
		if(!partcanon)
		{
			r = -1;
			break;
		}
		quilt_canon_reset_path(partcanon);
		quilt_canon_set_name(partcanon, NULL);
		quilt_canon_add_path(partcanon, spindle_indices[c].uri);
		partstr = quilt_canon_str(partcanon, QCO_ABSTRACT);
		if(!partstr)
		{
			r = -1;
			break;
		}
		if(spindle_indices[c].qclass)
		{
			st = quilt_st_create_uri(abstract, NS_VOID "classPartition", partstr);
		}
		else
		{
			st = quilt_st_create_uri(abstract, NS_VOID "rootResource", partstr);
		}
		if(!st)
		{
			r = -1;
			break;
		}
		librdf_model_context_add_statement(request->model, request->basegraph, st);
		librdf_free_statement(st);
	
		st = quilt_st_create_literal(partstr, NS_RDFS "label", spindle_indices[c].title, "en");
		if(!st)
		{
			r = -1;
			break;
		}
		librdf_model_context_add_statement(request->model, request->basegraph, st);
		librdf_free_statement(st);

		st = quilt_st_create_uri(partstr, NS_RDF "type", NS_VOID "Dataset");
		if(!st)
		{
			r = -1;
			break;
		}
		librdf_model_context_add_statement(request->model, request->basegraph, st);
		librdf_free_statement(st);

		if(spindle_indices[c].qclass)
		{
			st = quilt_st_create_uri(partstr, NS_VOID "class", spindle_indices[c].qclass);
			if(!st)
			{
				r = -1;
				break;
			}
			librdf_model_context_add_statement(request->model, request->basegraph, st);
			librdf_free_statement(st);
		}
	}
	if(partcanon)
	{
		quilt_canon_destroy(partcanon);
	}
	free(partstr);
	free(abstract);
	if(r)
	{
		return -1;
	}
	spindle_add_concrete(request);
	/* Return 200, rather than 0, to auto-serialise the model */
	return 200;
}
