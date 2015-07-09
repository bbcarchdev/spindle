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
	r = spindle_item_related(request);
	if(r != 200)
	{
		return r;
	}
	if(spindle_add_concrete(request))
	{
		return 500;
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

	memset(&query, 0, sizeof(struct query_struct));
	query.related = request->subject;
	return spindle_query(request, &query);
}
