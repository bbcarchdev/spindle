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

static int spindle_item_is_collection_(QUILTREQ *req);

/* Given an item's URI, attempt to redirect to it */
int
spindle_lookup(QUILTREQ *request, const char *target)
{
	quilt_canon_set_param(request->canonical, "uri", target);
	if(spindle_db)
	{
		return spindle_lookup_db(request, target);
	}
	return spindle_lookup_sparql(request, target);
}

/* Fetch an item */
int
spindle_item(QUILTREQ *request)
{
	int r;

	if(spindle_bucket)
	{
		r = spindle_item_s3(request);
	}
	else if(spindle_cachepath)
	{
		r = spindle_item_file(request);
	}
	else
	{
		r = spindle_item_sparql(request);
	}
	if(r != 200)
	{
		return r;
	}
	r = spindle_membership(request);
	if(r != 200)
	{
		return r;
	}
	r = spindle_item_related(request);
	if(r != 200)
	{
		return r;
	}
	r = spindle_add_concrete(request);
	if(r != 200)
	{
		return r;
	}
	/* Return 200 to auto-serialise */
	return 200;
}

/* Fetch additional metdata about an item
 * (invoked automatically by spindle_item())
 */
int
spindle_item_related(QUILTREQ *request)
{
	struct query_struct query;
	int r;

	spindle_query_init(&query);
	if(spindle_item_is_collection_(request))
	{
		query.collection = request->subject;
		r = spindle_query_request(&query, request, NULL);
		if(r != 200)
		{
			return r;
		}
		r = spindle_query(request, &query);
		if(r != 200)
		{
			return r;
		}
		r = spindle_query_meta(request, &query);
		if(r != 200)
		{
			return r;
		}
		r = spindle_query_osd(request);
		if(r != 200)
		{
			return r;
		}
		return 200;
	}
	query.related = request->subject;
	r = spindle_query(request, &query);
	if(r != 200)
	{
		return r;
	}
	return 200;
}

static int
spindle_item_is_collection_(QUILTREQ *req)
{
	librdf_statement *query;
	librdf_stream *stream;
	int r;
	char *uri;
	
	uri = quilt_canon_str(req->canonical, QCO_SUBJECT);	
	/* Look for <subject> a dmcitype:Collection */
	query = quilt_st_create_uri(uri, NS_RDF "type", NS_DCMITYPE "Collection");
	free(uri);
	stream = librdf_model_find_statements(req->model, query);
	if(!stream)
	{
		librdf_free_statement(query);
		return 0;
	}
	r = librdf_stream_end(stream) ? 0 : 1;
	librdf_free_stream(stream);
	librdf_free_statement(query);
	return r;
}
