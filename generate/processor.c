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

static int spindle_generate_all_(SPINDLEGENERATE *generate);

/* TODO: don't use SF_xxx flags here - use TK_xxx flags instead, as they're
 * used in the state and triggers tables
 */

/* Update handler: re-build the cached contents of the item with the supplied
 * identifier (which may be a UUID or a complete URI)
 *
 * When invoked using twine -u SPINDLE <ID>, and using an RDBMS index, the
 * special value 'all' is valid to trigger a re-build of all known proxies.
 */
int
spindle_generate_update(const char *name, const char *identifier, void *data)
{
	SPINDLEGENERATE *generate;
	int r;

	(void) name;

	generate = (SPINDLEGENERATE *) data;

	if(!strcasecmp(identifier, "all"))
	{
		if(generate->spindle->db)
		{
			return spindle_generate_all_(generate);
		}
		twine_logf(LOG_CRIT, PLUGIN_NAME ": can only update all items when using the a relational database index\n");
		return -1;
	}
	return spindle_generate(generate, identifier, SF_NONE);
}

/* Graph processing hook, invoked by Twine operations
 *
 * This hook is invoked once (preprocessed) RDF has been pushed into the
 * quad-store and is responsible for:
 *
 * - correlating references against each other and generating proxy URIs
 * - caching and transforming source data and storing it with the proxies
 */
int
spindle_generate_graph(twine_graph *graph, void *data)
{
	SPINDLEGENERATE *generate;

	generate = (SPINDLEGENERATE *) data;
	return spindle_generate(generate, graph->uri, SF_NONE);
}

/* Process a message containing Spindle proxy URIs by passing them to the
 * update handler.
 */
int
spindle_generate_message(const char *mime, const unsigned char *buf, size_t buflen, void *data)
{
	SPINDLEGENERATE *generate;
	char *str, *t;
	int r, mode;

	(void) mime;

	generate = (SPINDLEGENERATE *) data;
	mode = SF_NONE;
	/* Impose a hard limit on URL lengths */
	if(buflen > 1024)
	{
		buflen = 1024;
	}
	str = (char *) calloc(1, buflen + 1);
	if(!str)
	{
		return -1;
	}
	memcpy((void *) str, (void *) buf, buflen);
	str[buflen] = 0;
	t = strchr(str, '\n');
	if(t)
	{
		*t = 0;
	}
	t = strchr(str, ' ');
	if(t)
	{
		*t = 0;
		t++;
		if(!strcmp(t, "moved"))
		{
			mode = SF_MOVED;
		}
		else if(!strcmp(t, "updated"))
		{
			mode = SF_UPDATED;
		}
		else if(!strcmp(t, "refreshed"))
		{
			mode = SF_REFRESHED;
		}
		else
		{
			twine_logf(LOG_WARNING, PLUGIN_NAME ": update-mode flag '%s' for <%s> is not recognised\n", t, str);
		}
	}
	r = spindle_generate(generate, str, mode);
	free(str);
	return r;
}

/* Generate data for all known entities */
static int
spindle_generate_all_(SPINDLEGENERATE *generate)
{
	SQL_STATEMENT *rs;
	int r;
	const char *t;

	rs = sql_query(generate->db, "SELECT \"id\" FROM \"proxy\"");
	if(!rs)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to query for item UUIDs\n");
		return -1;
	}
	r = 0;
	for(; !sql_stmt_eof(rs); sql_stmt_next(rs))
	{
		t = sql_stmt_str(rs, 0);
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": will update {%s}\n", t);
		if(!t)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain UUID from database column\n");
			r = -1;
			break;
		}
		r = spindle_generate(generate, t, SF_NONE);
		if(r)
		{
			break;
		}
	}
	sql_stmt_destroy(rs);
	return r;
}
