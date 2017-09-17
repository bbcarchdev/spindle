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

static char *spindle_rulebase_loadfile_(const char *filename);
static int spindle_rulebase_add_statement_(SPINDLERULES *rules, librdf_model *model, librdf_statement *statement);

/* Create a new rulebase instance */
SPINDLERULES *
spindle_rulebase_create(void)
{
	SPINDLERULES *rules;

	rules = (SPINDLERULES *) calloc(1, sizeof(SPINDLERULES));
	if(!rules)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create rulebase object\n");
		return NULL;
	}
	/* Always cache these predicates */
	spindle_rulebase_cachepred_add(rules, NS_RDF "type");
	spindle_rulebase_cachepred_add(rules, NS_OWL "sameAs");	
	return rules;
}

/* Set the list of matching callbacks used by the correlation engine */
int
spindle_rulebase_set_matchtypes(SPINDLERULES *rules, const struct coref_match_struct *match_types)
{
	rules->match_types = match_types;
	return 0;
}

/* Add the statements from a librdf_model to a rulebase */
int
spindle_rulebase_add_model(SPINDLERULES *rules, librdf_model *model, const char *sourcefile)
{
	librdf_stream *stream;
	librdf_statement *statement;
	int r;

	if(!sourcefile)
	{
		sourcefile = "*builtin*";
	}
	stream = librdf_model_as_stream(model);
	if(!stream)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to obtain stream from rulebase model for %s\n", sourcefile);
		return -1;
	}
	for(r = 0; !librdf_stream_end(stream); librdf_stream_next(stream))
	{
		statement = librdf_stream_get_object(stream);
		if(spindle_rulebase_add_statement_(rules, model, statement) < 0)
		{
			r = -1;
			break;
		}
	}
	librdf_free_stream(stream);	
	return r;
}

/* Add a buffer containing Turtle to the rulebase */
int
spindle_rulebase_add_turtle(SPINDLERULES *rules, const char *buf, const char *sourcefile)
{
	librdf_model *model;
	int r;

	if(!sourcefile)
	{
		sourcefile = "*builtin*";
	}
	model = twine_rdf_model_create();
	if(!model)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create new RDF model for %s\n", sourcefile);
		return -1;
	}
	if(twine_rdf_model_parse(model, MIME_TURTLE, buf, strlen(buf)))
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to parse rulebase %s as " MIME_TURTLE "\n", sourcefile);	
		librdf_free_model(model);
		return -1;
	}
	r = spindle_rulebase_add_model(rules, model, sourcefile);
	librdf_free_model(model);
	return r;
}

/* Add the statements from a Turtle file to the rulebase */
int
spindle_rulebase_add_file(SPINDLERULES *rules, const char *path)
{
	int r;
	char *buf;

	buf = spindle_rulebase_loadfile_(path);
	if(!buf)
	{
		return -1;
	}
	r = spindle_rulebase_add_turtle(rules, buf, path);
	free(buf);
	return r;
}

/* Add all of the rules from the default file(s) */
int
spindle_rulebase_add_config(SPINDLERULES *rules)
{
	char *path;
	int r;

	path = twine_config_geta("spindle:rulebase", TWINEMODULEDIR "/rulebase.ttl");
	r = spindle_rulebase_add_file(rules, path);
	free(path);
	return r;
}

/* Once all rules have been loaded, finalise the
 * rulebase so that it can be used to process
 * source graphs.
 */
int
spindle_rulebase_finalise(SPINDLERULES *rules)
{
	spindle_rulebase_class_finalise(rules);
	spindle_rulebase_pred_finalise(rules);
	spindle_rulebase_cachepred_finalise(rules);	
	return 0;
}

int
spindle_rulebase_destroy(SPINDLERULES *rules)
{
	spindle_rulebase_class_cleanup(rules);
	spindle_rulebase_pred_cleanup(rules);
	spindle_rulebase_cachepred_cleanup(rules);
	free(rules);
	return 0;
}

int
spindle_rulebase_dump(SPINDLERULES *rules)
{
	spindle_rulebase_cachepred_dump(rules);
	spindle_rulebase_class_dump(rules);
	spindle_rulebase_pred_dump(rules);
	spindle_rulebase_coref_dump(rules);
	return 0;
}

