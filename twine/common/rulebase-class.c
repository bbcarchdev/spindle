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

static int spindle_rulebase_class_compare_(const void *ptra, const void *ptrb);
static struct spindle_classmap_struct *spindle_rulebase_class_add_(SPINDLERULES *rules, const char *uri);
static int spindle_rulebase_class_add_match_(struct spindle_classmap_struct *match, const char *uri, int prominence);
static int spindle_rulebase_class_set_score_(struct spindle_classmap_struct *entry, librdf_statement *statement);
static int spindle_rulebase_class_dump_(SPINDLERULES *rules);

/* Given an instance of a spindle:Class, add it to the rulebase */
int
spindle_rulebase_class_add_node(SPINDLERULES *rules, librdf_model *model, const char *uri, librdf_node *node)
{
	librdf_world *world;
	librdf_statement *query, *statement;
	librdf_node *s, *predicate;
	librdf_uri *pred;
	librdf_stream *stream;
	const char *preduri;
	int r;
	struct spindle_classmap_struct *classentry;
	
	world = twine_rdf_world();
	if(!(classentry = spindle_rulebase_class_add_(rules, uri)))
	{
		return -1;
	}
	/* Process the properties of the instance */
	s = librdf_new_node_from_node(node);
	query = librdf_new_statement_from_nodes(world, s, NULL, NULL);
	stream = librdf_model_find_statements(model, query);
	r = 0;
	while(!librdf_stream_end(stream))
	{
		statement = librdf_stream_get_object(stream);
		predicate = librdf_statement_get_predicate(statement);
		pred = librdf_node_get_uri(predicate);
		preduri = (const char *) librdf_uri_as_string(pred);
		if(!strcmp(preduri, NS_OLO "index"))
		{
			if((r = spindle_rulebase_class_set_score_(classentry, statement)))
			{
				break;
			}
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	if(r)
	{
		return -1;
	}
	return 1;
}

/* ex:Class spindle:expressedAs ex:OtherClass .
 * ex:OtherClass spindle:prominence 12 .
 */
int
spindle_rulebase_class_add_matchnode(SPINDLERULES *rules, librdf_model *model, const char *matchuri, librdf_node *node)
{
	int prominence;
	long i;
	librdf_uri *obj;
	const char *objuri;
	struct spindle_classmap_struct *classentry;
	librdf_statement *query, *subst;
	librdf_stream *st;

	prominence = 0;
	obj = librdf_node_get_uri(node);
	objuri = (const char *) librdf_uri_as_string(obj);
	if(!(classentry = spindle_rulebase_class_add_(rules, objuri)))
	{
		return -1;
	}
	query = twine_rdf_st_create();
	librdf_statement_set_subject(query, twine_rdf_node_createuri(matchuri));
	librdf_statement_set_predicate(query, twine_rdf_node_createuri(NS_SPINDLE "prominence"));
	st = librdf_model_find_statements(model, query);
	while(!librdf_stream_end(st))
	{
		subst = librdf_stream_get_object(st);
		if(twine_rdf_st_obj_intval(subst, &i) > 0)
		{
			prominence = (int) i;
			break;
		}
		librdf_stream_next(st);
	}
	librdf_free_stream(st);
	librdf_free_statement(query);
	if(spindle_rulebase_class_add_match_(classentry, matchuri, prominence) < 0)
	{
		return -1;
	}
	return 1;	
}


int
spindle_rulebase_class_finalise(SPINDLERULES *rules)
{
	qsort(rules->classes, rules->classcount, sizeof(struct spindle_classmap_struct), spindle_rulebase_class_compare_); 
	spindle_rulebase_class_dump_(rules);
	return 0;
}

int
spindle_rulebase_class_cleanup(SPINDLERULES *rules)
{
	size_t c, d;
	
	for(c = 0; c < rules->classcount; c++)
	{
		free(rules->classes[c].uri);
		for(d = 0; d < rules->classes[c].matchcount; d++)
		{
			free(rules->classes[c].match[d].uri);
		}
		free(rules->classes[c].match);
	}
	free(rules->classes);
	rules->classes = NULL;
	return 0;
}

static int
spindle_rulebase_class_compare_(const void *ptra, const void *ptrb)
{
	struct spindle_classmap_struct *a, *b;
	
	a = (struct spindle_classmap_struct *) ptra;
	b = (struct spindle_classmap_struct *) ptrb;
	
	return a->score - b->score;
}

static int
spindle_rulebase_class_dump_(SPINDLERULES *rules)
{
	size_t c, d;

	twine_logf(LOG_DEBUG, PLUGIN_NAME ": classes rule-base (%d entries):\n", (int) rules->classcount);
	for(c = 0; c < rules->classcount; c++)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": %d: <%s>\n", rules->classes[c].score, rules->classes[c].uri);
		for(d = 0; d < rules->classes[c].matchcount; d++)
		{
			twine_logf(LOG_DEBUG, PLUGIN_NAME "  +--> <%s>\n", rules->classes[c].match[d].uri);
		}
	}
	return 0;
}

/* Note that the return value from this function is only valid until the next
 * call, because it may reallocate the block of memory into which it points
 */
static struct spindle_classmap_struct *
spindle_rulebase_class_add_(SPINDLERULES *rules, const char *uri)
{
	size_t c;
	struct spindle_classmap_struct *p;

	for(c = 0; c < rules->classcount; c++)
	{
		if(!strcmp(uri, rules->classes[c].uri))
		{
			return &(rules->classes[c]);
		}
	}
	if(rules->classcount + 1 >= rules->classsize)
	{
		p = (struct spindle_classmap_struct *) realloc(rules->classes, sizeof(struct spindle_classmap_struct) * (rules->classsize + 4));
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to expand class-match list\n");
			return NULL;
		}
		rules->classes = p;
		rules->classsize += 4;
	}
	p = &(rules->classes[rules->classcount]);
	memset(p, 0, sizeof(struct spindle_classmap_struct));
	p->uri = strdup(uri);
	if(!p->uri)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate class URI\n");
		return NULL;
	}
	p->score = 100;
	rules->classcount++;
	spindle_rulebase_class_add_match_(p, uri, 0);
	return p;
}

static int
spindle_rulebase_class_add_match_(struct spindle_classmap_struct *match, const char *uri, int prominence)
{
	size_t c;
	struct spindle_classmatch_struct *p;

	for(c = 0; c < match->matchcount; c++)
	{
		if(!strcmp(match->match[c].uri, uri))
		{
			if(prominence)
			{
				match->match[c].prominence = prominence;
			}
			return 0;
		}
	}	
	if(match->matchcount + 1 > match->matchsize)
	{
		p = (struct spindle_classmatch_struct *) realloc(match->match, sizeof(struct spindle_classmatch_struct) * (match->matchsize + 4 + 1));
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to expand match URI list\n");
			return -1;
		}
		match->match = p;
		memset(&(p[match->matchsize]), 0, sizeof(char *) * (4 + 1));
		match->matchsize += 4;
	}
	p = &(match->match[match->matchcount]);
	p->uri = strdup(uri);
	if(!p->uri)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate match URI\n");
		return -1;
	}
	p->prominence = prominence;
	match->matchcount++;
	return 1;
}

static int
spindle_rulebase_class_set_score_(struct spindle_classmap_struct *entry, librdf_statement *statement)
{
	long score;

	if(twine_rdf_st_obj_intval(statement, &score) <= 0)
	{
		return 0;
	}
	if(score <= 0)
	{
		return 0;
	}
	entry->score = (int) score;
	return 1;
}
