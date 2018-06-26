/**
 * Copyright 2018 BBC
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

#include <cgreen/cgreen.h>
#include <cgreen/mocks.h>
#include <sys/syslog.h>
#include <ctype.h>
#include <errno.h>

/* mocks of dependancies */
#include "../../t/mock_librdf.h"
#include "../../t/mock_libsql.h"
#include "../../t/mock_libtwine.h"
#include "../../t/mock_spindle_core.h"
#include "../../t/mock_spindle_rulebase_cachepred.h"
#include "../../t/mock_spindle_rulebase_class.h"
#include "../../t/mock_spindle_rulebase_coref.h"
#include "../../t/mock_spindle_rulebase_pred.h"
#include "../../t/rdf_namespaces.h"

#define TWINEMODULEDIR "TWINEMODULEDIR "
#define MIME_TURTLE "MIME_TURTLE"

/* compile SUT inline due to static functions */
#define P_SPINDLE_H_
#define PLUGIN_NAME "spindle-rulebase"
int spindle_rulebase_destroy(SPINDLERULES *rules);
#include "../rulebase.c"

Describe(spindle_common_rulebase);
BeforeEach(spindle_common_rulebase) { always_expect(twine_logf); die_in(1); }
AfterEach(spindle_common_rulebase) {}

/*
	This suite does not test any failure paths triggered by libc calls such as malloc.
*/

#pragma mark -
#pragma mark spindle_rulebase_create

Ensure(spindle_common_rulebase, create_succeeds_and_installs_required_cachepreds) {
	expect(spindle_rulebase_cachepred_add, when(rules, is_non_null), when(uri, is_equal_to_string(NS_RDF "type")));
	expect(spindle_rulebase_cachepred_add, when(rules, is_non_null), when(uri, is_equal_to_string(NS_OWL "sameAs")));

	SPINDLERULES *rules = spindle_rulebase_create();
	assert_that(rules, is_non_null);
}

#pragma mark -
#pragma mark spindle_rulebase_set_matchtypes

Ensure(spindle_common_rulebase, set_matchtypes_succeeds) {
	SPINDLERULES rules = { 0 };
	struct coref_match_struct match_types = { 0 };

	int r = spindle_rulebase_set_matchtypes(&rules, &match_types);
	assert_that(r, is_equal_to(0));
	assert_that(rules.match_types, is_equal_to(&match_types));
}

#pragma mark -
#pragma mark spindle_rulebase_add_model

