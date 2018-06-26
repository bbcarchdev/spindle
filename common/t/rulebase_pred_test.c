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
#include "../../t/mock_spindle_rulebase.h"
#include "../../t/mock_spindle_rulebase_cachepred.h"
#include "../../t/mock_spindle_rulebase_class.h"
#include "../../t/mock_spindle_rulebase_coref.h"
#include "../../t/rdf_namespaces.h"

#define P_SPINDLE_H_
#define PLUGIN_NAME "spindle-rulebase"
#include "../rulebase-pred.c"

Describe(spindle_common_rulebase);
BeforeEach(spindle_common_rulebase) { always_expect(twine_logf); die_in(1); }
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
#pragma mark spindle_rulebase_pred_set_expecttype_

Ensure(spindle_common_rulebase, pred_set_expecttype_returns_success_if_the_statement_object_is_not_a_resource) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_resource, will_return(0), when(node, is_equal_to(node)));

	int r = spindle_rulebase_pred_set_expecttype_(&map, statement);
	assert_that(r, is_equal_to(0));
	assert_that(map.datatype, is_null);
}

Ensure(spindle_common_rulebase, pred_set_expecttype_returns_success_and_sets_the_expected_data_type_if_the_statement_object_is_a_resource) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_uri *uri = (librdf_uri *) 0xA03;
	const char *uri_str = "uri";

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_uri, will_return(uri), when(node, is_equal_to(node)));
	expect(librdf_uri_as_string, will_return(uri_str), when(uri, is_equal_to(uri)));

	int r = spindle_rulebase_pred_set_expecttype_(&map, statement);
	assert_that(r, is_equal_to(0));
	assert_that(map.datatype, is_equal_to_string(uri_str));
}

#pragma mark -
#pragma mark spindle_rulebase_pred_set_expect_

Ensure(spindle_common_rulebase, pred_set_expect_returns_false_if_the_statement_object_is_not_a_resource) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_resource, will_return(0), when(node, is_equal_to(node)));

	int r = spindle_rulebase_pred_set_expect_(&map, statement);
	assert_that(r, is_equal_to(0));
	assert_that(map.expected, is_equal_to(RAPTOR_TERM_TYPE_UNKNOWN));
}

Ensure(spindle_common_rulebase, pred_set_expect_returns_false_if_the_statement_object_is_not_rdfs_literal_nor_rdfs_resource) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_uri *uri = (librdf_uri *) 0xA03;
	const char *uri_str = "uri";

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_uri, will_return(uri), when(node, is_equal_to(node)));
	expect(librdf_uri_as_string, will_return(uri_str), when(uri, is_equal_to(uri)));

	int r = spindle_rulebase_pred_set_expect_(&map, statement);
	assert_that(r, is_equal_to(0));
	assert_that(map.expected, is_equal_to(RAPTOR_TERM_TYPE_UNKNOWN));
}

Ensure(spindle_common_rulebase, pred_set_expect_returns_true_and_sets_the_expected_term_type_to_raptor_literal_if_the_statement_object_is_rdfs_literal) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_uri *uri = (librdf_uri *) 0xA03;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_uri, will_return(uri), when(node, is_equal_to(node)));
	expect(librdf_uri_as_string, will_return(NS_RDFS "Literal"), when(uri, is_equal_to(uri)));

	int r = spindle_rulebase_pred_set_expect_(&map, statement);
	assert_that(r, is_equal_to(1));
	assert_that(map.expected, is_equal_to(RAPTOR_TERM_TYPE_LITERAL));
}

Ensure(spindle_common_rulebase, pred_set_expect_returns_true_and_sets_the_expected_term_type_to_raptor_uri_if_the_statement_object_is_rdfs_resource) {
	struct spindle_predicatemap_struct map = { 0 };
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_uri *uri = (librdf_uri *) 0xA03;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(node)));
	expect(librdf_node_get_uri, will_return(uri), when(node, is_equal_to(node)));
	expect(librdf_uri_as_string, will_return(NS_RDFS "Resource"), when(uri, is_equal_to(uri)));

	int r = spindle_rulebase_pred_set_expect_(&map, statement);
	assert_that(r, is_equal_to(1));
	assert_that(map.expected, is_equal_to(RAPTOR_TERM_TYPE_URI));
}

#pragma mark -
#pragma mark spindle_rulebase_pred_set_prominence_

