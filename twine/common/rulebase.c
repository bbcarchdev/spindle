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

static char *spindle_rulebase_loadfile_(const char *filename);
static int spindle_rulebase_add_statement_(SPINDLERULES *rules, librdf_model *model, librdf_statement *statement);

SPINDLERULES *
spindle_rulebase_create(const char *path, const struct coref_match_struct *match_types)
{
	SPINDLERULES *rules;
	char *rulebase, *buf;
	librdf_model *model;
	librdf_stream *stream;
	librdf_statement *statement;

	rules = (SPINDLERULES *) calloc(1, sizeof(SPINDLERULES));
	if(!rules)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create rulebase object\n");
		return NULL;
	}
	rules->match_types = match_types;
	/* Always cache these predicates, regardless of what the rulebase states */
	spindle_rulebase_cachepred_add(rules, NS_RDF "type");
	spindle_rulebase_cachepred_add(rules, NS_OWL "sameAs");

	model = twine_rdf_model_create();
	if(!model)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create new RDF model\n");
		free(rules);
		return NULL;
	}
	if(path)
	{
		buf = spindle_rulebase_loadfile_(path);
	}
	else
	{
		rulebase = twine_config_geta("spindle:rulebase", TWINEMODULEDIR "/rulebase.ttl");
		buf = spindle_rulebase_loadfile_(rulebase);
		free(rulebase);
	}
	if(!buf)
	{
		/* spindle_loadfile_() will have already logged the error */
		twine_rdf_model_destroy(model);
		spindle_rulebase_destroy(rules);
		return NULL;
	}
	if(twine_rdf_model_parse(model, MIME_TURTLE, buf, strlen(buf)))
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to parse rulebase as " MIME_TURTLE "\n");
		free(buf);
		twine_rdf_model_destroy(model);
		return NULL;
	}
	free(buf);
	/* Parse the model, adding data to the match lists */
	stream = librdf_model_as_stream(model);
	while(!librdf_stream_end(stream))
	{
		statement = librdf_stream_get_object(stream);
		spindle_rulebase_add_statement_(rules, model, statement);
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	twine_rdf_model_destroy(model);
	spindle_rulebase_class_finalise(rules);
	spindle_rulebase_pred_finalise(rules);
	spindle_rulebase_cachepred_finalise(rules);
	return rules;
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
			if(spindle_rulebase_class_add_node(rules, model, subjuri, subject) < 0)
			{
				return -1;
			}
			return 1;
		}
		if(!strcmp(objuri, NS_SPINDLE "Property"))
		{
			if(spindle_rulebase_pred_add_node(rules, model, subjuri, subject) < 0)
			{
				return -1;
			}
			return 1;
		}
		return 0;
	}
	/* ex:Class spindle:expressedAs ex:OtherClass */
	if(!strcmp(preduri, NS_SPINDLE "expressedAs"))
	{
		if(!librdf_node_is_resource(object))
		{
			return 0;
		}
		return spindle_rulebase_class_add_matchnode(rules, model, subjuri, object);
	}
	/* ex:predicate spindle:property [ ... ] */
	if(!strcmp(preduri, NS_SPINDLE "property"))
	{
		if(librdf_node_is_literal(object))
		{
			return 0;
		}
		return spindle_rulebase_pred_add_matchnode(rules, model, subjuri, object, 0);
	}
	/* ex:predicate spindle:inverseProperty [ ... ] */
	if(!strcmp(preduri, NS_SPINDLE "inverseProperty"))
	{
		if(librdf_node_is_literal(object))
		{
			return 0;
		}
		return spindle_rulebase_pred_add_matchnode(rules, model, subjuri, object, 1);
	}
	/* ex:predicate spindle:coref spindle:foo */
	if(!strcmp(preduri, NS_SPINDLE "coref"))
	{
		return spindle_rulebase_coref_add_node(rules, subjuri, object);
	}
	return 0;
}
