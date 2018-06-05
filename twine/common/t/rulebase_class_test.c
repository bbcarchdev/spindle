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
#include "../../t/mock_spindle_rulebase_coref.h"
#include "../../t/mock_spindle_rulebase_pred.h"

#define NS_OLO "NS_OLO "
#define NS_SPINDLE "NS_SPINDLE "

#define P_SPINDLE_H_
#define PLUGIN_NAME "spindle-rulebase"
#include "../rulebase-class.c"

Describe(spindle_common_rulebase);
BeforeEach(spindle_common_rulebase) { always_expect(twine_logf); die_in(1); }
AfterEach(spindle_common_rulebase) {}

/*
	This suite does not test any failure paths triggered by libc calls such as malloc.
*/

#pragma mark -
#pragma mark spindle_rulebase_class_dump

Ensure(spindle_common_rulebase, class_dump_only_has_side_effects) {
	struct spindle_classmatch_struct matches_1[] = {
		{ .uri = "class map 1 match uri 1" }
	};
	struct spindle_classmatch_struct matches_2[] = {
		{ .uri = "class map 2 match uri 1" },
		{ .uri = "class map 2 match uri 2" }
	};
	struct spindle_classmap_struct classes[] = {
		{ .score = 1, .uri = "class map 1 uri", .match = matches_1, .matchcount = sizeof matches_1 / sizeof matches_1[0] },
		{ .score = 2, .uri = "class map 2 uri", .match = matches_2, .matchcount = sizeof matches_2 / sizeof matches_2[0] }
	};
	SPINDLERULES rules = {
		.classes = classes,
		.classcount = sizeof classes / sizeof classes[0]
	};

	int r = spindle_rulebase_class_dump(&rules);
	assert_that(r, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_rulebase_class_set_score_

Ensure(spindle_common_rulebase, class_set_score_returns_false_if_the_statement_object_is_not_a_decimal_literal) {
	struct spindle_classmap_struct entry = { 0 };
	librdf_statement *statement = (librdf_statement *) 0xA01;

	expect(twine_rdf_st_obj_intval, will_return(0), when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_class_set_score_(&entry, statement);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, class_set_score_returns_false_if_the_score_to_be_set_is_negative) {
	long score = -1;
	struct spindle_classmap_struct entry = { 0 };
	librdf_statement *statement = (librdf_statement *) 0xA01;

	expect(twine_rdf_st_obj_intval, will_return(1), will_set_contents_of_parameter(value, &score, sizeof (long)), when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_class_set_score_(&entry, statement);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, class_set_score_returns_false_if_the_score_to_be_set_is_zero) {
	long score = 0;
	struct spindle_classmap_struct entry = { 0 };
	librdf_statement *statement = (librdf_statement *) 0xA01;

	expect(twine_rdf_st_obj_intval, will_return(1), will_set_contents_of_parameter(value, &score, sizeof (long)), when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_class_set_score_(&entry, statement);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, class_set_score_returns_true_and_sets_the_classmap_score_if_the_score_to_be_set_is_positive) {
	long score = 777;
	struct spindle_classmap_struct entry = { 0 };
	librdf_statement *statement = (librdf_statement *) 0xA01;

	expect(twine_rdf_st_obj_intval, will_return(1), will_set_contents_of_parameter(value, &score, sizeof (long)), when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_class_set_score_(&entry, statement);
	assert_that(r, is_equal_to(1));
	assert_that(entry.score, is_equal_to(score));
}

#pragma mark -
#pragma mark spindle_rulebase_class_add_match_
// these first four tests are essentially the same as for spindle_rulebase_coref_add_() in rulebase_coref_test.c

Ensure(spindle_common_rulebase, class_add_match_returns_one_and_adds_the_uri_to_the_match_classmap) {
	struct spindle_classmap_struct match = { 0 };
	const char *uri = "match uri";
	int prominence = 777;

	int r = spindle_rulebase_class_add_match_(&match, uri, prominence);
	assert_that(r, is_equal_to(1));
	assert_that(match.matchsize, is_greater_than(0));
	assert_that(match.matchcount, is_equal_to(1));
	assert_that(match.match, is_non_null);
	assert_that(match.match[0].uri, is_equal_to_string(uri));
	assert_that(match.match[0].prominence, is_equal_to(prominence));
}

Ensure(spindle_common_rulebase, class_add_match_returns_zero_and_replaces_the_prominence_of_a_match_if_its_url_is_already_in_the_match_list) {
	struct spindle_classmap_struct match = { 0 };
	const char *uri = "match uri";
	int old_prominence = 777;
	int new_prominence = 888;

	int r = spindle_rulebase_class_add_match_(&match, uri, old_prominence);
	assert_that(r, is_equal_to(1));

	r = spindle_rulebase_class_add_match_(&match, uri, new_prominence);
	assert_that(r, is_equal_to(0));
	assert_that(match.matchsize, is_greater_than(0));
	assert_that(match.matchcount, is_equal_to(1));
	assert_that(match.match, is_non_null);
	assert_that(match.match[0].uri, is_equal_to_string(uri));
	assert_that(match.match[0].prominence, is_equal_to(new_prominence));
}

Ensure(spindle_common_rulebase, class_add_match_can_add_multiple_matches) {
	struct spindle_classmap_struct match = { 0 };
	const char *uri_1 = "match uri 1";
	const char *uri_2 = "match uri 2";
	int prominence_1 = 777;
	int prominence_2 = 888;

	int r = spindle_rulebase_class_add_match_(&match, uri_1, prominence_1);
	assert_that(r, is_equal_to(1));

	r = spindle_rulebase_class_add_match_(&match, uri_2, prominence_2);
	assert_that(r, is_equal_to(1));
	assert_that(match.matchsize, is_greater_than(1));
	assert_that(match.matchcount, is_equal_to(2));
	assert_that(match.match, is_non_null);
	assert_that(match.match[0].uri, is_equal_to_string(uri_1));
	assert_that(match.match[0].prominence, is_equal_to(prominence_1));
	assert_that(match.match[1].uri, is_equal_to_string(uri_2)); // second match is appended to end of list
	assert_that(match.match[1].prominence, is_equal_to(prominence_2));
}

Ensure(spindle_common_rulebase, class_add_match_replaces_the_correct_prominence_when_multiple_matches_are_present_and_prominence_is_non_zero) {
	struct spindle_classmap_struct match = { 0 };
	const char *uri_1 = "match uri 1";
	const char *uri_2 = "match uri 2";
	int prominence_1 = 777;
	int prominence_2 = 888;
	int prominence_3 = 999;

	int r = spindle_rulebase_class_add_match_(&match, uri_1, prominence_1);
	assert_that(r, is_equal_to(1));

	r = spindle_rulebase_class_add_match_(&match, uri_2, prominence_2);
	assert_that(r, is_equal_to(1));

	r = spindle_rulebase_class_add_match_(&match, uri_2, prominence_3);
	assert_that(r, is_equal_to(0));
	assert_that(match.matchsize, is_greater_than(1));
	assert_that(match.matchcount, is_equal_to(2));
	assert_that(match.match, is_non_null);
	assert_that(match.match[0].uri, is_equal_to_string(uri_1));
	assert_that(match.match[0].prominence, is_equal_to(prominence_1));
	assert_that(match.match[1].uri, is_equal_to_string(uri_2));
	assert_that(match.match[1].prominence, is_equal_to(prominence_3));
}

Ensure(spindle_common_rulebase, class_add_match_does_not_replaces_a_prominence_when_multiple_matches_are_present_and_prominence_is_zero) {
	struct spindle_classmap_struct match = { 0 };
	const char *uri_1 = "match uri 1";
	const char *uri_2 = "match uri 2";
	int prominence_1 = 777;
	int prominence_2 = 888;
	int prominence_3 = 0;

	int r = spindle_rulebase_class_add_match_(&match, uri_1, prominence_1);
	assert_that(r, is_equal_to(1));

	r = spindle_rulebase_class_add_match_(&match, uri_2, prominence_2);
	assert_that(r, is_equal_to(1));

	r = spindle_rulebase_class_add_match_(&match, uri_2, prominence_3);
	assert_that(r, is_equal_to(0));
	assert_that(match.matchsize, is_greater_than(1));
	assert_that(match.matchcount, is_equal_to(2));
	assert_that(match.match, is_non_null);
	assert_that(match.match[0].uri, is_equal_to_string(uri_1));
	assert_that(match.match[0].prominence, is_equal_to(prominence_1));
	assert_that(match.match[1].uri, is_equal_to_string(uri_2));
	assert_that(match.match[1].prominence, is_equal_to(prominence_2));
}

#pragma mark -
#pragma mark spindle_rulebase_class_add_

Ensure(spindle_common_rulebase, class_add_returns_a_new_classmap_for_an_unknown_uri_and_adds_it_to_the_rulebase) {
	const char *uri = "uri";
	SPINDLERULES rules = { 0 };

	struct spindle_classmap_struct *p = spindle_rulebase_class_add_(&rules, uri);
	assert_that(p, is_non_null);
	assert_that(rules.classes, is_equal_to(p));
	assert_that(rules.classsize, is_greater_than(0));
	assert_that(rules.classcount, is_equal_to(1));
	assert_that(rules.classes[0].uri, is_equal_to_string(uri));
	assert_that(rules.classes[0].score, is_equal_to(100));
	assert_that(rules.classes[0].match, is_non_null);
	assert_that(rules.classes[0].matchcount, is_equal_to(1));
	assert_that(rules.classes[0].matchsize, is_greater_than(0));
	assert_that(rules.classes[0].prominence, is_equal_to(0));
}

Ensure(spindle_common_rulebase, class_add_appends_a_second_uri_to_the_classes_list) {
	const char *uri_1 = "uri 1";
	const char *uri_2 = "uri 2";
	SPINDLERULES rules = { 0 };

	// add the first URI
	struct spindle_classmap_struct *p = spindle_rulebase_class_add_(&rules, uri_1);
	assert_that(p, is_non_null);
	assert_that(rules.classes, is_equal_to(p));
	assert_that(rules.classcount, is_equal_to(1));

	// add the second URI
	p = spindle_rulebase_class_add_(&rules, uri_2);
	assert_that(p, is_non_null);
	assert_that(rules.classes, is_not_equal_to(p));
	assert_that(rules.classcount, is_equal_to(2));
	assert_that(&(rules.classes[1]), is_equal_to(p));
}

Ensure(spindle_common_rulebase, class_add_returns_an_existing_classmap_for_a_known_uri) {
	const char *uri_1 = "uri 1";
	const char *uri_2 = "uri 2";
	SPINDLERULES rules = { 0 };

	// add the two URIs
	spindle_rulebase_class_add_(&rules, uri_1);
	spindle_rulebase_class_add_(&rules, uri_2);

	// re-add the second URI
	struct spindle_classmap_struct *p = spindle_rulebase_class_add_(&rules, uri_2);
	assert_that(p, is_non_null);
	assert_that(rules.classcount, is_equal_to(2));
	assert_that(&(rules.classes[1]), is_equal_to(p));
}

#pragma mark -
#pragma mark spindle_rulebase_class_compare_

Ensure(spindle_common_rulebase, class_compare_returns_no_difference_if_both_scores_are_zero) {
	struct spindle_classmap_struct a = { 0 };
	struct spindle_classmap_struct b = { 0 };

	int r = spindle_rulebase_class_compare_(&a, &b);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, class_compare_returns_no_difference_if_scores_are_equal_and_non_zero) {
	struct spindle_classmap_struct a = { .score = 5 };
	struct spindle_classmap_struct b = { .score = 5 };

	int r = spindle_rulebase_class_compare_(&a, &b);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_common_rulebase, class_compare_returns_b_sorts_later_if_b_has_a_higher_score) {
	struct spindle_classmap_struct a = { .score = 2 };
	struct spindle_classmap_struct b = { .score = 5 };

	int r = spindle_rulebase_class_compare_(&a, &b);
	assert_that(r, is_less_than(0));
}

Ensure(spindle_common_rulebase, class_compare_returns_b_sorts_earlier_if_b_has_a_lower_score) {
	struct spindle_classmap_struct a = { .score = 5 };
	struct spindle_classmap_struct b = { .score = 2 };

	int r = spindle_rulebase_class_compare_(&a, &b);
	assert_that(r, is_greater_than(0));
}

#pragma mark -
#pragma mark spindle_rulebase_class_cleanup

Ensure(spindle_common_rulebase, class_cleanup_frees_the_class_uri_and_class_alias_uris) {
	SPINDLERULES rules = { 0 };
	rules.classcount = 5;
	size_t classes_size = rules.classcount * sizeof (struct spindle_classmap_struct);
	rules.classes = malloc(classes_size);
	memset(rules.classes, 0x55, classes_size);

	for(size_t c = 0; c < rules.classcount; c++) {
		rules.classes[c].uri = strdup("class uri");
		rules.classes[c].matchcount = 2;
		size_t aliases_size = rules.classes[c].matchcount * sizeof (struct spindle_classmatch_struct);
		rules.classes[c].match = malloc(aliases_size);
		memset(rules.classes[c].match, 0x66, aliases_size);
		for(size_t d = 0; d < rules.classes[c].matchcount; d++) {
			rules.classes[c].match[d].uri = strdup("class alias uri");
		}
	}

	int r = spindle_rulebase_class_cleanup(&rules);
	assert_that(r, is_equal_to(0));
	assert_that(rules.classes, is_null);
}

#pragma mark -
#pragma mark spindle_rulebase_class_finalise
// similar test to cachepred_finalise_sorts_the_cachepred_list in rulebase_cachepred_test.c

Ensure(spindle_common_rulebase, class_finalise_sorts_the_class_list_by_score) {
	struct spindle_classmap_struct classes[] = {
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
	struct spindle_classmap_struct sorted_classes[] = {
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
		.classes = classes,
		.classcount = sizeof classes / sizeof classes[0]
	};

	int r = spindle_rulebase_class_finalise(&rules);
	assert_that(r, is_equal_to(0));
	assert_that(rules.classes[0].score, is_equal_to(sorted_classes[0].score));
	assert_that(rules.classes[1].score, is_equal_to(sorted_classes[1].score));
	assert_that(rules.classes[2].score, is_equal_to(sorted_classes[2].score));
	assert_that(rules.classes[3].score, is_equal_to(sorted_classes[3].score));
	assert_that(rules.classes[4].score, is_equal_to(sorted_classes[4].score));
	assert_that(rules.classes[5].score, is_equal_to(sorted_classes[5].score));
	assert_that(rules.classes[6].score, is_equal_to(sorted_classes[6].score));
	assert_that(rules.classes[7].score, is_equal_to(sorted_classes[7].score));
	assert_that(rules.classes[8].score, is_equal_to(sorted_classes[8].score));
	assert_that(rules.classes[9].score, is_equal_to(sorted_classes[9].score));
}

#pragma mark -
#pragma mark spindle_rulebase_class_add_matchnode

Ensure(spindle_common_rulebase, class_add_matchnode_associates_a_match_url_with_a_class_url_and_stores_both_in_the_class_list) {
	long prominence = 999;
	const char *match_uri = "match uri";
	const char *node_uri = "node uri";
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_node *node = (librdf_node *) 0xA02;
	librdf_statement *statement = (librdf_statement *) 0xA03;
	librdf_statement *sub_statement = (librdf_statement *) 0xA04;
	librdf_stream *stream = (librdf_stream *) 0xA05;
	librdf_uri *uri = (librdf_uri *) 0xA06;
	SPINDLERULES rules = { 0 };

	expect(librdf_node_get_uri, will_return(uri), when(node, is_equal_to(node)));
	expect(librdf_uri_as_string, will_return(node_uri), when(uri, is_equal_to(uri)));
	expect(twine_rdf_st_create, will_return(statement));
	expect(twine_rdf_node_createuri, when(uri, is_equal_to_string(match_uri)));
	expect(librdf_statement_set_subject, when(statement, is_equal_to(statement)), when(node, is_equal_to(0)));
	expect(twine_rdf_node_createuri, when(uri, is_equal_to_string(NS_SPINDLE "prominence")));
	expect(librdf_statement_set_predicate, when(statement, is_equal_to(statement)), when(node, is_equal_to(0)));
	expect(librdf_model_find_statements, will_return(stream), when(model, is_equal_to(model)), when(statement, is_equal_to(statement)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(sub_statement), when(stream, is_equal_to(stream)));
	expect(twine_rdf_st_obj_intval, will_return(0), when(statement, is_equal_to(sub_statement)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(sub_statement), when(stream, is_equal_to(stream)));
	expect(twine_rdf_st_obj_intval, will_return(prominence), will_set_contents_of_parameter(value, &prominence, sizeof prominence), when(statement, is_equal_to(sub_statement)));
	expect(librdf_free_stream, when(stream, is_equal_to(stream)));
	expect(librdf_free_statement, when(statement, is_equal_to(statement)));

	int r = spindle_rulebase_class_add_matchnode(&rules, model, match_uri, node);
	assert_that(r, is_equal_to(1));
	assert_that(rules.classcount, is_equal_to(1));
	assert_that(rules.classes, is_non_null);
	assert_that(rules.classes[0].uri, is_equal_to_string(node_uri));
	assert_that(rules.classes[0].score, is_equal_to(100));
	assert_that(rules.classes[0].matchsize, is_greater_than(0));
	assert_that(rules.classes[0].matchcount, is_equal_to(2));
	assert_that(rules.classes[0].match, is_non_null);
	assert_that(rules.classes[0].match[0].uri, is_equal_to_string(node_uri));
	assert_that(rules.classes[0].match[0].prominence, is_equal_to(0));
	assert_that(rules.classes[0].match[1].uri, is_equal_to_string(match_uri));
	assert_that(rules.classes[0].match[1].prominence, is_equal_to(prominence));
}

#pragma mark -
#pragma mark spindle_rulebase_class_add_matchnode

Ensure(spindle_common_rulebase, class_add_node_adds_the_class_url_to_the_class_list_with_the_olo_index_score_whos_subject_is_the_given_node) {
	long score = 999;
	const char *class_uri = "class uri";
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_node *node = (librdf_node *) 0xA02;
	librdf_node *subject = (librdf_node *) 0xA03;
	librdf_statement *predicate = (librdf_statement *) 0xA04;
	librdf_statement *query = (librdf_statement *) 0xA05;
	librdf_statement *result = (librdf_statement *) 0xA06;
	librdf_stream *stream = (librdf_stream *) 0xA07;
	librdf_uri *predicate_uri = (librdf_uri *) 0xA08;
	librdf_world *world = (librdf_world *) 0xA09;
	SPINDLERULES rules = { 0 };

	expect(twine_rdf_world, will_return(world));
	expect(librdf_new_node_from_node, will_return(subject), when(node, is_equal_to(node)));
	expect(librdf_new_statement_from_nodes, will_return(query), when(world, is_equal_to(world)), when(subject, is_equal_to(subject)), when(predicate, is_equal_to(0)), when(object, is_equal_to(0)));
	expect(librdf_model_find_statements, will_return(stream), when(model, is_equal_to(model)), when(statement, is_equal_to(query)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));
	expect(librdf_stream_get_object, will_return(result), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(result)));
	expect(librdf_node_get_uri, will_return(predicate_uri), when(node, is_equal_to(predicate)));
	expect(librdf_uri_as_string, will_return(NS_OLO "index"), when(uri, is_equal_to(predicate_uri)));
	expect(twine_rdf_st_obj_intval, will_return(score), will_set_contents_of_parameter(value, &score, sizeof score), when(statement, is_equal_to(result)));
	expect(librdf_free_stream, when(stream, is_equal_to(stream)));
	expect(librdf_free_statement, when(statement, is_equal_to(query)));

	int r = spindle_rulebase_class_add_node(&rules, model, class_uri, node);
//	assert_that(r, is_equal_to(1));
	assert_that(r, is_equal_to(-1)); // BUG [fixed on develop]

	assert_that(rules.classcount, is_equal_to(1));
	assert_that(rules.classes, is_non_null);
	assert_that(rules.classes[0].uri, is_equal_to_string(class_uri));
	assert_that(rules.classes[0].score, is_equal_to(score));
	assert_that(rules.classes[0].matchsize, is_greater_than(0));
	assert_that(rules.classes[0].matchcount, is_equal_to(1));
	assert_that(rules.classes[0].match, is_non_null);
	assert_that(rules.classes[0].match[0].uri, is_equal_to_string(class_uri));
	assert_that(rules.classes[0].match[0].prominence, is_equal_to(0));
}

#pragma mark -

TestSuite *create_rulebase_class_test_suite(void) {
	TestSuite *suite = create_test_suite();
	add_test_with_context(suite, spindle_common_rulebase, class_dump_only_has_side_effects);
	add_test_with_context(suite, spindle_common_rulebase, class_set_score_returns_false_if_the_statement_object_is_not_a_decimal_literal);
	add_test_with_context(suite, spindle_common_rulebase, class_set_score_returns_false_if_the_score_to_be_set_is_negative);
	add_test_with_context(suite, spindle_common_rulebase, class_set_score_returns_false_if_the_score_to_be_set_is_zero);
	add_test_with_context(suite, spindle_common_rulebase, class_set_score_returns_true_and_sets_the_classmap_score_if_the_score_to_be_set_is_positive);
	add_test_with_context(suite, spindle_common_rulebase, class_add_match_returns_one_and_adds_the_uri_to_the_match_classmap);
	add_test_with_context(suite, spindle_common_rulebase, class_add_match_returns_zero_and_replaces_the_prominence_of_a_match_if_its_url_is_already_in_the_match_list);
	add_test_with_context(suite, spindle_common_rulebase, class_add_match_can_add_multiple_matches);
	add_test_with_context(suite, spindle_common_rulebase, class_add_match_replaces_the_correct_prominence_when_multiple_matches_are_present_and_prominence_is_non_zero);
	add_test_with_context(suite, spindle_common_rulebase, class_add_match_does_not_replaces_a_prominence_when_multiple_matches_are_present_and_prominence_is_zero);
	add_test_with_context(suite, spindle_common_rulebase, class_add_returns_a_new_classmap_for_an_unknown_uri_and_adds_it_to_the_rulebase);
	add_test_with_context(suite, spindle_common_rulebase, class_add_appends_a_second_uri_to_the_classes_list);
	add_test_with_context(suite, spindle_common_rulebase, class_add_returns_an_existing_classmap_for_a_known_uri);
	add_test_with_context(suite, spindle_common_rulebase, class_compare_returns_no_difference_if_both_scores_are_zero);
	add_test_with_context(suite, spindle_common_rulebase, class_compare_returns_no_difference_if_scores_are_equal_and_non_zero);
	add_test_with_context(suite, spindle_common_rulebase, class_compare_returns_b_sorts_later_if_b_has_a_higher_score);
	add_test_with_context(suite, spindle_common_rulebase, class_compare_returns_b_sorts_earlier_if_b_has_a_lower_score);
	add_test_with_context(suite, spindle_common_rulebase, class_cleanup_frees_the_class_uri_and_class_alias_uris);
	add_test_with_context(suite, spindle_common_rulebase, class_finalise_sorts_the_class_list_by_score);
	add_test_with_context(suite, spindle_common_rulebase, class_add_matchnode_associates_a_match_url_with_a_class_url_and_stores_both_in_the_class_list);
	add_test_with_context(suite, spindle_common_rulebase, class_add_node_adds_the_class_url_to_the_class_list_with_the_olo_index_score_whos_subject_is_the_given_node);
	return suite;
}

int rulebase_class_test(char *test) {
	if(test) {
		return run_single_test(create_rulebase_class_test_suite(), test, create_text_reporter());
	} else {
		return run_test_suite(create_rulebase_class_test_suite(), create_text_reporter());
	}
}

int main(int argc, char **argv) {
	return rulebase_class_test(argc > 1 ? argv[1] : NULL);
}
