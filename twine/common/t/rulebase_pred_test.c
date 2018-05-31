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
#include "../../t/mock_spindle_rulebase_coref.h"

#define NS_OLO "NS_OLO "
#define NS_RDFS "NS_RDFS "
#define NS_SPINDLE "NS_SPINDLE "
#define NS_XSD "NS_XSD "

#define P_SPINDLE_H_
#define PLUGIN_NAME "spindle-rulebase"
#include "../rulebase-pred.c"

Describe(spindle_common_rulebase);
BeforeEach(spindle_common_rulebase) { cgreen_mocks_are(learning_mocks); always_expect(twine_logf); die_in(1); }
AfterEach(spindle_common_rulebase) {}

/*
	This suite does not test any failure paths triggered by libc calls such as malloc.
*/

#pragma mark -
#pragma mark spindle_rulebase_pred_add_match_

Ensure(spindle_common_rulebase, pred_add_match_returns_success_and_adds_the_predicate_to_the_match_list) {
	struct spindle_predicatemap_struct map = { 0 };
	const char *match_uri = "match uri";
	int score = 222;
	int prominence = 333;
	int inverse = 444;

	int r = spindle_rulebase_pred_add_match_(&map, match_uri, NULL, score, prominence, inverse);
	assert_that(r, is_equal_to(1));
	assert_that(map.matchsize, is_greater_than(0));
	assert_that(map.matchcount, is_equal_to(1));
	assert_that(map.matches, is_non_null);
	assert_that(map.matches[0].predicate, is_equal_to_string(match_uri));
	assert_that(map.matches[0].onlyfor, is_null);
	assert_that(map.matches[0].priority, is_equal_to(score));
	assert_that(map.matches[0].prominence, is_equal_to(prominence));
	assert_that(map.matches[0].inverse, is_equal_to(inverse));
}

Ensure(spindle_common_rulebase, pred_add_match_returns_success_and_adds_the_predicate_to_the_match_list_when_class_restriction_is_not_null) {
	struct spindle_predicatemap_struct map = { 0 };
	const char *match_uri = "match uri";
	const char *class_uri = "class uri";
	int score = 222;
	int prominence = 333;
	int inverse = 444;

	int r = spindle_rulebase_pred_add_match_(&map, match_uri, class_uri, score, prominence, inverse);
	assert_that(r, is_equal_to(1));
	assert_that(map.matchsize, is_greater_than(0));
	assert_that(map.matchcount, is_equal_to(1));
	assert_that(map.matches, is_non_null);
	assert_that(map.matches[0].predicate, is_equal_to_string(match_uri));
	assert_that(map.matches[0].onlyfor, is_equal_to_string(class_uri));
	assert_that(map.matches[0].priority, is_equal_to(score));
	assert_that(map.matches[0].prominence, is_equal_to(prominence));
	assert_that(map.matches[0].inverse, is_equal_to(inverse));
}

Ensure(spindle_common_rulebase, pred_add_match_can_add_multiple_different_predicates_to_the_match_list) {
	struct spindle_predicatemap_struct map = { 0 };
	const char *match_uri_1 = "match uri 1";
	const char *match_uri_2 = "match uri 2";
	const char *match_uri_3 = "match uri 3";
	int score_1 = 201, score_2 = 202, score_3 = 203;
	int prominence_1 = 301, prominence_2 = 302, prominence_3 = 303;
	int inverse_1 = 401, inverse_2 = 402, inverse_3 = 403;

	int r = spindle_rulebase_pred_add_match_(&map, match_uri_1, NULL, score_1, prominence_1, inverse_1);
	assert_that(r, is_equal_to(1));
	r = spindle_rulebase_pred_add_match_(&map, match_uri_2, NULL, score_2, prominence_2, inverse_2);
	assert_that(r, is_equal_to(1));
	r = spindle_rulebase_pred_add_match_(&map, match_uri_3, NULL, score_3, prominence_3, inverse_3);
	assert_that(r, is_equal_to(1));
	assert_that(map.matchsize, is_greater_than(2));
	assert_that(map.matchcount, is_equal_to(3));
	assert_that(map.matches, is_non_null);
	assert_that(map.matches[0].predicate, is_equal_to_string(match_uri_1));
	assert_that(map.matches[0].onlyfor, is_null);
	assert_that(map.matches[0].priority, is_equal_to(score_1));
	assert_that(map.matches[0].prominence, is_equal_to(prominence_1));
	assert_that(map.matches[0].inverse, is_equal_to(inverse_1));
	assert_that(map.matches[1].predicate, is_equal_to_string(match_uri_2));
	assert_that(map.matches[1].onlyfor, is_null);
	assert_that(map.matches[1].priority, is_equal_to(score_2));
	assert_that(map.matches[1].prominence, is_equal_to(prominence_2));
	assert_that(map.matches[1].inverse, is_equal_to(inverse_2));
	assert_that(map.matches[2].predicate, is_equal_to_string(match_uri_3));
	assert_that(map.matches[2].onlyfor, is_null);
	assert_that(map.matches[2].priority, is_equal_to(score_3));
	assert_that(map.matches[2].prominence, is_equal_to(prominence_3));
	assert_that(map.matches[2].inverse, is_equal_to(inverse_3));
}

