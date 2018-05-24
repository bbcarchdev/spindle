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
#include "../../t/mock_spindle_rulebase.h"
#include "../../t/mock_spindle_rulebase_cachepred.h"
#include "../../t/mock_spindle_rulebase_class.h"
#include "../../t/mock_spindle_rulebase_pred.h"

#define P_SPINDLE_H_
#define PLUGIN_NAME "spindle-rulebase"
#include "../rulebase-coref.c"

Describe(spindle_common_rulebase);
BeforeEach(spindle_common_rulebase) { always_expect(twine_logf); die_in(1); }
AfterEach(spindle_common_rulebase) {}

/*
	This suite does not test any failure paths triggered by libc calls such as malloc.
*/

#pragma mark -
#pragma mark spindle_rulebase_coref_add_

Ensure(spindle_common_rulebase, coref_add_returns_one_and_adds_the_coref_to_the_coref_list) {
	char *predicate = "predicate";
	int (*callback)() = (int (*)()) 0xA01;
	struct coref_match_struct match = { .callback = callback };
	SPINDLERULES rules = { 0 };

	int r = spindle_rulebase_coref_add_(&rules, predicate, &match);
	assert_that(r, is_equal_to(1)); // added
	assert_that(rules.corefcount, is_equal_to(1));
	assert_that(rules.corefsize, is_greater_than(0));
	assert_that(rules.coref, is_non_null);
	assert_that(rules.coref[0].predicate, is_equal_to_string(predicate));
	assert_that(rules.coref[0].callback, is_equal_to(callback));
}

Ensure(spindle_common_rulebase, coref_add_returns_zero_and_replaces_the_callback_of_an_existing_coref) {
	char *predicate = "predicate";
	int (*callback_1)() = (int (*)()) 0xA01;
	int (*callback_2)() = (int (*)()) 0xA02;
	struct coref_match_struct match_1 = { .callback = callback_1 };
	struct coref_match_struct match_2 = { .callback = callback_2 };
	SPINDLERULES rules = { 0 };

	int r = spindle_rulebase_coref_add_(&rules, predicate, &match_1);
	assert_that(r, is_equal_to(1)); // added

	r = spindle_rulebase_coref_add_(&rules, predicate, &match_2);
	assert_that(r, is_equal_to(0)); // updated
	assert_that(rules.corefcount, is_equal_to(1));
	assert_that(rules.corefsize, is_greater_than(0));
	assert_that(rules.coref, is_non_null);
	assert_that(rules.coref[0].predicate, is_equal_to_string(predicate));
	assert_that(rules.coref[0].callback, is_equal_to(callback_2));
}

Ensure(spindle_common_rulebase, coref_add_can_add_multiple_corefs) {
	char *predicate_1 = "predicate 1";
	char *predicate_2 = "predicate 2";
	int (*callback_1)() = (int (*)()) 0xA01;
	int (*callback_2)() = (int (*)()) 0xA02;
	struct coref_match_struct match_1 = { .callback = callback_1 };
	struct coref_match_struct match_2 = { .callback = callback_2 };
	SPINDLERULES rules = { 0 };

	int r = spindle_rulebase_coref_add_(&rules, predicate_1, &match_1);
	assert_that(r, is_equal_to(1)); // added

	r = spindle_rulebase_coref_add_(&rules, predicate_2, &match_2);
	assert_that(r, is_equal_to(1)); // added
	assert_that(rules.corefcount, is_equal_to(2));
	assert_that(rules.corefsize, is_greater_than(1));
	assert_that(rules.coref, is_non_null);
	assert_that(rules.coref[0].predicate, is_equal_to_string(predicate_1));
	assert_that(rules.coref[0].callback, is_equal_to(callback_1));
	assert_that(rules.coref[1].predicate, is_equal_to_string(predicate_2));
	assert_that(rules.coref[1].callback, is_equal_to(callback_2));
}

Ensure(spindle_common_rulebase, coref_add_replaces_the_correct_callback_when_multiple_corefs_are_present) {
	char *predicate_1 = "predicate 1";
	char *predicate_2 = "predicate 2";
	int (*callback_1)() = (int (*)()) 0xA01;
	int (*callback_2)() = (int (*)()) 0xA02;
	int (*callback_3)() = (int (*)()) 0xA03;
	struct coref_match_struct match_1 = { .callback = callback_1 };
	struct coref_match_struct match_2 = { .callback = callback_2 };
	struct coref_match_struct match_3 = { .callback = callback_3 };
	SPINDLERULES rules = { 0 };

	int r = spindle_rulebase_coref_add_(&rules, predicate_1, &match_1);
	assert_that(r, is_equal_to(1)); // added

	r = spindle_rulebase_coref_add_(&rules, predicate_2, &match_2);
	assert_that(r, is_equal_to(1)); // added

	r = spindle_rulebase_coref_add_(&rules, predicate_2, &match_3);
	assert_that(r, is_equal_to(0)); // updated
	assert_that(rules.corefcount, is_equal_to(2));
	assert_that(rules.corefsize, is_greater_than(1));
	assert_that(rules.coref, is_non_null);
	assert_that(rules.coref[0].predicate, is_equal_to_string(predicate_1));
	assert_that(rules.coref[0].callback, is_equal_to(callback_1));
	assert_that(rules.coref[1].predicate, is_equal_to_string(predicate_2));
	assert_that(rules.coref[1].callback, is_equal_to(callback_3));
}

#pragma mark -
#pragma mark spindle_rulebase_coref_add_node

