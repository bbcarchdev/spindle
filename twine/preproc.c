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

#include "p_spindle.h"

static unsigned long long
gettimems(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

static int
gettimediffms(unsigned long long *start)
{
	struct timeval tv;
	int r;

	gettimeofday(&tv, NULL);
	r = (int) (((tv.tv_sec * 1000) + (tv.tv_usec / 1000)) - *start);
	*start = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
	return r;
}

int
spindle_preproc(twine_graph *graph, void *data)
{
	librdf_stream *st;
	librdf_statement *statement;
	librdf_node *predicate;
	librdf_uri *pred;
	const char *preduri;
	int match, r;
	size_t c;
	SPINDLE *spindle;

	unsigned long long start;

	start = gettimems();

	spindle = (SPINDLE *) data;
	st = librdf_model_as_stream(graph->store);
	while(!librdf_stream_end(st))
	{
		match = 0;
		statement = librdf_stream_get_object(st);
		predicate = librdf_statement_get_predicate(statement);
		if(librdf_node_is_resource(predicate) &&
		   (pred = librdf_node_get_uri(predicate)) &&
		   (preduri = (const char *) librdf_uri_as_string(pred)))
		{
			for(c = 0; c < spindle->cpcount; c++)
			{
				r = strcmp(spindle->cachepreds[c], preduri);
				if(!r)
				{
					match = 1;
					break;
				}
				if(r > 0)
				{
					/* The cachepreds list is lexigraphically sorted */
					break;
				}				
			}
			if(!match)
			{
				twine_logf(LOG_DEBUG, PLUGIN_NAME ": preprocessor: removing triple with predicate <%s>\n", preduri);
				librdf_model_remove_statement(graph->store, statement);
			}
		}
		librdf_stream_next(st);
	}
	librdf_free_stream(st);

	twine_logf(LOG_DEBUG, PLUGIN_NAME ": [%dms] pre-processing graph \n", gettimediffms(&start));

	return 0;
}
