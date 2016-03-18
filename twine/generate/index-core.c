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

/*
 * The 'index' table has a core set of fields:-
 *
 *  id       The proxy's UUID
 *  version  The index version (SPINDLE_DB_INDEX_VERSION)
 *  modified The last-modification date
 *  classes  A list of class URIs for this proxy
 *  score    The calculated prominence score for this proxy
 *
 * In addition, the index stores:
 *
 *  title        (v1) An hstore of language/title pairs
 *  description  (v1) An hstore of language/description pairs
 *  coordinates  (v1) A point specifying latitude/longitude
 *  index_en_gb  (v1) The en-gb (British English) full-text index
 *  index_cy_gb  (v1) The cy-gb (Welsh) full-text index
 *  index_gd_gb  (v1) The gd-gb (Scottish Gaelic) full-text index
 *  index_ga_gb  (v1) The ga-gb (Irish Gaelic) full-text index
 *
 */

static int spindle_index_updatelang_(SQL *sql, const char *id, const char *target, const char *specific, const char *generic);


/* Populate the core index entry for an object. This stores:
 * - Title
 * - Description
 * - Class URIs
 * - Prominence score
 * - Coordinates (for places)
 */
int
spindle_index_core(SQL *sql, const char *id, SPINDLEENTRY *data)
{
	const char *t;
	char *title, *desc, *classes;
	char lbuf[64];
	int r;

	title = spindle_db_literalset(&(data->titleset));
	desc = spindle_db_literalset(&(data->descset));
	classes = spindle_db_strset(data->classes);
	if(data->has_geo)
	{
		snprintf(lbuf, sizeof(lbuf), "(%f, %f)", data->lat, data->lon);
		t = lbuf;
	}
	else
	{
		t = NULL;
	}
	r = sql_executef(sql, "INSERT INTO \"index\" (\"id\", \"version\", \"modified\", \"score\", \"title\", \"description\", \"coordinates\", \"classes\") VALUES (%Q, %d, now(), %d, %Q, %Q, %Q, %Q)",
					 id, SPINDLE_DB_INDEX_VERSION, data->score, title, desc, t, classes);
	
	free(title);
	free(desc);
	free(classes);
	if(r)
	{
		return -1;
	}
	if(spindle_index_updatelang_(sql, id, "en_gb", "en-gb", "en"))
	{
		return -1;
	}
	if(spindle_index_updatelang_(sql, id, "cy_gb", "cy-gb", "cy"))
	{
		return -1;
	}
	if(spindle_index_updatelang_(sql, id, "ga_gb", "ga-gb", "ga"))
	{
		return -1;
	}
	if(spindle_index_updatelang_(sql, id, "gd_gb", "gd-gb", "gd"))
	{
		return -1;
	}
	return 0;
}

/* Update the language-specific index "index_<target>" using (in order of
 * preference), <specific>, <generic>, (none).
 * e.g., target="en_gb", specific="en-gb", generic="en"
 */
static int
spindle_index_updatelang_(SQL *sql, const char *id, const char *target, const char *specific, const char *generic)
{
	if(specific)
	{
		if(sql_executef(sql, "UPDATE \"index\" SET "
						"\"index_%s\" = setweight(to_tsvector(coalesce(\"title\" -> '%s', \"title\" -> '%s', \"title\" -> '_', '')), 'A') || "
						" setweight(to_tsvector(coalesce(\"description\" -> '%s', \"description\" -> '%s', \"description\" -> '_', '')), 'B')  "
						"WHERE \"id\" = %Q", target, specific, generic, specific, generic, id))
		{
			return -1;
		}
		return 0;
	}
	if(sql_executef(sql, "UPDATE \"index\" SET "
					"\"index_%s\" = setweight(to_tsvector(coalesce(\"title\" -> '%s', \"title\" -> '_', '')), 'A') || "
					" setweight(to_tsvector(coalesce(\"description\" -> '%s', \"description\" -> '_', '')), 'B')  "
					"WHERE \"id\" = %Q", target, generic, generic, id))
	{
		return -1;
	}
	return 0;
}
