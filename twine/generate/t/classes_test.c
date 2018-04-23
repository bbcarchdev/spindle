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

/* mocks of dependancies */
#include "../../t/mock_librdf.h"
#include "../../t/mock_libsql.h"
#include "../../t/mock_libtwine.h"
#include "../../t/mock_spindle_core.h"
#include "../../t/mock_spindle_db_methods.h"
#include "../../t/mock_spindle_proxy_methods.h"
#include "../../t/mock_spindle_strset_methods.h"

#define NS_RDF

/* compile SUT inline due to static functions */
#define P_SPINDLE_GENERATE_H_
#define PLUGIN_NAME "spindle-generate"
#include "../classes.c"

Describe(spindle_generate_classes);
BeforeEach(spindle_generate_classes) { always_expect(twine_logf); die_in(1); }
AfterEach(spindle_generate_classes) {}

/*
	This suite does not test any failure paths triggered by libc calls such as malloc.
*/

#pragma mark -
#pragma mark spindle_class_match

/**
	bool spindle_class_match(proxy cache entry, optional string set);
	"Determine the class of something"

# CONSEQUENCES OF CALLING

	string set is appended with all rdfs:types from the source data
	if returns true:
		after each appended type, string set contains some of that type's matching classes, if any, in descending order of score, with the best one last
		proxy.classname is set to URI of best matching rulebase class for any of the proxy's rdfs:types
	if returns false:
		proxy.classname is set to NULL

# ALGORITHM

	set best score found to 1000 (lower == better)
	find all statements in proxy.sourcedata matching { ?s <rdfs:type> ?o . }  [n.b. code assumes ?s is always the proxy in the sourcedata model]
	for each ?o that is a resource:
		add ?o to the string set parameter if present
		for each "class" in the rulebase that is better than the previous best score:
			if ?o is the same as a match URI for this rulebase class:
				add the rulebase class to the string set parameter if present
				save the rulebase class and match (current iterator values of two `for` loops)
				update the best score found to the score of this rulebase class and continue inner loop
	if a match was found:
		adjust the proxy.score by match prominence, or class prominence if match prominence is zero
		set proxy.classname to saved rulebase class
		return true
	else:
		set proxy.classname to NULL
		return false
*/