Ensure(spindle_common_rulebase, add_model_fails_if_stream_cannot_be_obtained_from_model) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;

	expect(librdf_model_as_stream, will_return(NULL), when(model, is_equal_to(model)));

	int r = spindle_rulebase_add_model(&rules, model, __FILE__);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_common_rulebase, add_model_fails_if_add_statement_fails) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_stream *stream = (librdf_stream *) 0xA02;

	always_expect(librdf_model_as_stream, will_return(stream));
	expect(librdf_stream_end, will_return(0));
	expect(librdf_stream_get_object, when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_subject);
	expect(librdf_statement_get_predicate);
	expect(librdf_statement_get_object);
	expect(librdf_node_is_resource, will_return(1));
	expect(librdf_node_is_resource, will_return(1));
	expect(librdf_node_get_uri);
	expect(librdf_uri_as_string, will_return("subject"));
	expect(librdf_node_get_uri);
	expect(librdf_uri_as_string, will_return(NS_SPINDLE "expressedAs"));
	expect(librdf_node_is_resource, will_return(1));
	expect(spindle_rulebase_class_add_matchnode, will_return(-1));
	// uses break so no more loop bounds checks
	expect(librdf_free_stream, when(stream, is_equal_to(stream)));

	int r = spindle_rulebase_add_model(&rules, model, __FILE__);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_common_rulebase, add_model_returns_no_error_if_add_statement_succeeds) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_stream *stream = (librdf_stream *) 0xA02;

	always_expect(librdf_model_as_stream, will_return(stream));
	expect(librdf_stream_end, will_return(0));
	expect(librdf_stream_get_object, when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_subject);
	expect(librdf_statement_get_predicate);
	expect(librdf_statement_get_object);
	expect(librdf_node_is_resource, will_return(1));
	expect(librdf_node_is_resource, will_return(1));
	expect(librdf_node_get_uri);
	expect(librdf_uri_as_string, will_return("subject"));
	expect(librdf_node_get_uri);
	expect(librdf_uri_as_string, will_return(NS_SPINDLE "expressedAs"));
	expect(librdf_node_is_resource, will_return(1));
	expect(spindle_rulebase_class_add_matchnode, will_return(0));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(1));
	expect(librdf_free_stream, when(stream, is_equal_to(stream)));

	int r = spindle_rulebase_add_model(&rules, model, __FILE__);
	assert_that(r, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_rulebase_add_turtle

Ensure(spindle_common_rulebase, add_turtle_fails_if_twine_rdf_model_create_fails) {
	SPINDLERULES rules = { 0 };
	const char *buffer = NULL;

	expect(twine_rdf_model_create, will_return(NULL));

	int r = spindle_rulebase_add_turtle(&rules, buffer, __FILE__);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_common_rulebase, add_turtle_fails_if_model_parse_fails) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	const char *buffer = "";

	expect(twine_rdf_model_create, will_return(model));
	expect(twine_rdf_model_parse, will_return(-1), when(model, is_equal_to(model)), when(mime, is_equal_to_string(MIME_TURTLE)));
	expect(librdf_free_model, when(model, is_equal_to(model)));

	int r = spindle_rulebase_add_turtle(&rules, buffer, __FILE__);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_common_rulebase, add_turtle_returns_the_result_of_add_model_if_the_buffer_parses) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_stream *stream = (librdf_stream *) 0xA02;
	const char *buffer = "";

	expect(twine_rdf_model_create, will_return(model));
	expect(twine_rdf_model_parse, will_return(0), when(model, is_equal_to(model)), when(mime, is_equal_to_string(MIME_TURTLE)));
	expect(librdf_model_as_stream, will_return(stream), when(model, is_equal_to(model)));
	expect(librdf_stream_end, will_return(1));
	expect(librdf_free_stream, when(stream, is_equal_to(stream)));
	expect(librdf_free_model, when(model, is_equal_to(model)));

	int r = spindle_rulebase_add_turtle(&rules, buffer, __FILE__);
	assert_that(r, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_rulebase_add_file

Ensure(spindle_common_rulebase, add_file_fails_if_twine_rdf_model_create_fails) {
	SPINDLERULES rules = { 0 };

	expect(twine_rdf_model_create, will_return(NULL));

	int r = spindle_rulebase_add_file(&rules, __FILE__);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_common_rulebase, add_file_loads_a_ruleset_from_passed_file) {
	char *path = strdup(__FILE__);
	struct coref_match_struct match_types = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_stream *stream = (librdf_stream *) 0xA02;

	expect(twine_rdf_model_create, will_return(model));
	expect(twine_rdf_model_parse, when(model, is_equal_to(model)), when(mime, is_equal_to_string(MIME_TURTLE)));
	expect(librdf_model_as_stream, will_return(stream), when(model, is_equal_to(model)));
	expect(librdf_stream_end, will_return(1));
	expect(librdf_free_stream);
	expect(librdf_free_model, when(model, is_equal_to(model)));

	SPINDLERULES rules = { 0 };
	int r = spindle_rulebase_add_file(&rules, __FILE__);
	assert_that(r, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_rulebase_add_config

Ensure(spindle_common_rulebase, add_config_fails_if_rulebase_file_cannot_be_opened) {
	char *path = strdup("fichier n'existe pas");

	expect(twine_config_geta, will_return(path));

	SPINDLERULES rules = { 0 };
	int r = spindle_rulebase_add_config(&rules);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_common_rulebase, add_config_loads_ruleset_from_configured_path) {
	char *path = strdup(__FILE__);
	struct coref_match_struct match_types = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_stream *stream = (librdf_stream *) 0xA02;

	expect(twine_config_geta, will_return(path), when(key, is_equal_to_string("spindle:rulebase")));
	expect(twine_rdf_model_create, will_return(model));
	expect(twine_rdf_model_parse, when(model, is_equal_to(model)), when(mime, is_equal_to_string(MIME_TURTLE)));
	expect(librdf_model_as_stream, will_return(stream), when(model, is_equal_to(model)));
	expect(librdf_stream_end, will_return(1));
	expect(librdf_free_stream);
	expect(librdf_free_model, when(model, is_equal_to(model)));

	SPINDLERULES rules = { 0 };
	int r = spindle_rulebase_add_config(&rules);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, add_config_loads_ruleset_from_default_path_if_none_configured) {
	char *path = strdup(__FILE__);
	struct coref_match_struct match_types = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_stream *stream = (librdf_stream *) 0xA02;

	expect(twine_config_geta, will_return(path), when(defval, is_equal_to_string(TWINEMODULEDIR "/rulebase.ttl")));
	expect(twine_rdf_model_create, will_return(model));
	expect(twine_rdf_model_parse, when(model, is_equal_to(model)), when(mime, is_equal_to_string(MIME_TURTLE)));
	expect(librdf_model_as_stream, will_return(stream), when(model, is_equal_to(model)));
	expect(librdf_stream_end, will_return(1));
	expect(librdf_free_stream);
	expect(librdf_free_model, when(model, is_equal_to(model)));

	SPINDLERULES rules = { 0 };
	int r = spindle_rulebase_add_config(&rules);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, add_config_loads_rules) {
	char *path = strdup(__FILE__);
	struct coref_match_struct match_types = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_stream *stream = (librdf_stream *) 0xA02;

	expect(twine_config_geta, will_return(path));
	always_expect(twine_rdf_model_create, will_return(model));
	always_expect(twine_rdf_model_parse);
	always_expect(librdf_model_as_stream, will_return(stream));
	expect(librdf_stream_end, will_return(0));
		expect(librdf_stream_get_object, when(stream, is_equal_to(stream)));
		expect(librdf_statement_get_subject);
		expect(librdf_statement_get_predicate);
		expect(librdf_statement_get_object);
		expect(librdf_node_is_resource, will_return(0));
		expect(librdf_stream_next);
	expect(librdf_stream_end, will_return(0));
		expect(librdf_stream_get_object);
		expect(librdf_statement_get_subject);
		expect(librdf_statement_get_predicate);
		expect(librdf_statement_get_object);
		expect(librdf_node_is_resource, will_return(1));
		expect(librdf_node_is_resource, will_return(0));
		expect(librdf_stream_next);
	expect(librdf_stream_end, will_return(0));
		expect(librdf_stream_get_object);
		expect(librdf_statement_get_subject);
		expect(librdf_statement_get_predicate);
		expect(librdf_statement_get_object);
		expect(librdf_node_is_resource, will_return(1));
		expect(librdf_node_is_resource, will_return(1));
		expect(librdf_node_get_uri);
		expect(librdf_uri_as_string, will_return("subject"));
		expect(librdf_node_get_uri);
		expect(librdf_uri_as_string, will_return("predicate"));
		expect(librdf_stream_next);
	expect(librdf_stream_end, will_return(1));
	always_expect(librdf_free_stream);
	always_expect(librdf_free_model);

	SPINDLERULES rules = { 0 };
	int r = spindle_rulebase_add_config(&rules);
	assert_that(r, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_rulebase_finalise

Ensure(spindle_common_rulebase, finalise_calls_subordinate_finalise_procedures) {
	SPINDLERULES rules = { 0 };

	expect(spindle_rulebase_class_finalise, when(rules, is_equal_to(&rules)));
	expect(spindle_rulebase_pred_finalise, when(rules, is_equal_to(&rules)));
	expect(spindle_rulebase_cachepred_finalise, when(rules, is_equal_to(&rules)));

	int r = spindle_rulebase_finalise(&rules);
	assert_that(r, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_rulebase_destroy

Ensure(spindle_common_rulebase, destroy_cleans_up) {
	SPINDLERULES *rules = malloc(sizeof (SPINDLERULES));

	expect(spindle_rulebase_class_cleanup, when(rules, is_equal_to(rules)));
	expect(spindle_rulebase_pred_cleanup, when(rules, is_equal_to(rules)));
	expect(spindle_rulebase_cachepred_cleanup, when(rules, is_equal_to(rules)));

	int r = spindle_rulebase_destroy(rules);
	assert_that(r, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_rulebase_dump

Ensure(spindle_common_rulebase, dump_dumps_contents) {
	SPINDLERULES rules = { 0 };

	expect(spindle_rulebase_cachepred_dump, when(rules, is_equal_to(&rules)));
	expect(spindle_rulebase_class_dump, when(rules, is_equal_to(&rules)));
	expect(spindle_rulebase_pred_dump, when(rules, is_equal_to(&rules)));
	expect(spindle_rulebase_coref_dump, when(rules, is_equal_to(&rules)));

	int r = spindle_rulebase_dump(&rules);
	assert_that(r, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_rulebase_loadfile_

Ensure(spindle_common_rulebase, loadfile_fails_if_file_does_not_exist) {
	char *buffer = spindle_rulebase_loadfile_("fichier n'existe pas");
	assert_that(buffer, is_null);
}

Ensure(spindle_common_rulebase, loadfile_reads_the_text_file_at_the_given_path_into_a_new_string_buffer) {
	char *buffer = spindle_rulebase_loadfile_(__FILE__);
	assert_that(buffer, is_non_null);
	assert_that(strlen(buffer), is_greater_than(0));
}

#pragma mark -
#pragma mark spindle_rulebase_add_statement_

Ensure(spindle_common_rulebase, add_statement_returns_no_error_if_subject_of_statement_is_not_a_resource) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xB01;

	expect(librdf_statement_get_subject, will_return(subject), when(statement, is_equal_to(statement)));
	expect(librdf_statement_get_predicate);
	expect(librdf_statement_get_object);
	expect(librdf_node_is_resource, will_return(0), when(node, is_equal_to(subject)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, statement);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, add_statement_returns_no_error_if_predicate_of_statement_is_not_a_resource) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_statement *statement = (librdf_statement *) 0xB01;

	expect(librdf_statement_get_subject, will_return(subject), when(statement, is_equal_to(statement)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(statement)));
	expect(librdf_statement_get_object);
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(subject)));
	expect(librdf_node_is_resource, will_return(0), when(node, is_equal_to(predicate)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, statement);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, add_statement_returns_no_error_if_predicate_is_unknown) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_uri *subject_uri = (librdf_uri *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;

	expect(librdf_statement_get_subject, will_return(subject));
	expect(librdf_statement_get_predicate, will_return(predicate));
	expect(librdf_statement_get_object);
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(subject)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(subject_uri), when(node, is_equal_to(subject)));
	expect(librdf_uri_as_string, will_return("subject"), when(uri, is_equal_to(subject_uri)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return("predicate"), when(uri, is_equal_to(predicate_uri)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, NULL);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, add_statement_returns_no_error_if_object_of_statement_is_not_a_resource) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_node *object = (librdf_node *) 0xA03;
	librdf_uri *subject_uri = (librdf_uri *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	librdf_uri *object_uri = (librdf_uri *) 0xA06;

	expect(librdf_statement_get_subject, will_return(subject));
	expect(librdf_statement_get_predicate, will_return(predicate));
	expect(librdf_statement_get_object, will_return(object));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(subject)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(subject_uri), when(node, is_equal_to(subject)));
	expect(librdf_uri_as_string, will_return("subject"), when(uri, is_equal_to(subject_uri)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(NS_RDF "type"), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_resource, will_return(0), when(node, is_equal_to(object)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, NULL);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, add_statement_returns_no_error_if_predicate_is_rdfs_type_but_object_is_neither_spindle_class_nor_spindle_property) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_node *object = (librdf_node *) 0xA03;
	librdf_uri *subject_uri = (librdf_uri *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	librdf_uri *object_uri = (librdf_uri *) 0xA06;

	expect(librdf_statement_get_subject, will_return(subject));
	expect(librdf_statement_get_predicate, will_return(predicate));
	expect(librdf_statement_get_object, will_return(object));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(subject)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(subject_uri), when(node, is_equal_to(subject)));
	expect(librdf_uri_as_string, will_return("subject"), when(uri, is_equal_to(subject_uri)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(NS_RDF "type"), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(object)));
	expect(librdf_node_get_uri, will_return(object_uri), when(node, is_equal_to(object)));
	expect(librdf_uri_as_string, will_return("object"), when(uri, is_equal_to(object_uri)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, NULL);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, add_statement_returns_success_if_predicate_is_rdfs_type_and_object_is_spindle_class) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_node *object = (librdf_node *) 0xA03;
	librdf_uri *subject_uri = (librdf_uri *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	librdf_uri *object_uri = (librdf_uri *) 0xA06;
	char *subject_str = "subject";
	char *predicate_str = NS_RDF "type";
	char *object_str = NS_SPINDLE "Class";

	expect(librdf_statement_get_subject, will_return(subject));
	expect(librdf_statement_get_predicate, will_return(predicate));
	expect(librdf_statement_get_object, will_return(object));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(subject)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(subject_uri), when(node, is_equal_to(subject)));
	expect(librdf_uri_as_string, will_return(subject_str), when(uri, is_equal_to(subject_uri)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(predicate_str), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(object)));
	expect(librdf_node_get_uri, will_return(object_uri), when(node, is_equal_to(object)));
	expect(librdf_uri_as_string, will_return(object_str), when(uri, is_equal_to(object_uri)));
	expect(spindle_rulebase_class_add_node, when(uri, is_equal_to(subject_str)), when(node, is_equal_to(subject)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, NULL);
	assert_that(r, is_equal_to(1));
}

Ensure(spindle_common_rulebase, add_statement_returns_error_if_predicate_is_rdfs_type_and_object_is_spindle_class_but_add_node_fails) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_node *object = (librdf_node *) 0xA03;
	librdf_uri *subject_uri = (librdf_uri *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	librdf_uri *object_uri = (librdf_uri *) 0xA06;
	char *subject_str = "subject";
	char *predicate_str = NS_RDF "type";
	char *object_str = NS_SPINDLE "Class";

	expect(librdf_statement_get_subject, will_return(subject));
	expect(librdf_statement_get_predicate, will_return(predicate));
	expect(librdf_statement_get_object, will_return(object));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(subject)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(subject_uri), when(node, is_equal_to(subject)));
	expect(librdf_uri_as_string, will_return(subject_str), when(uri, is_equal_to(subject_uri)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(predicate_str), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(object)));
	expect(librdf_node_get_uri, will_return(object_uri), when(node, is_equal_to(object)));
	expect(librdf_uri_as_string, will_return(object_str), when(uri, is_equal_to(object_uri)));
	expect(spindle_rulebase_class_add_node, will_return(-1), when(uri, is_equal_to(subject_str)), when(node, is_equal_to(subject)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, NULL);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_common_rulebase, add_statement_returns_success_if_predicate_is_rdfs_type_and_object_is_spindle_property) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_node *object = (librdf_node *) 0xA03;
	librdf_uri *subject_uri = (librdf_uri *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	librdf_uri *object_uri = (librdf_uri *) 0xA06;
	char *subject_str = "subject";
	char *predicate_str = NS_RDF "type";
	char *object_str = NS_SPINDLE "Property";

	expect(librdf_statement_get_subject, will_return(subject));
	expect(librdf_statement_get_predicate, will_return(predicate));
	expect(librdf_statement_get_object, will_return(object));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(subject)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(subject_uri), when(node, is_equal_to(subject)));
	expect(librdf_uri_as_string, will_return(subject_str), when(uri, is_equal_to(subject_uri)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(predicate_str), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(object)));
	expect(librdf_node_get_uri, will_return(object_uri), when(node, is_equal_to(object)));
	expect(librdf_uri_as_string, will_return(object_str), when(uri, is_equal_to(object_uri)));
	expect(spindle_rulebase_pred_add_node, when(uri, is_equal_to(subject_str)), when(node, is_equal_to(subject)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, NULL);
	assert_that(r, is_equal_to(1));
}

Ensure(spindle_common_rulebase, add_statement_returns_error_if_predicate_is_rdfs_type_and_object_is_spindle_property_but_add_node_fails) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_node *object = (librdf_node *) 0xA03;
	librdf_uri *subject_uri = (librdf_uri *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	librdf_uri *object_uri = (librdf_uri *) 0xA06;
	char *subject_str = "subject";
	char *predicate_str = NS_RDF "type";
	char *object_str = NS_SPINDLE "Property";

	expect(librdf_statement_get_subject, will_return(subject));
	expect(librdf_statement_get_predicate, will_return(predicate));
	expect(librdf_statement_get_object, will_return(object));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(subject)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(subject_uri), when(node, is_equal_to(subject)));
	expect(librdf_uri_as_string, will_return(subject_str), when(uri, is_equal_to(subject_uri)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(predicate_str), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(object)));
	expect(librdf_node_get_uri, will_return(object_uri), when(node, is_equal_to(object)));
	expect(librdf_uri_as_string, will_return(object_str), when(uri, is_equal_to(object_uri)));
	expect(spindle_rulebase_pred_add_node, will_return(-1), when(uri, is_equal_to(subject_str)), when(node, is_equal_to(subject)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, NULL);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_common_rulebase, add_statement_returns_no_error_if_predicate_is_spindle_expressedas_but_object_is_not_a_resource) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_node *object = (librdf_node *) 0xA03;
	librdf_uri *subject_uri = (librdf_uri *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	char *subject_str = "subject";
	char *predicate_str = NS_SPINDLE "expressedAs";

	expect(librdf_statement_get_subject, will_return(subject));
	expect(librdf_statement_get_predicate, will_return(predicate));
	expect(librdf_statement_get_object, will_return(object));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(subject)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(subject_uri), when(node, is_equal_to(subject)));
	expect(librdf_uri_as_string, will_return(subject_str), when(uri, is_equal_to(subject_uri)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(predicate_str), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_resource, will_return(0), when(node, is_equal_to(object)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, NULL);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, add_statement_returns_error_if_predicate_is_spindle_expressedas_and_object_is_a_resource_and_the_result_of_class_add_matchnode_is_an_error) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_node *object = (librdf_node *) 0xA03;
	librdf_uri *subject_uri = (librdf_uri *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	char *subject_str = "subject";
	char *predicate_str = NS_SPINDLE "expressedAs";

	expect(librdf_statement_get_subject, will_return(subject));
	expect(librdf_statement_get_predicate, will_return(predicate));
	expect(librdf_statement_get_object, will_return(object));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(subject)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(subject_uri), when(node, is_equal_to(subject)));
	expect(librdf_uri_as_string, will_return(subject_str), when(uri, is_equal_to(subject_uri)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(predicate_str), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(object)));
	expect(spindle_rulebase_class_add_matchnode, will_return(-1), when(matchuri, is_equal_to(subject_str)), when(node, is_equal_to(object)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, NULL);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_common_rulebase, add_statement_returns_success_if_predicate_is_spindle_expressedas_and_object_is_a_resource_and_the_result_of_class_add_matchnode_is_not_an_error) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_node *object = (librdf_node *) 0xA03;
	librdf_uri *subject_uri = (librdf_uri *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	char *subject_str = "subject";
	char *predicate_str = NS_SPINDLE "expressedAs";

	expect(librdf_statement_get_subject, will_return(subject));
	expect(librdf_statement_get_predicate, will_return(predicate));
	expect(librdf_statement_get_object, will_return(object));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(subject)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(subject_uri), when(node, is_equal_to(subject)));
	expect(librdf_uri_as_string, will_return(subject_str), when(uri, is_equal_to(subject_uri)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(predicate_str), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(object)));
	expect(spindle_rulebase_class_add_matchnode, will_return(0), when(matchuri, is_equal_to(subject_str)), when(node, is_equal_to(object)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, NULL);
	assert_that(r, is_equal_to(1));
}

Ensure(spindle_common_rulebase, add_statement_returns_no_error_if_predicate_is_spindle_property_but_object_is_a_literal) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_node *object = (librdf_node *) 0xA03;
	librdf_uri *subject_uri = (librdf_uri *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	char *subject_str = "subject";
	char *predicate_str = NS_SPINDLE "property";

	expect(librdf_statement_get_subject, will_return(subject));
	expect(librdf_statement_get_predicate, will_return(predicate));
	expect(librdf_statement_get_object, will_return(object));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(subject)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(subject_uri), when(node, is_equal_to(subject)));
	expect(librdf_uri_as_string, will_return(subject_str), when(uri, is_equal_to(subject_uri)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(predicate_str), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(object)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, NULL);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, add_statement_returns_the_result_of_pred_add_matchnode_if_predicate_is_spindle_property_and_object_is_not_a_literal) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_node *object = (librdf_node *) 0xA03;
	librdf_uri *subject_uri = (librdf_uri *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	char *subject_str = "subject";
	char *predicate_str = NS_SPINDLE "property";
	int result_of_pred_add_matchnode = 999;

	expect(librdf_statement_get_subject, will_return(subject));
	expect(librdf_statement_get_predicate, will_return(predicate));
	expect(librdf_statement_get_object, will_return(object));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(subject)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(subject_uri), when(node, is_equal_to(subject)));
	expect(librdf_uri_as_string, will_return(subject_str), when(uri, is_equal_to(subject_uri)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(predicate_str), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_literal, will_return(0), when(node, is_equal_to(object)));
	expect(spindle_rulebase_pred_add_matchnode, will_return(result_of_pred_add_matchnode), when(matchuri, is_equal_to(subject_str)), when(matchnode, is_equal_to(object)), when(inverse, is_equal_to(0)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, NULL);
	assert_that(r, is_equal_to(result_of_pred_add_matchnode));
}

Ensure(spindle_common_rulebase, add_statement_returns_no_error_if_predicate_is_spindle_inverseproperty_but_object_is_a_literal) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_node *object = (librdf_node *) 0xA03;
	librdf_uri *subject_uri = (librdf_uri *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	char *subject_str = "subject";
	char *predicate_str = NS_SPINDLE "inverseProperty";

	expect(librdf_statement_get_subject, will_return(subject));
	expect(librdf_statement_get_predicate, will_return(predicate));
	expect(librdf_statement_get_object, will_return(object));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(subject)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(subject_uri), when(node, is_equal_to(subject)));
	expect(librdf_uri_as_string, will_return(subject_str), when(uri, is_equal_to(subject_uri)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(predicate_str), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(object)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, NULL);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, add_statement_returns_the_result_of_pred_add_matchnode_if_predicate_is_spindle_inverseproperty_and_object_is_not_a_literal) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_node *object = (librdf_node *) 0xA03;
	librdf_uri *subject_uri = (librdf_uri *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	char *subject_str = "subject";
	char *predicate_str = NS_SPINDLE "inverseProperty";
	int result_of_pred_add_matchnode = 999;

	expect(librdf_statement_get_subject, will_return(subject));
	expect(librdf_statement_get_predicate, will_return(predicate));
	expect(librdf_statement_get_object, will_return(object));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(subject)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(subject_uri), when(node, is_equal_to(subject)));
	expect(librdf_uri_as_string, will_return(subject_str), when(uri, is_equal_to(subject_uri)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(predicate_str), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_literal, will_return(0), when(node, is_equal_to(object)));
	expect(spindle_rulebase_pred_add_matchnode, will_return(result_of_pred_add_matchnode), when(matchuri, is_equal_to(subject_str)), when(matchnode, is_equal_to(object)), when(inverse, is_equal_to(1)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, NULL);
	assert_that(r, is_equal_to(result_of_pred_add_matchnode));
}

Ensure(spindle_common_rulebase, add_statement_returns_the_result_of_coref_add_node_if_predicate_is_spindle_coref) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_node *object = (librdf_node *) 0xA03;
	librdf_uri *subject_uri = (librdf_uri *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	char *subject_str = "subject";
	char *predicate_str = NS_SPINDLE "coref";
	int result_of_coref_add_node = 999;

	expect(librdf_statement_get_subject, will_return(subject));
	expect(librdf_statement_get_predicate, will_return(predicate));
	expect(librdf_statement_get_object, will_return(object));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(subject)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(subject_uri), when(node, is_equal_to(subject)));
	expect(librdf_uri_as_string, will_return(subject_str), when(uri, is_equal_to(subject_uri)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(predicate_str), when(uri, is_equal_to(predicate_uri)));
	expect(spindle_rulebase_coref_add_node, will_return(result_of_coref_add_node), when(candidate, is_equal_to(subject_str)), when(matchnode, is_equal_to(object)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, NULL);
	assert_that(r, is_equal_to(result_of_coref_add_node));
}

#pragma mark -

TestSuite *rulebase_test_suite(void) {
	TestSuite *suite = create_test_suite();
	add_test_with_context(suite, spindle_common_rulebase, create_succeeds_and_installs_required_cachepreds);
	add_test_with_context(suite, spindle_common_rulebase, set_matchtypes_succeeds);
	add_test_with_context(suite, spindle_common_rulebase, add_model_fails_if_stream_cannot_be_obtained_from_model);
	add_test_with_context(suite, spindle_common_rulebase, add_model_fails_if_add_statement_fails);
	add_test_with_context(suite, spindle_common_rulebase, add_model_returns_no_error_if_add_statement_succeeds);
	add_test_with_context(suite, spindle_common_rulebase, add_turtle_fails_if_twine_rdf_model_create_fails);
	add_test_with_context(suite, spindle_common_rulebase, add_turtle_fails_if_model_parse_fails);
	add_test_with_context(suite, spindle_common_rulebase, add_turtle_returns_the_result_of_add_model_if_the_buffer_parses);
	add_test_with_context(suite, spindle_common_rulebase, add_file_fails_if_twine_rdf_model_create_fails);
	add_test_with_context(suite, spindle_common_rulebase, add_file_loads_a_ruleset_from_passed_file);
	add_test_with_context(suite, spindle_common_rulebase, add_config_fails_if_rulebase_file_cannot_be_opened);
	add_test_with_context(suite, spindle_common_rulebase, add_config_loads_ruleset_from_configured_path);
	add_test_with_context(suite, spindle_common_rulebase, add_config_loads_ruleset_from_default_path_if_none_configured);
	add_test_with_context(suite, spindle_common_rulebase, add_config_loads_rules);
	add_test_with_context(suite, spindle_common_rulebase, finalise_calls_subordinate_finalise_procedures);
	add_test_with_context(suite, spindle_common_rulebase, destroy_cleans_up);
	add_test_with_context(suite, spindle_common_rulebase, dump_dumps_contents);
	add_test_with_context(suite, spindle_common_rulebase, loadfile_fails_if_file_does_not_exist);
	add_test_with_context(suite, spindle_common_rulebase, loadfile_reads_the_text_file_at_the_given_path_into_a_new_string_buffer);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_no_error_if_subject_of_statement_is_not_a_resource);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_no_error_if_predicate_of_statement_is_not_a_resource);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_no_error_if_predicate_is_unknown);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_no_error_if_object_of_statement_is_not_a_resource);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_no_error_if_predicate_is_rdfs_type_but_object_is_neither_spindle_class_nor_spindle_property);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_success_if_predicate_is_rdfs_type_and_object_is_spindle_class);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_error_if_predicate_is_rdfs_type_and_object_is_spindle_class_but_add_node_fails);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_success_if_predicate_is_rdfs_type_and_object_is_spindle_property);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_error_if_predicate_is_rdfs_type_and_object_is_spindle_property_but_add_node_fails);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_no_error_if_predicate_is_spindle_expressedas_but_object_is_not_a_resource);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_error_if_predicate_is_spindle_expressedas_and_object_is_a_resource_and_the_result_of_class_add_matchnode_is_an_error);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_success_if_predicate_is_spindle_expressedas_and_object_is_a_resource_and_the_result_of_class_add_matchnode_is_not_an_error);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_no_error_if_predicate_is_spindle_property_but_object_is_a_literal);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_the_result_of_pred_add_matchnode_if_predicate_is_spindle_property_and_object_is_not_a_literal);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_no_error_if_predicate_is_spindle_inverseproperty_but_object_is_a_literal);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_the_result_of_pred_add_matchnode_if_predicate_is_spindle_inverseproperty_and_object_is_not_a_literal);
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_the_result_of_coref_add_node_if_predicate_is_spindle_coref);
	return suite;
}

int run(char *test) {
	if(test) {
		return run_single_test(rulebase_test_suite(), test, create_text_reporter());
	} else {
		return run_test_suite(rulebase_test_suite(), create_text_reporter());
	}
}

int main(int argc, char **argv) {
	return run(argc > 1 ? argv[1] : NULL);
}