Ensure(spindle_common_rulebase, pred_set_prominence_returns_false_and_does_not_change_prominence_if_getting_intval_fails) {
	long prominence = 555;
	struct spindle_predicatemap_struct map = { .prominence = prominence };
	librdf_statement *statement = (librdf_statement *) 0xA01;

	expect(twine_rdf_st_obj_intval, will_return(0), when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_pred_set_prominence_(&map, statement);
	assert_that(r, is_equal_to(0));
	assert_that(map.prominence, is_equal_to(prominence));
}

Ensure(spindle_common_rulebase, pred_set_prominence_returns_false_and_does_not_change_prominence_if_object_intval_is_negative) {
	long prominence = -333, old_prominence = 555;
	struct spindle_predicatemap_struct map = { .prominence = old_prominence };
	librdf_statement *statement = (librdf_statement *) 0xA01;

	expect(twine_rdf_st_obj_intval, will_return(prominence), will_set_contents_of_parameter(value, &prominence, sizeof prominence), when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_pred_set_prominence_(&map, statement);
	assert_that(r, is_equal_to(0));
	assert_that(map.prominence, is_equal_to(old_prominence));
}

Ensure(spindle_common_rulebase, pred_set_prominence_returns_true_and_sets_prominence_if_object_intval_is_positive) {
	long prominence = 555;
	struct spindle_predicatemap_struct map = { 0 };
	librdf_statement *statement = (librdf_statement *) 0xA01;

	expect(twine_rdf_st_obj_intval, will_return(prominence), will_set_contents_of_parameter(value, &prominence, sizeof prominence), when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_pred_set_prominence_(&map, statement);
	assert_that(r, is_equal_to(1));
	assert_that(map.prominence, is_equal_to(prominence));
}

#pragma mark -
#pragma mark spindle_rulebase_pred_set_score_

Ensure(spindle_common_rulebase, pred_set_score_returns_false_and_does_not_change_score_if_getting_intval_fails) {
	long score = 555;
	struct spindle_predicatemap_struct map = { .score = score };
	librdf_statement *statement = (librdf_statement *) 0xA01;

	expect(twine_rdf_st_obj_intval, will_return(0), when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_pred_set_score_(&map, statement);
	assert_that(r, is_equal_to(0));
	assert_that(map.score, is_equal_to(score));
}

Ensure(spindle_common_rulebase, pred_set_score_returns_false_and_does_not_change_score_if_object_intval_is_negative) {
	long score = -333, old_score = 555;
	struct spindle_predicatemap_struct map = { .score = old_score };
	librdf_statement *statement = (librdf_statement *) 0xA01;

	expect(twine_rdf_st_obj_intval, will_return(score), will_set_contents_of_parameter(value, &score, sizeof score), when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_pred_set_score_(&map, statement);
	assert_that(r, is_equal_to(0));
	assert_that(map.score, is_equal_to(old_score));
}

Ensure(spindle_common_rulebase, pred_set_score_returns_true_and_sets_score_if_object_intval_is_positive) {
	long score = 555;
	struct spindle_predicatemap_struct map = { 0 };
	librdf_statement *statement = (librdf_statement *) 0xA01;

	expect(twine_rdf_st_obj_intval, will_return(score), will_set_contents_of_parameter(value, &score, sizeof score), when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_pred_set_score_(&map, statement);
	assert_that(r, is_equal_to(1));
	assert_that(map.score, is_equal_to(score));
}

#pragma mark -
#pragma mark spindle_rulebase_pred_dump

Ensure(spindle_common_rulebase, pred_dump_returns_no_error_and_only_has_side_effects_when_the_predicate_list_is_empty) {
	SPINDLERULES rules = { 0 };

	int r = spindle_rulebase_pred_dump(&rules);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, pred_dump_returns_no_error_and_only_has_side_effects_when_the_predicate_list_has_items) {
	struct spindle_predicatematch_struct matches[] = {
		{
			.predicate = "some:predicate",
			.priority = 5
		},
		{
			.onlyfor = "<some_class>",
			.predicate = "some:predicate",
			.priority = 5
		}
	};
	struct spindle_predicatemap_struct predicates[] = {
		{ 0 },
		{
			.expected = RAPTOR_TERM_TYPE_URI,
			.proxyonly = 1,
			.datatype = "datatype",
			.matches = matches,
			.matchcount = sizeof matches / sizeof matches[0]
		}
	};
	SPINDLERULES rules = {
		.predicates = predicates,
		.predcount = sizeof predicates / sizeof predicates[0]
	};

	int r = spindle_rulebase_pred_dump(&rules);
	assert_that(r, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_rulebase_pred_compare_
/* these tests are identical to the class_compare ones in rulebase_class_test.c */

Ensure(spindle_common_rulebase, pred_compare_returns_no_difference_if_both_scores_are_zero) {
	struct spindle_predicatemap_struct a = { 0 };
	struct spindle_predicatemap_struct b = { 0 };

	int r = spindle_rulebase_pred_compare_(&a, &b);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, pred_compare_returns_no_difference_if_scores_are_equal_and_non_zero) {
	struct spindle_predicatemap_struct a = { .score = 5 };
	struct spindle_predicatemap_struct b = { .score = 5 };

	int r = spindle_rulebase_pred_compare_(&a, &b);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, pred_compare_returns_b_sorts_later_if_b_has_a_higher_score) {
	struct spindle_predicatemap_struct a = { .score = 2 };
	struct spindle_predicatemap_struct b = { .score = 5 };

	int r = spindle_rulebase_pred_compare_(&a, &b);
	assert_that(r, is_less_than(0));
}

Ensure(spindle_common_rulebase, pred_compare_returns_b_sorts_earlier_if_b_has_a_lower_score) {
	struct spindle_predicatemap_struct a = { .score = 5 };
	struct spindle_predicatemap_struct b = { .score = 2 };

	int r = spindle_rulebase_pred_compare_(&a, &b);
	assert_that(r, is_greater_than(0));
}

#pragma mark -
#pragma mark spindle_rulebase_pred_cleanup

Ensure(spindle_common_rulebase, pred_cleanup_only_has_side_effects) {
	struct spindle_predicatematch_struct *matches = calloc(1, sizeof (struct spindle_predicatematch_struct));
	struct spindle_predicatemap_struct *predicates = calloc(1, sizeof (struct spindle_predicatemap_struct));
	SPINDLERULES rules = {
		.predicates = predicates,
		.predcount = 1
	};

	predicates[0].matchcount = 1;
	predicates[0].matches = matches;
	predicates[0].target = strdup("target");
	predicates[0].datatype = strdup("datatype");
	matches[0].predicate = strdup("some:predicate");
	matches[0].onlyfor = strdup("<some_class>");

	int r = spindle_rulebase_pred_cleanup(&rules);
	assert_that(r, is_equal_to(0));
	assert_that(rules.predicates, is_null);
}

#pragma mark -
#pragma mark spindle_rulebase_pred_finalise
/*same test as class_finalise_sorts_the_class_list_by_score in rulebase_class_test.c */

Ensure(spindle_common_rulebase, pred_finalise_sorts_the_predicate_list_by_score) {
	struct spindle_predicatemap_struct predicates[] = {
		{ .score = 29 },
		{ .score = 3 },
		{ .score = 13 },
		{ .score = 23 },
		{ .score = 5 },
		{ .score = 17 },
		{ .score = 11 },
		{ .score = 2 },
		{ .score = 7 },
		{ .score = 19 }
	};
	struct spindle_predicatemap_struct sorted[] = {
		{ .score = 2 },
		{ .score = 3 },
		{ .score = 5 },
		{ .score = 7 },
		{ .score = 11 },
		{ .score = 13 },
		{ .score = 17 },
		{ .score = 19 },
		{ .score = 23 },
		{ .score = 29 }
	};
	SPINDLERULES rules = {
		.predicates = predicates,
		.predcount = sizeof predicates / sizeof predicates[0]
	};

	int r = spindle_rulebase_pred_finalise(&rules);
	assert_that(r, is_equal_to(0));
	assert_that(rules.predicates[0].score, is_equal_to(sorted[0].score));
	assert_that(rules.predicates[1].score, is_equal_to(sorted[1].score));
	assert_that(rules.predicates[2].score, is_equal_to(sorted[2].score));
	assert_that(rules.predicates[3].score, is_equal_to(sorted[3].score));
	assert_that(rules.predicates[4].score, is_equal_to(sorted[4].score));
	assert_that(rules.predicates[5].score, is_equal_to(sorted[5].score));
	assert_that(rules.predicates[6].score, is_equal_to(sorted[6].score));
	assert_that(rules.predicates[7].score, is_equal_to(sorted[7].score));
	assert_that(rules.predicates[8].score, is_equal_to(sorted[8].score));
	assert_that(rules.predicates[9].score, is_equal_to(sorted[9].score));
}

#pragma mark -
#pragma mark spindle_rulebase_pred_add_matchnode

Ensure(spindle_common_rulebase, pred_add_matchnode_returns_no_error_if_model_contains_no_triples_whos_subject_is_the_matchnode) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_node *matchnode = (librdf_node *) 0xA02;
	librdf_statement *statement = (librdf_statement *) 0xA03;
	librdf_stream *stream = (librdf_stream *) 0xA04;
	librdf_world *world = (librdf_world *) 0xA05;
	const char *matchuri = "match uri";
	int inverse = 0;

	expect(twine_rdf_world, will_return(world));
	expect(librdf_new_node_from_node, when(node, is_equal_to(matchnode)));
	expect(librdf_new_statement_from_nodes, will_return(statement), when(world, is_equal_to(world)), when(subject, is_equal_to(0)), when(predicate, is_equal_to(0)), when(object, is_equal_to(0)));
	expect(librdf_model_find_statements, will_return(stream), when(model, is_equal_to(model)), when(statement, is_equal_to(statement)));
	expect(librdf_stream_end, will_return(1));
	expect(librdf_free_stream, when(stream, is_equal_to(stream)));
	expect(librdf_free_statement, when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_pred_add_matchnode(&rules, model, matchuri, matchnode, inverse);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, pred_add_matchnode_returns_no_error_if_model_contains_triples_whos_subject_is_the_matchnode_and_triples_predicate_is_not_a_resource) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_node *matchnode = (librdf_node *) 0xA02;
	librdf_node *predicate = (librdf_node *) 0xB02;
	librdf_node *object = (librdf_node *) 0xC02;
	librdf_statement *statement = (librdf_statement *) 0xA03;
	librdf_statement *result = (librdf_statement *) 0xB03;
	librdf_stream *stream = (librdf_stream *) 0xA04;
	librdf_world *world = (librdf_world *) 0xA05;
	const char *matchuri = "match uri";
	int inverse = 0;

	expect(twine_rdf_world, will_return(world));
	expect(librdf_new_node_from_node, when(node, is_equal_to(matchnode)));
	expect(librdf_new_statement_from_nodes, will_return(statement), when(world, is_equal_to(world)), when(subject, is_equal_to(0)), when(predicate, is_equal_to(0)), when(object, is_equal_to(0)));
	expect(librdf_model_find_statements, will_return(stream), when(model, is_equal_to(model)), when(statement, is_equal_to(statement)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(result), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(result)));
	expect(librdf_statement_get_object, will_return(object), when(statement, is_equal_to(result)));
	expect(librdf_node_is_resource, will_return(0), when(node, is_equal_to(predicate)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(stream)));
	expect(librdf_free_stream, when(stream, is_equal_to(stream)));
	expect(librdf_free_statement, when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_pred_add_matchnode(&rules, model, matchuri, matchnode, inverse);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, pred_add_matchnode_returns_no_error_if_model_contains_triples_whos_subject_is_the_matchnode_and_triples_object_is_not_a_resource) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_node *matchnode = (librdf_node *) 0xA02;
	librdf_node *predicate = (librdf_node *) 0xB02;
	librdf_node *object = (librdf_node *) 0xC02;
	librdf_statement *statement = (librdf_statement *) 0xA03;
	librdf_statement *result = (librdf_statement *) 0xB03;
	librdf_stream *stream = (librdf_stream *) 0xA04;
	librdf_uri *uri = (librdf_uri *) 0xA05;
	librdf_world *world = (librdf_world *) 0xA06;
	const char *matchuri = "match uri";
	const char *preduri = NS_SPINDLE "expressedAs";
	int inverse = 0;

	expect(twine_rdf_world, will_return(world));
	expect(librdf_new_node_from_node, when(node, is_equal_to(matchnode)));
	expect(librdf_new_statement_from_nodes, will_return(statement), when(world, is_equal_to(world)), when(subject, is_equal_to(0)), when(predicate, is_equal_to(0)), when(object, is_equal_to(0)));
	expect(librdf_model_find_statements, will_return(stream), when(model, is_equal_to(model)), when(statement, is_equal_to(statement)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(result), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(result)));
	expect(librdf_statement_get_object, will_return(object), when(statement, is_equal_to(result)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(preduri), when(uri, is_equal_to(uri)));
	expect(librdf_node_is_resource, will_return(0), when(node, is_equal_to(object)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(stream)));
	expect(librdf_free_stream, when(stream, is_equal_to(stream)));
	expect(librdf_free_statement, when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_pred_add_matchnode(&rules, model, matchuri, matchnode, inverse);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, pred_add_matchnode_returns_no_error_and_adds_a_general_match_for_the_predicate_if_the_model_contains_spindle_expressedas) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_node *matchnode = (librdf_node *) 0xA02;
	librdf_node *predicate = (librdf_node *) 0xB02;
	librdf_node *object = (librdf_node *) 0xC02;
	librdf_statement *statement = (librdf_statement *) 0xA03;
	librdf_statement *result = (librdf_statement *) 0xB03;
	librdf_stream *stream = (librdf_stream *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	librdf_uri *object_uri = (librdf_uri *) 0xB05;
	librdf_world *world = (librdf_world *) 0xA06;
	const char *matchuri = "match uri";
	const char *preduri = NS_SPINDLE "expressedAs";
	const char *objuri = "object";
	int inverse = 0;

	expect(twine_rdf_world, will_return(world));
	expect(librdf_new_node_from_node, when(node, is_equal_to(matchnode)));
	expect(librdf_new_statement_from_nodes, will_return(statement), when(world, is_equal_to(world)), when(subject, is_equal_to(0)), when(predicate, is_equal_to(0)), when(object, is_equal_to(0)));
	expect(librdf_model_find_statements, will_return(stream), when(model, is_equal_to(model)), when(statement, is_equal_to(statement)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(result), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(result)));
	expect(librdf_statement_get_object, will_return(object), when(statement, is_equal_to(result)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(preduri), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(object)));
	expect(librdf_node_get_uri, will_return(object_uri), when(node, is_equal_to(object)));
	expect(librdf_uri_as_string, will_return(objuri), when(uri, is_equal_to(object_uri)));
	expect(spindle_rulebase_cachepred_add, when(rules, is_equal_to(&rules)), when(uri, is_equal_to(objuri)));
	expect(spindle_rulebase_cachepred_add, when(rules, is_equal_to(&rules)), when(uri, is_equal_to(matchuri)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(stream)));
	expect(librdf_free_stream, when(stream, is_equal_to(stream)));
	expect(librdf_free_statement, when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_pred_add_matchnode(&rules, model, matchuri, matchnode, inverse);
	assert_that(r, is_equal_to(0));
	assert_that(rules.predcount, is_equal_to(1));
	assert_that(rules.predicates[0].target, is_equal_to_string(objuri));
	assert_that(rules.predicates[0].matchcount, is_equal_to(1));
	assert_that(rules.predicates[0].matches[0].predicate, is_equal_to_string(matchuri));
	assert_that(rules.predicates[0].matches[0].onlyfor, is_null);
}

Ensure(spindle_common_rulebase, pred_add_matchnode_returns_no_error_and_adds_a_domain_match_for_the_predicate_if_the_model_contains_spindle_expressedas) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_node *matchnode = (librdf_node *) 0xA02;
	librdf_node *predicate = (librdf_node *) 0xB02;
	librdf_node *object = (librdf_node *) 0xC02;
	librdf_node *predicate_2 = (librdf_node *) 0xD02;
	librdf_node *object_2 = (librdf_node *) 0xE02;
	librdf_statement *statement = (librdf_statement *) 0xA03;
	librdf_statement *result = (librdf_statement *) 0xB03;
	librdf_statement *result_2 = (librdf_statement *) 0xC03;
	librdf_stream *stream = (librdf_stream *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	librdf_uri *object_uri = (librdf_uri *) 0xB05;
	librdf_uri *predicate_uri_2 = (librdf_uri *) 0xC05;
	librdf_world *world = (librdf_world *) 0xA06;
	const char *matchuri = "match uri";
	const char *spindle_expressed_as = NS_SPINDLE "expressedAs";
	const char *rdfs_domain = NS_RDFS "domain";
	const char *objuri = "object";
	int inverse = 0;

	expect(twine_rdf_world, will_return(world));
	expect(librdf_new_node_from_node, when(node, is_equal_to(matchnode)));
	expect(librdf_new_statement_from_nodes, will_return(statement), when(world, is_equal_to(world)), when(subject, is_equal_to(0)), when(predicate, is_equal_to(0)), when(object, is_equal_to(0)));
	expect(librdf_model_find_statements, will_return(stream), when(model, is_equal_to(model)), when(statement, is_equal_to(statement)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(result), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(result)));
	expect(librdf_statement_get_object, will_return(object), when(statement, is_equal_to(result)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(spindle_expressed_as), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(object)));
	expect(librdf_node_get_uri, will_return(object_uri), when(node, is_equal_to(object)));
	expect(librdf_uri_as_string, will_return(objuri), when(uri, is_equal_to(object_uri)));
	expect(spindle_rulebase_cachepred_add, when(rules, is_equal_to(&rules)), when(uri, is_equal_to(objuri)));
	expect(spindle_rulebase_cachepred_add, when(rules, is_equal_to(&rules)), when(uri, is_equal_to(matchuri)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(result_2), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate_2), when(statement, is_equal_to(result_2)));
	expect(librdf_statement_get_object, will_return(object_2), when(statement, is_equal_to(result_2)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate_2)));
	expect(librdf_node_get_uri, will_return(predicate_uri_2), when(node, is_equal_to(predicate_2)));
	expect(librdf_uri_as_string, will_return(rdfs_domain), when(uri, is_equal_to(predicate_uri_2)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(stream)));
	expect(librdf_free_stream, when(stream, is_equal_to(stream)));
	expect(librdf_free_statement, when(statement, is_equal_to(statement)));

	librdf_node *subject_copy = (librdf_node *) 0xA10;
	librdf_node *predicate_copy = (librdf_node *) 0xA11;
	librdf_node *domain = (librdf_node *) 0xA12;
	librdf_statement *query_2 = (librdf_statement *) 0xB10;
	librdf_statement *result_3 = (librdf_statement *) 0xB11;
	librdf_stream *results_2 = (librdf_stream *) 0xC10;
	librdf_uri *domain_uri = (librdf_uri *) 0xD10;
	const char *domain_str = "<domain>";

	expect(librdf_new_node_from_node, will_return(subject_copy), when(node, is_equal_to(matchnode)));
	expect(librdf_new_node_from_uri_string, will_return(predicate_copy), when(world, is_equal_to(world)), when(uri, is_equal_to_string(rdfs_domain)));
	expect(librdf_new_statement_from_nodes, will_return(query_2), when(world, is_equal_to(world)), when(subject, is_equal_to(subject_copy)), when(predicate, is_equal_to(predicate_copy)), when(object, is_null));
	expect(librdf_model_find_statements, will_return(results_2), when(model, is_equal_to(model)), when(statement, is_equal_to(query_2)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(results_2)));
	expect(librdf_stream_get_object, will_return(result_3), when(stream, is_equal_to(results_2)));
	expect(librdf_statement_get_object, will_return(domain), when(statement, is_equal_to(result_3)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(domain)));
	expect(librdf_node_get_uri, will_return(domain_uri), when(node, is_equal_to(domain)));
	expect(librdf_uri_as_string, will_return(domain_str), when(uri, is_equal_to(domain_uri)));
	expect(librdf_stream_next, when(stream, is_equal_to(results_2)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(results_2)));
	expect(librdf_free_stream, when(stream, is_equal_to(results_2)));
	expect(librdf_free_statement, when(statement, is_equal_to(query_2)));

	int r = spindle_rulebase_pred_add_matchnode(&rules, model, matchuri, matchnode, inverse);
	assert_that(r, is_equal_to(0));
	assert_that(rules.predcount, is_equal_to(1));
	assert_that(rules.predicates[0].target, is_equal_to_string(objuri));
	assert_that(rules.predicates[0].matchcount, is_equal_to(1));
	assert_that(rules.predicates[0].matches[0].predicate, is_equal_to_string(matchuri));
	assert_that(rules.predicates[0].matches[0].onlyfor, is_equal_to_string(domain_str));
}

Ensure(spindle_common_rulebase, pred_add_matchnode_passes_the_priority_when_adding_a_general_match) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_node *matchnode = (librdf_node *) 0xA02;
	librdf_node *predicate = (librdf_node *) 0xB02;
	librdf_node *object = (librdf_node *) 0xC02;
	librdf_node *predicate_2 = (librdf_node *) 0xD02;
	librdf_node *object_2 = (librdf_node *) 0xE02;
	librdf_statement *statement = (librdf_statement *) 0xA03;
	librdf_statement *result = (librdf_statement *) 0xB03;
	librdf_statement *result_2 = (librdf_statement *) 0xC03;
	librdf_stream *stream = (librdf_stream *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	librdf_uri *object_uri = (librdf_uri *) 0xB05;
	librdf_uri *predicate_uri_2 = (librdf_uri *) 0xC05;
	librdf_world *world = (librdf_world *) 0xA06;
	const char *matchuri = "match uri";
	const char *spindle_expressed_as = NS_SPINDLE "expressedAs";
	const char *olo_index = NS_OLO "index";
	const char *objuri = "object";
	int inverse = 0;
	int priority = 333;

	expect(twine_rdf_world, will_return(world));
	expect(librdf_new_node_from_node, when(node, is_equal_to(matchnode)));
	expect(librdf_new_statement_from_nodes, will_return(statement), when(world, is_equal_to(world)), when(subject, is_equal_to(0)), when(predicate, is_equal_to(0)), when(object, is_equal_to(0)));
	expect(librdf_model_find_statements, will_return(stream), when(model, is_equal_to(model)), when(statement, is_equal_to(statement)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(result), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(result)));
	expect(librdf_statement_get_object, will_return(object), when(statement, is_equal_to(result)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(spindle_expressed_as), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(object)));
	expect(librdf_node_get_uri, will_return(object_uri), when(node, is_equal_to(object)));
	expect(librdf_uri_as_string, will_return(objuri), when(uri, is_equal_to(object_uri)));
	expect(spindle_rulebase_cachepred_add, when(rules, is_equal_to(&rules)), when(uri, is_equal_to(objuri)));
	expect(spindle_rulebase_cachepred_add, when(rules, is_equal_to(&rules)), when(uri, is_equal_to(matchuri)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(result_2), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate_2), when(statement, is_equal_to(result_2)));
	expect(librdf_statement_get_object, will_return(object_2), when(statement, is_equal_to(result_2)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate_2)));
	expect(librdf_node_get_uri, will_return(predicate_uri_2), when(node, is_equal_to(predicate_2)));
	expect(librdf_uri_as_string, will_return(olo_index), when(uri, is_equal_to(predicate_uri_2)));
	expect(twine_rdf_node_intval, will_return(priority), will_set_contents_of_parameter(value, &priority, sizeof priority), when(node, is_equal_to(object_2)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(stream)));
	expect(librdf_free_stream, when(stream, is_equal_to(stream)));
	expect(librdf_free_statement, when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_pred_add_matchnode(&rules, model, matchuri, matchnode, inverse);
	assert_that(r, is_equal_to(0));
	assert_that(rules.predcount, is_equal_to(1));
	assert_that(rules.predicates[0].target, is_equal_to_string(objuri));
	assert_that(rules.predicates[0].matchcount, is_equal_to(1));
	assert_that(rules.predicates[0].matches[0].predicate, is_equal_to_string(matchuri));
	assert_that(rules.predicates[0].matches[0].onlyfor, is_null);
	assert_that(rules.predicates[0].matches[0].priority, is_equal_to(priority));
}

Ensure(spindle_common_rulebase, pred_add_matchnode_passes_the_prominence_when_adding_a_general_match) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_node *matchnode = (librdf_node *) 0xA02;
	librdf_node *predicate = (librdf_node *) 0xB02;
	librdf_node *object = (librdf_node *) 0xC02;
	librdf_node *predicate_2 = (librdf_node *) 0xD02;
	librdf_node *object_2 = (librdf_node *) 0xE02;
	librdf_statement *statement = (librdf_statement *) 0xA03;
	librdf_statement *result = (librdf_statement *) 0xB03;
	librdf_statement *result_2 = (librdf_statement *) 0xC03;
	librdf_stream *stream = (librdf_stream *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	librdf_uri *object_uri = (librdf_uri *) 0xB05;
	librdf_uri *predicate_uri_2 = (librdf_uri *) 0xC05;
	librdf_world *world = (librdf_world *) 0xA06;
	const char *matchuri = "match uri";
	const char *spindle_expressed_as = NS_SPINDLE "expressedAs";
	const char *spindle_prominence = NS_SPINDLE "prominence";
	const char *objuri = "object";
	int inverse = 0;
	int prominence = 333;

	expect(twine_rdf_world, will_return(world));
	expect(librdf_new_node_from_node, when(node, is_equal_to(matchnode)));
	expect(librdf_new_statement_from_nodes, will_return(statement), when(world, is_equal_to(world)), when(subject, is_equal_to(0)), when(predicate, is_equal_to(0)), when(object, is_equal_to(0)));
	expect(librdf_model_find_statements, will_return(stream), when(model, is_equal_to(model)), when(statement, is_equal_to(statement)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(result), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(result)));
	expect(librdf_statement_get_object, will_return(object), when(statement, is_equal_to(result)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(spindle_expressed_as), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(object)));
	expect(librdf_node_get_uri, will_return(object_uri), when(node, is_equal_to(object)));
	expect(librdf_uri_as_string, will_return(objuri), when(uri, is_equal_to(object_uri)));
	expect(spindle_rulebase_cachepred_add, when(rules, is_equal_to(&rules)), when(uri, is_equal_to(objuri)));
	expect(spindle_rulebase_cachepred_add, when(rules, is_equal_to(&rules)), when(uri, is_equal_to(matchuri)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(result_2), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate_2), when(statement, is_equal_to(result_2)));
	expect(librdf_statement_get_object, will_return(object_2), when(statement, is_equal_to(result_2)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate_2)));
	expect(librdf_node_get_uri, will_return(predicate_uri_2), when(node, is_equal_to(predicate_2)));
	expect(librdf_uri_as_string, will_return(spindle_prominence), when(uri, is_equal_to(predicate_uri_2)));
	expect(twine_rdf_node_intval, will_return(prominence), will_set_contents_of_parameter(value, &prominence, sizeof prominence), when(node, is_equal_to(object_2)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(stream)));
	expect(librdf_free_stream, when(stream, is_equal_to(stream)));
	expect(librdf_free_statement, when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_pred_add_matchnode(&rules, model, matchuri, matchnode, inverse);
	assert_that(r, is_equal_to(0));
	assert_that(rules.predcount, is_equal_to(1));
	assert_that(rules.predicates[0].target, is_equal_to_string(objuri));
	assert_that(rules.predicates[0].matchcount, is_equal_to(1));
	assert_that(rules.predicates[0].matches[0].predicate, is_equal_to_string(matchuri));
	assert_that(rules.predicates[0].matches[0].onlyfor, is_null);
	assert_that(rules.predicates[0].matches[0].prominence, is_equal_to(prominence));
}

Ensure(spindle_common_rulebase, pred_add_matchnode_passes_the_priority_when_adding_a_domain_match) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_node *matchnode = (librdf_node *) 0xA02;
	librdf_node *predicate = (librdf_node *) 0xB02;
	librdf_node *object = (librdf_node *) 0xC02;
	librdf_node *predicate_2 = (librdf_node *) 0xD02;
	librdf_node *object_2 = (librdf_node *) 0xE02;
	librdf_node *predicate_3 = (librdf_node *) 0xF02;
	librdf_node *object_3 = (librdf_node *) 0xF02;
	librdf_statement *statement = (librdf_statement *) 0xA03;
	librdf_statement *result = (librdf_statement *) 0xB03;
	librdf_statement *result_2 = (librdf_statement *) 0xC03;
	librdf_statement *result_3 = (librdf_statement *) 0xD03;
	librdf_stream *stream = (librdf_stream *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	librdf_uri *object_uri = (librdf_uri *) 0xB05;
	librdf_uri *predicate_uri_2 = (librdf_uri *) 0xC05;
	librdf_uri *predicate_uri_3 = (librdf_uri *) 0xD05;
	librdf_world *world = (librdf_world *) 0xA06;
	const char *matchuri = "match uri";
	const char *spindle_expressed_as = NS_SPINDLE "expressedAs";
	const char *rdfs_domain = NS_RDFS "domain";
	const char *olo_index = NS_OLO "index";
	const char *objuri = "object";
	int inverse = 0;
	int priority = 333;

	expect(twine_rdf_world, will_return(world));
	expect(librdf_new_node_from_node, when(node, is_equal_to(matchnode)));
	expect(librdf_new_statement_from_nodes, will_return(statement), when(world, is_equal_to(world)), when(subject, is_equal_to(0)), when(predicate, is_equal_to(0)), when(object, is_equal_to(0)));
	expect(librdf_model_find_statements, will_return(stream), when(model, is_equal_to(model)), when(statement, is_equal_to(statement)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(result), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(result)));
	expect(librdf_statement_get_object, will_return(object), when(statement, is_equal_to(result)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(spindle_expressed_as), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(object)));
	expect(librdf_node_get_uri, will_return(object_uri), when(node, is_equal_to(object)));
	expect(librdf_uri_as_string, will_return(objuri), when(uri, is_equal_to(object_uri)));
	expect(spindle_rulebase_cachepred_add, when(rules, is_equal_to(&rules)), when(uri, is_equal_to(objuri)));
	expect(spindle_rulebase_cachepred_add, when(rules, is_equal_to(&rules)), when(uri, is_equal_to(matchuri)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(result_2), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate_2), when(statement, is_equal_to(result_2)));
	expect(librdf_statement_get_object, will_return(object_2), when(statement, is_equal_to(result_2)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate_2)));
	expect(librdf_node_get_uri, will_return(predicate_uri_2), when(node, is_equal_to(predicate_2)));
	expect(librdf_uri_as_string, will_return(rdfs_domain), when(uri, is_equal_to(predicate_uri_2)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(result_3), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate_3), when(statement, is_equal_to(result_3)));
	expect(librdf_statement_get_object, will_return(object_3), when(statement, is_equal_to(result_3)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate_3)));
	expect(librdf_node_get_uri, will_return(predicate_uri_3), when(node, is_equal_to(predicate_3)));
	expect(librdf_uri_as_string, will_return(olo_index), when(uri, is_equal_to(predicate_uri_3)));
	expect(twine_rdf_node_intval, will_return(priority), will_set_contents_of_parameter(value, &priority, sizeof priority), when(node, is_equal_to(object_3)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(stream)));
	expect(librdf_free_stream, when(stream, is_equal_to(stream)));
	expect(librdf_free_statement, when(statement, is_equal_to(statement)));

	librdf_node *subject_copy = (librdf_node *) 0xA10;
	librdf_node *predicate_copy = (librdf_node *) 0xA11;
	librdf_node *domain = (librdf_node *) 0xA12;
	librdf_statement *query_2 = (librdf_statement *) 0xB10;
	librdf_statement *result_4 = (librdf_statement *) 0xB11;
	librdf_stream *results_2 = (librdf_stream *) 0xC10;
	librdf_uri *domain_uri = (librdf_uri *) 0xD10;
	const char *domain_str = "<domain>";

	expect(librdf_new_node_from_node, will_return(subject_copy), when(node, is_equal_to(matchnode)));
	expect(librdf_new_node_from_uri_string, will_return(predicate_copy), when(world, is_equal_to(world)), when(uri, is_equal_to_string(rdfs_domain)));
	expect(librdf_new_statement_from_nodes, will_return(query_2), when(world, is_equal_to(world)), when(subject, is_equal_to(subject_copy)), when(predicate, is_equal_to(predicate_copy)), when(object, is_null));
	expect(librdf_model_find_statements, will_return(results_2), when(model, is_equal_to(model)), when(statement, is_equal_to(query_2)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(results_2)));
	expect(librdf_stream_get_object, will_return(result_4), when(stream, is_equal_to(results_2)));
	expect(librdf_statement_get_object, will_return(domain), when(statement, is_equal_to(result_4)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(domain)));
	expect(librdf_node_get_uri, will_return(domain_uri), when(node, is_equal_to(domain)));
	expect(librdf_uri_as_string, will_return(domain_str), when(uri, is_equal_to(domain_uri)));
	expect(librdf_stream_next, when(stream, is_equal_to(results_2)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(results_2)));
	expect(librdf_free_stream, when(stream, is_equal_to(results_2)));
	expect(librdf_free_statement, when(statement, is_equal_to(query_2)));

	int r = spindle_rulebase_pred_add_matchnode(&rules, model, matchuri, matchnode, inverse);
	assert_that(r, is_equal_to(0));
	assert_that(rules.predcount, is_equal_to(1));
	assert_that(rules.predicates[0].target, is_equal_to_string(objuri));
	assert_that(rules.predicates[0].matchcount, is_equal_to(1));
	assert_that(rules.predicates[0].matches[0].predicate, is_equal_to_string(matchuri));
	assert_that(rules.predicates[0].matches[0].onlyfor, is_equal_to_string(domain_str));
	assert_that(rules.predicates[0].matches[0].priority, is_equal_to(priority));
}

Ensure(spindle_common_rulebase, pred_add_matchnode_passes_the_prominence_when_adding_a_domain_match) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_node *matchnode = (librdf_node *) 0xA02;
	librdf_node *predicate = (librdf_node *) 0xB02;
	librdf_node *object = (librdf_node *) 0xC02;
	librdf_node *predicate_2 = (librdf_node *) 0xD02;
	librdf_node *object_2 = (librdf_node *) 0xE02;
	librdf_node *predicate_3 = (librdf_node *) 0xF02;
	librdf_node *object_3 = (librdf_node *) 0xF02;
	librdf_statement *statement = (librdf_statement *) 0xA03;
	librdf_statement *result = (librdf_statement *) 0xB03;
	librdf_statement *result_2 = (librdf_statement *) 0xC03;
	librdf_statement *result_3 = (librdf_statement *) 0xD03;
	librdf_stream *stream = (librdf_stream *) 0xA04;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA05;
	librdf_uri *object_uri = (librdf_uri *) 0xB05;
	librdf_uri *predicate_uri_2 = (librdf_uri *) 0xC05;
	librdf_uri *predicate_uri_3 = (librdf_uri *) 0xD05;
	librdf_world *world = (librdf_world *) 0xA06;
	const char *matchuri = "match uri";
	const char *spindle_expressed_as = NS_SPINDLE "expressedAs";
	const char *rdfs_domain = NS_RDFS "domain";
	const char *spindle_prominence = NS_SPINDLE "prominence";
	const char *objuri = "object";
	int inverse = 0;
	int prominence = 333;

	expect(twine_rdf_world, will_return(world));
	expect(librdf_new_node_from_node, when(node, is_equal_to(matchnode)));
	expect(librdf_new_statement_from_nodes, will_return(statement), when(world, is_equal_to(world)), when(subject, is_equal_to(0)), when(predicate, is_equal_to(0)), when(object, is_equal_to(0)));
	expect(librdf_model_find_statements, will_return(stream), when(model, is_equal_to(model)), when(statement, is_equal_to(statement)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(result), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(result)));
	expect(librdf_statement_get_object, will_return(object), when(statement, is_equal_to(result)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(spindle_expressed_as), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(object)));
	expect(librdf_node_get_uri, will_return(object_uri), when(node, is_equal_to(object)));
	expect(librdf_uri_as_string, will_return(objuri), when(uri, is_equal_to(object_uri)));
	expect(spindle_rulebase_cachepred_add, when(rules, is_equal_to(&rules)), when(uri, is_equal_to(objuri)));
	expect(spindle_rulebase_cachepred_add, when(rules, is_equal_to(&rules)), when(uri, is_equal_to(matchuri)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(result_2), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate_2), when(statement, is_equal_to(result_2)));
	expect(librdf_statement_get_object, will_return(object_2), when(statement, is_equal_to(result_2)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate_2)));
	expect(librdf_node_get_uri, will_return(predicate_uri_2), when(node, is_equal_to(predicate_2)));
	expect(librdf_uri_as_string, will_return(rdfs_domain), when(uri, is_equal_to(predicate_uri_2)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(result_3), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate_3), when(statement, is_equal_to(result_3)));
	expect(librdf_statement_get_object, will_return(object_3), when(statement, is_equal_to(result_3)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(predicate_3)));
	expect(librdf_node_get_uri, will_return(predicate_uri_3), when(node, is_equal_to(predicate_3)));
	expect(librdf_uri_as_string, will_return(spindle_prominence), when(uri, is_equal_to(predicate_uri_3)));
	expect(twine_rdf_node_intval, will_return(prominence), will_set_contents_of_parameter(value, &prominence, sizeof prominence), when(node, is_equal_to(object_3)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(stream)));
	expect(librdf_free_stream, when(stream, is_equal_to(stream)));
	expect(librdf_free_statement, when(statement, is_equal_to(statement)));

	librdf_node *subject_copy = (librdf_node *) 0xA10;
	librdf_node *predicate_copy = (librdf_node *) 0xA11;
	librdf_node *domain = (librdf_node *) 0xA12;
	librdf_statement *query_2 = (librdf_statement *) 0xB10;
	librdf_statement *result_4 = (librdf_statement *) 0xB11;
	librdf_stream *results_2 = (librdf_stream *) 0xC10;
	librdf_uri *domain_uri = (librdf_uri *) 0xD10;
	const char *domain_str = "<domain>";

	expect(librdf_new_node_from_node, will_return(subject_copy), when(node, is_equal_to(matchnode)));
	expect(librdf_new_node_from_uri_string, will_return(predicate_copy), when(world, is_equal_to(world)), when(uri, is_equal_to_string(rdfs_domain)));
	expect(librdf_new_statement_from_nodes, will_return(query_2), when(world, is_equal_to(world)), when(subject, is_equal_to(subject_copy)), when(predicate, is_equal_to(predicate_copy)), when(object, is_null));
	expect(librdf_model_find_statements, will_return(results_2), when(model, is_equal_to(model)), when(statement, is_equal_to(query_2)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(results_2)));
	expect(librdf_stream_get_object, will_return(result_4), when(stream, is_equal_to(results_2)));
	expect(librdf_statement_get_object, will_return(domain), when(statement, is_equal_to(result_4)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(domain)));
	expect(librdf_node_get_uri, will_return(domain_uri), when(node, is_equal_to(domain)));
	expect(librdf_uri_as_string, will_return(domain_str), when(uri, is_equal_to(domain_uri)));
	expect(librdf_stream_next, when(stream, is_equal_to(results_2)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(results_2)));
	expect(librdf_free_stream, when(stream, is_equal_to(results_2)));
	expect(librdf_free_statement, when(statement, is_equal_to(query_2)));

	int r = spindle_rulebase_pred_add_matchnode(&rules, model, matchuri, matchnode, inverse);
	assert_that(r, is_equal_to(0));
	assert_that(rules.predcount, is_equal_to(1));
	assert_that(rules.predicates[0].target, is_equal_to_string(objuri));
	assert_that(rules.predicates[0].matchcount, is_equal_to(1));
	assert_that(rules.predicates[0].matches[0].predicate, is_equal_to_string(matchuri));
	assert_that(rules.predicates[0].matches[0].onlyfor, is_equal_to_string(domain_str));
	assert_that(rules.predicates[0].matches[0].prominence, is_equal_to(prominence));
}

#pragma mark -
#pragma mark spindle_rulebase_pred_add_node

Ensure(spindle_common_rulebase, pred_add_node_returns_success_and_adds_the_predicate_to_the_rulebase_and_loops_over_all_statements_in_the_model_whos_subject_is_the_node) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA00;
	librdf_node *node = (librdf_node *) 0xA10;
	librdf_node *subject = (librdf_node *) 0xA11;
	librdf_statement *query = (librdf_stream *) 0xA20;
	librdf_stream *results = (librdf_stream *) 0xA30;
	librdf_world *world = (librdf_world *) 0xA40;
	const char *uri = "node as string";

	expect(twine_rdf_world, will_return(world));
	expect(spindle_rulebase_cachepred_add, will_return(0), when(rules, is_equal_to(&rules)), when(uri, is_equal_to(uri)));
	expect(librdf_new_node_from_node, will_return(subject), when(node, is_equal_to(node)));
	expect(librdf_new_statement_from_nodes, will_return(query), when(world, is_equal_to(world)), when(subject, is_equal_to(subject)), when(predicate, is_null), when(object, is_null));
	expect(librdf_model_find_statements, will_return(results), when(model, is_equal_to(model)), when(statement, is_equal_to(query)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(results)));
	expect(librdf_free_stream, when(stream, is_equal_to(results)));
	expect(librdf_free_statement, when(statement, is_equal_to(query)));

	int r = spindle_rulebase_pred_add_node(&rules, model, uri, node);
	assert_that(r, is_equal_to(1));
	assert_that(rules.predcount, is_equal_to(1));
	assert_that(rules.predicates[0].target, is_equal_to_string(uri));
}

Ensure(spindle_common_rulebase, pred_add_node_returns_success_and_adds_the_predicate_to_the_rulebase_and_sets_the_score_when_present_in_the_model) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA00;
	librdf_node *node = (librdf_node *) 0xA10;
	librdf_node *subject = (librdf_node *) 0xA11;
	librdf_node *predicate = (librdf_node *) 0xA12;
	librdf_statement *query = (librdf_stream *) 0xA20;
	librdf_statement *result = (librdf_stream *) 0xA21;
	librdf_stream *results = (librdf_stream *) 0xA30;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA40;
	librdf_world *world = (librdf_world *) 0xA50;
	const char *uri = "node as string";
	const char *olo_index = NS_OLO "index";
	long score = 999;

	expect(twine_rdf_world, will_return(world));
	expect(spindle_rulebase_cachepred_add, will_return(0), when(rules, is_equal_to(&rules)), when(uri, is_equal_to(uri)));
	expect(librdf_new_node_from_node, will_return(subject), when(node, is_equal_to(node)));
	expect(librdf_new_statement_from_nodes, will_return(query), when(world, is_equal_to(world)), when(subject, is_equal_to(subject)), when(predicate, is_null), when(object, is_null));
	expect(librdf_model_find_statements, will_return(results), when(model, is_equal_to(model)), when(statement, is_equal_to(query)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(results)));
	expect(librdf_stream_get_object, will_return(result), when(stream, is_equal_to(results)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(result)));
	// does not check the predicate is a URI (librdf_node_is_resource)
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(olo_index), when(uri, is_equal_to(predicate_uri)));
	expect(twine_rdf_st_obj_intval, will_return(1), will_set_contents_of_parameter(value, &score, sizeof score), when(statement, is_equal_to(result)));
	expect(librdf_stream_next, when(stream, is_equal_to(results)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(results)));
	expect(librdf_free_stream, when(stream, is_equal_to(results)));
	expect(librdf_free_statement, when(statement, is_equal_to(query)));

	int r = spindle_rulebase_pred_add_node(&rules, model, uri, node);
	assert_that(r, is_equal_to(1));
	assert_that(rules.predcount, is_equal_to(1));
	assert_that(rules.predicates[0].target, is_equal_to_string(uri));
	assert_that(rules.predicates[0].score, is_equal_to(score));
}

Ensure(spindle_common_rulebase, pred_add_node_returns_success_and_adds_the_predicate_to_the_rulebase_and_sets_the_prominence_when_present_in_the_model) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA00;
	librdf_node *node = (librdf_node *) 0xA10;
	librdf_node *subject = (librdf_node *) 0xA11;
	librdf_node *predicate = (librdf_node *) 0xA12;
	librdf_statement *query = (librdf_stream *) 0xA20;
	librdf_statement *result = (librdf_stream *) 0xA21;
	librdf_stream *results = (librdf_stream *) 0xA30;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA40;
	librdf_world *world = (librdf_world *) 0xA50;
	const char *uri = "node as string";
	const char *spindle_prominence = NS_SPINDLE "prominence";
	long prominence = 999;

	expect(twine_rdf_world, will_return(world));
	expect(spindle_rulebase_cachepred_add, will_return(0), when(rules, is_equal_to(&rules)), when(uri, is_equal_to(uri)));
	expect(librdf_new_node_from_node, will_return(subject), when(node, is_equal_to(node)));
	expect(librdf_new_statement_from_nodes, will_return(query), when(world, is_equal_to(world)), when(subject, is_equal_to(subject)), when(predicate, is_null), when(object, is_null));
	expect(librdf_model_find_statements, will_return(results), when(model, is_equal_to(model)), when(statement, is_equal_to(query)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(results)));
	expect(librdf_stream_get_object, will_return(result), when(stream, is_equal_to(results)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(result)));
	// does not check the predicate is a URI (librdf_node_is_resource)
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(spindle_prominence), when(uri, is_equal_to(predicate_uri)));
	expect(twine_rdf_st_obj_intval, will_return(1), will_set_contents_of_parameter(value, &prominence, sizeof prominence), when(statement, is_equal_to(result)));
	expect(librdf_stream_next, when(stream, is_equal_to(results)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(results)));
	expect(librdf_free_stream, when(stream, is_equal_to(results)));
	expect(librdf_free_statement, when(statement, is_equal_to(query)));

	int r = spindle_rulebase_pred_add_node(&rules, model, uri, node);
	assert_that(r, is_equal_to(1));
	assert_that(rules.predcount, is_equal_to(1));
	assert_that(rules.predicates[0].target, is_equal_to_string(uri));
	assert_that(rules.predicates[0].prominence, is_equal_to(prominence));
}