Ensure(spindle_generate_classes, can_determine_the_class_to_use_for_a_proxy_entry_given_the_current_rules) {
	struct spindle_classmatch_struct *rule_class_0_matches = malloc(2 * sizeof *rule_class_0_matches);
	struct spindle_classmatch_struct *rule_class_1_matches = malloc(2 * sizeof *rule_class_1_matches);
	librdf_node *sameas = (librdf_node *) 0xA01;
	librdf_statement *query = (librdf_statement *) 0xA02;
	librdf_stream *stream = (librdf_stream *) 0xA03;
	SPINDLE spindle = {
		.world = (librdf_world *) 0xA04
	};
	struct spindle_classmap_struct *rule_classes = malloc(2 * sizeof *rule_classes);
	SPINDLERULES rules = {
		.classcount = 2,
		.classes = rule_classes
	};
	SPINDLEENTRY entry = {
		.spindle = &spindle,
		.rules = &rules,
		.score = 500
	};
	struct spindle_strset_struct classes;

	struct spindle_classmap_struct rule_class_0 = {
		.uri = "rulebase class uri 0",
		.match = rule_class_0_matches,
		.matchcount = 2,
		.score = 15,
		.prominence = 100
	};
	struct spindle_classmap_struct rule_class_1 = {
		.uri = "rulebase class uri 1",
		.match = rule_class_1_matches,
		.matchcount = 2,
		.score = 5,
		.prominence = 200
	};
	memcpy(&(rule_classes[0]), &rule_class_0, sizeof rule_class_0);
	memcpy(&(rule_classes[1]), &rule_class_1, sizeof rule_class_1);

	rule_class_0_matches[0].uri = "rulebase class uri 0 match 0";
	rule_class_0_matches[0].prominence = 5;
	rule_class_0_matches[1].uri = "rulebase class uri 0 match 1";
	rule_class_0_matches[1].prominence = -10;
	rule_class_1_matches[0].uri = "rulebase class uri 1 match 0";
	rule_class_1_matches[0].prominence = 15;
	rule_class_1_matches[1].uri = "rulebase class uri 1 match 1";
	rule_class_1_matches[1].prominence = -20;

	expect(librdf_new_node_from_uri_string, when(world, is_equal_to(spindle.world)), when(uri, is_equal_to_string(NS_RDF "type")));
	expect(librdf_new_statement, will_return(query), when(world, is_equal_to(spindle.world)));
	expect(librdf_statement_set_predicate, when(statement, is_equal_to(query)), when(node, is_equal_to(0)));
	expect(librdf_model_find_statements, will_return(stream), when(model, is_equal_to(0)), when(statement, is_equal_to(query)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));

	expect(librdf_stream_get_object, will_return(0x333), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_object, will_return(0x444), when(statement, is_equal_to(0x333)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(0x444)));
	expect(librdf_node_get_uri, will_return(0x555), when(node, is_equal_to(0x444)));
	expect(librdf_uri_as_string, will_return(rule_class_0_matches[1].uri), when(uri, is_equal_to(0x555)));
	expect(spindle_strset_add, when(set, is_equal_to(&classes)), when(str, is_equal_to_string(rule_class_0_matches[1].uri)));
	expect(spindle_strset_add, when(set, is_equal_to(&classes)), when(str, is_equal_to_string(rule_class_0.uri)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(stream)));

	expect(librdf_stream_get_object, will_return(0x666), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_object, will_return(0x777), when(statement, is_equal_to(0x666)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(0x777)));
	expect(librdf_node_get_uri, will_return(0x888), when(node, is_equal_to(0x777)));
	expect(librdf_uri_as_string, will_return(rule_class_1_matches[1].uri), when(uri, is_equal_to(0x888)));
	expect(spindle_strset_add, when(set, is_equal_to(&classes)), when(str, is_equal_to_string(rule_class_1_matches[1].uri)));
	expect(spindle_strset_add, when(set, is_equal_to(&classes)), when(str, is_equal_to_string(rule_class_1.uri)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(stream)));

	expect(librdf_free_stream, when(stream, is_equal_to(stream)));
	expect(librdf_free_statement, when(statement, is_equal_to(query)));

	int r = spindle_class_match(&entry, &classes);
	assert_that(r, is_true);
	assert_that(entry.classname, is_equal_to_string(rule_class_1.uri));
	assert_that(entry.score, is_equal_to(520));
	free(rule_class_1_matches);
	free(rule_class_0_matches);
	free(rule_classes);
}

#define TEST_TO_WRITE "ignores rule classes with score greater than 1000";

#pragma mark -
#pragma mark spindle_class_update_entry

