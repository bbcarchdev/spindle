/* Spindle: Co-reference aggregation engine
 *
 * Copyright (c) 2018 BBC
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

/* Add all of the rules from the specified file(s) */
static int spindle_rulebase_add_config_(RULEBASE *rules);

/* Add all of the rules from the default file(s) */
RULEBASE *rules
spindle_rulebase_create(void)
{
	RULEBASE *rules

	rules = rulebase_create(spindle.world, spindle.root, spindle.multigraph);
	if(!rules)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create rulebase object\n");
		return NULL;
	}
/*
	if(spindle_rulebase_add_config_(rules))
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to initialise rulebase\n");
		rulebase_destroy(rules);
		return NULL;
	}
*/
	return rules;
}

/* Add all of the rules from the default file(s) */
int
spindle_rulebase_add_config(RULEBASE *rules, const char *path)
{
	return rulebase_add_file(rules, path);
}

/* Add all of the rules from the default file(s) */
static int
spindle_rulebase_add_config_(RULEBASE *rules)
{
	const char *path;

	path = twine_config_geta("spindle:rulebase", rulebase_default_config_path());
	return rulebase_add_file(rules, path);
}