Ensure(spindle_common_rulebase, pred_add_node_returns_success_and_adds_the_predicate_to_the_rulebase_and_sets_the_expected_term_type_when_present_in_the_model) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA00;
	librdf_node *node = (librdf_node *) 0xA10;
	librdf_node *subject = (librdf_node *) 0xA11;
	librdf_node *predicate = (librdf_node *) 0xA12;
	librdf_node *object = (librdf_node *) 0xA13;
	librdf_statement *query = (librdf_stream *) 0xA20;
	librdf_statement *result = (librdf_stream *) 0xA21;
	librdf_stream *results = (librdf_stream *) 0xA30;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA40;
	librdf_uri *object_uri = (librdf_uri *) 0xA41;
	librdf_world *world = (librdf_world *) 0xA50;
	const char *uri = "node as string";
	const char *spindle_expect = NS_SPINDLE "expect";
	const char *rdfs_resource = NS_RDFS "Resource";
	raptor_term_type expected_term_type = RAPTOR_TERM_TYPE_URI;

	expect(twine_rdf_world, will_return(world));
	expect(spindle_rulebase_cachepred_add, will_return(0), when(rules, is_equal_to(&rules)), when(uri, is_equal_to(uri)));
	expect(librdf_new_node_from_node, will_return(subject), when(node, is_equal_to(node)));
	expect(librdf_new_statement_from_nodes, will_return(query), when(world, is_equal_to(world)), when(subject, is_equal_to(subject)), when(predicate, is_null), when(object, is_null));
	expect(librdf_model_find_statements, will_return(results), when(model, is_equal_to(model)), when(statement, is_equal_to(query)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(results)));
	expect(librdf_stream_get_object, will_return(result), when(stream, is_equal_to(results)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(result)));
	// does not check the predicate is a URI (librdf_node_is_resource)
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(spindle_expect), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_statement_get_object, will_return(object), when(statement, is_equal_to(result)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(object)));
	expect(librdf_node_get_uri, will_return(object_uri), when(node, is_equal_to(object)));
	expect(librdf_uri_as_string, will_return(rdfs_resource), when(uri, is_equal_to(object_uri)));
	expect(librdf_stream_next, when(stream, is_equal_to(results)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(results)));
	expect(librdf_free_stream, when(stream, is_equal_to(results)));
	expect(librdf_free_statement, when(statement, is_equal_to(query)));

	int r = spindle_rulebase_pred_add_node(&rules, model, uri, node);
	assert_that(r, is_equal_to(1));
	assert_that(rules.predcount, is_equal_to(1));
	assert_that(rules.predicates[0].target, is_equal_to_string(uri));
	assert_that(rules.predicates[0].expected, is_equal_to(expected_term_type));
}