Ensure(spindle_common_rulebase, pred_add_match_returns_success_and_overwrites_the_priority_and_prominance_when_the_predicate_is_already_in_the_match_list) {
	struct spindle_predicatemap_struct map = { 0 };
	const char *match_uri = "match uri";
	int score_1 = 201, score_2 = 202;
	int prominence_1 = 301, prominence_2 = 302;
	int inverse = 444;

	int r = spindle_rulebase_pred_add_match_(&map, match_uri, NULL, score_1, prominence_1, inverse);
	assert_that(r, is_equal_to(1));
	r = spindle_rulebase_pred_add_match_(&map, match_uri, NULL, score_2, prominence_2, inverse);
	assert_that(r, is_equal_to(1));
	assert_that(map.matchsize, is_greater_than(0));
	assert_that(map.matchcount, is_equal_to(1));
	assert_that(map.matches, is_non_null);
	assert_that(map.matches[0].predicate, is_equal_to_string(match_uri));
	assert_that(map.matches[0].onlyfor, is_null);
	assert_that(map.matches[0].priority, is_equal_to(score_2));
	assert_that(map.matches[0].prominence, is_equal_to(prominence_2));
	assert_that(map.matches[0].inverse, is_equal_to(inverse));
}

Ensure(spindle_common_rulebase, pred_add_match_returns_success_and_does_not_overwrite_an_existing_entry_when_the_predicate_is_already_in_the_match_list_but_the_inverse_differs) {
	struct spindle_predicatemap_struct map = { 0 };
	const char *match_uri = "match uri";
	int score = 222;
	int prominence = 333;
	int inverse_true = 1, inverse_false = 0;

	spindle_rulebase_pred_add_match_(&map, match_uri, NULL, score, prominence, inverse_true);
	spindle_rulebase_pred_add_match_(&map, match_uri, NULL, score, prominence, inverse_false);
	assert_that(map.matchcount, is_equal_to(2));
}

Ensure(spindle_common_rulebase, pred_add_match_returns_success_and_does_not_overwrite_an_existing_entry_when_the_predicate_is_already_in_the_match_list_but_the_classmatch_is_empty) {
	struct spindle_predicatemap_struct map = { 0 };
	const char *match_uri = "match uri";
	const char *class_uri = "class uri";
	int score = 222;
	int prominence = 333;
	int inverse = 444;

	spindle_rulebase_pred_add_match_(&map, match_uri, NULL, score, prominence, inverse);
	spindle_rulebase_pred_add_match_(&map, match_uri, class_uri, score, prominence, inverse);
	assert_that(map.matchcount, is_equal_to(2));
}

Ensure(spindle_common_rulebase, pred_add_match_returns_success_and_does_not_overwrite_an_existing_entry_when_the_predicate_is_already_in_the_match_list_but_the_classmatch_differs) {
	struct spindle_predicatemap_struct map = { 0 };
	const char *match_uri = "match uri";
	const char *class_uri_1 = "class uri 1";
	const char *class_uri_2 = "class uri 2";
	int score = 222;
	int prominence = 333;
	int inverse = 444;

	spindle_rulebase_pred_add_match_(&map, match_uri, class_uri_1, score, prominence, inverse);
	spindle_rulebase_pred_add_match_(&map, match_uri, class_uri_2, score, prominence, inverse);
	assert_that(map.matchcount, is_equal_to(2));
}