Ensure(spindle_generate_classes, should_set_up_and_destroy_the_environment_needed_to_update_the_classes_of_a_proxy_entry) {
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_node *node2 = (librdf_node *) 0xA02;
	librdf_statement *base = (librdf_statement *) 0xA03;
	librdf_world *world = (librdf_world *) 0xA04;
	SPINDLE spindle = {
		.world = world,
		.rdftype = (librdf_node *) 0xA05
	};
	SPINDLEENTRY entry = { .spindle = &spindle };
	struct spindle_strset_struct classes = { 0 };

	expect(spindle_strset_create, will_return(&classes));

	// here spindle_class_update_entry() calls spindle_class_match() which
	// we cannot stub out in a single executable without #defining the function
	// name and wrapping it. for now, we will short-circuit this as best we can.
	expect(librdf_new_node_from_uri_string);
	expect(librdf_new_statement);
	expect(librdf_statement_set_predicate);
	expect(librdf_model_find_statements);
	expect(librdf_stream_end, will_return(1));
	expect(librdf_free_stream);
	expect(librdf_free_statement);

	expect(librdf_new_statement, will_return(base), when(world, is_equal_to(world)));
	expect(librdf_new_node_from_uri_string, will_return(node), when(world, is_equal_to(world)), when(uri, is_equal_to(0)));
	expect(librdf_statement_set_subject, when(statement, is_equal_to(base)), when(node, is_equal_to(node)));
	expect(librdf_new_node_from_node, will_return(node2), when(node, is_equal_to(spindle.rdftype)));
	expect(librdf_statement_set_predicate, when(statement, is_equal_to(base)), when(node, is_equal_to(node2)));
	expect(librdf_free_statement, when(statement, is_equal_to(base)));

	int r = spindle_class_update_entry(&entry);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_classes, can_update_the_classes_of_a_proxy_entry) {
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_node *node2 = (librdf_node *) 0xA02;
	librdf_statement *base = (librdf_statement *) 0xA03;
	librdf_world *world = (librdf_world *) 0xA04;
	SPINDLE spindle = {
		.world = world,
		.rdftype = (librdf_node *) 0xA05,
		.rootgraph = (librdf_node *) 0xA06,
		.multigraph = 1
	};
	struct spindle_classmatch_struct match = {
		.uri = "match class"
	};
	struct spindle_classmap_struct rule_classes = {
		.match = &match,
		.matchcount = 1 // values > 1 are ignored because the loop breaks out in first pass due to success
	};
	SPINDLERULES rules = {
		.classcount = 1,
		.classes = &rule_classes
	};
	SPINDLEENTRY entry = {
		.spindle = &spindle,
		.rules = &rules,
		.rootdata = (librdf_model *) 0xD01,
		.proxydata = (librdf_model *) 0xD02,
		.graph = (librdf_node *) 0xD03
	};
	struct spindle_strset_struct classes = {
		.count = 3 // number of loop iterations in spindle_class_update_entry() given no error occurs
	};
	classes.strings = malloc(classes.count * sizeof (char *));
	classes.strings[0] = "class 0";
	classes.strings[1] = "class 1";
	classes.strings[2] = "class 2";

	// the class name should compare equal to one of the above - I chose the middle one
	rule_classes.uri = classes.strings[1];

	// everything in this block is a repeat of the previous test, because the loop is not isolated
	expect(spindle_strset_create, will_return(&classes));
		expect(librdf_new_node_from_uri_string);
		expect(librdf_new_statement);
		expect(librdf_statement_set_predicate);
		expect(librdf_model_find_statements);
		expect(librdf_stream_end, will_return(0));
			expect(librdf_stream_get_object);
			expect(librdf_statement_get_object);
			expect(librdf_node_is_resource, will_return(1));
			expect(librdf_node_get_uri, will_return(0x333));
			expect(librdf_uri_as_string, will_return(match.uri)); // this ensures that the stream statement's object URI will match the first class rule tested
			expect(spindle_strset_add);
			expect(spindle_strset_add);
			expect(librdf_stream_next);
			expect(librdf_stream_end, will_return(1));
		expect(librdf_free_stream);
		expect(librdf_free_statement);
	expect(librdf_new_statement, will_return(base), when(world, is_equal_to(world)));
	expect(librdf_new_node_from_uri_string, will_return(node), when(world, is_equal_to(world)), when(uri, is_equal_to(0)));
	expect(librdf_statement_set_subject, when(statement, is_equal_to(base)), when(node, is_equal_to(node)));
	expect(librdf_new_node_from_node, will_return(node2), when(node, is_equal_to(spindle.rdftype)));
	expect(librdf_statement_set_predicate, when(statement, is_equal_to(base)), when(node, is_equal_to(node2)));

	// these next three blocks are the actual expectations being tested here
	librdf_statement *st = (librdf_statement *) 0xB01;
	librdf_node *obj = (librdf_node *) 0xC01;
	expect(librdf_new_statement_from_statement, will_return(st), when(statement, is_equal_to(base)));
	expect(librdf_new_node_from_uri_string, will_return(obj), when(world, is_equal_to(world)), when(uri, is_equal_to(classes.strings[0])));
	expect(librdf_statement_set_object, when(statement, is_equal_to(st)), when(node, is_equal_to(obj)));
	expect(librdf_model_context_add_statement, will_return(0), when(model, is_equal_to(entry.proxydata)), when(context, is_equal_to(entry.graph)), when(statement, is_equal_to(st)));
	expect(librdf_free_statement, when(statement, is_equal_to(st)));

	st = (librdf_statement *) 0xB02;
	obj = (librdf_node *) 0xC02;
	expect(librdf_new_statement_from_statement, will_return(st), when(statement, is_equal_to(base)));
	expect(librdf_new_node_from_uri_string, will_return(obj), when(world, is_equal_to(world)), when(uri, is_equal_to(classes.strings[1])));
	expect(librdf_statement_set_object, when(statement, is_equal_to(st)), when(node, is_equal_to(obj)));
	expect(librdf_model_context_add_statement, will_return(0), when(model, is_equal_to(entry.proxydata)), when(context, is_equal_to(entry.graph)), when(statement, is_equal_to(st)));
	expect(librdf_model_context_add_statement, will_return(0), when(model, is_equal_to(entry.rootdata)), when(context, is_equal_to(spindle.rootgraph)), when(statement, is_equal_to(st)));
	expect(librdf_free_statement, when(statement, is_equal_to(st)));

	st = (librdf_statement *) 0xB03;
	obj = (librdf_node *) 0xC03;
	expect(librdf_new_statement_from_statement, will_return(st), when(statement, is_equal_to(base)));
	expect(librdf_new_node_from_uri_string, will_return(obj), when(world, is_equal_to(world)), when(uri, is_equal_to(classes.strings[2])));
	expect(librdf_statement_set_object, when(statement, is_equal_to(st)), when(node, is_equal_to(obj)));
	expect(librdf_model_context_add_statement, will_return(0), when(model, is_equal_to(entry.proxydata)), when(context, is_equal_to(entry.graph)), when(statement, is_equal_to(st)));
	expect(librdf_free_statement, when(statement, is_equal_to(st)));

	// this line is also a repeat of part of the previous test
	expect(librdf_free_statement, when(statement, is_equal_to(base)));

	int r = spindle_class_update_entry(&entry);
	assert_that(r, is_equal_to(0));
	assert_that(entry.classes, is_equal_to(&classes));

	free(classes.strings);
}

