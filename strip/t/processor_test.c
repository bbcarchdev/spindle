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
#include "../../t/mock_libsql.h"
#include "../../t/mock_librdf.h"
#include "../../t/mock_libtwine.h"
#include "../../t/mock_spindle_core.h"

int mock_processor(SPINDLERULES *rules, librdf_statement *statement, const char *uri) {
	return (int) mock(rules, statement, uri);
}

/* include file with static functions */
#define P_SPINDLE_STRIP_H_
#define PLUGIN_NAME "spindle-strip"
#include "../processor.c"

Describe(spindle_strip_processor);
BeforeEach(spindle_strip_processor) { always_expect(twine_logf); die_in(1); }
AfterEach(spindle_strip_processor) {}

Ensure(spindle_strip_processor, maps_the_statements_in_a_model_over_a_list_of_processors) {
	twine_graph graph = { .store = (void *) 111 };
	SPINDLESTRIPFN old_f = spindle_strip_rules_[0];
	spindle_strip_rules_[0] = mock_processor;

	expect(twine_rdf_model_create, will_return(222));
	expect(librdf_model_as_stream, will_return(333), when(model, is_equal_to(111)));
	expect(librdf_stream_end, will_return(0), when(stream, is_equal_to(333)));
	expect(librdf_stream_get_object, will_return(444), when(stream, is_equal_to(333)));
	expect(librdf_statement_get_predicate, will_return(555), when(statement, is_equal_to(444)));
	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(555)));
	expect(librdf_node_get_uri, will_return(666), when(node, is_equal_to(555)));
	expect(librdf_uri_as_string, will_return(777), when(uri, is_equal_to(666)));
	expect(mock_processor, when(rules, is_equal_to(888)), when(statement, is_equal_to(444)), when(uri, is_equal_to(777)));
	expect(librdf_stream_next, when(stream, is_equal_to(333)));
	expect(librdf_stream_end, will_return(1), when(stream, is_equal_to(333)));
	expect(librdf_free_stream, when(stream, is_equal_to(333)));
	expect(twine_rdf_model_destroy, when(model, is_equal_to(111)));

	assert_that(spindle_strip(&graph, (SPINDLERULES *) 888), is_equal_to(0));
	assert_that(graph.store == (librdf_model *) 222);

	spindle_strip_rules_[0] = old_f;
}

Ensure(spindle_strip_processor, keeps_statements_the_processor_functions_are_interested_in) {
	twine_graph graph = { .store = (void *) 111 };
	SPINDLESTRIPFN old_f = spindle_strip_rules_[0];
	spindle_strip_rules_[0] = mock_processor;

	expect(twine_rdf_model_create, will_return(333));
	expect(librdf_stream_end, will_return(0));
	expect(librdf_stream_get_object, will_return(444));
	expect(mock_processor, will_return(1));
	expect(librdf_model_add_statement, will_return(1), when(model, is_equal_to(333)), when(statement, is_equal_to(444)));
	expect(librdf_stream_end, will_return(1));
	always_expect(librdf_model_as_stream, will_return(222));
	always_expect(librdf_statement_get_predicate, will_return(222));
	always_expect(librdf_node_is_resource, will_return(1));
	always_expect(librdf_node_get_uri, will_return(222));
	always_expect(librdf_uri_as_string, will_return(222));
	always_expect(librdf_stream_next);
	always_expect(librdf_free_stream);
	always_expect(twine_rdf_model_destroy);

	assert_that(spindle_strip(&graph, NULL), is_equal_to(0));
	assert_that(graph.store == (librdf_model *) 333);

	spindle_strip_rules_[0] = old_f;
}

Ensure(spindle_strip_processor, discards_statements_the_processor_functions_are_not_interested_in) {
	twine_graph graph = { .store = (void *) 111 };
	SPINDLESTRIPFN old_f = spindle_strip_rules_[0];
	spindle_strip_rules_[0] = mock_processor;

	expect(twine_rdf_model_create, will_return(333));
	expect(librdf_stream_end, will_return(0));
	expect(mock_processor, will_return(0));
	never_expect(librdf_model_add_statement);
	expect(librdf_stream_end, will_return(1));
	always_expect(librdf_model_as_stream, will_return(222));
	always_expect(librdf_stream_get_object, will_return(222));
	always_expect(librdf_statement_get_predicate, will_return(222));
	always_expect(librdf_node_is_resource, will_return(1));
	always_expect(librdf_node_get_uri, will_return(222));
	always_expect(librdf_uri_as_string, will_return(222));
	always_expect(librdf_stream_next);
	always_expect(librdf_free_stream);
	always_expect(twine_rdf_model_destroy);

	assert_that(spindle_strip(&graph, NULL), is_equal_to(0));
	assert_that(graph.store == (librdf_model *) 333);

	spindle_strip_rules_[0] = old_f;
}

Ensure(spindle_strip_processor, includes_is_cachepred_function_in_list_of_processors) {
	/* not bothering to iterate over list when I know it only has one item at present */
	assert_that(spindle_strip_rules_[0] == spindle_strip_is_cachepred_);
}

Ensure(spindle_strip_processor, is_cachepred_function_returns_true_for_cached_predicates) {
	/* ensure same string has different pointers */
	char *predicate1 = "predicate";
	char predicate2[10];
	sprintf(predicate2, "predicate");
	SPINDLERULES rules = { .cpcount = 1, .cachepreds = &predicate1 };
	assert_that(spindle_strip_is_cachepred_(&rules, NULL, predicate2), is_true);
}

Ensure(spindle_strip_processor, is_cachepred_function_returns_false_for_noncached_predicates) {
	/* ensure strings differ */
	char *predicate1 = "predicate 1";
	char *predicate2 = "predicate 2";
	SPINDLERULES rules = { .cpcount = 1, .cachepreds = &predicate1 };
	assert_that(spindle_strip_is_cachepred_(&rules, NULL, predicate2), is_false);
}

int main(int argc, char **argv) {
	TestSuite *suite = create_test_suite();
	add_test_with_context(suite, spindle_strip_processor, maps_the_statements_in_a_model_over_a_list_of_processors);
	add_test_with_context(suite, spindle_strip_processor, keeps_statements_the_processor_functions_are_interested_in);
	add_test_with_context(suite, spindle_strip_processor, discards_statements_the_processor_functions_are_not_interested_in);
	add_test_with_context(suite, spindle_strip_processor, includes_is_cachepred_function_in_list_of_processors);
	add_test_with_context(suite, spindle_strip_processor, is_cachepred_function_returns_true_for_cached_predicates);
	add_test_with_context(suite, spindle_strip_processor, is_cachepred_function_returns_false_for_noncached_predicates);
	return run_test_suite(suite, create_text_reporter());
}
