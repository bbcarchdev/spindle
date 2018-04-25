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
#include "../../t/mock_spindle_proxy_methods.h"

#define NS_DCTERMS
#define NS_GEO
#define NS_RDFS
#define NS_XSD

/* compile SUT inline due to static functions */
#define P_SPINDLE_GENERATE_H_
#define PLUGIN_NAME "spindle-generate"
#include "../props.c"

Describe(spindle_generate_props);
BeforeEach(spindle_generate_props) { cgreen_mocks_are(learning_mocks); always_expect(twine_logf); die_in(1); }
AfterEach(spindle_generate_props) {}

/*
	This suite does not test any failure paths triggered by libc calls such as malloc.
*/

#pragma mark -
#pragma mark spindle_prop_copystrings_

Ensure(spindle_generate_props, literal_copy_with_NULL_source_does_not_copy_and_returns_no_error) {
	struct spindle_literalset_struct dest = {
		.literals = (struct spindle_literalstring_struct *) 0xA01,
		.nliterals = 0xA02
	};
	struct spindle_literalset_struct expected = {
		.literals = dest.literals,
		.nliterals = dest.nliterals
	};
	int r = spindle_prop_copystrings_(&dest, NULL);
	assert_that(r, is_equal_to(0));
	assert_that(&dest, is_equal_to_contents_of(&expected, sizeof expected));
}

Ensure(spindle_generate_props, literal_copy_with_no_source_literals_does_not_copy_and_returns_no_error) {
	struct spindle_literalset_struct dest = {
		.literals = (struct spindle_literalstring_struct *) 0xA01,
		.nliterals = 0xA02
	};
	struct spindle_literalset_struct expected = {
		.literals = dest.literals,
		.nliterals = dest.nliterals
	};
	struct propmatch_struct source = { 0 };
	int r = spindle_prop_copystrings_(&dest, &source);
	assert_that(r, is_equal_to(0));
	assert_that(&dest, is_equal_to_contents_of(&expected, sizeof expected));
}

Ensure(spindle_generate_props, literal_copy_copies_all_source_literals_and_returns_no_error) {
	struct spindle_literalset_struct dest = { 0 };
	struct spindle_literalset_struct expected = { .nliterals = 5 };
	size_t ptr_size = expected.nliterals * sizeof (struct spindle_literalstring_struct);
	expected.literals = malloc(ptr_size);
	struct literal_struct src_literals[expected.nliterals];
	struct propmatch_struct source = {
		.literals = src_literals,
		.nliterals = expected.nliterals
	};
	expected.literals[0].str = "Bore da";
	expected.literals[1].str = "G'day";
	expected.literals[2].str = "Доброе утро";
	expected.literals[3].str = "お早うございます";
	expected.literals[4].str = "👍🏻🌄";
	strcpy((char *) &(expected.literals[0].lang), "cy");
	strcpy((char *) &(expected.literals[1].lang), "en-AU");
	strcpy((char *) &(expected.literals[2].lang), "ru");
	strcpy((char *) &(expected.literals[3].lang), "ja");
	strcpy((char *) &(expected.literals[4].lang), "x-emoji");

	expect(librdf_node_get_literal_value, will_return(expected.literals[0].str));
	expect(librdf_node_get_literal_value, will_return(expected.literals[1].str));
	expect(librdf_node_get_literal_value, will_return(expected.literals[2].str));
	expect(librdf_node_get_literal_value, will_return(expected.literals[3].str));
	expect(librdf_node_get_literal_value, will_return(expected.literals[4].str));
	strcpy((char *) &(source.literals[0].lang), (char *) &(expected.literals[0].lang));
	strcpy((char *) &(source.literals[1].lang), (char *) &(expected.literals[1].lang));
	strcpy((char *) &(source.literals[2].lang), (char *) &(expected.literals[2].lang));
	strcpy((char *) &(source.literals[3].lang), (char *) &(expected.literals[3].lang));
	strcpy((char *) &(source.literals[4].lang), (char *) &(expected.literals[4].lang));

	int r = spindle_prop_copystrings_(&dest, &source);
	assert_that(r, is_equal_to(0));
	assert_that(dest.nliterals, is_equal_to(expected.nliterals));
	assert_that(dest.literals[0].lang, is_equal_to_string(expected.literals[0].lang));
	assert_that(dest.literals[1].lang, is_equal_to_string(expected.literals[1].lang));
	assert_that(dest.literals[2].lang, is_equal_to_string(expected.literals[2].lang));
	assert_that(dest.literals[3].lang, is_equal_to_string(expected.literals[3].lang));
	assert_that(dest.literals[4].lang, is_equal_to_string(expected.literals[4].lang));
	assert_that(dest.literals[0].str, is_equal_to_string(expected.literals[0].str));
	assert_that(dest.literals[1].str, is_equal_to_string(expected.literals[1].str));
	assert_that(dest.literals[2].str, is_equal_to_string(expected.literals[2].str));
	assert_that(dest.literals[3].str, is_equal_to_string(expected.literals[3].str));
	assert_that(dest.literals[4].str, is_equal_to_string(expected.literals[4].str));
	free(expected.literals);
}