Ensure(spindle_generate_classes, does_not_add_the_statement_to_the_root_model_when_not_in_multigraph_mode) {
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_node *node2 = (librdf_node *) 0xA02;
	librdf_statement *base = (librdf_statement *) 0xA03;
	librdf_world *world = (librdf_world *) 0xA04;
	SPINDLE spindle = {
		.world = world,
		.rdftype = (librdf_node *) 0xA05,
		.rootgraph = (librdf_node *) 0xA06,
		.multigraph = 0 // FLAG UNDER TEST
	};
	struct spindle_classmatch_struct match = {
		.uri = "match class"
	};
	struct spindle_classmap_struct rule_classes = {
		.match = &match,
		.matchcount = 1
	};
	SPINDLERULES rules = {
		.classcount = 1,
		.classes = &rule_classes
	};
	SPINDLEENTRY entry = {
		.spindle = &spindle,
		.rules = &rules,
		.proxydata = (librdf_model *) 0xD01,
		.graph = (librdf_node *) 0xD02
	};
	struct spindle_strset_struct classes = {
		.count = 1
	};
	classes.strings = malloc(classes.count * sizeof (char *));
	classes.strings[0] = "class 0";
	rule_classes.uri = classes.strings[0];

	// everything in this block is a repeat of the previous test, because the loop is not isolated
	expect(spindle_strset_create, will_return(&classes));
		expect(librdf_new_node_from_uri_string);
		expect(librdf_new_statement);
		expect(librdf_statement_set_predicate);
		expect(librdf_model_find_statements);
		expect(librdf_stream_end, will_return(0));
			expect(librdf_stream_get_object);
			expect(librdf_statement_get_object);
			expect(librdf_node_is_resource, will_return(1));
			expect(librdf_node_get_uri, will_return(0x333));
			expect(librdf_uri_as_string, will_return(match.uri));
			expect(spindle_strset_add);
			expect(spindle_strset_add);
			expect(librdf_stream_next);
			expect(librdf_stream_end, will_return(1));
		expect(librdf_free_stream);
		expect(librdf_free_statement);
	expect(librdf_new_statement, will_return(base), when(world, is_equal_to(world)));
	expect(librdf_new_node_from_uri_string, will_return(node), when(world, is_equal_to(world)), when(uri, is_equal_to(0)));
	expect(librdf_statement_set_subject, when(statement, is_equal_to(base)), when(node, is_equal_to(node)));
	expect(librdf_new_node_from_node, will_return(node2), when(node, is_equal_to(spindle.rdftype)));
	expect(librdf_statement_set_predicate, when(statement, is_equal_to(base)), when(node, is_equal_to(node2)));

	// this block only expects one call to librdf_model_context_add_statement, not two
	librdf_statement *st = (librdf_statement *) 0xB01;
	librdf_node *obj = (librdf_node *) 0xC01;
	expect(librdf_new_statement_from_statement, will_return(st), when(statement, is_equal_to(base)));
	expect(librdf_new_node_from_uri_string, will_return(obj), when(world, is_equal_to(world)), when(uri, is_equal_to(classes.strings[0])));
	expect(librdf_statement_set_object, when(statement, is_equal_to(st)), when(node, is_equal_to(obj)));
	expect(librdf_model_context_add_statement, will_return(0), when(model, is_equal_to(entry.proxydata)), when(context, is_equal_to(entry.graph)), when(statement, is_equal_to(st)));
	never_expect(librdf_model_context_add_statement);
	expect(librdf_free_statement, when(statement, is_equal_to(st)));

	// this line is also a repeat of part of the previous test
	expect(librdf_free_statement, when(statement, is_equal_to(base)));

	int r = spindle_class_update_entry(&entry);
	assert_that(r, is_equal_to(0));
	assert_that(entry.classes, is_equal_to(&classes));

	free(classes.strings);
}