#pragma mark -
#pragma mark spindle_rulebase_pred_add_

Ensure(spindle_common_rulebase, pred_add_adds_the_predicate_to_the_match_list_and_returns_a_pointer_to_the_list_entry) {
	SPINDLERULES rules = { 0 };
	const char *predicate_uri = "predicate uri";

	expect(spindle_rulebase_cachepred_add, will_return(0), when(rules, is_equal_to(&rules)), when(uri, is_equal_to(predicate_uri)));

	struct spindle_predicatemap_struct *p = spindle_rulebase_pred_add_(&rules, predicate_uri);
	assert_that(rules.predsize, is_greater_than(0));
	assert_that(rules.predcount, is_equal_to(1));
	assert_that(rules.predicates, is_non_null);
	assert_that(rules.predicates[0].target, is_equal_to_string(predicate_uri));
	assert_that(rules.predicates[0].expected, is_equal_to(RAPTOR_TERM_TYPE_UNKNOWN));
	assert_that(rules.predicates[0].proxyonly, is_equal_to(0));
	assert_that(rules.predicates[0].score, is_equal_to(100));
	assert_that(p, is_equal_to(&(rules.predicates[0])));
}

Ensure(spindle_common_rulebase, pred_add_adds_returns_a_pointer_to_an_existing_list_entry_and_does_not_add_a_new_entry_if_adding_a_duplicate_predicate) {
	SPINDLERULES rules = { 0 };
	const char *predicate_uri_1 = "predicate uri 1";
	const char *predicate_uri_2 = "predicate uri 2";
	struct spindle_predicatemap_struct *p, *q;

	always_expect(spindle_rulebase_cachepred_add, will_return(0));

	spindle_rulebase_pred_add_(&rules, predicate_uri_1);
	p = spindle_rulebase_pred_add_(&rules, predicate_uri_2);
	q = spindle_rulebase_pred_add_(&rules, predicate_uri_2);
	assert_that(rules.predcount, is_equal_to(2));
	assert_that(q, is_equal_to(p));
}

Ensure(spindle_common_rulebase, pred_add_adds_returns_null_and_does_not_add_a_new_entry_if_adding_the_predicate_to_the_list_of_cached_predicates_fails) {
	SPINDLERULES rules = { 0 };
	const char *predicate_uri = "predicate uri";

	expect(spindle_rulebase_cachepred_add, will_return(-1));

	struct spindle_predicatemap_struct *p = spindle_rulebase_pred_add_(&rules, predicate_uri);
	assert_that(p, is_null);
	assert_that(rules.predcount, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_rulebase_pred_set_inverse_

Ensure(spindle_common_rulebase, pred_set_inverse_returns_false_if_the_statement_object_is_not_a_literal) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, will_return(0), when(node, is_equal_to(node)));

	int r = spindle_rulebase_pred_set_inverse_(&map, statement);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, pred_set_inverse_returns_false_if_the_statement_object_has_no_datatype_uri) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_uri *type = (librdf_uri *) 0xA03;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(0), when(node, is_equal_to(node)));

	int r = spindle_rulebase_pred_set_inverse_(&map, statement);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, pred_set_inverse_returns_false_if_the_statement_object_datatype_uri_is_not_xsd_boolean) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_uri *type = (librdf_uri *) 0xA03;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(type), when(node, is_equal_to(node)));
	expect(librdf_uri_as_string, will_return(NS_XSD "integer"), when(uri, is_equal_to(type)));

	int r = spindle_rulebase_pred_set_inverse_(&map, statement);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, pred_set_inverse_returns_true_and_sets_the_inverse_to_true_if_the_statement_object_contains_true) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_uri *type = (librdf_uri *) 0xA03;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(type), when(node, is_equal_to(node)));
	expect(librdf_uri_as_string, will_return(NS_XSD "boolean"), when(uri, is_equal_to(type)));
	expect(librdf_node_get_literal_value, will_return("true"), when(node, is_equal_to(node)));

	int r = spindle_rulebase_pred_set_inverse_(&map, statement);
	assert_that(r, is_equal_to(1));
	assert_that(map.inverse, is_equal_to(1));
}