Ensure(spindle_common_rulebase, pred_add_node_returns_success_and_adds_the_predicate_to_the_rulebase_and_sets_the_expected_literal_type_when_present_in_the_model) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA00;
	librdf_node *node = (librdf_node *) 0xA10;
	librdf_node *subject = (librdf_node *) 0xA11;
	librdf_node *predicate = (librdf_node *) 0xA12;
	librdf_node *object = (librdf_node *) 0xA13;
	librdf_statement *query = (librdf_stream *) 0xA20;
	librdf_statement *result = (librdf_stream *) 0xA21;
	librdf_stream *results = (librdf_stream *) 0xA30;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA40;
	librdf_uri *object_uri = (librdf_uri *) 0xA41;
	librdf_world *world = (librdf_world *) 0xA50;
	const char *uri = "node as string";
	const char *spindle_expect = NS_SPINDLE "expectType";
	const char *expected_literal_type = NS_XSD "nonNegativeInteger";

	expect(twine_rdf_world, will_return(world));
	expect(spindle_rulebase_cachepred_add, will_return(0), when(rules, is_equal_to(&rules)), when(uri, is_equal_to(uri)));
	expect(librdf_new_node_from_node, will_return(subject), when(node, is_equal_to(node)));
	expect(librdf_new_statement_from_nodes, will_return(query), when(world, is_equal_to(world)), when(subject, is_equal_to(subject)), when(predicate, is_null), when(object, is_null));
	expect(librdf_model_find_statements, will_return(results), when(model, is_equal_to(model)), when(statement, is_equal_to(query)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(results)));
	expect(librdf_stream_get_object, will_return(result), when(stream, is_equal_to(results)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(result)));
	// does not check the predicate is a URI (librdf_node_is_resource)
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(spindle_expect), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_statement_get_object, will_return(object), when(statement, is_equal_to(result)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(object)));
	expect(librdf_node_get_uri, will_return(object_uri), when(node, is_equal_to(object)));
	expect(librdf_uri_as_string, will_return(expected_literal_type), when(uri, is_equal_to(object_uri)));
	expect(librdf_stream_next, when(stream, is_equal_to(results)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(results)));
	expect(librdf_free_stream, when(stream, is_equal_to(results)));
	expect(librdf_free_statement, when(statement, is_equal_to(query)));

	int r = spindle_rulebase_pred_add_node(&rules, model, uri, node);
	assert_that(r, is_equal_to(1));
	assert_that(rules.predcount, is_equal_to(1));
	assert_that(rules.predicates[0].target, is_equal_to_string(uri));
	assert_that(rules.predicates[0].datatype, is_equal_to_string(expected_literal_type));
}