#pragma mark -
#pragma mark spindle_prop_candidate_lang_

Ensure(spindle_generate_props, language_tag_test_returns_error_on_unsupported_value) {
	// whilst they are valid BCP-47 tags, values like "de-DE-1901" and "es-419" are not supported
	int r = spindle_prop_candidate_lang_(NULL, NULL, NULL, NULL, NULL, "de-1996");
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, language_tag_test_returns_error_on_too_short_string_length) {
	int r = spindle_prop_candidate_lang_(NULL, NULL, NULL, NULL, NULL, "a");
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, language_tag_test_returns_error_on_too_long_string_length) {
	// Spindle does not support language tags longer than 7 characters
	int r = spindle_prop_candidate_lang_(NULL, NULL, NULL, NULL, NULL, "abcdefgh");
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, language_tag_test_returns_false_if_matching_language_has_lower_priority_than_criterion) {
	struct literal_struct literal = { .lang = "en", .priority = 1 };
	struct propmatch_struct match = { .nliterals = 1, .literals = &literal };
	struct spindle_predicatematch_struct criterion = { .priority = literal.priority + 1 };

	int r = spindle_prop_candidate_lang_(NULL, &match, &criterion, NULL, NULL, literal.lang);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, language_tag_test_returns_false_if_matching_language_has_equal_priority_to_criterion) {
	struct literal_struct literal = { .lang = "en", .priority = 1 };
	struct propmatch_struct match = { .nliterals = 1, .literals = &literal };
	struct spindle_predicatematch_struct criterion = { .priority = literal.priority };

	int r = spindle_prop_candidate_lang_(NULL, &match, &criterion, NULL, NULL, literal.lang);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, language_tag_test_returns_error_if_twine_rdf_node_clone_fails) {
	struct literal_struct literal = { .lang = "en", .priority = 2 };
	struct propmatch_struct match = { .nliterals = 1, .literals = &literal };
	struct spindle_predicatematch_struct criterion = { .priority = literal.priority - 1 };
	librdf_node *obj = (librdf_node *) 0xA01;

	expect(twine_rdf_node_clone, will_return(NULL), when(node, is_equal_to(obj)));

	int r = spindle_prop_candidate_lang_(NULL, &match, &criterion, NULL, obj, literal.lang);
	assert_that(r, is_equal_to(-1));
}

#pragma mark -

int props_test(void) {
	TestSuite *suite = create_test_suite();
	add_test_with_context(suite, spindle_generate_props, literal_copy_with_NULL_source_does_not_copy_and_returns_no_error);
	add_test_with_context(suite, spindle_generate_props, literal_copy_with_no_source_literals_does_not_copy_and_returns_no_error);
	add_test_with_context(suite, spindle_generate_props, literal_copy_copies_all_source_literals_and_returns_no_error);
	add_test_with_context(suite, spindle_generate_props, language_tag_test_returns_error_on_unsupported_value);
	add_test_with_context(suite, spindle_generate_props, language_tag_test_returns_error_on_too_short_string_length);
	add_test_with_context(suite, spindle_generate_props, language_tag_test_returns_error_on_too_long_string_length);
	add_test_with_context(suite, spindle_generate_props, language_tag_test_returns_false_if_matching_language_has_lower_priority_than_criterion);
	add_test_with_context(suite, spindle_generate_props, language_tag_test_returns_false_if_matching_language_has_equal_priority_to_criterion);
	add_test_with_context(suite, spindle_generate_props, language_tag_test_returns_error_if_twine_rdf_node_clone_fails);
	return run_test_suite(suite, create_text_reporter());
}

int main(int argc, char **argv) {
	return props_test();
}
