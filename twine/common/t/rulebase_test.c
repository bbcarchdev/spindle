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

typedef enum {
	RAPTOR_TERM_TYPE_UNKNOWN = 0,
	RAPTOR_TERM_TYPE_URI = 1,
	RAPTOR_TERM_TYPE_LITERAL = 2,
	/* unused type 3 */
	RAPTOR_TERM_TYPE_BLANK = 4
} raptor_term_type;

/* mocks of dependancies */
#include "../../t/mock_librdf.h"
#include "../../t/mock_libsql.h"
#include "../../t/mock_libtwine.h"
#include "../../t/mock_spindle_core.h"
#include "../../t/mock_spindle_rulebase_cachepred.h"
#include "../../t/mock_spindle_rulebase_class.h"
#include "../../t/mock_spindle_rulebase_coref.h"
#include "../../t/mock_spindle_rulebase_pred.h"

#define TWINEMODULEDIR "TWINEMODULEDIR "
#define NS_OWL "NS_OWL "
#define NS_RDF "NS_RDF "
#define NS_SPINDLE "NS_SPINDLE "

#define MIME_TURTLE "MIME_TURTLE"

/* compile SUT inline due to static functions */
#define P_SPINDLE_H_
#define PLUGIN_NAME "spindle-rulebase"
int spindle_rulebase_destroy(SPINDLERULES *rules);
#include "../rulebase.c"

Describe(spindle_common_rulebase);
BeforeEach(spindle_common_rulebase) { cgreen_mocks_are(learning_mocks); always_expect(twine_logf); die_in(1); }
AfterEach(spindle_common_rulebase) {}

/*
	This suite does not test any failure paths triggered by libc calls such as malloc.
*/

#pragma mark -
#pragma mark spindle_rulebase_create

Ensure(spindle_common_rulebase, create_fails_if_twine_rdf_model_create_fails) {
	char *path = NULL;
	struct coref_match_struct match_types = { 0 };

	expect(twine_rdf_model_create, will_return(NULL));
	always_expect(spindle_rulebase_cachepred_add);

	SPINDLERULES *rules = spindle_rulebase_create(path, &match_types);
	assert_that(rules, is_null);
}

Ensure(spindle_common_rulebase, create_fails_if_rulebase_file_cannot_be_opened) {
	char *path = "fichier n'existe pas";
	struct coref_match_struct match_types = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;

	expect(twine_rdf_model_create, will_return(model));
	always_expect(spindle_rulebase_cachepred_add);
	always_expect(twine_rdf_model_destroy);
	always_expect(spindle_rulebase_class_cleanup);
	always_expect(spindle_rulebase_pred_cleanup);
	always_expect(spindle_rulebase_cachepred_cleanup);

	SPINDLERULES *rules = spindle_rulebase_create(path, &match_types);
	assert_that(rules, is_null);
}

Ensure(spindle_common_rulebase, create_loads_an_empty_ruleset_from_passed_file) {
	char *path = __FILE__;
	struct coref_match_struct match_types = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;

	expect(spindle_rulebase_cachepred_add);
	expect(spindle_rulebase_cachepred_add);
	expect(twine_rdf_model_create, will_return(model));
	expect(twine_rdf_model_parse, when(model, is_equal_to(model)), when(mime, is_equal_to_string(MIME_TURTLE)));
	expect(librdf_model_as_stream, when(model, is_equal_to(model)));
	expect(librdf_stream_end, will_return(1));
	expect(librdf_free_stream);
	expect(twine_rdf_model_destroy, when(model, is_equal_to(model)));
	expect(spindle_rulebase_class_finalise);
	expect(spindle_rulebase_pred_finalise);
	expect(spindle_rulebase_cachepred_finalise);

	SPINDLERULES *rules = spindle_rulebase_create(path, &match_types);
	assert_that(rules, is_non_null);
}

