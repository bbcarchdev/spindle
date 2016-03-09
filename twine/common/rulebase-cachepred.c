/* Spindle: Co-reference aggregation engine
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2015 BBC
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

static int spindle_rulebase_cachepred_compare_(const void *ptra, const void *ptrb);

int
spindle_rulebase_cachepred_add(SPINDLERULES *rules, const char *uri)
{
	size_t c;
	char **p;

	for(c = 0; c < rules->cpcount; c++)
	{
		if(!strcmp(uri, rules->cachepreds[c]))
		{
			return 0;
		}
	}
	if(rules->cpcount + 1 >= rules->cpsize)
	{
		p = (char **) realloc(rules->cachepreds, sizeof(char *) * (rules->cpsize + 8 + 1));
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to expand cached predicates list\n");
			return -1;
		}
		rules->cachepreds = p;
		memset(&(p[rules->cpcount]), 0, sizeof(char *) * (8 + 1));
		rules->cpsize += 8;
	}
	p = &(rules->cachepreds[rules->cpcount]);
	p[0] = strdup(uri);
	if(!p[0])
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate cached predicate URI\n");
		return -1;
	}
	rules->cpcount++;
	return 0;
}

int
spindle_rulebase_cachepred_finalise(SPINDLERULES *rules)
{
	qsort(rules->cachepreds, rules->cpcount, sizeof(char *), spindle_rulebase_cachepred_compare_);
	return 0;
}

int
spindle_rulebase_cachepred_cleanup(SPINDLERULES *rules)
{
	size_t c;

	for(c = 0; c < rules->cpcount; c++)
	{
		free(rules->cachepreds[c]);
	}
	free(rules->cachepreds);
	rules->cachepreds = NULL;

	return 0;
}

int
spindle_rulebase_cachepred_dump(SPINDLERULES *rules)
{
	size_t c;

	twine_logf(LOG_DEBUG, PLUGIN_NAME ": cached predicates set (%d entries):\n", (int) rules->cpcount);
	for(c = 0; c < rules->cpcount; c++)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": %d: <%s>\n", (int) c, rules->cachepreds[c]);
	}
	return 0;
}

static int
spindle_rulebase_cachepred_compare_(const void *ptra, const void *ptrb)
{
	char **a, **b;
	
	a = (char **) ptra;
	b = (char **) ptrb;
	
	return strcmp(*a, *b);
}