Ensure(spindle_common_rulebase, pred_set_inverse_returns_true_and_sets_the_inverse_to_false_if_the_statement_object_contains_false) {
	struct spindle_predicatemap_struct map = { .inverse = 1 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_uri *type = (librdf_uri *) 0xA03;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(type), when(node, is_equal_to(node)));
	expect(librdf_uri_as_string, will_return(NS_XSD "boolean"), when(uri, is_equal_to(type)));
	expect(librdf_node_get_literal_value, will_return("false"), when(node, is_equal_to(node)));

	int r = spindle_rulebase_pred_set_inverse_(&map, statement);
	assert_that(r, is_equal_to(1));
	assert_that(map.inverse, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_rulebase_pred_set_indexed_

Ensure(spindle_common_rulebase, pred_set_indexed_returns_false_if_the_statement_object_is_not_a_literal) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, will_return(0), when(node, is_equal_to(node)));

	int r = spindle_rulebase_pred_set_indexed_(&map, statement);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, pred_set_indexed_returns_false_if_the_statement_object_has_no_datatype_uri) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_uri *type = (librdf_uri *) 0xA03;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(0), when(node, is_equal_to(node)));

	int r = spindle_rulebase_pred_set_indexed_(&map, statement);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, pred_set_indexed_returns_false_if_the_statement_object_datatype_uri_is_not_xsd_boolean) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_uri *type = (librdf_uri *) 0xA03;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(type), when(node, is_equal_to(node)));
	expect(librdf_uri_as_string, will_return(NS_XSD "integer"), when(uri, is_equal_to(type)));

	int r = spindle_rulebase_pred_set_indexed_(&map, statement);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, pred_set_indexed_returns_true_and_sets_indexed_to_true_if_the_statement_object_contains_true) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_uri *type = (librdf_uri *) 0xA03;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(type), when(node, is_equal_to(node)));
	expect(librdf_uri_as_string, will_return(NS_XSD "boolean"), when(uri, is_equal_to(type)));
	expect(librdf_node_get_literal_value, will_return("true"), when(node, is_equal_to(node)));

	int r = spindle_rulebase_pred_set_indexed_(&map, statement);
	assert_that(r, is_equal_to(1));
	assert_that(map.indexed, is_equal_to(1));
}

Ensure(spindle_common_rulebase, pred_set_indexed_returns_true_and_sets_indexed_to_false_if_the_statement_object_contains_false) {
	struct spindle_predicatemap_struct map = { .indexed = 1 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_uri *type = (librdf_uri *) 0xA03;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(type), when(node, is_equal_to(node)));
	expect(librdf_uri_as_string, will_return(NS_XSD "boolean"), when(uri, is_equal_to(type)));
	expect(librdf_node_get_literal_value, will_return("false"), when(node, is_equal_to(node)));

	int r = spindle_rulebase_pred_set_indexed_(&map, statement);
	assert_that(r, is_equal_to(1));
	assert_that(map.indexed, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_rulebase_pred_set_proxyonly_

Ensure(spindle_common_rulebase, pred_set_proxyonly_returns_false_if_the_statement_object_is_not_a_literal) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, will_return(0), when(node, is_equal_to(node)));

	int r = spindle_rulebase_pred_set_proxyonly_(&map, statement);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, pred_set_proxyonly_returns_false_if_the_statement_object_has_no_datatype_uri) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_uri *type = (librdf_uri *) 0xA03;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(0), when(node, is_equal_to(node)));

	int r = spindle_rulebase_pred_set_proxyonly_(&map, statement);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, pred_set_proxyonly_returns_false_if_the_statement_object_datatype_uri_is_not_xsd_boolean) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_uri *type = (librdf_uri *) 0xA03;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(type), when(node, is_equal_to(node)));
	expect(librdf_uri_as_string, will_return(NS_XSD "integer"), when(uri, is_equal_to(type)));

	int r = spindle_rulebase_pred_set_proxyonly_(&map, statement);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, pred_set_proxyonly_returns_true_and_sets_proxyonly_to_true_if_the_statement_object_contains_true) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_uri *type = (librdf_uri *) 0xA03;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(type), when(node, is_equal_to(node)));
	expect(librdf_uri_as_string, will_return(NS_XSD "boolean"), when(uri, is_equal_to(type)));
	expect(librdf_node_get_literal_value, will_return("true"), when(node, is_equal_to(node)));

	int r = spindle_rulebase_pred_set_proxyonly_(&map, statement);
	assert_that(r, is_equal_to(1));
	assert_that(map.proxyonly, is_equal_to(1));
}