Ensure(spindle_common_rulebase, create_loads_an_empty_ruleset_from_configured_file_if_none_passed) {
	char *path = NULL, *default_path;
	struct coref_match_struct match_types = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;

	default_path = malloc(strlen(__FILE__) + 1);
	strcpy(default_path, __FILE__);

	expect(spindle_rulebase_cachepred_add);
	expect(spindle_rulebase_cachepred_add);
	expect(twine_rdf_model_create, will_return(model));
	expect(twine_config_geta, will_return(default_path), when(key, is_equal_to_string("spindle:rulebase")), when(defval, is_equal_to_string(TWINEMODULEDIR "/rulebase.ttl")));
	expect(twine_rdf_model_parse, when(model, is_equal_to(model)), when(mime, is_equal_to_string(MIME_TURTLE)));
	expect(librdf_model_as_stream, when(model, is_equal_to(model)));
	expect(librdf_stream_end, will_return(1));
	expect(librdf_free_stream);
	expect(twine_rdf_model_destroy, when(model, is_equal_to(model)));
	expect(spindle_rulebase_class_finalise);
	expect(spindle_rulebase_pred_finalise);
	expect(spindle_rulebase_cachepred_finalise);

	SPINDLERULES *rules = spindle_rulebase_create(path, &match_types);
	assert_that(rules, is_non_null);
}

Ensure(spindle_common_rulebase, create_loads_rules) {
	char *path = __FILE__;
	struct coref_match_struct match_types = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;

	always_expect(spindle_rulebase_cachepred_add);
	always_expect(twine_rdf_model_create, will_return(model));
	always_expect(twine_rdf_model_parse);
	always_expect(librdf_model_as_stream);
	expect(librdf_stream_end, will_return(0));
		expect(librdf_stream_get_object, when(stream, is_equal_to(0)));
		expect(librdf_statement_get_subject);
		expect(librdf_statement_get_predicate);
		expect(librdf_statement_get_object);
		expect(librdf_node_is_resource);
		expect(librdf_stream_next);
	expect(librdf_stream_end, will_return(0));
		expect(librdf_stream_get_object);
		expect(librdf_statement_get_subject);
		expect(librdf_statement_get_predicate);
		expect(librdf_statement_get_object);
		expect(librdf_node_is_resource, will_return(1));
		expect(librdf_node_is_resource);
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
	always_expect(twine_rdf_model_destroy);
	always_expect(spindle_rulebase_class_finalise);
	always_expect(spindle_rulebase_pred_finalise);
	always_expect(spindle_rulebase_cachepred_finalise);

	SPINDLERULES *rules = spindle_rulebase_create(path, &match_types);
	assert_that(rules, is_non_null);
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

Ensure(spindle_common_rulebase, add_statement_returns_the_result_of_class_add_matchnode_if_predicate_is_spindle_expressedas_and_object_is_a_resource) {
	librdf_node *subject = (librdf_node *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_node *object = (librdf_node *) 0xA03;
	librdf_uri *subject_uri = (librdf_uri *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	char *subject_str = "subject";
	char *predicate_str = NS_SPINDLE "expressedAs";
	int result_of_class_add_matchnode = 999;

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
	expect(spindle_rulebase_class_add_matchnode, will_return(result_of_class_add_matchnode), when(matchuri, is_equal_to(subject_str)), when(node, is_equal_to(object)));

	int r = spindle_rulebase_add_statement_(NULL, NULL, NULL);
	assert_that(r, is_equal_to(result_of_class_add_matchnode));
}

#pragma mark -

TestSuite *create_rulebase_test_suite(void) {
	TestSuite *suite = create_test_suite();
	add_test_with_context(suite, spindle_common_rulebase, create_fails_if_twine_rdf_model_create_fails);
	add_test_with_context(suite, spindle_common_rulebase, create_fails_if_rulebase_file_cannot_be_opened);
	add_test_with_context(suite, spindle_common_rulebase, create_loads_an_empty_ruleset_from_passed_file);
	add_test_with_context(suite, spindle_common_rulebase, create_loads_an_empty_ruleset_from_configured_file_if_none_passed);
	add_test_with_context(suite, spindle_common_rulebase, create_loads_rules);
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
	add_test_with_context(suite, spindle_common_rulebase, add_statement_returns_the_result_of_class_add_matchnode_if_predicate_is_spindle_expressedas_and_object_is_a_resource);
	return suite;
}

int rulebase_test(char *test) {
	if(test) {
		return run_single_test(create_rulebase_test_suite(), test, create_text_reporter());
	} else {
		return run_test_suite(create_rulebase_test_suite(), create_text_reporter());
	}
}

int main(int argc, char **argv) {
	return rulebase_test(argc > 1 ? argv[1] : NULL);
}