Ensure(spindle_common_rulebase, pred_add_node_returns_success_and_adds_the_predicate_to_the_rulebase_and_sets_the_proxy_only_flag_when_present_in_the_model) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA00;
	librdf_node *node = (librdf_node *) 0xA10;
	librdf_node *subject = (librdf_node *) 0xA11;
	librdf_node *predicate = (librdf_node *) 0xA12;
	librdf_node *object = (librdf_node *) 0xA13;
	librdf_statement *query = (librdf_stream *) 0xA20;
	librdf_statement *result = (librdf_stream *) 0xA21;
	librdf_stream *results = (librdf_stream *) 0xA30;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA40;
	librdf_uri *object_dt_uri = (librdf_uri *) 0xA41;
	librdf_world *world = (librdf_world *) 0xA50;
	const char *uri = "node as string";
	const char *spindle_proxyonly = NS_SPINDLE "proxyOnly";
	const char *xsd_boolean = NS_XSD "boolean";
	int expected_proxy_only_flag = 1;

	expect(twine_rdf_world, will_return(world));
	expect(spindle_rulebase_cachepred_add, will_return(0), when(rules, is_equal_to(&rules)), when(uri, is_equal_to(uri)));
	expect(librdf_new_node_from_node, will_return(subject), when(node, is_equal_to(node)));
	expect(librdf_new_statement_from_nodes, will_return(query), when(world, is_equal_to(world)), when(subject, is_equal_to(subject)), when(predicate, is_null), when(object, is_null));
	expect(librdf_model_find_statements, will_return(results), when(model, is_equal_to(model)), when(statement, is_equal_to(query)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(results)));
	expect(librdf_stream_get_object, will_return(result), when(stream, is_equal_to(results)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(result)));
	// does not check the predicate is a URI (librdf_node_is_resource)
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(spindle_proxyonly), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_statement_get_object, will_return(object), when(statement, is_equal_to(result)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(object)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(object_dt_uri), when(node, is_equal_to(object)));
	expect(librdf_uri_as_string, will_return(xsd_boolean), when(uri, is_equal_to(object_dt_uri)));
	expect(librdf_node_get_literal_value, will_return("true"), when(node, is_equal_to(object)));
	expect(librdf_stream_next, when(stream, is_equal_to(results)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(results)));
	expect(librdf_free_stream, when(stream, is_equal_to(results)));
	expect(librdf_free_statement, when(statement, is_equal_to(query)));

	int r = spindle_rulebase_pred_add_node(&rules, model, uri, node);
	assert_that(r, is_equal_to(1));
	assert_that(rules.predcount, is_equal_to(1));
	assert_that(rules.predicates[0].target, is_equal_to_string(uri));
	assert_that(rules.predicates[0].proxyonly, is_equal_to(expected_proxy_only_flag));
}

