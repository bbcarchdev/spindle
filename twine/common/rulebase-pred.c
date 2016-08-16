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

static int spindle_rulebase_pred_compare_(const void *ptra, const void *ptrb);
static struct spindle_predicatemap_struct *spindle_rulebase_pred_add_(SPINDLERULES *rules, const char *preduri);
static int spindle_rulebase_pred_add_match_(struct spindle_predicatemap_struct *map, const char *matchuri, const char *classuri, int score, int prominence, int inverse);
static int spindle_rulebase_pred_set_score_(struct spindle_predicatemap_struct *map, librdf_statement *statement);
static int spindle_rulebase_pred_set_prominence_(struct spindle_predicatemap_struct *map, librdf_statement *statement);
static int spindle_rulebase_pred_set_expect_(struct spindle_predicatemap_struct *entry, librdf_statement *statement);
static int spindle_rulebase_pred_set_expecttype_(struct spindle_predicatemap_struct *entry, librdf_statement *statement);
static int spindle_rulebase_pred_set_proxyonly_(struct spindle_predicatemap_struct *entry, librdf_statement *statement);
static int spindle_rulebase_pred_set_indexed_(struct spindle_predicatemap_struct *entry, librdf_statement *statement);
static int spindle_rulebase_pred_set_inverse_(struct spindle_predicatemap_struct *entry, librdf_statement *statement);

