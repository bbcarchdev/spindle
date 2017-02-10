/* Spindle: Co-reference aggregation engine
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2015-2017 BBC
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

static int spindle_rulebase_coref_add_(SPINDLERULES *rules, const char *candidate, const struct coref_match_struct *match);

/* Add a spindle:coref statement to the coreference matching ruleset
 *
 * Rulebase triples have the form:
 *
 *  <candidate> spindle:coref <match-type> .
 *
 * "candidate" is the URI of a predicate which will appear in source
 * data and should be used to trigger matching.
 *
 * "match-type" is one of the spindle matching triples indicating what
 * sort of matching should occur - 
 *
 */
int
spindle_rulebase_coref_add_node(SPINDLERULES *rules, const char *candidate, librdf_node *matchnode)
{
	librdf_uri *matchuri;
	const char *matchtype;
	size_t c;

	/* If there are no match types defined, then there's no list to add
	 * any candidates to.
	 */
	if(!rules->match_types)
	{
		return 0;
	}
	if(!librdf_node_is_resource(matchnode))
	{
		/* If the match-type node isn't a resource, we can't do anything
		 * useful with it.
		 */
		twine_logf(LOG_ERR, PLUGIN_NAME ": spindle:coref statement expected a resource object\n");
		return 0;
	}
	matchuri = librdf_node_get_uri(matchnode);
	matchtype = (const char *) librdf_uri_as_string(matchuri);
	/* Compare the matchtype URI string against the list of
	 * match types; when we find a match, mark the candidate
	 * as being cacheable and add it to the list of predicates
	 * that trigger that matching rule.
	 */
	for(c = 0; rules->match_types[c].predicate; c++)
	{
		if(!strcmp(matchtype, rules->match_types[c].predicate))
		{
			spindle_rulebase_cachepred_add(rules, candidate);
			return spindle_rulebase_coref_add_(rules, candidate, &(rules->match_types[c]));
		}
	}
	/* The match type wasn't one we know about */
	twine_logf(LOG_ERR, PLUGIN_NAME ": co-reference match type <%s> is not supported\n", matchtype);
	return 0;
}

static int
spindle_rulebase_coref_add_(SPINDLERULES *rules, const char *candidate, const struct coref_match_struct *match)
{
	struct coref_match_struct *p;
	size_t c;

	for(c = 0; c < rules->corefcount; c++)
	{
		if(!strcmp(candidate, rules->coref[c].predicate))
		{
			/* If the candidate URI is already in the list, just update
			 * the callback to the chosen matching function.
			 */
			rules->coref[c].callback = match->callback;
			return 0;
		}
	}
	/* If adding a new URI to the list would overflow, expand it first */
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
	/* Add a new entry at the end of the coref matching rules list */
	p = &(rules->coref[rules->corefcount]);
	p->predicate = strdup(candidate);
	if(!p->predicate)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate co-reference candidate URI\n");
		return -1;
	}
	p->callback = match->callback;
	rules->corefcount++;
	return 1;
}

