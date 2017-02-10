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

static int spindle_rulebase_coref_add_(SPINDLERULES *rules, const char *predicate, const struct coref_match_struct *match);

/* Add a spindle:coref statement to the coreference matching ruleset */
int
spindle_rulebase_coref_add_node(SPINDLERULES *rules, const char *predicate, librdf_node *node)
{
	librdf_uri *uri;
	const char *uristr;
	size_t c;

	if(!rules->match_types)
	{
		return 0;
	}
	if(!librdf_node_is_resource(node))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": spindle:coref statement expected a resource object\n");
		return 0;
	}
	uri = librdf_node_get_uri(node);
	uristr = (const char *) librdf_uri_as_string(uri);
	for(c = 0; rules->match_types[c].predicate; c++)
	{
		if(!strcmp(uristr, rules->match_types[c].predicate))
		{
			spindle_rulebase_cachepred_add(rules, predicate);
			return spindle_rulebase_coref_add_(rules, predicate, &(rules->match_types[c]));
		}
	}
	twine_logf(LOG_ERR, PLUGIN_NAME ": co-reference match type <%s> is not supported\n", uristr);
	return 0;
}

static int
spindle_rulebase_coref_add_(SPINDLERULES *rules, const char *predicate, const struct coref_match_struct *match)
{
	struct coref_match_struct *p;
	size_t c;

	for(c = 0; c < rules->corefcount; c++)
	{
		if(!strcmp(predicate, rules->coref[c].predicate))
		{
			rules->coref[c].callback = match->callback;
			return 0;
		}
	}
	if(rules->corefcount + 1 > rules->corefsize)
	{
		p = (struct coref_match_struct *) realloc(rules->coref, sizeof(struct coref_match_struct) * (rules->corefsize + 4 + 1));
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to resize co-reference match type list\n");
			return -1;
		}
		rules->coref = p;
		rules->corefsize += 4;
	}
	p = &(rules->coref[rules->corefcount]);
	p->predicate = strdup(predicate);
	if(!p->predicate)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate co-reference predicate URI\n");
		return -1;
	}
	p->callback = match->callback;
	rules->corefcount++;
	return 1;
}

int
spindle_rulebase_coref_dump(SPINDLERULES *rules)
{
	size_t c;

	if(rules->match_types)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": match types:\n");
		for(c = 0; rules->match_types[c].predicate; c++)
		{
			twine_logf(LOG_DEBUG, PLUGIN_NAME ": %d: <%s>\n", (int) c, rules->match_types[c]);
		}
	}
	return 0;
}
