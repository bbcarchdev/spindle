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
spindle_index(QUILTREQ *request, const char *qclass)
{
	struct query_struct query;
	int r;

	memset(&query, 0, sizeof(query));	
	r = spindle_query_request(&query, request, qclass);
	if(r != 200)
	{
		return r;
	}
	if(!request->indextitle)
	{
		request->indextitle = "Everything";
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
	r = spindle_add_concrete(request);
	if(r != 200)
	{
		return r;
	}
	return 200;
}