Ensure(spindle_common_rulebase, pred_add_node_returns_success_and_adds_the_predicate_to_the_rulebase_and_sets_the_indexed_flag_when_present_in_the_model) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA00;
	librdf_node *node = (librdf_node *) 0xA10;
	librdf_node *subject = (librdf_node *) 0xA11;
	librdf_node *predicate = (librdf_node *) 0xA12;
	librdf_node *object = (librdf_node *) 0xA13;
	librdf_statement *query = (librdf_stream *) 0xA20;
	librdf_statement *result = (librdf_stream *) 0xA21;
	librdf_stream *results = (librdf_stream *) 0xA30;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA40;
	librdf_uri *object_dt_uri = (librdf_uri *) 0xA41;
	librdf_world *world = (librdf_world *) 0xA50;
	const char *uri = "node as string";
	const char *spindle_indexed = NS_SPINDLE "indexed";
	const char *xsd_boolean = NS_XSD "boolean";
	int expected_indexed_flag = 1;

	expect(twine_rdf_world, will_return(world));
	expect(spindle_rulebase_cachepred_add, will_return(0), when(rules, is_equal_to(&rules)), when(uri, is_equal_to(uri)));
	expect(librdf_new_node_from_node, will_return(subject), when(node, is_equal_to(node)));
	expect(librdf_new_statement_from_nodes, will_return(query), when(world, is_equal_to(world)), when(subject, is_equal_to(subject)), when(predicate, is_null), when(object, is_null));
	expect(librdf_model_find_statements, will_return(results), when(model, is_equal_to(model)), when(statement, is_equal_to(query)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(results)));
	expect(librdf_stream_get_object, will_return(result), when(stream, is_equal_to(results)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(result)));
	// does not check the predicate is a URI (librdf_node_is_resource)
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(spindle_indexed), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_statement_get_object, will_return(object), when(statement, is_equal_to(result)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(object)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(object_dt_uri), when(node, is_equal_to(object)));
	expect(librdf_uri_as_string, will_return(xsd_boolean), when(uri, is_equal_to(object_dt_uri)));
	expect(librdf_node_get_literal_value, will_return("true"), when(node, is_equal_to(object)));
	expect(librdf_stream_next, when(stream, is_equal_to(results)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(results)));
	expect(librdf_free_stream, when(stream, is_equal_to(results)));
	expect(librdf_free_statement, when(statement, is_equal_to(query)));

	int r = spindle_rulebase_pred_add_node(&rules, model, uri, node);
	assert_that(r, is_equal_to(1));
	assert_that(rules.predcount, is_equal_to(1));
	assert_that(rules.predicates[0].target, is_equal_to_string(uri));
	assert_that(rules.predicates[0].indexed, is_equal_to(expected_indexed_flag));
}