Ensure(spindle_generate_classes, fails_when_updating_the_classes_of_a_proxy_entry_if_cannot_create_string_set) {
	SPINDLEENTRY entry = { 0 };

	expect(spindle_strset_create, will_return(NULL));

	int r = spindle_class_update_entry(&entry);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_classes, fails_when_updating_the_classes_of_a_proxy_entry_if_cannot_add_statement_to_proxy_data) {
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_node *node2 = (librdf_node *) 0xA02;
	librdf_statement *base = (librdf_statement *) 0xA03;
	librdf_world *world = (librdf_world *) 0xA04;
	SPINDLE spindle = {
		.world = world,
		.rdftype = (librdf_node *) 0xA05,
		.rootgraph = (librdf_node *) 0xA06,
		.multigraph = 1
	};
	struct spindle_classmatch_struct match = {
		.uri = "match class"
	};
	struct spindle_classmap_struct rule_classes = {
		.match = &match,
		.matchcount = 1 // values > 1 are ignored because the loop breaks out in first pass due to success
	};
	SPINDLERULES rules = {
		.classcount = 1,
		.classes = &rule_classes
	};
	SPINDLEENTRY entry = {
		.spindle = &spindle,
		.rules = &rules,
		.rootdata = (librdf_model *) 0xD01,
		.proxydata = (librdf_model *) 0xD02,
		.graph = (librdf_node *) 0xD03
	};
	struct spindle_strset_struct classes = {
		.count = 1
	};
	classes.strings = malloc(classes.count * sizeof (char *));
	classes.strings[0] = "class 0";

	// the class name should compare equal to one of the above
	rule_classes.uri = classes.strings[1];

	// everything in this block is a repeat of the previous test, because the loop is not isolated
	expect(spindle_strset_create, will_return(&classes));
		expect(librdf_new_node_from_uri_string);
		expect(librdf_new_statement);
		expect(librdf_statement_set_predicate);
		expect(librdf_model_find_statements);
		expect(librdf_stream_end, will_return(0));
			expect(librdf_stream_get_object);
			expect(librdf_statement_get_object);
			expect(librdf_node_is_resource, will_return(1));
			expect(librdf_node_get_uri, will_return(0x333));
			expect(librdf_uri_as_string, will_return(match.uri)); // this ensures that the stream statement's object URI will match the first class rule tested
			expect(spindle_strset_add);
			expect(spindle_strset_add);
			expect(librdf_stream_next);
			expect(librdf_stream_end, will_return(1));
		expect(librdf_free_stream);
		expect(librdf_free_statement);
	expect(librdf_new_statement, will_return(base), when(world, is_equal_to(world)));
	expect(librdf_new_node_from_uri_string, will_return(node), when(world, is_equal_to(world)), when(uri, is_equal_to(0)));
	expect(librdf_statement_set_subject, when(statement, is_equal_to(base)), when(node, is_equal_to(node)));
	expect(librdf_new_node_from_node, will_return(node2), when(node, is_equal_to(spindle.rdftype)));
	expect(librdf_statement_set_predicate, when(statement, is_equal_to(base)), when(node, is_equal_to(node2)));
	librdf_statement *st = (librdf_statement *) 0xB01;
	librdf_node *obj = (librdf_node *) 0xC01;
	expect(librdf_new_statement_from_statement, will_return(st), when(statement, is_equal_to(base)));
	expect(librdf_new_node_from_uri_string, will_return(obj), when(world, is_equal_to(world)), when(uri, is_equal_to(classes.strings[0])));
	expect(librdf_statement_set_object, when(statement, is_equal_to(st)), when(node, is_equal_to(obj)));

	// these are the actual expectations being tested here
	expect(librdf_model_context_add_statement, will_return(-1), when(model, is_equal_to(entry.proxydata)), when(context, is_equal_to(entry.graph)), when(statement, is_equal_to(st)));
	expect(librdf_free_statement, when(statement, is_equal_to(st)));
	expect(librdf_free_statement, when(statement, is_equal_to(base)));
	expect(spindle_strset_destroy, when(set, is_equal_to(&classes)));

	int r = spindle_class_update_entry(&entry);
	assert_that(r, is_equal_to(-1));

	free(classes.strings);
}