static char *
spindle_rulebase_loadfile_(const char *filename)
{
	char *buffer, *p;
	size_t bufsize, buflen;
	ssize_t r;
	FILE *f;

	buffer = NULL;
	bufsize = 0;
	buflen = 0;
	f = fopen(filename, "rb");
	if(!f)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": %s: %s\n", filename, strerror(errno));
		return NULL;
	}
	while(!feof(f))
	{
		if(bufsize - buflen < 1024)
		{
			p = (char *) realloc(buffer, bufsize + 1024);
			if(!p)
			{
				twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to reallocate buffer from %u bytes to %u bytes\n", (unsigned) bufsize, (unsigned) bufsize + 1024);
				fclose(f);
				free(buffer);
				return NULL;
			}
			buffer = p;
			bufsize += 1024;
		}
		r = fread(&(buffer[buflen]), 1, 1023, f);
		if(r < 0)
		{
			twine_logf(LOG_CRIT, "error reading from %s: %s\n", filename, strerror(errno));
			free(buffer);
			return NULL;
		}
		buflen += r;
		buffer[buflen] = 0;
	}
	fclose(f);
	return buffer;
}

/* Invoked for every statement in the rulebase */
static int
spindle_rulebase_add_statement_(SPINDLERULES *rules, librdf_model *model, librdf_statement *statement)
{
	librdf_node *subject, *predicate, *object;
	librdf_uri *subj, *pred, *obj;
	const char *subjuri, *preduri, *objuri;

	/* ex:Class rdf:type spindle:Class =>
	 *    Add class to match list if not present.
	 *
	 *  -- ex:Class olo:index nnn =>
	 *     set score to nnn.
	 *
	 * ex:Class1 spindle:expressedAs ex:Class2 =>
	 *    Add ex:Class2 to match list if not present; add ex:Class1 as
	 *    one of the matched classes of ex:Class2.
	 *
	 * ex:prop rdf:type spindle:Property =>
	 *    Add predicate to mapping list if not present
	 *
	 *  -- ex:prop spindle:expect spindle:literal, spindle:uri =>
	 *     set predicate mapping 'expected type'
	 *  -- ex:prop spindle:expectDataType =>
	 *     set predicate mapping 'expected datatype'
	 *  -- ex:prop spindle:proxyOnly =>
	 *     set predicate mapping 'proxy-only' flag
	 *
	 * ex:prop spindle:property _:bnode =>
	 *    Process _:bnode as a predicate-match entry for the predicate ex:prop
	 *    (The bnode itself will specify the predicate mapping to attach
	 *    the match entry to).
	 */
	subject = librdf_statement_get_subject(statement);
	predicate = librdf_statement_get_predicate(statement);
	object = librdf_statement_get_object(statement);
	if(!librdf_node_is_resource(subject) || !librdf_node_is_resource(predicate))
	{
		/* We're not interested in this triple */
		return 0;
	}
	subj = librdf_node_get_uri(subject);
	subjuri = (const char *) librdf_uri_as_string(subj);
	pred = librdf_node_get_uri(predicate);
	preduri = (const char *) librdf_uri_as_string(pred);
	/* ex:Class a spindle:Class
	 * ex:predicate a spindle:Property
	 */
	if(!strcmp(preduri, NS_RDF "type"))
	{
		if(!librdf_node_is_resource(object))
		{
			return 0;
		}
		obj = librdf_node_get_uri(object);
		objuri = (const char *) librdf_uri_as_string(obj);
		if(!strcmp(objuri, NS_SPINDLE "Class"))
		{
			/* Create a new class entry */
			if(spindle_rulebase_class_add_node(rules, model, subjuri, subject) < 0)
			{
				return -1;
			}
			return 1;
		}
		if(!strcmp(objuri, NS_SPINDLE "Property"))
		{
			/* Create a new property entry */
			if(spindle_rulebase_pred_add_node(rules, model, subjuri, subject) < 0)
			{
				return -1;
			}
			return 1;
		}
		return 0;
	}
	/* ex:Class spindle:expressedAs ex:OtherClass
	 * Add a class match rule
	 */
	if(!strcmp(preduri, NS_SPINDLE "expressedAs"))
	{
		if(!librdf_node_is_resource(object))
		{
			return 0;
		}
		return spindle_rulebase_class_add_matchnode(rules, model, subjuri, object);
	}
	/* ex:predicate spindle:property [ ... ]
	 * Add a property match rule
	 */
	if(!strcmp(preduri, NS_SPINDLE "property"))
	{
		if(librdf_node_is_literal(object))
		{
			return 0;
		}
		return spindle_rulebase_pred_add_matchnode(rules, model, subjuri, object, 0);
	}
	/* ex:predicate spindle:inverseProperty [ ... ]
	 * Add an inverse property-match rule
	 */
	if(!strcmp(preduri, NS_SPINDLE "inverseProperty"))
	{
		if(librdf_node_is_literal(object))
		{
			return 0;
		}
		return spindle_rulebase_pred_add_matchnode(rules, model, subjuri, object, 1);
	}
	/* ex:predicate spindle:coref spindle:foo
	 * Add a co-reference match type rule
	 */
	if(!strcmp(preduri, NS_SPINDLE "coref"))
	{
		return spindle_rulebase_coref_add_node(rules, subjuri, object);
	}
	return 0;
}