Ensure(spindle_common_rulebase, pred_add_node_returns_success_and_adds_the_predicate_to_the_rulebase_and_sets_the_inverse_flag_when_present_in_the_model) {
	SPINDLERULES rules = { 0 };
	librdf_model *model = (librdf_model *) 0xA00;
	librdf_node *node = (librdf_node *) 0xA10;
	librdf_node *subject = (librdf_node *) 0xA11;
	librdf_node *predicate = (librdf_node *) 0xA12;
	librdf_node *object = (librdf_node *) 0xA13;
	librdf_statement *query = (librdf_stream *) 0xA20;
	librdf_statement *result = (librdf_stream *) 0xA21;
	librdf_stream *results = (librdf_stream *) 0xA30;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA40;
	librdf_uri *object_dt_uri = (librdf_uri *) 0xA41;
	librdf_world *world = (librdf_world *) 0xA50;
	const char *uri = "node as string";
	const char *spindle_inverse = NS_SPINDLE "inverse";
	const char *xsd_boolean = NS_XSD "boolean";
	int expected_inverse_flag = 1;

	expect(twine_rdf_world, will_return(world));
	expect(spindle_rulebase_cachepred_add, will_return(0), when(rules, is_equal_to(&rules)), when(uri, is_equal_to(uri)));
	expect(librdf_new_node_from_node, will_return(subject), when(node, is_equal_to(node)));
	expect(librdf_new_statement_from_nodes, will_return(query), when(world, is_equal_to(world)), when(subject, is_equal_to(subject)), when(predicate, is_null), when(object, is_null));
	expect(librdf_model_find_statements, will_return(results), when(model, is_equal_to(model)), when(statement, is_equal_to(query)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(results)));
	expect(librdf_stream_get_object, will_return(result), when(stream, is_equal_to(results)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(result)));
	// does not check the predicate is a URI (librdf_node_is_resource)
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(spindle_inverse), when(uri, is_equal_to(predicate_uri)));
	expect(librdf_statement_get_object, will_return(object), when(statement, is_equal_to(result)));
	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(object)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(object_dt_uri), when(node, is_equal_to(object)));
	expect(librdf_uri_as_string, will_return(xsd_boolean), when(uri, is_equal_to(object_dt_uri)));
	expect(librdf_node_get_literal_value, will_return("true"), when(node, is_equal_to(object)));
	expect(librdf_stream_next, when(stream, is_equal_to(results)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(results)));
	expect(librdf_free_stream, when(stream, is_equal_to(results)));
	expect(librdf_free_statement, when(statement, is_equal_to(query)));

	int r = spindle_rulebase_pred_add_node(&rules, model, uri, node);
	assert_that(r, is_equal_to(1));
	assert_that(rules.predcount, is_equal_to(1));
	assert_that(rules.predicates[0].target, is_equal_to_string(uri));
	assert_that(rules.predicates[0].inverse, is_equal_to(expected_inverse_flag));
}

