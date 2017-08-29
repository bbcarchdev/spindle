/* This engine processes requests for coreference graphs populated
 * by Twine's "spindle" post-processing module.
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2016 BBC
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
	librdf_node *graph;
	size_t c;
	QUILTCANON *partcanon;
	char *abstract, *partstr;
	int r;

	graph = quilt_request_graph(request);
	r = spindle_query_osd(request);
	if(r != 200)
	{
		return 200;
	}

	/* Add all of the indices as void:Datasets */
	r = 0;
	abstract = quilt_canon_str(request->canonical, (request->ext ? QCO_ABSTRACT : QCO_REQUEST));
	/* Add data describing the root dataset itself */
	st = quilt_st_create_literal(abstract, NS_RDFS "label", "Research & Education Space", "en");
	librdf_model_context_add_statement(request->model, graph, st);
	librdf_free_statement(st);
	
	/* Add class partitions */
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
		librdf_model_context_add_statement(request->model, graph, st);
		librdf_free_statement(st);
	
		st = quilt_st_create_literal(partstr, NS_RDFS "label", spindle_indices[c].title, "en");
		if(!st)
		{
			r = -1;
			break;
		}
		librdf_model_context_add_statement(request->model, graph, st);
		librdf_free_statement(st);

		st = quilt_st_create_uri(partstr, NS_RDF "type", NS_VOID "Dataset");
		if(!st)
		{
			r = -1;
			break;
		}
		librdf_model_context_add_statement(request->model, graph, st);
		librdf_free_statement(st);

		if(spindle_indices[c].qclass)
		{
			st = quilt_st_create_uri(partstr, NS_VOID "class", spindle_indices[c].qclass);
			if(!st)
			{
				r = -1;
				break;
			}
			librdf_model_context_add_statement(request->model, graph, st);
			librdf_free_statement(st);
		}
	}
	if(partcanon)
	{
		quilt_canon_destroy(partcanon);
	}
	free(partstr);
	if(r)
	{
		free(abstract);		
		return -1;
	}
	/* Add entries for dynamic endpoints */
	partcanon = quilt_canon_create(request->canonical);
	quilt_canon_reset_path(partcanon);
	quilt_canon_reset_params(partcanon);
	quilt_canon_set_name(partcanon, NULL);
	quilt_canon_add_path(partcanon, "audiences");
	partstr = quilt_canon_str(partcanon, QCO_ABSTRACT);
	st = quilt_st_create_uri(abstract, NS_RDFS "seeAlso", partstr);
	librdf_model_context_add_statement(request->model, graph, st);
	librdf_free_statement(st);	
	st = quilt_st_create_uri(partstr, NS_RDF "type", NS_VOID "Dataset");
	librdf_model_context_add_statement(request->model, graph, st);
	librdf_free_statement(st);
	st = quilt_st_create_literal(partstr, NS_RDFS "label", "Audiences", "en");
	librdf_model_context_add_statement(request->model, graph, st);
	librdf_free_statement(st);	
	free(partstr);
	quilt_canon_destroy(partcanon);	
	free(abstract);

	r = spindle_add_concrete(request);
	if(r != 200)
	{
		return r;
	}
	/* Return 200, rather than 0, to auto-serialise the model */
	return 200;
}