/* Given an instance of a spindle:Property, add it to the rulebase */
int
spindle_rulebase_pred_add_node(SPINDLERULES *rules, librdf_model *model, const char *uri, librdf_node *node)
{
	librdf_world *world;
	librdf_statement *query, *statement;
	librdf_node *s, *predicate;
	librdf_uri *pred;
	librdf_stream *stream;
	const char *preduri;
	int r;
	struct spindle_predicatemap_struct *predentry;
	
	world = twine_rdf_world();
	if(!(predentry = spindle_rulebase_pred_add_(rules, uri)))
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
		/* ex:predicate olo:index nnn */
		if(!strcmp(preduri, NS_OLO "index"))
		{
			r = spindle_rulebase_pred_set_score_(predentry, statement);
		}
		/* ex:predicate spindle:expect rdfs:Literal
		 * ex:predicate spindle:expect rdfs:Resource
		 */
		if(!strcmp(preduri, NS_SPINDLE "expect"))
		{
			r = spindle_rulebase_pred_set_expect_(predentry, statement);
		}
		/* ex:predicate spindle:expectType xsd:decimal */
		if(!strcmp(preduri, NS_SPINDLE "expectType"))
		{
			r = spindle_rulebase_pred_set_expecttype_(predentry, statement);
		}
		/* ex:predicate spindle:proxyOnly "true"^^xsd:boolean (default false) */
		if(!strcmp(preduri, NS_SPINDLE "proxyOnly"))
		{
			r = spindle_rulebase_pred_set_proxyonly_(predentry, statement);
		}
		/* ex:predicate spindle:indexed "true"^^xsd:boolean (default false) */
		if(!strcmp(preduri, NS_SPINDLE "indexed"))
		{
			r = spindle_rulebase_pred_set_indexed_(predentry, statement);
		}
		/* ex:predicate spindle:inverse "true"^^xsd:boolean (default false) */
		if(!strcmp(preduri, NS_SPINDLE "inverse"))
		{
			r = spindle_rulebase_pred_set_inverse_(predentry, statement);
		}
		/* ex:predicate spindle:prominence nnn */
		if(!strcmp(preduri, NS_SPINDLE "prominence"))
		{
			r = spindle_rulebase_pred_set_prominence_(predentry, statement);
		}
		if(r < 0)
		{
			break;
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

int
spindle_rulebase_pred_add_matchnode(SPINDLERULES *rules, librdf_model *model, const char *matchuri, librdf_node *matchnode, int inverse)
{
	librdf_world *world;
	librdf_node *s, *predicate, *p, *object;
	librdf_uri *pred, *obj;
	librdf_statement *query, *statement;
	librdf_stream *stream;
	const char *preduri, *objuri;
	struct spindle_predicatemap_struct *entry;
	int r, hasdomain, score, prominence;
	long i;

	/* Find triples whose subject is <matchnode>, which are expected to take
	 * the form:
	 *
	 * _:foo olo:index nnn ;
	 *   spindle:expressedAs ex:property1 ;
	 *   rdfs:domain ex:Class1, ex:Class2 ... .
	 */
	world = twine_rdf_world();
	s = librdf_new_node_from_node(matchnode);
	query = librdf_new_statement_from_nodes(world, s, NULL, NULL);
	stream = librdf_model_find_statements(model, query);
	score = 100;
	prominence = 0;
	r = 0;
	hasdomain = 0;
	entry = NULL;
	while(!librdf_stream_end(stream))
	{
		statement = librdf_stream_get_object(stream);		
		predicate = librdf_statement_get_predicate(statement);
		object = librdf_statement_get_object(statement);
		if(!librdf_node_is_resource(predicate))
		{
			librdf_stream_next(stream);
			continue;
		}
		pred = librdf_node_get_uri(predicate);
		preduri = (const char *) librdf_uri_as_string(pred);
		if(!strcmp(preduri, NS_RDFS "domain"))
		{
			hasdomain = 1;
		}
		else if(!strcmp(preduri, NS_OLO "index"))
		{
			if(twine_rdf_node_intval(object, &i) > 0 && i >= 0)
			{
				score = (int) i;
			}
		}
		else if(!strcmp(preduri, NS_SPINDLE "prominence"))
		{
			if(twine_rdf_node_intval(object, &i) > 0 && i != 0)
			{
				prominence = (int) i;
			}
		}
		else if(!strcmp(preduri, NS_SPINDLE "expressedAs"))
		{
			if(!librdf_node_is_resource(object))
			{
				librdf_stream_next(stream);
				continue;
			}
			obj = librdf_node_get_uri(object);
			objuri = (const char *) librdf_uri_as_string(obj);
			if(!(entry = spindle_rulebase_pred_add_(rules, objuri)))
			{
				r = -1;
				break;
			}
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	if(r < 0)
	{
		return -1;
	}
	if(!entry)
	{
		return 0;
	}
	if(spindle_rulebase_cachepred_add(rules, matchuri))
	{
		return -1;
	}
	if(!hasdomain)
	{
		/* This isn't a domain-specific mapping; just add the target
		 * predicate <matchuri> to entry's match list.
		 */
		spindle_rulebase_pred_add_match_(entry, matchuri, NULL, score, prominence, inverse);
		return 0;
	}
	/* For each rdfs:domain, add the target predicate <matchuri> along with
	 * the listed domain to the entry's match list.
	 */
	s = librdf_new_node_from_node(matchnode);
	p = librdf_new_node_from_uri_string(world, (const unsigned char *) NS_RDFS "domain");
	query = librdf_new_statement_from_nodes(world, s, p, NULL);
	stream = librdf_model_find_statements(model, query);
	while(!librdf_stream_end(stream))
	{
		statement = librdf_stream_get_object(stream);
		object = librdf_statement_get_object(statement);
		if(!librdf_node_is_resource(object))
		{
			librdf_stream_next(stream);
			continue;
		}
		obj = librdf_node_get_uri(object);
		objuri = (const char *) librdf_uri_as_string(obj);
		spindle_rulebase_pred_add_match_(entry, matchuri, objuri, score, prominence, inverse);
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	return 0;
}

int
spindle_rulebase_pred_finalise(SPINDLERULES *rules)
{
	qsort(rules->predicates, rules->predcount, sizeof(struct spindle_predicatemap_struct), spindle_rulebase_pred_compare_);
	return 0;
}

int
spindle_rulebase_pred_cleanup(SPINDLERULES *rules)
{
	size_t c, d;

	for(c = 0; c < rules->predcount; c++)
	{
		free(rules->predicates[c].target);
		free(rules->predicates[c].datatype);
		for(d = 0; d < rules->predicates[c].matchcount; d++)
		{
			free(rules->predicates[c].matches[d].predicate);
			free(rules->predicates[c].matches[d].onlyfor);
		}
		free(rules->predicates[c].matches);
	}
	free(rules->predicates);
	rules->predicates = NULL;
	return 0;
}

static int
spindle_rulebase_pred_compare_(const void *ptra, const void *ptrb)
{
	struct spindle_predicatemap_struct *a, *b;
	
	a = (struct spindle_predicatemap_struct *) ptra;
	b = (struct spindle_predicatemap_struct *) ptrb;
	
	return a->score - b->score;
}

int
spindle_rulebase_pred_dump(SPINDLERULES *rules)
{
	size_t c, d;
	const char *expect, *po;

	twine_logf(LOG_DEBUG, PLUGIN_NAME ": predicates rule-base (%d entries):\n", (int) rules->predcount);
	for(c = 0; c < rules->predcount; c++)
	{
		switch(rules->predicates[c].expected)
		{
		case RAPTOR_TERM_TYPE_URI:
			expect = "URI";
			break;
		case RAPTOR_TERM_TYPE_LITERAL:
			expect = "literal";
			break;
		default:
			expect = "unknown";
		}
		if(rules->predicates[c].proxyonly)
		{
			po = " [proxy-only]";
		}
		else
		{
			po = "";
		}
		if(rules->predicates[c].datatype)
		{
			twine_logf(LOG_DEBUG, PLUGIN_NAME ": %d: <%s> (%s <%s>) %s\n", rules->predicates[c].score, rules->predicates[c].target, expect, rules->predicates[c].datatype, po);
		}
		else
		{
			twine_logf(LOG_DEBUG, PLUGIN_NAME ": %d: <%s> (%s) %s\n", rules->predicates[c].score, rules->predicates[c].target, expect, po);
		}
		for(d = 0; d < rules->predicates[c].matchcount; d++)
		{
			if(rules->predicates[c].matches[d].onlyfor)
			{
				twine_logf(LOG_DEBUG, PLUGIN_NAME "  +--> %d: <%s> (for <%s>)\n",
						   rules->predicates[c].matches[d].priority,
						   rules->predicates[c].matches[d].predicate,
						   rules->predicates[c].matches[d].onlyfor);
			}
			else
			{
				twine_logf(LOG_DEBUG, PLUGIN_NAME "  +--> %d: <%s>\n",
						   rules->predicates[c].matches[d].priority,
						   rules->predicates[c].matches[d].predicate);
			}
		}
	}	
	return 0;
}

static int
spindle_rulebase_pred_set_score_(struct spindle_predicatemap_struct *entry, librdf_statement *statement)
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

static int
spindle_rulebase_pred_set_prominence_(struct spindle_predicatemap_struct *entry, librdf_statement *statement)
{
	long score;

	if(twine_rdf_st_obj_intval(statement, &score) <= 0)
	{
		return 0;
	}
	if(!score)
	{
		return 0;
	}
	entry->prominence = score;
	return 1;
}

static int
spindle_rulebase_pred_set_expect_(struct spindle_predicatemap_struct *entry, librdf_statement *statement)
{
	librdf_node *object;
	librdf_uri *obj;
	const char *objuri;
		
	object = librdf_statement_get_object(statement);
	if(!librdf_node_is_resource(object))
	{
		return 0;
	}
	obj = librdf_node_get_uri(object);
	objuri = (const char *) librdf_uri_as_string(obj);
	if(!strcmp(objuri, NS_RDFS "Literal"))
	{
		entry->expected = RAPTOR_TERM_TYPE_LITERAL;
		return 1;
	}
	if(!strcmp(objuri, NS_RDFS "Resource"))
	{
		entry->expected = RAPTOR_TERM_TYPE_URI;
		return 1;
	}
	twine_logf(LOG_WARNING, PLUGIN_NAME ": unexpected spindle:expect value <%s> for <%s>\n", objuri, entry->target);
	return 0;
}

static int
spindle_rulebase_pred_set_expecttype_(struct spindle_predicatemap_struct *entry, librdf_statement *statement)
{
	librdf_node *object;
	librdf_uri *obj;
	const char *objuri;
	char *str;

	object = librdf_statement_get_object(statement);
	if(!librdf_node_is_resource(object))
	{
		return 0;
	}
	obj = librdf_node_get_uri(object);
	objuri = (const char *) librdf_uri_as_string(obj);
	str = strdup(objuri);
	if(!str)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate datatype URI\n");
		return -1;
	}
	if(entry->datatype)
	{
		free(entry->datatype);
	}
	entry->datatype = str;
	return 0;
}

static int
spindle_rulebase_pred_set_proxyonly_(struct spindle_predicatemap_struct *entry, librdf_statement *statement)
{
	librdf_node *object;
	librdf_uri *dt;
	const char *dturi, *objstr;
	
	object = librdf_statement_get_object(statement);
	if(!librdf_node_is_literal(object))
	{
		return 0;
	}
	dt = librdf_node_get_literal_value_datatype_uri(object);
	if(!dt)
	{
		return 0;
	}
	dturi = (const char *) librdf_uri_as_string(dt);
	if(strcmp(dturi, NS_XSD "boolean"))
	{
		return 0;
	}
	objstr = (const char *) librdf_node_get_literal_value(object);
	if(!strcmp(objstr, "true"))
	{
		entry->proxyonly = 1;
	}
	else
	{
		entry->proxyonly = 0;
	}
	return 1;
}

static int
spindle_rulebase_pred_set_indexed_(struct spindle_predicatemap_struct *entry, librdf_statement *statement)
{
	librdf_node *object;
	librdf_uri *dt;
	const char *dturi, *objstr;
	
	object = librdf_statement_get_object(statement);
	if(!librdf_node_is_literal(object))
	{
		return 0;
	}
	dt = librdf_node_get_literal_value_datatype_uri(object);
	if(!dt)
	{
		return 0;
	}
	dturi = (const char *) librdf_uri_as_string(dt);
	if(strcmp(dturi, NS_XSD "boolean"))
	{
		return 0;
	}
	objstr = (const char *) librdf_node_get_literal_value(object);
	if(!strcmp(objstr, "true"))
	{
		entry->indexed = 1;
	}
	else
	{
		entry->indexed = 0;
	}
	return 1;
}

static int
spindle_rulebase_pred_set_inverse_(struct spindle_predicatemap_struct *entry, librdf_statement *statement)
{
	librdf_node *object;
	librdf_uri *dt;
	const char *dturi, *objstr;
	
	object = librdf_statement_get_object(statement);
	if(!librdf_node_is_literal(object))
	{
		return 0;
	}
	dt = librdf_node_get_literal_value_datatype_uri(object);
	if(!dt)
	{
		return 0;
	}
	dturi = (const char *) librdf_uri_as_string(dt);
	if(strcmp(dturi, NS_XSD "boolean"))
	{
		return 0;
	}
	objstr = (const char *) librdf_node_get_literal_value(object);
	if(!strcmp(objstr, "true"))
	{
		entry->inverse = 1;
	}
	else
	{
		entry->inverse = 0;
	}
	return 1;
}

static struct spindle_predicatemap_struct *
spindle_rulebase_pred_add_(SPINDLERULES *rules, const char *preduri)
{
	size_t c;
	struct spindle_predicatemap_struct *p;

	if(spindle_rulebase_cachepred_add(rules, preduri))
	{
		return NULL;
	}
	for(c = 0; c < rules->predcount; c++)
	{
		if(!strcmp(rules->predicates[c].target, preduri))
		{
			return &(rules->predicates[c]);
		}
	}
	if(rules->predcount + 1 >= rules->predsize)
	{
		p = (struct spindle_predicatemap_struct *) realloc(rules->predicates, sizeof(struct spindle_predicatemap_struct) * (rules->predsize + 4 + 1));
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to expand predicate map list\n");
			return NULL;
		}
		rules->predicates = p;
		memset(&(p[rules->predsize]), 0, sizeof(struct spindle_predicatemap_struct) * (4 + 1));
		rules->predsize += 4;
	}
	p = &(rules->predicates[rules->predcount]);
	p->target = strdup(preduri);
	p->expected = RAPTOR_TERM_TYPE_UNKNOWN;
	p->proxyonly = 0;
	p->score = 100;
	if(!p->target)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate predicate URI\n");
		return NULL;
	}
	rules->predcount++;
	return p;
}

static int
spindle_rulebase_pred_add_match_(struct spindle_predicatemap_struct *map, const char *matchuri, const char *classuri, int score, int prominence, int inverse)
{
	size_t c;
	struct spindle_predicatematch_struct *p;

	for(c = 0; c < map->matchcount; c++)
	{
		if(strcmp(map->matches[c].predicate, matchuri))
		{
			continue;
		}
		if((classuri && !map->matches[c].onlyfor) ||
		   (!classuri && map->matches[c].onlyfor))
		{
			continue;
		}
		if(inverse != map->matches[c].inverse)
		{
			continue;
		}
		if(classuri && strcmp(map->matches[c].onlyfor, classuri))
		{
			continue;
		}
		map->matches[c].priority = score;
		map->matches[c].prominence = prominence;
		return 1;
	}
	if(map->matchcount + 1 >= map->matchsize)
	{
		p = (struct spindle_predicatematch_struct *) realloc(map->matches, sizeof(struct spindle_predicatematch_struct) * (map->matchsize + 4 + 1));
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to resize predicate match list\n");
			return -1;
		}
		map->matches = p;
		memset(&(p[map->matchsize]), 0, sizeof(struct spindle_predicatematch_struct) * (4 + 1));
		map->matchsize += 4;
	}
	p = &(map->matches[map->matchcount]);
	p->predicate = strdup(matchuri);
	if(!p->predicate)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate predicate match URI\n");
		return -1;
	}
	if(classuri)
	{
		p->onlyfor = strdup(classuri);
		if(!p->onlyfor)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate predicate match class URI\n");
			free(p->predicate);
			p->predicate = NULL;
			return -1;
		}
	}
	p->priority = score;
	p->prominence = prominence;
	p->inverse = inverse;
	map->matchcount++;
	return 1;
}
