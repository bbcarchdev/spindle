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

static int spindle_item_post_(QUILTREQ *request);

/* Given an item's URI, attempt to redirect to it */
int
spindle_lookup(QUILTREQ *request, const char *target)
{
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
	else
	{
		r = spindle_item_sparql(request);
	}
	if(r != 200)
	{
		return r;
	}
	return spindle_item_post_(request);
}

/* Fetch additional information about an item */
static int
spindle_item_post_(QUILTREQ *request)
{
	SPARQL *sparql;
	
	sparql = quilt_sparql();
	
	if(sparql_queryf_model(sparql, request->model, "SELECT DISTINCT ?s ?p ?o ?g WHERE {\n"
						   "  GRAPH ?g {\n"
						   "    ?s <" NS_FOAF "topic> <%s#id>\n"
						   "  }"
						   "  GRAPH ?g {\n"
						   "    ?s ?p ?o\n"
						   "  }\n"
						   "  FILTER regex(str(?g), \"^%s\", \"i\")\n"
						   "}",
						   request->subject, request->base))
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to retrieve related items\n");
		return 500;
	}

	spindle_add_concrete(request);
	/* Return 200, rather than 0, to auto-serialise the model */
	return 200;
}

