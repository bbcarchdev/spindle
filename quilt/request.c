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
	else if(spindle_bucket)
	{
		r = spindle_item_s3(request);
	}
	else
	{
		r = spindle_item(request);
	}
	free(qclass);
	return r;
}

