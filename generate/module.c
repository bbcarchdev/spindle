/* Spindle: Co-reference aggregation engine
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2017 BBC
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

static int spindle_generate_init_(SPINDLEGENERATE *generate);
static int spindle_generate_cleanup_(SPINDLEGENERATE *generate);

static SPINDLE spindle;
static SPINDLEGENERATE generate;

/* Twine plug-in entry-point */
int
twine_plugin_init(void)
{	
	twine_logf(LOG_DEBUG, PLUGIN_NAME " plug-in: initialising\n");
	if(spindle_generate_init_(&generate))
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": initialisation failed\n");
		return -1;
	}
	twine_logf(LOG_INFO, PLUGIN_NAME ": URI prefix is <%s>\n", spindle.root);
	twine_postproc_register(PLUGIN_NAME, spindle_generate_graph, &generate);
	twine_graph_register(PLUGIN_NAME, spindle_generate_graph, &generate);
	twine_update_register("spindle", spindle_generate_update, &generate);
	twine_plugin_register(SPINDLE_URI_MIME, "Spindle proxy URI", spindle_generate_message, &generate);
	spindle_mq_init(NULL);
	return 0;
}

int
twine_plugin_done(void)
{
	twine_logf(LOG_DEBUG, PLUGIN_NAME " plug-in: cleaning up\n");
	spindle_generate_cleanup_(&generate);
	return 0;
}

static int
spindle_generate_init_(SPINDLEGENERATE *generate)
{
	memset(generate, 0, sizeof(SPINDLEGENERATE));
	if(spindle_init(&spindle))
	{
		return -1;
	}
	generate->spindle = &spindle;
	spindle.rules = spindle_rulebase_create(NULL, NULL);
	if(!spindle.rules)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to load rule-base\n");
		return -1;
	}
	generate->rules = spindle.rules;
	if(spindle_cache_init(generate))
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to initialise S3 bucket\n");
		return -1;
	}
	if(spindle_doc_init(generate))
	{
		return -1;
	}
	if(spindle_license_init(generate))
	{
		return -1;
	}
	if(spindle_db_init(&spindle))
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to connect to database\n");
		return -1;
	}	
	generate->db = spindle.db;
	generate->sparql = spindle.sparql;
	if(twine_config_get_bool(PLUGIN_NAME ":dumprules", twine_config_get_bool("spindle:dumprules", 0)))
	{
		spindle_rulebase_dump(generate->rules);
	}
	generate->aboutself = twine_config_get_bool(PLUGIN_NAME ":about-self", twine_config_get_bool("spindle:about-self", 0));
	if(generate->aboutself)
	{
		twine_logf(LOG_INFO, PLUGIN_NAME ": creative works will be 'about' themselves\n");
	}
	generate->describedby = twine_config_get_bool(PLUGIN_NAME ":describedby", twine_config_get_bool("spindle:describedby", 1));
	return 0;
}

static int
spindle_generate_cleanup_(SPINDLEGENERATE *generate)
{
	if(generate->spindle)
	{
		spindle_cleanup(generate->spindle);
	}
	free(generate->titlepred);
	return 0;
}
