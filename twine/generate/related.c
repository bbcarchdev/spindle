/* Spindle: Co-reference aggregation engine
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

#include "p_spindle-generate.h"

int
spindle_related_fetch_entry(SPINDLEENTRY *data)
{
	librdf_iterator *iterator;
	librdf_node *context;
	librdf_uri *uri;
	const char *uristr;
	int r;
	
	/* If there's no cache destination, the extradata model won't be used, so
	 * there's nothing to do here
	 */
	if(!data->generate->bucket && !data->generate->cachepath)
	{
		return 0;
	}
	if((data->flags & TK_PROXY) || (data->flags & TK_MEDIA))
	{
		r = 0;
	}
	else
	{
		r = spindle_cache_fetch(data, "related", data->extradata);
	}
	if(r < 0)
	{
		return -1;
	}
	if(r == 0)
	{
		/* Cache information about external resources related to this
		 * entity, restricted by the predicate used for the relation
		 */
		if(twine_cache_fetch_media_(data->extradata, data->graph, data->self))
		{
			return -1;
		}
		spindle_cache_store(data, "related", data->extradata);
	}
	/* Remove anything in a local graph */
	iterator = librdf_model_get_contexts(data->extradata);
	while(!librdf_iterator_end(iterator))
	{
		context = librdf_iterator_get_object(iterator);
		uri = librdf_node_get_uri(context);
		uristr = (const char *) librdf_uri_as_string(uri);
		if(!strncmp(uristr, data->spindle->root, strlen(data->spindle->root)))
		{
			librdf_model_context_remove_statements(data->extradata, context);
		}
		librdf_iterator_next(iterator);
	}
	librdf_free_iterator(iterator);
	return 0;
}