Ensure(spindle_common_rulebase, pred_set_proxyonly_returns_true_and_sets_proxyonly_to_false_if_the_statement_object_contains_false) {
	struct spindle_predicatemap_struct map = { .proxyonly = 1 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_uri *type = (librdf_uri *) 0xA03;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(type), when(node, is_equal_to(node)));
	expect(librdf_uri_as_string, will_return(NS_XSD "boolean"), when(uri, is_equal_to(type)));
	expect(librdf_node_get_literal_value, will_return("false"), when(node, is_equal_to(node)));

	int r = spindle_rulebase_pred_set_proxyonly_(&map, statement);
	assert_that(r, is_equal_to(1));
	assert_that(map.proxyonly, is_equal_to(0));
}

#pragma mark -

TestSuite *create_rulebase_pred_test_suite(void) {
	TestSuite *suite = create_test_suite();
	add_test_with_context(suite, spindle_common_rulebase, pred_add_match_returns_success_and_adds_the_predicate_to_the_match_list);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_match_returns_success_and_adds_the_predicate_to_the_match_list_when_class_restriction_is_not_null);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_match_can_add_multiple_different_predicates_to_the_match_list);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_match_returns_success_and_overwrites_the_priority_and_prominance_when_the_predicate_is_already_in_the_match_list);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_match_returns_success_and_does_not_overwrite_an_existing_entry_when_the_predicate_is_already_in_the_match_list_but_the_inverse_differs);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_match_returns_success_and_does_not_overwrite_an_existing_entry_when_the_predicate_is_already_in_the_match_list_but_the_classmatch_is_empty);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_match_returns_success_and_does_not_overwrite_an_existing_entry_when_the_predicate_is_already_in_the_match_list_but_the_classmatch_differs);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_adds_the_predicate_to_the_match_list_and_returns_a_pointer_to_the_list_entry);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_adds_returns_a_pointer_to_an_existing_list_entry_and_does_not_add_a_new_entry_if_adding_a_duplicate_predicate);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_adds_returns_null_and_does_not_add_a_new_entry_if_adding_the_predicate_to_the_list_of_cached_predicates_fails);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_inverse_returns_false_if_the_statement_object_is_not_a_literal);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_inverse_returns_false_if_the_statement_object_has_no_datatype_uri);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_inverse_returns_false_if_the_statement_object_datatype_uri_is_not_xsd_boolean);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_inverse_returns_true_and_sets_the_inverse_to_true_if_the_statement_object_contains_true);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_inverse_returns_true_and_sets_the_inverse_to_false_if_the_statement_object_contains_false);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_indexed_returns_false_if_the_statement_object_is_not_a_literal);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_indexed_returns_false_if_the_statement_object_has_no_datatype_uri);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_indexed_returns_false_if_the_statement_object_datatype_uri_is_not_xsd_boolean);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_indexed_returns_true_and_sets_indexed_to_true_if_the_statement_object_contains_true);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_indexed_returns_true_and_sets_indexed_to_false_if_the_statement_object_contains_false);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_proxyonly_returns_false_if_the_statement_object_is_not_a_literal);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_proxyonly_returns_false_if_the_statement_object_has_no_datatype_uri);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_proxyonly_returns_false_if_the_statement_object_datatype_uri_is_not_xsd_boolean);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_proxyonly_returns_true_and_sets_proxyonly_to_true_if_the_statement_object_contains_true);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_proxyonly_returns_true_and_sets_proxyonly_to_false_if_the_statement_object_contains_false);
	return suite;
}

int rulebase_pred_test(char *test) {
	if(test) {
		return run_single_test(create_rulebase_pred_test_suite(), test, create_text_reporter());
	} else {
		return run_test_suite(create_rulebase_pred_test_suite(), create_text_reporter());
	}
}

int main(int argc, char **argv) {
	return rulebase_pred_test(argc > 1 ? argv[1] : NULL);
}
