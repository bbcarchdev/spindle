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

#include "p_spindle-correlate.h"

static SPINDLE spindle;

static struct coref_match_struct coref_match_types[] = 
{
	{ NS_SPINDLE "resourceMatch", spindle_match_sameas },
	{ NS_SPINDLE "wikipediaMatch", spindle_match_wikipedia },
	{ NULL, NULL }
};

/* Twine plug-in entry-point */
int
twine_plugin_init(void)
{	
	twine_logf(LOG_DEBUG, PLUGIN_NAME " plug-in: initialising\n");
	if(spindle_init(&spindle))
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": initialisation failed\n");
		return -1;
	}
	spindle.rules = spindle_rulebase_create(NULL, coref_match_types);
	if(!spindle.rules)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to load rule-base\n");
		return -1;
	}
	if(spindle_db_init(&spindle))
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to connect to database\n");
		return -1;
	}
	twine_logf(LOG_INFO, PLUGIN_NAME ": URI prefix is <%s>\n", spindle.root);
	twine_postproc_register("spindle", spindle_correlate, &spindle);
	twine_graph_register(PLUGIN_NAME, spindle_correlate, &spindle);
	return 0;
}

int
twine_plugin_done(void)
{
	twine_logf(LOG_DEBUG, PLUGIN_NAME " plug-in: cleaning up\n");
	spindle_cleanup(&spindle);
	return 0;
}

/* Discard cached information about a graph */
int
spindle_graph_discard(SPINDLE *spindle, const char *uri)
{
	/* This is a no-op in the stand-alone post-processor; it can be
	 * removed when we no longer have a monolithic module.
	 */

	(void) spindle;
	(void) uri;

	return 0;
}

/* Ask the cache module to update a set of entries */
int
spindle_cache_update_set(SPINDLE *spindle, struct spindle_strset_struct *set)
{
	(void) spindle;
	(void) set;
	
	return 0;
}