#pragma mark -

TestSuite *rulebase_pred_test_suite(void) {
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
	add_test_with_context(suite, spindle_common_rulebase, pred_set_expecttype_returns_success_if_the_statement_object_is_not_a_resource);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_expecttype_returns_success_and_sets_the_expected_data_type_if_the_statement_object_is_a_resource);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_expect_returns_false_if_the_statement_object_is_not_a_resource);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_expect_returns_false_if_the_statement_object_is_not_rdfs_literal_nor_rdfs_resource);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_expect_returns_true_and_sets_the_expected_term_type_to_raptor_literal_if_the_statement_object_is_rdfs_literal);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_expect_returns_true_and_sets_the_expected_term_type_to_raptor_uri_if_the_statement_object_is_rdfs_resource);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_prominence_returns_false_and_does_not_change_prominence_if_getting_intval_fails);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_prominence_returns_false_and_does_not_change_prominence_if_object_intval_is_negative);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_prominence_returns_true_and_sets_prominence_if_object_intval_is_positive);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_score_returns_false_and_does_not_change_score_if_getting_intval_fails);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_score_returns_false_and_does_not_change_score_if_object_intval_is_negative);
	add_test_with_context(suite, spindle_common_rulebase, pred_set_score_returns_true_and_sets_score_if_object_intval_is_positive);
	add_test_with_context(suite, spindle_common_rulebase, pred_dump_returns_no_error_and_only_has_side_effects_when_the_predicate_list_is_empty);
	add_test_with_context(suite, spindle_common_rulebase, pred_dump_returns_no_error_and_only_has_side_effects_when_the_predicate_list_has_items);
	add_test_with_context(suite, spindle_common_rulebase, pred_compare_returns_no_difference_if_both_scores_are_zero);
	add_test_with_context(suite, spindle_common_rulebase, pred_compare_returns_no_difference_if_scores_are_equal_and_non_zero);
	add_test_with_context(suite, spindle_common_rulebase, pred_compare_returns_b_sorts_later_if_b_has_a_higher_score);
	add_test_with_context(suite, spindle_common_rulebase, pred_compare_returns_b_sorts_earlier_if_b_has_a_lower_score);
	add_test_with_context(suite, spindle_common_rulebase, pred_cleanup_only_has_side_effects);
	add_test_with_context(suite, spindle_common_rulebase, pred_finalise_sorts_the_predicate_list_by_score);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_matchnode_returns_no_error_if_model_contains_no_triples_whos_subject_is_the_matchnode);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_matchnode_returns_no_error_if_model_contains_triples_whos_subject_is_the_matchnode_and_triples_predicate_is_not_a_resource);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_matchnode_returns_no_error_if_model_contains_triples_whos_subject_is_the_matchnode_and_triples_object_is_not_a_resource);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_matchnode_returns_no_error_and_adds_a_general_match_for_the_predicate_if_the_model_contains_spindle_expressedas);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_matchnode_returns_no_error_and_adds_a_domain_match_for_the_predicate_if_the_model_contains_spindle_expressedas);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_matchnode_passes_the_priority_when_adding_a_general_match);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_matchnode_passes_the_prominence_when_adding_a_general_match);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_matchnode_passes_the_priority_when_adding_a_domain_match);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_matchnode_passes_the_prominence_when_adding_a_domain_match);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_node_returns_success_and_adds_the_predicate_to_the_rulebase_and_loops_over_all_statements_in_the_model_whos_subject_is_the_node);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_node_returns_success_and_adds_the_predicate_to_the_rulebase_and_sets_the_score_when_present_in_the_model);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_node_returns_success_and_adds_the_predicate_to_the_rulebase_and_sets_the_prominence_when_present_in_the_model);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_node_returns_success_and_adds_the_predicate_to_the_rulebase_and_sets_the_expected_term_type_when_present_in_the_model);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_node_returns_success_and_adds_the_predicate_to_the_rulebase_and_sets_the_expected_literal_type_when_present_in_the_model);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_node_returns_success_and_adds_the_predicate_to_the_rulebase_and_sets_the_proxy_only_flag_when_present_in_the_model);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_node_returns_success_and_adds_the_predicate_to_the_rulebase_and_sets_the_indexed_flag_when_present_in_the_model);
	add_test_with_context(suite, spindle_common_rulebase, pred_add_node_returns_success_and_adds_the_predicate_to_the_rulebase_and_sets_the_inverse_flag_when_present_in_the_model);
	return suite;
}

int run(char *test) {
	if(test) {
		return run_single_test(rulebase_pred_test_suite(), test, create_text_reporter());
	} else {
		return run_test_suite(rulebase_pred_test_suite(), create_text_reporter());
	}
}

int main(int argc, char **argv) {
	return run(argc > 1 ? argv[1] : NULL);
}