/* untestable code:

At the point where spindle_class_update_entry() calls spindle_class_match(), it
checks if the returned value is negative, and if so, attempts to return -1.
But spindle_class_match() can only return the values of 0 or 1, so this code
path is never reached.

spindle_class_match() is also not called from anywhere else in acropolis, and
so could be private.

*/

#pragma mark -

int classes_test(void) {
	TestSuite *suite = create_test_suite();
	add_test_with_context(suite, spindle_generate_classes, can_determine_the_class_to_use_for_a_proxy_entry_given_the_current_rules);
	add_test_with_context(suite, spindle_generate_classes, should_set_up_and_destroy_the_environment_needed_to_update_the_classes_of_a_proxy_entry);
	add_test_with_context(suite, spindle_generate_classes, can_update_the_classes_of_a_proxy_entry);
	add_test_with_context(suite, spindle_generate_classes, does_not_add_the_statement_to_the_root_model_when_not_in_multigraph_mode);
	add_test_with_context(suite, spindle_generate_classes, fails_when_updating_the_classes_of_a_proxy_entry_if_cannot_create_string_set);
	add_test_with_context(suite, spindle_generate_classes, fails_when_updating_the_classes_of_a_proxy_entry_if_cannot_add_statement_to_proxy_data);
	return run_test_suite(suite, create_text_reporter());
}

int main(int argc, char **argv) {
	return classes_test();
}