Ensure(spindle_common_rulebase, coref_add_node_marks_the_candidate_predicate_as_being_cacheable_if_there_are_no_match_types) {
	char *predicate = "predicate";
	librdf_node *node = NULL;
	SPINDLERULES rules = { 0 };

	expect(spindle_rulebase_cachepred_add, when(rules, is_equal_to(&rules)), when(uri, is_equal_to(predicate)));

	int r = spindle_rulebase_coref_add_node(&rules, predicate, node);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, coref_add_node_returns_no_error_and_does_not_mark_the_candidate_predicate_as_being_cacheable_if_the_node_is_not_a_resource) {
	char *predicate = "predicate";
	librdf_node *node = (librdf_node *) 0xA01;
	struct coref_match_struct match_types[] = {
		{ 0 }
	};
	SPINDLERULES rules = { .match_types = match_types };

	expect(librdf_node_is_resource, will_return(0), when(node, is_equal_to(node)));

	int r = spindle_rulebase_coref_add_node(&rules, predicate, node);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, coref_add_node_returns_no_error_and_does_not_mark_the_candidate_predicate_as_being_cacheable_if_the_match_type_is_unknown) {
	char *predicate = "predicate";
	struct coref_match_struct match_types[] = {
		{ 0 }
	};
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_uri *uri = (librdf_uri *) 0xB01;
	SPINDLERULES rules = { .match_types = match_types };

	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_uri, will_return(uri), when(node, is_equal_to(node)));
	expect(librdf_uri_as_string, will_return(predicate), when(uri, is_equal_to(uri)));

	int r = spindle_rulebase_coref_add_node(&rules, predicate, node);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, coref_add_node_marks_the_candidate_predicate_as_being_cacheable_if_the_match_type_is_known) {
	char *predicate_1 = "predicate 1";
	char *predicate_2 = "predicate 2";
	int (*callback_1)() = (int (*)()) 0xA01;
	int (*callback_2)() = (int (*)()) 0xA02;
	struct coref_match_struct match_types[] = {
		{ .predicate = predicate_1, .callback = callback_1 },
		{ .predicate = predicate_2, .callback = callback_2 },
		{ 0 }
	};
	librdf_node *node = (librdf_node *) 0xB01;
	librdf_uri *uri = (librdf_uri *) 0xB02;
	SPINDLERULES rules = { .match_types = match_types };

	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_uri, will_return(uri), when(node, is_equal_to(node)));
	expect(librdf_uri_as_string, will_return(predicate_2), when(uri, is_equal_to(uri)));
	expect(spindle_rulebase_cachepred_add, when(rules, is_equal_to(&rules)), when(uri, is_equal_to(predicate_2)));

	int r = spindle_rulebase_coref_add_node(&rules, predicate_2, node);
	assert_that(r, is_equal_to(1));
	assert_that(rules.corefcount, is_equal_to(1));
	assert_that(rules.corefsize, is_greater_than(0));
	assert_that(rules.coref, is_non_null);
	assert_that(rules.coref[0].predicate, is_equal_to_string(predicate_2));
	assert_that(rules.coref[0].callback, is_equal_to(callback_2));
}

#pragma mark -
#pragma mark spindle_rulebase_coref_add_node

Ensure(spindle_common_rulebase, coref_dump_only_has_side_effects) {
	char *predicate_1 = "predicate 1";
	char *predicate_2 = "predicate 2";
	struct coref_match_struct match_types[] = {
		{ .predicate = predicate_1 },
		{ .predicate = predicate_2 },
		{ 0 }
	};
	struct coref_match_struct corefs[] = {
		{ .predicate = predicate_1 },
		{ .predicate = predicate_2 }
	};
	SPINDLERULES rules = {
		.match_types = match_types,
		.coref = corefs,
		.corefcount = sizeof corefs / sizeof corefs[0]
	};

	int r = spindle_rulebase_coref_dump(&rules);
	assert_that(r, is_equal_to(0));
}

#pragma mark -

TestSuite *create_rulebase_coref_test_suite(void) {
	TestSuite *suite = create_test_suite();
	add_test_with_context(suite, spindle_common_rulebase, coref_add_returns_one_and_adds_the_coref_to_the_coref_list);
	add_test_with_context(suite, spindle_common_rulebase, coref_add_returns_zero_and_replaces_the_callback_of_an_existing_coref);
	add_test_with_context(suite, spindle_common_rulebase, coref_add_can_add_multiple_corefs);
	add_test_with_context(suite, spindle_common_rulebase, coref_add_replaces_the_correct_callback_when_multiple_corefs_are_present);
	add_test_with_context(suite, spindle_common_rulebase, coref_add_node_marks_the_candidate_predicate_as_being_cacheable_if_there_are_no_match_types);
	add_test_with_context(suite, spindle_common_rulebase, coref_add_node_returns_no_error_and_does_not_mark_the_candidate_predicate_as_being_cacheable_if_the_node_is_not_a_resource);
	add_test_with_context(suite, spindle_common_rulebase, coref_add_node_returns_no_error_and_does_not_mark_the_candidate_predicate_as_being_cacheable_if_the_match_type_is_unknown);
	add_test_with_context(suite, spindle_common_rulebase, coref_add_node_marks_the_candidate_predicate_as_being_cacheable_if_the_match_type_is_known);
	add_test_with_context(suite, spindle_common_rulebase, coref_dump_only_has_side_effects);
	return suite;
}

int rulebase_coref_test(char *test) {
	if(test) {
		return run_single_test(create_rulebase_coref_test_suite(), test, create_text_reporter());
	} else {
		return run_test_suite(create_rulebase_coref_test_suite(), create_text_reporter());
	}
}

int main(int argc, char **argv) {
	return rulebase_coref_test(argc > 1 ? argv[1] : NULL);
}
