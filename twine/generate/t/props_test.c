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

/* mocks of dependancies */
#include "../../t/mock_librdf.h"
#include "../../t/mock_libsql.h"
#include "../../t/mock_libtwine.h"
#include "../../t/mock_spindle_core.h"
#include "../../t/mock_spindle_proxy_methods.h"
#include "../../t/rdf_namespaces.h"

/* compile SUT inline due to static functions */
#define P_SPINDLE_GENERATE_H_
#define PLUGIN_NAME "spindle-generate"
#include "../props.c"

Describe(spindle_generate_props);
BeforeEach(spindle_generate_props) { always_expect(twine_logf); die_in(1); }
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
#pragma mark spindle_dt_is_int

/**
 * The function spindle_dt_is_int() actually returns whether a
 * datatype is a derived type of xsd:decimal or not, according to
 * https://www.w3.org/TR/xmlschema11-2/#built-in-datatypes
 */

Ensure(spindle_generate_props, datatype_is_int_returns_true_iff_type_is_derived_from_xsd_decimal) {
	// true
	assert_that(spindle_dt_is_int(NS_XSD "integer"), is_equal_to(1));
	assert_that(spindle_dt_is_int(NS_XSD "long"), is_equal_to(1));
	assert_that(spindle_dt_is_int(NS_XSD "short"), is_equal_to(1));
	assert_that(spindle_dt_is_int(NS_XSD "byte"), is_equal_to(1));
	assert_that(spindle_dt_is_int(NS_XSD "int"), is_equal_to(1));
	assert_that(spindle_dt_is_int(NS_XSD "nonPositiveInteger"), is_equal_to(1));
	assert_that(spindle_dt_is_int(NS_XSD "nonNegativeInteger"), is_equal_to(1));
	assert_that(spindle_dt_is_int(NS_XSD "negativeInteger"), is_equal_to(1));
	assert_that(spindle_dt_is_int(NS_XSD "positiveInteger"), is_equal_to(1));
	assert_that(spindle_dt_is_int(NS_XSD "unsignedLong"), is_equal_to(1));
	assert_that(spindle_dt_is_int(NS_XSD "unsignedInt"), is_equal_to(1));
	assert_that(spindle_dt_is_int(NS_XSD "unsignedShort"), is_equal_to(1));
	assert_that(spindle_dt_is_int(NS_XSD "unsignedByte"), is_equal_to(1));

	// false - this list is infinite, but we'll just test the built-in types
	assert_that(spindle_dt_is_int(NS_XSD "decimal"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "anyType"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "anySimpleType"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "anyAtomicType"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "anyURI"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "base64Binary"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "boolean"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "date"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "dateTime"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "dateTimeStamp"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "double"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "duration"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "dayTimeDuration"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "yearMonthDuration"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "float"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "gDay"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "gMonth"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "gMonthDay"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "gYear"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "gYearMonth"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "hexBinary"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "NOTATION"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "QName"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "string"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "normalizedString"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "token"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "language"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "Name"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "NCName"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "ENTITY"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "ID"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "IDREF"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "NMTOKEN"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "time"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "ENTITIES"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "IDREFS"), is_equal_to(0));
	assert_that(spindle_dt_is_int(NS_XSD "NMTOKENS"), is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_prop_candidate_lang_

Ensure(spindle_generate_props, candidate_lang_returns_error_on_unsupported_value) {
	// whilst they are valid BCP-47 tags, values like "de-DE-1901" and "es-419" are not supported
	int r = spindle_prop_candidate_lang_(NULL, NULL, NULL, NULL, NULL, "de-1996");
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, candidate_lang_returns_error_on_too_short_string_length) {
	int r = spindle_prop_candidate_lang_(NULL, NULL, NULL, NULL, NULL, "a");
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, candidate_lang_returns_error_on_too_long_string_length) {
	// Spindle does not support language tags longer than 7 characters
	int r = spindle_prop_candidate_lang_(NULL, NULL, NULL, NULL, NULL, "abcdefgh");
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, candidate_lang_returns_false_if_matching_language_has_lower_priority_than_criterion) {
	struct literal_struct literal = { .lang = "en", .priority = 1 };
	struct propmatch_struct match = { .nliterals = 1, .literals = &literal };
	struct spindle_predicatematch_struct criterion = { .priority = literal.priority + 1 };

	int r = spindle_prop_candidate_lang_(NULL, &match, &criterion, NULL, NULL, literal.lang);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, candidate_lang_returns_false_if_matching_language_has_equal_priority_to_criterion) {
	struct literal_struct literal = { .lang = "en", .priority = 1 };
	struct propmatch_struct match = { .nliterals = 1, .literals = &literal };
	struct spindle_predicatematch_struct criterion = { .priority = literal.priority };

	int r = spindle_prop_candidate_lang_(NULL, &match, &criterion, NULL, NULL, literal.lang);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, candidate_lang_returns_error_if_twine_rdf_node_clone_fails) {
	struct literal_struct literal = { .lang = "en", .priority = 2 };
	struct propmatch_struct match = { .nliterals = 1, .literals = &literal };
	struct spindle_predicatematch_struct criterion = { .priority = literal.priority - 1 };
	librdf_node *obj = (librdf_node *) 0xA01;

	expect(twine_rdf_node_clone, will_return(NULL), when(node, is_equal_to(obj)));

	int r = spindle_prop_candidate_lang_(NULL, &match, &criterion, NULL, obj, literal.lang);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, candidate_lang_returns_true_on_success) {
	struct literal_struct literal = { .lang = "en", .priority = 2, .node = (librdf_node *) 0xA01 };
	struct propmatch_struct match = { .nliterals = 1, .literals = &literal };
	struct spindle_predicatematch_struct criterion = { .priority = literal.priority - 1, .prominence = 5 };
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *obj = (librdf_node *) 0xA03;
	SPINDLEGENERATE generate = { 0 };
	SPINDLEENTRY entry = { .generate = &generate };
	struct propdata_struct data = { .entry = &entry };

	expect(twine_rdf_node_clone, will_return(clone), when(node, is_equal_to(obj)));
	expect(twine_rdf_node_destroy, when(node, is_equal_to(literal.node)));

	int r = spindle_prop_candidate_lang_(&data, &match, &criterion, NULL, obj, literal.lang);
	assert_that(r, is_equal_to(1));
	assert_that(match.literals[0].node, is_equal_to(clone));
	assert_that(match.literals[0].priority, is_equal_to(criterion.priority));
	assert_that(match.prominence, is_equal_to(criterion.prominence));
}

Ensure(spindle_generate_props, candidate_lang_converts_language_tag_to_canonical_form_for_comparison) {
	// Spindle's idea of canonical is all lower-case with hyphens, not e.g. "ja-JP-Latn" as BCP-47 would have
	const char *input_tag = "EN_GB";
	struct literal_struct literal = { .lang = "en-gb", .priority = 2 };
	struct propmatch_struct match = { .nliterals = 1, .literals = &literal };
	struct spindle_predicatematch_struct criterion = { .priority = literal.priority - 1, .prominence = 5 };
	librdf_node *clone = (librdf_node *) 0xA02;
	SPINDLEGENERATE generate = { 0 };
	SPINDLEENTRY entry = { .generate = &generate };
	struct propdata_struct data = { .entry = &entry };

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_node_destroy);

	int r = spindle_prop_candidate_lang_(&data, &match, &criterion, NULL, NULL, input_tag);
	assert_that(r, is_equal_to(1));
}

Ensure(spindle_generate_props, candidate_lang_uses_map_prominence_if_criterion_prominence_is_zero) {
	struct literal_struct literal = { .lang = "en", .priority = 2 };
	struct spindle_predicatemap_struct predicate_map = { .prominence = 5 };
	struct propmatch_struct match = { .nliterals = 1, .literals = &literal, .map = &predicate_map };
	struct spindle_predicatematch_struct criterion = { .priority = literal.priority - 1 };
	librdf_node *clone = (librdf_node *) 0xA02;
	SPINDLEGENERATE generate = { 0 };
	SPINDLEENTRY entry = { .generate = &generate };
	struct propdata_struct data = { .entry = &entry };

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_node_destroy);

	int r = spindle_prop_candidate_lang_(&data, &match, &criterion, NULL, NULL, literal.lang);
	assert_that(r, is_equal_to(1));
	assert_that(match.prominence, is_equal_to(predicate_map.prominence));
}

Ensure(spindle_generate_props, candidate_lang_appends_candidate_language_to_propmatch_literals_if_no_match_found) {
	struct propmatch_struct match = { .nliterals = 1 };
	match.literals = calloc(match.nliterals, sizeof (struct literal_struct));
	match.literals[0].lang[0] = 'e';
	match.literals[0].lang[1] = 'n';
	match.literals[0].lang[2] = '\0';
	match.literals[0].priority = 5;
	struct spindle_predicatematch_struct criterion = { .priority = match.literals[0].priority - 1, .prominence = 5 };
	librdf_node *clone = (librdf_node *) 0xA02;
	SPINDLEGENERATE generate = { 0 };
	SPINDLEENTRY entry = { .generate = &generate };
	struct propdata_struct data = { .entry = &entry };

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_node_destroy);

	const char *input_lang = "ga-latg";
	int r = spindle_prop_candidate_lang_(&data, &match, &criterion, NULL, NULL, input_lang);
	assert_that(r, is_equal_to(1));
	assert_that(match.nliterals, is_equal_to(2));
	assert_that(match.literals[1].lang, is_equal_to_string(input_lang));
	assert_that(match.literals[1].node, is_equal_to(clone));
	assert_that(match.literals[1].priority, is_equal_to(4));
}

Ensure(spindle_generate_props, candidate_lang_sets_the_english_title) {
	SPINDLEGENERATE generate = { .titlepred = "dc:title" };
	SPINDLEENTRY entry = { .generate = &generate };
	struct propdata_struct data = { .entry = &entry };
	struct literal_struct literal = { .lang = "en", .priority = 2 };
	struct spindle_predicatematch_struct criterion = { .priority = literal.priority - 1, .prominence = 5 };
	struct spindle_predicatemap_struct predicate_map = { .target = generate.titlepred, .prominence = criterion.prominence };
	struct propmatch_struct match = { .nliterals = 1, .literals = &literal, .map = &predicate_map };
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *obj = (librdf_node *) 0xA03;
	const char *title = "title";

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_node_destroy);
	expect(librdf_node_get_literal_value, will_return(title), when(node, is_equal_to(obj)));

	int r = spindle_prop_candidate_lang_(&data, &match, &criterion, NULL, obj, literal.lang);
	assert_that(r, is_equal_to(1));
	assert_that(entry.title_en, is_equal_to_string(title));
	assert_that(entry.title, is_null);
}

Ensure(spindle_generate_props, candidate_lang_sets_the_base_title_only_when_input_lang_is_NULL) {
	SPINDLEGENERATE generate = { .titlepred = "dc:title" };
	SPINDLEENTRY entry = { .generate = &generate };
	struct propdata_struct data = { .entry = &entry };
	struct spindle_predicatematch_struct criterion = { .priority = 1, .prominence = 5 };
	struct spindle_predicatemap_struct predicate_map = { .target = generate.titlepred, .prominence = criterion.prominence };
	struct propmatch_struct match = { .nliterals = 1, .map = &predicate_map };
	match.literals = calloc(match.nliterals, sizeof (struct literal_struct));
	match.literals[0].lang[0] = 'e';
	match.literals[0].lang[1] = 'n';
	match.literals[0].lang[2] = '\0';
	match.literals[0].priority = criterion.priority + 1;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *obj = (librdf_node *) 0xA03;
	const char *title = "title";

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_node_destroy);
	expect(librdf_node_get_literal_value, will_return(title), when(node, is_equal_to(obj)));

	int r = spindle_prop_candidate_lang_(&data, &match, &criterion, NULL, obj, NULL);
	assert_that(r, is_equal_to(1));
	assert_that(entry.title, is_equal_to_string(title));
	assert_that(entry.title_en, is_null);
}

Ensure(spindle_generate_props, candidate_lang_does_not_set_a_title_title_when_input_lang_is_not_english) {
	SPINDLEGENERATE generate = { .titlepred = "dc:title" };
	SPINDLEENTRY entry = { .generate = &generate };
	struct propdata_struct data = { .entry = &entry };
	struct literal_struct literal = { .lang = "fr", .priority = 2 };
	struct spindle_predicatematch_struct criterion = { .priority = literal.priority - 1, .prominence = 5 };
	struct spindle_predicatemap_struct predicate_map = { .target = generate.titlepred, .prominence = criterion.prominence };
	struct propmatch_struct match = { .nliterals = 1, .literals = &literal, .map = &predicate_map };
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *obj = (librdf_node *) 0xA03;
	const char *title = "l'intitulé";

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_node_destroy);

	int r = spindle_prop_candidate_lang_(&data, &match, &criterion, NULL, obj, literal.lang);
	assert_that(r, is_equal_to(1));
	assert_that(entry.title, is_null);
	assert_that(entry.title_en, is_null);
}

#pragma mark -
#pragma mark spindle_prop_candidate_literal_

Ensure(spindle_generate_props, candidate_literal_matches_by_language_if_no_datatype_is_specified) {
	struct spindle_predicatemap_struct predicate_map = { 0 };
	struct propmatch_struct match = { .map = &predicate_map, .priority = 1 };
	librdf_node *obj = (librdf_node *) 0xA03;

	expect(librdf_node_get_literal_value_language, will_return("en"), when(node, is_equal_to(obj)));
	always_expect(twine_rdf_node_clone, will_return(NULL), when(node, is_equal_to(obj)));

	int r = spindle_prop_candidate_literal_(NULL, &match, NULL, NULL, obj);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, candidate_literal_returns_false_if_matching_literal_has_lower_priority_than_criterion) {
	struct propdata_struct data = { 0 };
	struct spindle_predicatemap_struct predicate_map = { .datatype = "xsd:sometype" };
	struct propmatch_struct match = { .map = &predicate_map, .priority = 1 };
	struct spindle_predicatematch_struct criterion = { .priority = match.priority + 1 };
	librdf_node *obj = (librdf_node *) 0xA03;

	expect(librdf_node_get_literal_value_language, when(node, is_equal_to(obj)));

	int r = spindle_prop_candidate_literal_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, candidate_literal_returns_false_if_matching_literal_has_equal_priority_to_criterion) {
	struct propdata_struct data = { 0 };
	struct spindle_predicatemap_struct predicate_map = { .datatype = "xsd:sometype" };
	struct propmatch_struct match = { .map = &predicate_map, .priority = 1 };
	struct spindle_predicatematch_struct criterion = { .priority = match.priority };
	librdf_node *obj = (librdf_node *) 0xA03;

	expect(librdf_node_get_literal_value_language, when(node, is_equal_to(obj)));

	int r = spindle_prop_candidate_literal_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, candidate_literal_returns_false_if_object_datatype_has_no_uri_and_language_is_not_NULL) {
	struct propdata_struct data = { 0 };
	struct spindle_predicatemap_struct predicate_map = { .datatype = "xsd:sometype" };
	struct propmatch_struct match = { .map = &predicate_map };
	struct spindle_predicatematch_struct criterion = { 0 };
	librdf_node *obj = (librdf_node *) 0xA03;

	expect(librdf_node_get_literal_value_language, will_return("en"), when(node, is_equal_to(obj)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(NULL), when(node, is_equal_to(obj)));

	int r = spindle_prop_candidate_literal_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, candidate_literal_returns_false_if_object_datatype_has_different_uri_to_predicate_map_datatype_and_predicate_map_datatype_is_not_xsd_decimal) {
	struct propdata_struct data = { 0 };
	struct spindle_predicatemap_struct predicate_map = { .datatype = "xsd:sometype" };
	struct propmatch_struct match = { .map = &predicate_map };
	struct spindle_predicatematch_struct criterion = { 0 };
	librdf_node *obj = (librdf_node *) 0xA03;
	librdf_uri *dturi = (librdf_uri *) 0xA04;

	expect(librdf_node_get_literal_value_language, will_return(NULL), when(node, is_equal_to(obj)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(dturi), when(node, is_equal_to(obj)));
	expect(librdf_uri_as_string, will_return("xsd:differenttype"), when(uri, is_equal_to(dturi)));

	int r = spindle_prop_candidate_literal_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, candidate_literal_returns_false_if_predicate_map_datatype_is_xsd_decimal_and_object_datatype_cannot_be_coerced_to_xsd_decimal) {
	struct propdata_struct data = { 0 };
	struct spindle_predicatemap_struct predicate_map = { .datatype = NS_XSD "decimal" };
	struct propmatch_struct match = { .map = &predicate_map };
	struct spindle_predicatematch_struct criterion = { 0 };
	librdf_node *obj = (librdf_node *) 0xA03;
	librdf_uri *dturi = (librdf_uri *) 0xA04;

	expect(librdf_node_get_literal_value_language, will_return(NULL), when(node, is_equal_to(obj)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(dturi), when(node, is_equal_to(obj)));
	expect(librdf_uri_as_string, will_return("xsd:cannotcoerce"), when(uri, is_equal_to(dturi)));

	int r = spindle_prop_candidate_literal_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, candidate_literal_returns_error_if_creating_a_new_node_from_a_typed_literal_fails) {
	librdf_world *world = (librdf_world *) 0xA01;
	SPINDLE spindle = { .world = world };
	struct propdata_struct data = { .spindle = &spindle };
	struct spindle_predicatemap_struct predicate_map = { .datatype = "xsd:sometype" };
	struct propmatch_struct match = { .map = &predicate_map };
	struct spindle_predicatematch_struct criterion = { 0 };
	librdf_node *obj = (librdf_node *) 0xA03;
	librdf_uri *uri = (librdf_uri *) 0xA04;
	const char *value = "x";

	expect(librdf_node_get_literal_value_language, will_return(NULL), when(node, is_equal_to(obj)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(NULL), when(node, is_equal_to(obj)));
	expect(librdf_new_uri, will_return(uri), when(world, is_equal_to(world)), when(uri_string, is_equal_to(predicate_map.datatype)));
	expect(librdf_node_get_literal_value, will_return(value), when(node, is_equal_to(obj)));
	expect(librdf_new_node_from_typed_literal, will_return(NULL), when(world, is_equal_to(world)), when(value, is_equal_to(value)), when(xml_language, is_equal_to(NULL)), when(datatype_uri, is_equal_to(uri)));
	expect(librdf_free_uri, when(uri, is_equal_to(uri)));

	int r = spindle_prop_candidate_literal_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, candidate_literal_returns_true_if_object_datatype_has_no_uri_and_language_is_NULL) {
	librdf_world *world = (librdf_world *) 0xA01;
	SPINDLE spindle = { .world = world };
	struct propdata_struct data = { .spindle = &spindle };
	struct spindle_predicatemap_struct predicate_map = { .datatype = "xsd:sometype" };
	struct propmatch_struct match = { .map = &predicate_map, .resource = (librdf_node *) 0xB01 };
	struct spindle_predicatematch_struct criterion = { 0 };
	librdf_node *node = (librdf_node *) 0xA02;
	librdf_node *obj = (librdf_node *) 0xA03;
	librdf_uri *uri = (librdf_uri *) 0xA04;
	const char *value = "x";

	expect(librdf_node_get_literal_value_language, will_return(NULL), when(node, is_equal_to(obj)));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(NULL), when(node, is_equal_to(obj)));
	expect(librdf_new_uri, will_return(uri), when(world, is_equal_to(world)), when(uri_string, is_equal_to(predicate_map.datatype)));
	expect(librdf_node_get_literal_value, will_return(value), when(node, is_equal_to(obj)));
	expect(librdf_new_node_from_typed_literal, will_return(node), when(world, is_equal_to(world)), when(value, is_equal_to(value)), when(xml_language, is_equal_to(NULL)), when(datatype_uri, is_equal_to(uri)));
	expect(librdf_free_uri, when(uri, is_equal_to(uri)));
	expect(twine_rdf_node_destroy, when(node, is_equal_to(match.resource)));

	int r = spindle_prop_candidate_literal_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(1));
}

Ensure(spindle_generate_props, candidate_literal_returns_true_if_object_datatype_matches_predicate_map_datatype) {
	librdf_world *world = (librdf_world *) 0xA01;
	SPINDLE spindle = { .world = world };
	struct propdata_struct data = { .spindle = &spindle };
	struct spindle_predicatemap_struct predicate_map = { .datatype = "xsd:sometype" };
	struct propmatch_struct match = { .map = &predicate_map, .resource = (librdf_node *) 0xB01 };
	struct spindle_predicatematch_struct criterion = { 0 };
	librdf_node *node = (librdf_node *) 0xA02;
	librdf_node *obj = (librdf_node *) 0xA03;
	librdf_uri *uri = (librdf_uri *) 0xA04;
	librdf_uri *datatype_uri = (librdf_uri *) 0xA05;
	const char *value = "x";

	always_expect(librdf_node_get_literal_value_language);
	expect(librdf_node_get_literal_value_datatype_uri, will_return(datatype_uri), when(node, is_equal_to(obj)));
	expect(librdf_uri_as_string, will_return(predicate_map.datatype), when(uri, is_equal_to(datatype_uri)));
	expect(librdf_new_uri, will_return(uri), when(world, is_equal_to(world)), when(uri_string, is_equal_to(predicate_map.datatype)));
	expect(librdf_node_get_literal_value, will_return(value), when(node, is_equal_to(obj)));
	expect(librdf_new_node_from_typed_literal, will_return(node), when(world, is_equal_to(world)), when(value, is_equal_to(value)), when(xml_language, is_equal_to(NULL)), when(datatype_uri, is_equal_to(uri)));
	expect(librdf_free_uri, when(uri, is_equal_to(uri)));
	expect(twine_rdf_node_destroy, when(node, is_equal_to(match.resource)));

	int r = spindle_prop_candidate_literal_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(1));
}

Ensure(spindle_generate_props, candidate_literal_returns_true_if_predicate_map_datatype_is_xsd_decimal_and_object_datatype_can_be_coerced_to_xsd_decimal) {
	librdf_world *world = (librdf_world *) 0xA01;
	SPINDLE spindle = { .world = world };
	struct propdata_struct data = { .spindle = &spindle };
	struct spindle_predicatemap_struct predicate_map = { .datatype = NS_XSD "decimal" };
	struct propmatch_struct match = { .map = &predicate_map, .resource = (librdf_node *) 0xB01 };
	struct spindle_predicatematch_struct criterion = { 0 };
	librdf_node *node = (librdf_node *) 0xA02;
	librdf_node *obj = (librdf_node *) 0xA03;
	librdf_uri *uri = (librdf_uri *) 0xA04;
	librdf_uri *datatype_uri = (librdf_uri *) 0xA05;
	const char *value = "x";

	always_expect(librdf_node_get_literal_value_language);
	expect(librdf_node_get_literal_value_datatype_uri, will_return(datatype_uri), when(node, is_equal_to(obj)));
	expect(librdf_uri_as_string, will_return(NS_XSD "int"), when(uri, is_equal_to(datatype_uri)));
	expect(librdf_new_uri, will_return(uri), when(world, is_equal_to(world)), when(uri_string, is_equal_to(predicate_map.datatype)));
	expect(librdf_node_get_literal_value, will_return(value), when(node, is_equal_to(obj)));
	expect(librdf_new_node_from_typed_literal, will_return(node), when(world, is_equal_to(world)), when(value, is_equal_to(value)), when(xml_language, is_equal_to(NULL)), when(datatype_uri, is_equal_to(uri)));
	expect(librdf_free_uri, when(uri, is_equal_to(uri)));
	expect(twine_rdf_node_destroy, when(node, is_equal_to(match.resource)));

	int r = spindle_prop_candidate_literal_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(1));
}

Ensure(spindle_generate_props, candidate_literal_when_successful_sets_match_resource) {
	SPINDLE spindle = { .world = (librdf_world *) 0xA01 };
	struct propdata_struct data = { .spindle = &spindle };
	struct spindle_predicatemap_struct predicate_map = { .datatype = "xsd:sometype" };
	struct propmatch_struct match = { .map = &predicate_map, .resource = (librdf_node *) 0xB01 };
	struct spindle_predicatematch_struct criterion = { 0 };
	librdf_node *node = (librdf_node *) 0xA02;
	librdf_node *obj = (librdf_node *) 0xA03;

	always_expect(librdf_node_get_literal_value_language);
	always_expect(librdf_node_get_literal_value_datatype_uri);
	always_expect(librdf_new_uri);
	always_expect(librdf_node_get_literal_value);
	always_expect(librdf_new_node_from_typed_literal, will_return(node));
	always_expect(librdf_free_uri);
	always_expect(twine_rdf_node_destroy);

	int r = spindle_prop_candidate_literal_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(1));
	assert_that(match.resource, is_equal_to(node));
}

Ensure(spindle_generate_props, candidate_literal_when_successful_sets_match_priority) {
	SPINDLE spindle = { .world = (librdf_world *) 0xA01 };
	struct propdata_struct data = { .spindle = &spindle };
	struct spindle_predicatemap_struct predicate_map = { .datatype = "xsd:sometype" };
	struct propmatch_struct match = { .map = &predicate_map };
	struct spindle_predicatematch_struct criterion = { 0 };
	librdf_node *node = (librdf_node *) 0xA02;
	librdf_node *obj = (librdf_node *) 0xA03;

	always_expect(librdf_node_get_literal_value_language);
	always_expect(librdf_node_get_literal_value_datatype_uri);
	always_expect(librdf_new_uri);
	always_expect(librdf_node_get_literal_value);
	always_expect(librdf_new_node_from_typed_literal, will_return(node));
	always_expect(librdf_free_uri);
	always_expect(twine_rdf_node_destroy);

	int expected_priority = 5;
	criterion.priority = expected_priority;

	int r = spindle_prop_candidate_literal_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(1));
	assert_that(match.priority, is_equal_to(expected_priority));
}

Ensure(spindle_generate_props, candidate_literal_when_successful_sets_match_prominence_to_criterion_prominence_if_criterion_prominence_is_non_zero) {
	SPINDLE spindle = { .world = (librdf_world *) 0xA01 };
	struct propdata_struct data = { .spindle = &spindle };
	struct spindle_predicatemap_struct predicate_map = { .datatype = "xsd:sometype" };
	struct propmatch_struct match = { .map = &predicate_map };
	struct spindle_predicatematch_struct criterion = { 0 };
	librdf_node *node = (librdf_node *) 0xA02;
	librdf_node *obj = (librdf_node *) 0xA03;

	always_expect(librdf_node_get_literal_value_language);
	always_expect(librdf_node_get_literal_value_datatype_uri);
	always_expect(librdf_new_uri);
	always_expect(librdf_node_get_literal_value);
	always_expect(librdf_new_node_from_typed_literal, will_return(node));
	always_expect(librdf_free_uri);
	always_expect(twine_rdf_node_destroy);

	int expected_prominence = 5;
	criterion.prominence = expected_prominence;
	predicate_map.prominence = 1; // should be ignored

	int r = spindle_prop_candidate_literal_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(1));
	assert_that(match.prominence, is_equal_to(expected_prominence));
}

Ensure(spindle_generate_props, candidate_literal_when_successful_sets_match_prominence_to_predicate_map_prominence_if_criterion_prominence_is_zero) {
	SPINDLE spindle = { .world = (librdf_world *) 0xA01 };
	struct propdata_struct data = { .spindle = &spindle };
	struct spindle_predicatemap_struct predicate_map = { .datatype = "xsd:sometype" };
	struct propmatch_struct match = { .map = &predicate_map };
	struct spindle_predicatematch_struct criterion = { 0 };
	librdf_node *node = (librdf_node *) 0xA02;
	librdf_node *obj = (librdf_node *) 0xA03;

	always_expect(librdf_node_get_literal_value_language);
	always_expect(librdf_node_get_literal_value_datatype_uri);
	always_expect(librdf_new_uri);
	always_expect(librdf_node_get_literal_value);
	always_expect(librdf_new_node_from_typed_literal, will_return(node));
	always_expect(librdf_free_uri);
	always_expect(twine_rdf_node_destroy);

	int expected_prominence = 5;
	criterion.prominence = 0;
	predicate_map.prominence = expected_prominence;

	int r = spindle_prop_candidate_literal_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(1));
	assert_that(match.prominence, is_equal_to(expected_prominence));
}

#pragma mark -
#pragma mark spindle_prop_candidate_uri_

Ensure(spindle_generate_props, candidate_uri_returns_false_if_earlier_match_has_lower_priority_than_criterion) {
	struct propmatch_struct match = { .priority = 1 };
	struct spindle_predicatematch_struct criterion = { .priority = match.priority + 1 };

	int r = spindle_prop_candidate_uri_(NULL, &match, &criterion, NULL, NULL);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, candidate_uri_returns_false_if_earlier_match_has_equal_priority_to_criterion) {
	struct propmatch_struct match = { .priority = 1 };
	struct spindle_predicatematch_struct criterion = { .priority = match.priority };

	int r = spindle_prop_candidate_uri_(NULL, &match, &criterion, NULL, NULL);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, candidate_uri_returns_false_if_the_required_proxy_does_not_exist) {
	SPINDLE spindle = { 0 };
	struct propdata_struct data = { .spindle = &spindle };
	struct spindle_predicatemap_struct predicate_map = { .proxyonly = 1 };
	struct propmatch_struct match = { .map = &predicate_map };
	struct spindle_predicatematch_struct criterion = { 0 };
	librdf_node *obj = (librdf_node *) 0xA03;
	librdf_uri *uri = (librdf_uri *) 0xA04;
	const char *uristr = "uri";

	expect(librdf_node_get_uri, will_return(uri), when(node, is_equal_to(obj)));
	expect(librdf_uri_as_string, will_return(uristr), when(uri, is_equal_to(uri)));
	expect(spindle_proxy_locate, will_return(NULL), when(spindle, is_equal_to(&spindle)), when(uri, is_equal_to(uristr)));

	int r = spindle_prop_candidate_uri_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, candidate_uri_returns_false_if_the_required_proxy_matches_localname) {
	SPINDLE spindle = { 0 };
	struct propdata_struct data = { .spindle = &spindle, .localname = "local" };
	struct spindle_predicatemap_struct predicate_map = { .proxyonly = 1 };
	struct propmatch_struct match = { .map = &predicate_map };
	struct spindle_predicatematch_struct criterion = { 0 };
	librdf_node *obj = (librdf_node *) 0xA03;
	librdf_uri *uri = (librdf_uri *) 0xA04;
	const char *uristr = "uri";

	char *local = malloc(strlen(data.localname) + 1);
	strcpy(local, data.localname);

	expect(librdf_node_get_uri, will_return(uri), when(node, is_equal_to(obj)));
	expect(librdf_uri_as_string, will_return(uristr), when(uri, is_equal_to(uri)));
	expect(spindle_proxy_locate, will_return(local), when(spindle, is_equal_to(&spindle)), when(uri, is_equal_to(uristr)));

	int r = spindle_prop_candidate_uri_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, candidate_uri_returns_error_if_cannot_create_node_for_proxy_uri) {
	SPINDLE spindle = { 0 };
	struct propdata_struct data = { .spindle = &spindle, .localname = "local" };
	struct spindle_predicatemap_struct predicate_map = { .proxyonly = 1 };
	struct propmatch_struct match = { .map = &predicate_map };
	struct spindle_predicatematch_struct criterion = { 0 };
	librdf_node *obj = (librdf_node *) 0xA03;
	librdf_uri *uri = (librdf_uri *) 0xA04;
	const char *uristr = "uri";

	char *proxy = malloc(6);
	strcpy(proxy, "proxy");

	always_expect(librdf_node_get_uri);
	always_expect(librdf_uri_as_string);
	always_expect(spindle_proxy_locate, will_return(proxy));
	expect(twine_rdf_node_createuri, will_return(NULL), when(uri, is_equal_to(proxy)));

	int r = spindle_prop_candidate_uri_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, candidate_uri_returns_error_if_cloning_object_node_fails) {
	SPINDLE spindle = { 0 };
	struct propdata_struct data = { .spindle = &spindle };
	struct spindle_predicatemap_struct predicate_map = { 0 };
	struct spindle_predicatematch_struct criterion = { .priority = 1 };
	struct propmatch_struct match = { .map = &predicate_map, .priority = criterion.priority + 1 };
	librdf_node *obj = (librdf_node *) 0xA03;

	expect(twine_rdf_node_clone, will_return(NULL), when(node, is_equal_to(obj)));

	int r = spindle_prop_candidate_uri_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, candidate_uri_returns_true_on_success) {
	SPINDLE spindle = { 0 };
	struct propdata_struct data = { .spindle = &spindle };
	struct spindle_predicatemap_struct predicate_map = { 0 };
	struct spindle_predicatematch_struct criterion = { .priority = 1 };
	struct propmatch_struct match = {
		.map = &predicate_map,
		.priority = criterion.priority + 1,
		.resource = (librdf_node *) 0xB01
	};
	librdf_node *obj = (librdf_node *) 0xA03;
	librdf_node *clone = (librdf_node *) 0xA04;

	expect(twine_rdf_node_clone, will_return(clone), when(node, is_equal_to(obj)));
	expect(twine_rdf_node_destroy, when(node, is_equal_to(match.resource)));

	int r = spindle_prop_candidate_uri_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(1));
}

Ensure(spindle_generate_props, candidate_uri_when_successful_sets_match_resource) {
	SPINDLE spindle = { 0 };
	struct propdata_struct data = { .spindle = &spindle };
	struct spindle_predicatemap_struct predicate_map = { 0 };
	struct spindle_predicatematch_struct criterion = { .priority = 1 };
	struct propmatch_struct match = {
		.map = &predicate_map,
		.priority = criterion.priority + 1,
		.resource = (librdf_node *) 0xB01
	};
	librdf_node *obj = (librdf_node *) 0xA03;
	librdf_node *clone = (librdf_node *) 0xA04;

	expect(twine_rdf_node_clone, will_return(clone), when(node, is_equal_to(obj)));
	expect(twine_rdf_node_destroy, when(node, is_equal_to(match.resource)));

	int r = spindle_prop_candidate_uri_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(1));
	assert_that(match.resource, is_equal_to(clone));
}

Ensure(spindle_generate_props, candidate_uri_when_successful_sets_match_priority) {
	SPINDLE spindle = { 0 };
	struct propdata_struct data = { .spindle = &spindle };
	struct spindle_predicatemap_struct predicate_map = { 0 };
	struct spindle_predicatematch_struct criterion = { 0 };
	struct propmatch_struct match = {
		.map = &predicate_map,
		.resource = (librdf_node *) 0xB01
	};
	librdf_node *obj = (librdf_node *) 0xA03;
	librdf_node *clone = (librdf_node *) 0xA04;

	expect(twine_rdf_node_clone, will_return(clone), when(node, is_equal_to(obj)));
	expect(twine_rdf_node_destroy, when(node, is_equal_to(match.resource)));

	int expected_priority = 5;
	criterion.priority = expected_priority;
	match.priority = expected_priority + 1;

	int r = spindle_prop_candidate_uri_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(1));
	assert_that(match.priority, is_equal_to(expected_priority));
}

Ensure(spindle_generate_props, candidate_uri_when_successful_sets_match_prominence_to_criterion_prominence_if_criterion_prominence_is_non_zero) {
	SPINDLE spindle = { 0 };
	struct propdata_struct data = { .spindle = &spindle };
	struct spindle_predicatemap_struct predicate_map = { 0 };
	struct spindle_predicatematch_struct criterion = { .priority = 1 };
	struct propmatch_struct match = {
		.map = &predicate_map,
		.priority = criterion.priority + 1,
		.resource = (librdf_node *) 0xB01
	};
	librdf_node *obj = (librdf_node *) 0xA03;
	librdf_node *clone = (librdf_node *) 0xA04;

	expect(twine_rdf_node_clone, will_return(clone), when(node, is_equal_to(obj)));
	expect(twine_rdf_node_destroy, when(node, is_equal_to(match.resource)));

	int expected_prominence = 5;
	criterion.prominence = expected_prominence;
	predicate_map.prominence = 1; // should be ignored

	int r = spindle_prop_candidate_uri_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(1));
	assert_that(match.prominence, is_equal_to(expected_prominence));
}

Ensure(spindle_generate_props, candidate_uri_when_successful_sets_match_prominence_to_predicate_map_prominence_if_criterion_prominence_is_zero) {
	SPINDLE spindle = { 0 };
	struct propdata_struct data = { .spindle = &spindle };
	struct spindle_predicatemap_struct predicate_map = { 0 };
	struct spindle_predicatematch_struct criterion = { .priority = 1 };
	struct propmatch_struct match = {
		.map = &predicate_map,
		.priority = criterion.priority + 1,
		.resource = (librdf_node *) 0xB01
	};
	librdf_node *obj = (librdf_node *) 0xA03;
	librdf_node *clone = (librdf_node *) 0xA04;

	expect(twine_rdf_node_clone, will_return(clone), when(node, is_equal_to(obj)));
	expect(twine_rdf_node_destroy, when(node, is_equal_to(match.resource)));

	int expected_prominence = 5;
	criterion.prominence = 0;
	predicate_map.prominence = expected_prominence;

	int r = spindle_prop_candidate_uri_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(1));
	assert_that(match.prominence, is_equal_to(expected_prominence));
}

#pragma mark -
#pragma mark spindle_prop_candidate_

/*
	spindle_prop_candidate_ always returns zero.
*/

Ensure(spindle_generate_props, candidate_returns_false_for_raptor_term_type_unknown) {
	struct spindle_predicatemap_struct predicate_map = { .expected = RAPTOR_TERM_TYPE_UNKNOWN };
	struct propmatch_struct match = { .map = &predicate_map };

	int r = spindle_prop_candidate_(NULL, &match, NULL, NULL, NULL);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, candidate_returns_false_for_raptor_term_type_blank) {
	struct spindle_predicatemap_struct predicate_map = { .expected = RAPTOR_TERM_TYPE_BLANK };
	struct propmatch_struct match = { .map = &predicate_map };

	int r = spindle_prop_candidate_(NULL, &match, NULL, NULL, NULL);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, candidate_returns_false_for_raptor_term_type_uri_if_object_is_not_a_resource) {
	struct spindle_predicatemap_struct predicate_map = { .expected = RAPTOR_TERM_TYPE_URI };
	struct propmatch_struct match = { .map = &predicate_map };
	librdf_node *obj = (librdf_node *) 0xA03;

	expect(librdf_node_is_resource, will_return(0), when(node, is_equal_to(obj)));

	int r = spindle_prop_candidate_(NULL, &match, NULL, NULL, obj);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, candidate_returns_false_for_raptor_term_type_literal_if_object_is_not_a_literal) {
	struct spindle_predicatemap_struct predicate_map = { .expected = RAPTOR_TERM_TYPE_LITERAL };
	struct propmatch_struct match = { .map = &predicate_map };
	librdf_node *obj = (librdf_node *) 0xA03;

	expect(librdf_node_is_literal, will_return(0), when(node, is_equal_to(obj)));

	int r = spindle_prop_candidate_(NULL, &match, NULL, NULL, obj);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, candidate_returns_result_from_candidate_uri_for_raptor_term_type_uri_if_object_is_a_resource) {
	// config from "candidate_uri_returns_true_on_success" but calls
	// spindle_prop_candidate_(RAPTOR_TERM_TYPE_URI) and also expects a
	// call to librdf_node_is_resource()

	SPINDLE spindle = { 0 };
	struct propdata_struct data = { .spindle = &spindle };
	struct spindle_predicatemap_struct predicate_map = { .expected = RAPTOR_TERM_TYPE_URI };
	struct spindle_predicatematch_struct criterion = { .priority = 1 };
	struct propmatch_struct match = {
		.map = &predicate_map,
		.priority = criterion.priority + 1,
		.resource = (librdf_node *) 0xB01
	};
	librdf_node *obj = (librdf_node *) 0xA03;
	librdf_node *clone = (librdf_node *) 0xA04;

	expect(librdf_node_is_resource, will_return(1), when(node, is_equal_to(obj)));
	expect(twine_rdf_node_clone, will_return(clone), when(node, is_equal_to(obj)));
	expect(twine_rdf_node_destroy, when(node, is_equal_to(match.resource)));

	int r = spindle_prop_candidate_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(1));
}

Ensure(spindle_generate_props, candidate_returns_result_from_candidate_literal_for_raptor_term_type_literal_if_object_is_a_literal) {
	// config from "candidate_literal_returns_false_if_matching_literal_has_lower_priority_than_criterion"
	// but calls spindle_prop_candidate_(RAPTOR_TERM_TYPE_LITERAL) and also
	// expects a call to librdf_node_is_literal()

	struct propdata_struct data = { 0 };
	struct spindle_predicatemap_struct predicate_map = {
		.expected = RAPTOR_TERM_TYPE_LITERAL,
		.datatype = "xsd:sometype"
	};
	struct propmatch_struct match = { .map = &predicate_map, .priority = 1 };
	struct spindle_predicatematch_struct criterion = { .priority = match.priority + 1 };
	librdf_node *obj = (librdf_node *) 0xA03;

	expect(librdf_node_is_literal, will_return(1), when(node, is_equal_to(obj)));
	expect(librdf_node_get_literal_value_language, when(node, is_equal_to(obj)));

	int r = spindle_prop_candidate_(&data, &match, &criterion, NULL, obj);
	assert_that(r, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_prop_test_

/*
	These tests really only prove the function exits before strcmp(NULL, NULL) is reached.
	After the code under test is refactored, we can test each conditional/filter in isolation.

	spindle_prop_test_ always returns zero.
*/

Ensure(spindle_generate_props, prop_test_returns_no_error_for_empty_predicate_map_list) {
	struct spindle_predicatemap_struct maps = { 0 };
	struct propdata_struct data = { .maps = &maps };

	int r = spindle_prop_test_(&data, NULL, NULL, 0);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_test_skips_predicate_map_if_predicate_map_has_no_matches_list) {
	struct spindle_predicatemap_struct maps[] = {
		{ .target = "target" },
		{ 0 }
	};
	struct propdata_struct data = { .maps = maps };

	int r = spindle_prop_test_(&data, NULL, NULL, 0);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_test_stops_iterating_predicate_map_matches_list_when_at_end) {
	struct spindle_predicatematch_struct matches = { 0 };
	struct spindle_predicatemap_struct maps[] = {
		{ .target = "target", .matches = &matches },
		{ 0 }
	};
	struct propdata_struct data = { .maps = maps };

	int r = spindle_prop_test_(&data, NULL, NULL, 0);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_test_skips_predicate_map_match_if_match_inversion_property_is_true_and_inverse_parameter_is_false) {
	struct spindle_predicatematch_struct matches[] = {
		{ .predicate = "pred", .inverse = 1 },
		{ 0 }
	};
	struct spindle_predicatemap_struct maps[] = {
		{ .target = "target", .matches = matches },
		{ 0 }
	};
	struct propdata_struct data = { .maps = maps };

	int r = spindle_prop_test_(&data, NULL, NULL, 0);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_test_skips_predicate_map_match_if_match_inversion_property_is_false_and_inverse_parameter_is_true) {
	struct spindle_predicatematch_struct matches[] = {
		{ .predicate = "pred" },
		{ 0 }
	};
	struct spindle_predicatemap_struct maps[] = {
		{ .target = "target", .matches = matches },
		{ 0 }
	};
	struct propdata_struct data = { .maps = maps };

	int r = spindle_prop_test_(&data, NULL, NULL, 1);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_test_skips_predicate_map_match_if_match_onlyfor_property_is_not_NULL_and_propdata_classname_is_NULL) {
	struct spindle_predicatematch_struct matches[] = {
		{ .predicate = "pred", .onlyfor = "only for this class" },
		{ 0 }
	};
	struct spindle_predicatemap_struct maps[] = {
		{ .target = "target", .matches = matches },
		{ 0 }
	};
	struct propdata_struct data = { .maps = maps };

	int r = spindle_prop_test_(&data, NULL, NULL, 0);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_test_skips_predicate_map_match_if_match_onlyfor_property_is_not_equal_to_propdata_classname) {
	struct spindle_predicatematch_struct matches[] = {
		{ .predicate = "pred", .onlyfor = "only for this class" },
		{ 0 }
	};
	struct spindle_predicatemap_struct maps[] = {
		{ .target = "target", .matches = matches },
		{ 0 }
	};
	struct propdata_struct data = {
		.maps = maps,
		.classname = "different class"
	};

	int r = spindle_prop_test_(&data, NULL, NULL, 0);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_test_skips_predicate_map_match_if_predicate_parameter_differs_from_match_predicate) {
	struct spindle_predicatematch_struct matches[] = {
		{ .predicate = "pred" },
		{ 0 }
	};
	struct spindle_predicatemap_struct maps[] = {
		{ .target = "target", .matches = matches },
		{ 0 }
	};
	struct propdata_struct data = { .maps = maps };

	int r = spindle_prop_test_(&data, NULL, "different pred", 0);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_test_processes_statement_object_if_predicate_parameter_is_equal_to_predicate_map_match_predicate_and_inverse_parameter_is_false) {
	struct spindle_predicatematch_struct matches[] = {
		{ .predicate = "pred" },
		{ 0 }
	};
	struct spindle_predicatemap_struct maps[] = {
		{ .target = "target", .matches = matches },
		{ 0 }
	};
	struct spindle_predicatemap_struct predicate_map = {
		.expected = RAPTOR_TERM_TYPE_LITERAL
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &predicate_map },
		{ 0 }
	};
	struct propdata_struct data = { .maps = maps, .matches = prop_matches };
	librdf_statement *statement = (librdf_statement *) 0xA01;
	librdf_node *node = (librdf_node *) 0xA02;

	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, when(node, is_equal_to(node)));

	int r = spindle_prop_test_(&data, statement, "pred", 0);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_test_processes_statement_subject_if_predicate_parameter_is_equal_to_predicate_map_match_predicate_and_inverse_parameter_is_true) {
	struct spindle_predicatematch_struct matches[] = {
		{ .predicate = "pred", .inverse = 1 },
		{ 0 }
	};
	struct spindle_predicatemap_struct maps[] = {
		{ .target = "target", .matches = matches },
		{ 0 }
	};
	struct spindle_predicatemap_struct predicate_map = {
		.expected = RAPTOR_TERM_TYPE_LITERAL
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &predicate_map },
		{ 0 }
	};
	struct propdata_struct data = { .maps = maps, .matches = prop_matches };
	librdf_statement *statement = (librdf_statement *) 0xA01;
	librdf_node *node = (librdf_node *) 0xA02;

	expect(librdf_statement_get_subject, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, when(node, is_equal_to(node)));

	int r = spindle_prop_test_(&data, statement, "pred", 1);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_test_can_match_the_same_predicate_from_match_lists_corresponding_to_different_targets) {
	struct spindle_predicatematch_struct matches_1[] = {
		{ .predicate = "pred 1" },
		{ .predicate = "pred 2" },
		{ .predicate = "pred 3" },
		{ 0 }
	};
	struct spindle_predicatematch_struct matches_2[] = {
		{ .predicate = "pred 2" },
		{ .predicate = "pred 3" },
		{ .predicate = "pred 4" },
		{ 0 }
	};
	struct spindle_predicatemap_struct maps[] = {
		{ .target = "target 1", .matches = matches_1 },
		{ .target = "target 2", .matches = matches_2 },
		{ 0 }
	};
	struct spindle_predicatemap_struct predicate_map_1 = {
		.expected = RAPTOR_TERM_TYPE_LITERAL
	};
	struct spindle_predicatemap_struct predicate_map_2 = {
		.expected = RAPTOR_TERM_TYPE_URI
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &predicate_map_1 },
		{ .map = &predicate_map_2 },
		{ 0 }
	};
	struct propdata_struct data = { .maps = maps, .matches = prop_matches };
	librdf_statement *statement = (librdf_statement *) 0xA01;
	librdf_node *node_1 = (librdf_node *) 0xA02;
	librdf_node *node_2 = (librdf_node *) 0xA03;

	expect(librdf_statement_get_object, will_return(node_1), when(statement, is_equal_to(statement)));
	expect(librdf_statement_get_object, will_return(node_2), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, when(node, is_equal_to(node_1)));
	expect(librdf_node_is_resource, when(node, is_equal_to(node_2)));
	never_expect(librdf_statement_get_object);
	never_expect(librdf_statement_get_subject);

	int r = spindle_prop_test_(&data, statement, "pred 3", 0);
	assert_that(r, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_prop_apply_

Ensure(spindle_generate_props, prop_apply_returns_error_if_self_node_cannot_be_cloned) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = NULL;
	SPINDLEENTRY entry = { .self = self };
	struct propdata_struct data = { .entry = &entry };

	expect(twine_rdf_node_clone, will_return(clone), when(node, is_equal_to(self)));

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, prop_apply_returns_error_if_cannot_create_empty_statement) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	SPINDLEENTRY entry = { .self = self };
	struct propmatch_struct prop_matches = { 0 };
	struct propdata_struct data = { .entry = &entry, .matches = &prop_matches };

	expect(twine_rdf_node_clone, will_return(clone), when(node, is_equal_to(self)));
	expect(twine_rdf_st_create, will_return(NULL));
	expect(librdf_free_node, when(node, is_equal_to(clone)));

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, prop_apply_returns_no_error_if_predicate_match_list_map_is_empty) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_statement *statement = (librdf_statement *) 0xA03;
	SPINDLEENTRY entry = { .self = self };
	struct propmatch_struct prop_matches = { 0 };
	struct propdata_struct data = { .entry = &entry, .matches = &prop_matches };

	expect(twine_rdf_node_clone, will_return(clone), when(node, is_equal_to(self)));
	expect(twine_rdf_st_create, will_return(statement));
	expect(librdf_statement_set_subject, when(statement, is_equal_to(statement)), when(node, is_equal_to(clone)));
	expect(librdf_free_statement, when(statement, is_equal_to(statement)));

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_apply_returns_error_if_cloning_the_statement_contining_only_a_subject_fails) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_statement *statement = (librdf_statement *) 0xA03;
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = "target"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map },
		{ 0 }
	};
	struct propdata_struct data = { .entry = &entry, .matches = prop_matches };

	expect(twine_rdf_node_clone, will_return(clone), when(node, is_equal_to(self)));
	expect(twine_rdf_st_create, will_return(statement));
	expect(librdf_statement_set_subject, when(statement, is_equal_to(statement)), when(node, is_equal_to(clone)));
	expect(twine_rdf_st_clone, will_return(NULL), when(src, is_equal_to(statement)));
	expect(librdf_free_statement, when(statement, is_equal_to(statement)));

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, prop_apply_returns_error_if_creating_a_node_for_the_matchs_map_target_fails) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_statement *subject_statement = (librdf_statement *) 0xA03;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA04;
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = "target"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map },
		{ 0 }
	};
	struct propdata_struct data = { .entry = &entry, .matches = prop_matches };

	expect(twine_rdf_node_clone, will_return(clone), when(node, is_equal_to(self)));
	expect(twine_rdf_st_create, will_return(subject_statement));
	expect(librdf_statement_set_subject, when(statement, is_equal_to(subject_statement)), when(node, is_equal_to(clone)));
	expect(twine_rdf_st_clone, will_return(predicate_statement), when(src, is_equal_to(subject_statement)));
	expect(twine_rdf_node_createuri, will_return(NULL), when(uri, is_equal_to_string(map.target)));
	expect(librdf_free_statement, when(statement, is_equal_to(predicate_statement)));
	expect(librdf_free_statement, when(statement, is_equal_to(subject_statement)));

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, prop_apply_returns_no_error_if_the_match_has_no_resource_or_literals) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *target = (librdf_node *) 0xA03;
	librdf_statement *subject_statement = (librdf_statement *) 0xA04;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA05;
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = "target"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map },
		{ 0 }
	};
	struct propdata_struct data = { .entry = &entry, .matches = prop_matches };

	expect(twine_rdf_node_clone, will_return(clone), when(node, is_equal_to(self)));
	expect(twine_rdf_st_create, will_return(subject_statement));
	expect(librdf_statement_set_subject, when(statement, is_equal_to(subject_statement)), when(node, is_equal_to(clone)));
	expect(twine_rdf_st_clone, will_return(predicate_statement), when(src, is_equal_to(subject_statement)));
	expect(twine_rdf_node_createuri, will_return(target), when(uri, is_equal_to_string(map.target)));
	expect(librdf_statement_set_predicate, when(statement, is_equal_to(predicate_statement)), when(node, is_equal_to(target)));
	expect(librdf_free_statement, when(statement, is_equal_to(predicate_statement)));
	expect(librdf_free_statement, when(statement, is_equal_to(subject_statement)));

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_apply_builds_and_adds_a_target_statement_to_the_proxy_model_if_the_match_has_a_resource) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *target = (librdf_node *) 0xA03;
	librdf_node *resource = (librdf_node *) 0xA04;
	librdf_statement *subject_statement = (librdf_statement *) 0xA05;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA06;
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = "target"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .resource = resource },
		{ 0 }
	};
	struct propdata_struct data = {
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	expect(twine_rdf_node_clone, will_return(clone), when(node, is_equal_to(self)));
	expect(twine_rdf_st_create, will_return(subject_statement));
	expect(librdf_statement_set_subject, when(statement, is_equal_to(subject_statement)), when(node, is_equal_to(clone)));
	expect(twine_rdf_st_clone, will_return(predicate_statement), when(src, is_equal_to(subject_statement)));
	expect(twine_rdf_node_createuri, will_return(target), when(uri, is_equal_to_string(map.target)));
	expect(librdf_statement_set_predicate, when(statement, is_equal_to(predicate_statement)), when(node, is_equal_to(target)));
	expect(librdf_statement_set_object, when(statement, is_equal_to(predicate_statement)), when(node, is_equal_to(resource)));
	expect(twine_rdf_model_add_st, when(model, is_equal_to(data.proxymodel)), when(statement, is_equal_to(predicate_statement)), when(ctx, is_equal_to(data.context)));
/*
	NOT CALLED: PROBABLY A BUG -- twine_rdf_model_add_st() calls librdf_model_add_statement() which copies, not assigns, the statement into the model
	expect(librdf_free_statement, when(statement, is_equal_to(predicate_statement)));
*/
	expect(librdf_free_statement, when(statement, is_equal_to(subject_statement)));

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_apply_returns_error_if_the_match_has_a_resource_and_adding_statement_to_proxy_model_fails) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *target = (librdf_node *) 0xA03;
	librdf_node *resource = (librdf_node *) 0xA04;
	librdf_statement *subject_statement = (librdf_statement *) 0xA05;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA06;
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = "target"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .resource = resource },
		{ 0 }
	};
	struct propdata_struct data = {
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_st_create, will_return(subject_statement));
	always_expect(librdf_statement_set_subject);
	always_expect(twine_rdf_st_clone, will_return(predicate_statement));
	always_expect(twine_rdf_node_createuri, will_return(target));
	always_expect(librdf_statement_set_predicate);
	always_expect(librdf_statement_set_object);
	expect(twine_rdf_model_add_st, will_return(-1), when(model, is_equal_to(data.proxymodel)), when(statement, is_equal_to(predicate_statement)), when(ctx, is_equal_to(data.context)));
	always_expect(librdf_free_statement);

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, prop_apply_clears_the_resource_of_a_successful_match) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *target = (librdf_node *) 0xA03;
	librdf_node *resource = (librdf_node *) 0xA04;
	librdf_statement *subject_statement = (librdf_statement *) 0xA05;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA06;
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = "target"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .resource = resource },
		{ 0 }
	};
	struct propdata_struct data = {
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_st_create, will_return(subject_statement));
	always_expect(librdf_statement_set_subject);
	always_expect(twine_rdf_st_clone, will_return(predicate_statement));
	always_expect(twine_rdf_node_createuri, will_return(target));
	always_expect(librdf_statement_set_predicate);
	always_expect(librdf_statement_set_object);
	always_expect(twine_rdf_model_add_st);
	always_expect(librdf_free_statement);

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
	assert_that(data.matches[0].resource, is_equal_to(NULL));
}

Ensure(spindle_generate_props, prop_apply_when_the_match_has_a_resource_and_is_successful_adds_the_target_statement_to_the_root_model_if_the_matched_map_is_indexed_and_not_inverted_and_if_multigraph_is_true) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *target = (librdf_node *) 0xA03;
	librdf_node *resource = (librdf_node *) 0xA04;
	librdf_statement *subject_statement = (librdf_statement *) 0xA05;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA06;
	SPINDLE spindle = { .rootgraph = (librdf_node *) 0xC01, .multigraph = 1 };
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = "target",
		.indexed = 1
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .resource = resource },
		{ 0 }
	};
	struct propdata_struct data = {
		.spindle = &spindle,
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_st_create, will_return(subject_statement));
	always_expect(librdf_statement_set_subject);
	always_expect(twine_rdf_st_clone, will_return(predicate_statement));
	always_expect(twine_rdf_node_createuri, will_return(target));
	always_expect(librdf_statement_set_predicate);
	always_expect(librdf_statement_set_object);
	expect(twine_rdf_model_add_st, when(model, is_equal_to(data.proxymodel)), when(statement, is_equal_to(predicate_statement)), when(ctx, is_equal_to(data.context)));
	expect(twine_rdf_model_add_st, when(model, is_equal_to(data.rootmodel)), when(statement, is_equal_to(predicate_statement)), when(ctx, is_equal_to(spindle.rootgraph)));
	always_expect(librdf_free_statement);

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_apply_returns_error_if_the_match_has_a_resource_and_adding_the_target_statement_to_the_root_model_fails) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *target = (librdf_node *) 0xA03;
	librdf_node *resource = (librdf_node *) 0xA04;
	librdf_statement *subject_statement = (librdf_statement *) 0xA05;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA06;
	SPINDLE spindle = { .rootgraph = (librdf_node *) 0xC01, .multigraph = 1 };
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = "target",
		.indexed = 1
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .resource = resource },
		{ 0 }
	};
	struct propdata_struct data = {
		.spindle = &spindle,
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_st_create, will_return(subject_statement));
	always_expect(librdf_statement_set_subject);
	always_expect(twine_rdf_st_clone, will_return(predicate_statement));
	always_expect(twine_rdf_node_createuri, will_return(target));
	always_expect(librdf_statement_set_predicate);
	always_expect(librdf_statement_set_object);
	expect(twine_rdf_model_add_st);
	expect(twine_rdf_model_add_st, will_return(-1), when(model, is_equal_to(data.rootmodel)), when(statement, is_equal_to(predicate_statement)), when(ctx, is_equal_to(spindle.rootgraph)));
	always_expect(librdf_free_statement);

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, prop_apply_sets_the_title_match_to_the_match_struct_mapping_to_rdfs_label) {
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	SPINDLEENTRY entry = { .self = node };
	struct spindle_predicatemap_struct map1 = {
		.target = "target"
	};
	struct spindle_predicatemap_struct map2 = {
		.target = NS_RDFS "label"
	};
	struct spindle_predicatemap_struct map3 = {
		.target = NS_DCTERMS "description"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map1, .resource = node },
		{ .map = &map2, .resource = node },
		{ .map = &map3, .resource = node },
		{ 0 }
	};
	struct propdata_struct data = {
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(twine_rdf_node_clone, will_return(node));
	always_expect(twine_rdf_st_create, will_return(statement));
	always_expect(librdf_statement_set_subject);
	always_expect(twine_rdf_st_clone, will_return(statement));
	always_expect(twine_rdf_node_createuri, will_return(node));
	always_expect(librdf_statement_set_predicate);
	always_expect(librdf_statement_set_object);
	always_expect(twine_rdf_model_add_st);
	always_expect(librdf_free_statement);

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
	assert_that(data.titlematch, is_equal_to(&prop_matches[1]));
}

Ensure(spindle_generate_props, prop_apply_sets_the_description_match_to_the_match_struct_mapping_to_dct_description) {
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	SPINDLEENTRY entry = { .self = node };
	struct spindle_predicatemap_struct map1 = {
		.target = "target"
	};
	struct spindle_predicatemap_struct map2 = {
		.target = NS_RDFS "label"
	};
	struct spindle_predicatemap_struct map3 = {
		.target = NS_DCTERMS "description"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map1, .resource = node },
		{ .map = &map2, .resource = node },
		{ .map = &map3, .resource = node },
		{ 0 }
	};
	struct propdata_struct data = {
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(twine_rdf_node_clone, will_return(node));
	always_expect(twine_rdf_st_create, will_return(statement));
	always_expect(librdf_statement_set_subject);
	always_expect(twine_rdf_st_clone, will_return(statement));
	always_expect(twine_rdf_node_createuri, will_return(node));
	always_expect(librdf_statement_set_predicate);
	always_expect(librdf_statement_set_object);
	always_expect(twine_rdf_model_add_st);
	always_expect(librdf_free_statement);

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
	assert_that(data.descmatch, is_equal_to(&prop_matches[2]));
}

Ensure(spindle_generate_props, prop_apply_decrements_the_proxy_entry_score_by_the_prominance_of_each_match) {
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	SPINDLEENTRY entry = { .self = node, .score = 99 };
	struct spindle_predicatemap_struct map = {
		.target = "target"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .resource = node, .prominence = 33 },
		{ .map = &map, .resource = node, .prominence = 22 },
		{ 0 }
	};
	struct propdata_struct data = {
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(twine_rdf_node_clone, will_return(node));
	always_expect(twine_rdf_st_create, will_return(statement));
	always_expect(librdf_statement_set_subject);
	always_expect(twine_rdf_st_clone, will_return(statement));
	always_expect(twine_rdf_node_createuri, will_return(node));
	always_expect(librdf_statement_set_predicate);
	always_expect(librdf_statement_set_object);
	always_expect(twine_rdf_model_add_st);
	always_expect(librdf_free_statement);

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
	assert_that(entry.score, is_equal_to(44));
}

Ensure(spindle_generate_props, prop_apply_does_not_set_longitude_and_geo_flag_if_the_match_has_a_resource_and_map_target_is_geo_long_and_the_literal_has_no_datatype_uri) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *target = (librdf_node *) 0xA03;
	librdf_node *resource = (librdf_node *) 0xA04;
	librdf_statement *subject_statement = (librdf_statement *) 0xA05;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA06;
	librdf_uri *xsd_decimal = (librdf_uri *) 0xA07;
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = NS_GEO "long"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .resource = resource },
		{ 0 }
	};
	struct propdata_struct data = {
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_st_create, will_return(subject_statement));
	always_expect(librdf_statement_set_subject);
	always_expect(twine_rdf_st_clone, will_return(predicate_statement));
	always_expect(twine_rdf_node_createuri, will_return(target));
	always_expect(librdf_statement_set_predicate);
	expect(librdf_node_get_literal_value_datatype_uri, will_return(NULL));
	never_expect(librdf_uri_as_string);
	never_expect(librdf_node_get_literal_value);
	always_expect(librdf_statement_set_object);
	always_expect(twine_rdf_model_add_st);
	always_expect(librdf_free_statement);

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
	assert_that(data.has_geo, is_equal_to(0));
	assert_that_double(data.lon, is_equal_to_double(0.0));
}

Ensure(spindle_generate_props, prop_apply_does_not_set_latitude_and_geo_flag_if_the_match_has_a_resource_and_map_target_is_geo_lat_and_the_literal_has_no_datatype_uri) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *target = (librdf_node *) 0xA03;
	librdf_node *resource = (librdf_node *) 0xA04;
	librdf_statement *subject_statement = (librdf_statement *) 0xA05;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA06;
	librdf_uri *xsd_decimal = (librdf_uri *) 0xA07;
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = NS_GEO "lat"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .resource = resource },
		{ 0 }
	};
	struct propdata_struct data = {
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_st_create, will_return(subject_statement));
	always_expect(librdf_statement_set_subject);
	always_expect(twine_rdf_st_clone, will_return(predicate_statement));
	always_expect(twine_rdf_node_createuri, will_return(target));
	always_expect(librdf_statement_set_predicate);
	expect(librdf_node_get_literal_value_datatype_uri, will_return(NULL));
	never_expect(librdf_uri_as_string);
	never_expect(librdf_node_get_literal_value);
	always_expect(librdf_statement_set_object);
	always_expect(twine_rdf_model_add_st);
	always_expect(librdf_free_statement);

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
	assert_that(data.has_geo, is_equal_to(0));
	assert_that_double(data.lat, is_equal_to_double(0.0));
}

Ensure(spindle_generate_props, prop_apply_does_not_set_longitude_and_geo_flag_if_the_match_has_a_resource_and_map_target_is_geo_long_and_the_datatype_uri_string_is_NULL) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *target = (librdf_node *) 0xA03;
	librdf_node *resource = (librdf_node *) 0xA04;
	librdf_statement *subject_statement = (librdf_statement *) 0xA05;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA06;
	librdf_uri *xsd_decimal = (librdf_uri *) 0xA07;
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = NS_GEO "long"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .resource = resource },
		{ 0 }
	};
	struct propdata_struct data = {
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_st_create, will_return(subject_statement));
	always_expect(librdf_statement_set_subject);
	always_expect(twine_rdf_st_clone, will_return(predicate_statement));
	always_expect(twine_rdf_node_createuri, will_return(target));
	always_expect(librdf_statement_set_predicate);
	expect(librdf_node_get_literal_value_datatype_uri, will_return(xsd_decimal));
	expect(librdf_uri_as_string, will_return(NULL), when(uri, is_equal_to(xsd_decimal)));
	never_expect(librdf_node_get_literal_value);
	always_expect(librdf_statement_set_object);
	always_expect(twine_rdf_model_add_st);
	always_expect(librdf_free_statement);

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
	assert_that(data.has_geo, is_equal_to(0));
	assert_that_double(data.lon, is_equal_to_double(0.0));
}

Ensure(spindle_generate_props, prop_apply_does_not_set_latitude_and_geo_flag_if_the_match_has_a_resource_and_map_target_is_geo_lat_and_the_datatype_uri_string_is_NULL) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *target = (librdf_node *) 0xA03;
	librdf_node *resource = (librdf_node *) 0xA04;
	librdf_statement *subject_statement = (librdf_statement *) 0xA05;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA06;
	librdf_uri *xsd_decimal = (librdf_uri *) 0xA07;
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = NS_GEO "lat"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .resource = resource },
		{ 0 }
	};
	struct propdata_struct data = {
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_st_create, will_return(subject_statement));
	always_expect(librdf_statement_set_subject);
	always_expect(twine_rdf_st_clone, will_return(predicate_statement));
	always_expect(twine_rdf_node_createuri, will_return(target));
	always_expect(librdf_statement_set_predicate);
	expect(librdf_node_get_literal_value_datatype_uri, will_return(xsd_decimal));
	expect(librdf_uri_as_string, will_return(NULL), when(uri, is_equal_to(xsd_decimal)));
	never_expect(librdf_node_get_literal_value);
	always_expect(librdf_statement_set_object);
	always_expect(twine_rdf_model_add_st);
	always_expect(librdf_free_statement);

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
	assert_that(data.has_geo, is_equal_to(0));
	assert_that_double(data.lat, is_equal_to_double(0.0));
}

Ensure(spindle_generate_props, prop_apply_does_not_set_longitude_and_geo_flag_if_the_match_has_a_resource_and_map_target_is_geo_long_and_the_datatype_uri_is_not_xsd_decimal) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *target = (librdf_node *) 0xA03;
	librdf_node *resource = (librdf_node *) 0xA04;
	librdf_statement *subject_statement = (librdf_statement *) 0xA05;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA06;
	librdf_uri *xsd_decimal = (librdf_uri *) 0xA07;
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = NS_GEO "long"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .resource = resource },
		{ 0 }
	};
	struct propdata_struct data = {
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_st_create, will_return(subject_statement));
	always_expect(librdf_statement_set_subject);
	always_expect(twine_rdf_st_clone, will_return(predicate_statement));
	always_expect(twine_rdf_node_createuri, will_return(target));
	always_expect(librdf_statement_set_predicate);
	expect(librdf_node_get_literal_value_datatype_uri, will_return(xsd_decimal));
	expect(librdf_uri_as_string, will_return("not xsd:decimal"), when(uri, is_equal_to(xsd_decimal)));
	never_expect(librdf_node_get_literal_value);
	always_expect(librdf_statement_set_object);
	always_expect(twine_rdf_model_add_st);
	always_expect(librdf_free_statement);

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
	assert_that(data.has_geo, is_equal_to(0));
	assert_that_double(data.lon, is_equal_to_double(0.0));
}

Ensure(spindle_generate_props, prop_apply_does_not_set_latitude_and_geo_flag_if_the_match_has_a_resource_and_map_target_is_geo_lat_and_the_datatype_uri_is_not_xsd_decimal) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *target = (librdf_node *) 0xA03;
	librdf_node *resource = (librdf_node *) 0xA04;
	librdf_statement *subject_statement = (librdf_statement *) 0xA05;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA06;
	librdf_uri *xsd_decimal = (librdf_uri *) 0xA07;
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = NS_GEO "lat"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .resource = resource },
		{ 0 }
	};
	struct propdata_struct data = {
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_st_create, will_return(subject_statement));
	always_expect(librdf_statement_set_subject);
	always_expect(twine_rdf_st_clone, will_return(predicate_statement));
	always_expect(twine_rdf_node_createuri, will_return(target));
	always_expect(librdf_statement_set_predicate);
	expect(librdf_node_get_literal_value_datatype_uri, will_return(xsd_decimal));
	expect(librdf_uri_as_string, will_return("not xsd:decimal"), when(uri, is_equal_to(xsd_decimal)));
	never_expect(librdf_node_get_literal_value);
	always_expect(librdf_statement_set_object);
	always_expect(twine_rdf_model_add_st);
	always_expect(librdf_free_statement);

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
	assert_that(data.has_geo, is_equal_to(0));
	assert_that_double(data.lat, is_equal_to_double(0.0));
}

Ensure(spindle_generate_props, prop_apply_sets_longitude_and_geo_flag_if_the_match_has_a_resource_and_map_target_is_geo_long_and_the_datatype_uri_is_xsd_decimal) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *target = (librdf_node *) 0xA03;
	librdf_node *resource = (librdf_node *) 0xA04;
	librdf_statement *subject_statement = (librdf_statement *) 0xA05;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA06;
	librdf_uri *xsd_decimal = (librdf_uri *) 0xA07;
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = NS_GEO "long"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .resource = resource },
		{ 0 }
	};
	struct propdata_struct data = {
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_st_create, will_return(subject_statement));
	always_expect(librdf_statement_set_subject);
	always_expect(twine_rdf_st_clone, will_return(predicate_statement));
	always_expect(twine_rdf_node_createuri, will_return(target));
	always_expect(librdf_statement_set_predicate);
	expect(librdf_node_get_literal_value_datatype_uri, will_return(xsd_decimal), when(node, is_equal_to(resource)));
	expect(librdf_uri_as_string, will_return(NS_XSD "decimal"), when(uri, is_equal_to(xsd_decimal)));
	expect(librdf_node_get_literal_value, will_return("-123.45"), when(node, is_equal_to(resource)));
	always_expect(librdf_statement_set_object);
	always_expect(twine_rdf_model_add_st);
	always_expect(librdf_free_statement);

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
	assert_that(data.has_geo, is_equal_to(1));
	assert_that_double(data.lon, is_equal_to_double(-123.45));
}

Ensure(spindle_generate_props, prop_apply_sets_latitude_and_geo_flag_if_the_match_has_a_resource_and_map_target_is_geo_lat_and_the_datatype_uri_is_xsd_decimal) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *target = (librdf_node *) 0xA03;
	librdf_node *resource = (librdf_node *) 0xA04;
	librdf_statement *subject_statement = (librdf_statement *) 0xA05;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA06;
	librdf_uri *xsd_decimal = (librdf_uri *) 0xA07;
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = NS_GEO "lat"
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .resource = resource },
		{ 0 }
	};
	struct propdata_struct data = {
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(twine_rdf_node_clone, will_return(clone));
	always_expect(twine_rdf_st_create, will_return(subject_statement));
	always_expect(librdf_statement_set_subject);
	always_expect(twine_rdf_st_clone, will_return(predicate_statement));
	always_expect(twine_rdf_node_createuri, will_return(target));
	always_expect(librdf_statement_set_predicate);
	expect(librdf_node_get_literal_value_datatype_uri, will_return(xsd_decimal), when(node, is_equal_to(resource)));
	expect(librdf_uri_as_string, will_return(NS_XSD "decimal"), when(uri, is_equal_to(xsd_decimal)));
	expect(librdf_node_get_literal_value, will_return("-67.89"), when(node, is_equal_to(resource)));
	always_expect(librdf_statement_set_object);
	always_expect(twine_rdf_model_add_st);
	always_expect(librdf_free_statement);

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
	assert_that(data.has_geo, is_equal_to(2));
	assert_that_double(data.lat, is_equal_to_double(-67.89));
}

Ensure(spindle_generate_props, prop_apply_adds_the_statement_to_the_proxy_model_and_returns_no_error_if_the_match_has_a_literal_and_no_resource) {
	librdf_node *self = (librdf_node *) 0xA01;
	librdf_node *clone = (librdf_node *) 0xA02;
	librdf_node *target = (librdf_node *) 0xA03;
	librdf_node *match_literal = (librdf_node *) 0xA04;
	librdf_node *match_literal_clone = (librdf_node *) 0xA05;
	librdf_statement *subject_statement = (librdf_statement *) 0xA06;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA07;
	librdf_statement *local_predicate_statement = (librdf_statement *) 0xA08;
	SPINDLEENTRY entry = { .self = self };
	struct spindle_predicatemap_struct map = {
		.target = "target"
	};
	struct literal_struct literals[] = {
		{ .node = match_literal }
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .nliterals = 1, .literals = literals },
		{ 0 }
	};
	struct propdata_struct data = {
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	expect(twine_rdf_node_clone, will_return(clone), when(node, is_equal_to(self)));
	expect(twine_rdf_st_create, will_return(subject_statement));
	expect(librdf_statement_set_subject, when(statement, is_equal_to(subject_statement)), when(node, is_equal_to(clone)));
	expect(twine_rdf_st_clone, will_return(predicate_statement), when(src, is_equal_to(subject_statement)));
	expect(twine_rdf_node_createuri, will_return(target), when(uri, is_equal_to_string(map.target)));
	expect(librdf_statement_set_predicate, when(statement, is_equal_to(predicate_statement)), when(node, is_equal_to(target)));
	expect(twine_rdf_st_clone, will_return(local_predicate_statement), when(src, is_equal_to(predicate_statement)));
	expect(twine_rdf_node_clone, will_return(match_literal_clone), when(node, is_equal_to(match_literal)));
	expect(librdf_statement_set_object, when(statement, is_equal_to(local_predicate_statement)), when(node, is_equal_to(match_literal_clone)));
	expect(twine_rdf_model_add_st, will_return(0), when(model, is_equal_to(data.proxymodel)), when(statement, is_equal_to(local_predicate_statement)), when(ctx, is_equal_to(data.context)));
	expect(librdf_free_statement, when(statement, is_equal_to(local_predicate_statement)));
	expect(librdf_free_statement, when(statement, is_equal_to(predicate_statement)));
	expect(librdf_free_statement, when(statement, is_equal_to(subject_statement)));

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_apply_returns_error_if_the_match_has_a_literal_and_no_resource_and_cloning_the_predicate_statement_fails) {
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *subject_statement = (librdf_statement *) 0xA06;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA07;
	SPINDLEENTRY entry = { 0 };
	struct spindle_predicatemap_struct map = {
		.target = "target"
	};
	struct literal_struct literals[] = {
		{ .node = node }
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .nliterals = 1, .literals = literals },
		{ 0 }
	};
	struct propdata_struct data = { .entry = &entry, .matches = prop_matches };

	always_expect(librdf_statement_set_subject);
	always_expect(librdf_statement_set_predicate);
	always_expect(librdf_free_statement);
	always_expect(twine_rdf_node_clone, will_return(node));
	always_expect(twine_rdf_node_createuri, will_return(node));

	expect(twine_rdf_st_create, will_return(subject_statement));
	expect(twine_rdf_st_clone, will_return(predicate_statement), when(src, is_equal_to(subject_statement)));
	expect(twine_rdf_st_clone, will_return(NULL), when(src, is_equal_to(predicate_statement)));

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, prop_apply_returns_error_if_the_match_has_a_literal_and_no_resource_and_adding_the_statement_to_the_proxy_model_fails) {
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	SPINDLEENTRY entry = { 0 };
	struct spindle_predicatemap_struct map = {
		.target = "target"
	};
	struct literal_struct literals[] = {
		{ .node = node }
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .nliterals = 1, .literals = literals },
		{ 0 }
	};
	struct propdata_struct data = {
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(librdf_statement_set_subject);
	always_expect(librdf_statement_set_predicate);
	always_expect(librdf_statement_set_object);
	always_expect(librdf_free_statement);
	always_expect(twine_rdf_node_clone, will_return(node));
	always_expect(twine_rdf_node_createuri, will_return(node));
	always_expect(twine_rdf_st_create, will_return(statement));
	always_expect(twine_rdf_st_clone, will_return(statement));

	expect(twine_rdf_model_add_st, will_return(-1), when(model, is_equal_to(data.proxymodel)));

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(-1));
}

Ensure(spindle_generate_props, prop_apply_when_the_match_has_no_resource_and_is_successful_adds_the_statement_to_the_root_model_and_returns_no_error_if_the_matched_map_is_indexed_and_not_inverted_and_if_multigraph_is_true) {
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *subject_statement = (librdf_statement *) 0xA06;
	librdf_statement *predicate_statement = (librdf_statement *) 0xA07;
	librdf_statement *local_predicate_statement = (librdf_statement *) 0xA08;
	SPINDLE spindle = { .rootgraph = (librdf_node *) 0xC01, .multigraph = 1 };
	SPINDLEENTRY entry = { 0 };
	struct spindle_predicatemap_struct map = {
		.target = "target",
		.indexed = 1
	};
	struct literal_struct literals[] = {
		{ .node = node }
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .nliterals = 1, .literals = literals },
		{ 0 }
	};
	struct propdata_struct data = {
		.spindle = &spindle,
		.entry = &entry,
		.proxymodel = (librdf_model *) 0xB01,
		.matches = prop_matches
	};

	always_expect(librdf_statement_set_subject);
	always_expect(librdf_statement_set_predicate);
	always_expect(librdf_statement_set_object);
	always_expect(librdf_free_statement);
	always_expect(twine_rdf_node_clone, will_return(node));
	always_expect(twine_rdf_node_createuri, will_return(node));

	expect(twine_rdf_st_create, will_return(subject_statement));
	expect(twine_rdf_st_clone, will_return(predicate_statement), when(src, is_equal_to(subject_statement)));
	expect(twine_rdf_st_clone, will_return(local_predicate_statement), when(src, is_equal_to(predicate_statement)));
	expect(twine_rdf_model_add_st, will_return(0), when(model, is_equal_to(data.proxymodel)));
	expect(twine_rdf_model_add_st, will_return(0), when(model, is_equal_to(data.rootmodel)), when(statement, is_equal_to(local_predicate_statement)), when(ctx, is_equal_to(spindle.rootgraph)));

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_apply_returns_error_if_the_match_has_no_resource_and_adding_the_target_statement_to_the_root_model_fails) {
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	SPINDLE spindle = { .rootgraph = (librdf_node *) 0xC01, .multigraph = 1 };
	SPINDLEENTRY entry = { 0 };
	struct spindle_predicatemap_struct map = {
		.target = "target",
		.indexed = 1
	};
	struct literal_struct literals[] = {
		{ .node = node }
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &map, .nliterals = 1, .literals = literals },
		{ 0 }
	};
	struct propdata_struct data = {
		.spindle = &spindle,
		.entry = &entry,
		.context = (librdf_node *) 0xB01,
		.proxymodel = (librdf_model *) 0xB02,
		.matches = prop_matches
	};

	always_expect(librdf_statement_set_subject);
	always_expect(librdf_statement_set_predicate);
	always_expect(librdf_statement_set_object);
	always_expect(librdf_free_statement);
	always_expect(twine_rdf_node_clone, will_return(node));
	always_expect(twine_rdf_node_createuri, will_return(node));
	always_expect(twine_rdf_st_create, will_return(statement));
	always_expect(twine_rdf_st_clone, will_return(statement));

	expect(twine_rdf_model_add_st, will_return(0), when(model, is_equal_to(data.proxymodel)));
	expect(twine_rdf_model_add_st, will_return(-1), when(model, is_equal_to(data.rootmodel)));

	int r = spindle_prop_apply_(&data);
	assert_that(r, is_equal_to(-1));
}

#pragma mark -
#pragma mark spindle_prop_loop_

/*
	spindle_prop_loop_ always returns zero, as its return value is solely
	determined by a call to spindle_prop_test_, which itself always returns zero.
*/

Ensure(spindle_generate_props, prop_loop_returns_no_error_if_there_are_no_statements_in_source_model) {
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	librdf_stream *stream = (librdf_stream *) 0xA03;
	librdf_world *world = (librdf_world *) 0xA04;
	SPINDLE spindle = { .world = world };
	struct propdata_struct data = {
		.spindle = &spindle,
		.source = model
	};

	expect(librdf_new_statement, will_return(statement), when(world, is_equal_to(world)));
	expect(librdf_model_find_statements, will_return(stream), when(model, is_equal_to(model)), when(statement, is_equal_to(statement)));
	expect(librdf_stream_end, will_return(1));
	expect(librdf_free_stream, when(stream, is_equal_to(stream)));
	expect(librdf_free_statement, when(statement, is_equal_to(statement)));

	int r = spindle_prop_loop_(&data);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_loop_returns_no_error_if_the_predicate_is_not_a_resource) {
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	librdf_statement *statement = (librdf_statement *) 0xA03;
	librdf_stream *stream = (librdf_stream *) 0xA04;
	SPINDLE spindle = { 0 };
	struct propdata_struct data = {
		.spindle = &spindle,
		.source = model
	};

	expect(librdf_stream_end, will_return(0));
	expect(librdf_stream_get_object, will_return(statement), when(stream, is_equal_to(stream)));
	expect(librdf_statement_get_predicate, will_return(predicate), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_resource, will_return(0), when(node, is_equal_to(predicate)));
	expect(librdf_stream_next, when(stream, is_equal_to(stream)));

	always_expect(librdf_new_statement);
	always_expect(librdf_model_find_statements, will_return(stream));
	always_expect(librdf_statement_get_subject);
	always_expect(librdf_statement_get_object);
	always_expect(librdf_stream_end, will_return(1));
	always_expect(librdf_free_stream);
	always_expect(librdf_free_statement);

	int r = spindle_prop_loop_(&data);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_loop_returns_no_error_if_the_predicate_uri_is_NULL) {
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_node *predicate = (librdf_node *) 0xA02;
	SPINDLE spindle = { 0 };
	struct propdata_struct data = {
		.spindle = &spindle,
		.source = model
	};

	expect(librdf_stream_end, will_return(0));
	expect(librdf_node_get_uri, will_return(NULL), when(node, is_equal_to(predicate)));

	always_expect(librdf_new_statement);
	always_expect(librdf_model_find_statements);
	always_expect(librdf_stream_get_object);
	always_expect(librdf_statement_get_subject);
	always_expect(librdf_statement_get_predicate, will_return(predicate));
	always_expect(librdf_statement_get_object);
	always_expect(librdf_node_is_resource, will_return(1));
	always_expect(librdf_stream_next);
	always_expect(librdf_stream_end, will_return(1));
	always_expect(librdf_free_stream);
	always_expect(librdf_free_statement);

	int r = spindle_prop_loop_(&data);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_loop_returns_no_error_if_the_predicate_uri_as_string_is_NULL) {
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_uri *uri = (librdf_uri *) 0xA02;
	SPINDLE spindle = { 0 };
	struct propdata_struct data = {
		.spindle = &spindle,
		.source = model
	};

	expect(librdf_stream_end, will_return(0));
	expect(librdf_uri_as_string, will_return(NULL), when(uri, is_equal_to(uri)));

	always_expect(librdf_new_statement);
	always_expect(librdf_model_find_statements);
	always_expect(librdf_stream_get_object);
	always_expect(librdf_statement_get_subject);
	always_expect(librdf_statement_get_predicate);
	always_expect(librdf_statement_get_object);
	always_expect(librdf_node_is_resource, will_return(1));
	always_expect(librdf_node_get_uri, will_return(uri));
	always_expect(librdf_stream_next);
	always_expect(librdf_stream_end, will_return(1));
	always_expect(librdf_free_stream);
	always_expect(librdf_free_statement);

	int r = spindle_prop_loop_(&data);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_loop_returns_no_error_if_no_entry_reference_is_found_matching_the_subject_or_object) {
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_uri *uri = (librdf_uri *) 0xA02;
	char *predicate_str = "predicate";
	char *references[] = {
		"ref",
		NULL
	};
	SPINDLE spindle = { 0 };
	SPINDLEENTRY entry = { .refs = references };
	struct propdata_struct data = {
		.spindle = &spindle,
		.entry = &entry,
		.source = model
	};

	expect(librdf_stream_end, will_return(0));
	expect(librdf_uri_as_string, will_return(predicate_str));
	expect(librdf_uri_as_string);
	expect(librdf_uri_as_string);

	always_expect(librdf_new_statement);
	always_expect(librdf_model_find_statements);
	always_expect(librdf_stream_get_object);
	always_expect(librdf_statement_get_subject);
	always_expect(librdf_statement_get_predicate);
	always_expect(librdf_statement_get_object);
	always_expect(librdf_node_is_resource, will_return(1));
	always_expect(librdf_node_get_uri, will_return(uri));
	always_expect(librdf_stream_next);
	always_expect(librdf_stream_end, will_return(1));
	always_expect(librdf_free_stream);
	always_expect(librdf_free_statement);

	int r = spindle_prop_loop_(&data);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_loop_returns_result_from_prop_test_if_subject_from_a_source_model_triple_matches_an_entry_reference) {
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_node *node = (librdf_node *) 0xA02;
	librdf_statement *statement = (librdf_statement *) 0xA03;
	librdf_uri *uri = (librdf_uri *) 0xA04;
	char *subject_str = "subject";
	char *predicate_str = "predicate";
	char *references[] = {
		subject_str,
		NULL
	};
	SPINDLE spindle = { 0 };
	SPINDLEENTRY entry = { .refs = references };
	struct spindle_predicatematch_struct matches[] = {
		{ .predicate = predicate_str },
		{ 0 }
	};
	struct spindle_predicatemap_struct maps[] = {
		{ .target = "target", .matches = matches },
		{ 0 }
	};
	struct spindle_predicatemap_struct predicate_map = {
		.expected = RAPTOR_TERM_TYPE_LITERAL
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &predicate_map },
		{ 0 }
	};
	struct propdata_struct data = {
		.spindle = &spindle,
		.entry = &entry,
		.source = model,
		.maps = maps,
		.matches = prop_matches
	};

	expect(librdf_stream_end, will_return(0));
	expect(librdf_statement_get_subject);
	expect(librdf_statement_get_predicate);
	expect(librdf_statement_get_object);
	expect(librdf_uri_as_string, will_return(predicate_str));
	expect(librdf_uri_as_string, will_return(subject_str));
	expect(librdf_uri_as_string);
	expect(librdf_statement_get_object, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, when(node, is_equal_to(node)));

	always_expect(librdf_new_statement);
	always_expect(librdf_model_find_statements);
	always_expect(librdf_stream_get_object, will_return(statement));
	always_expect(librdf_statement_get_subject);
	always_expect(librdf_statement_get_predicate);
	always_expect(librdf_statement_get_object);
	always_expect(librdf_node_is_resource, will_return(1));
	always_expect(librdf_node_get_uri, will_return(uri));
	always_expect(librdf_stream_next);
	always_expect(librdf_stream_end, will_return(1));
	always_expect(librdf_free_stream);
	always_expect(librdf_free_statement);

	int r = spindle_prop_loop_(&data);
	assert_that(r, is_equal_to(0));
}

Ensure(spindle_generate_props, prop_loop_returns_result_from_prop_test_if_object_from_a_source_model_triple_matches_an_entry_reference) {
	librdf_model *model = (librdf_model *) 0xA01;
	librdf_node *node = (librdf_node *) 0xA02;
	librdf_statement *statement = (librdf_statement *) 0xA03;
	librdf_uri *uri = (librdf_uri *) 0xA04;
	char *subject_str = "subject";
	char *predicate_str = "predicate";
	char *object_str = "object";
	char *references[] = {
		object_str,
		NULL
	};
	SPINDLE spindle = { 0 };
	SPINDLEENTRY entry = { .refs = references };
	struct spindle_predicatematch_struct matches[] = {
		{ .predicate = predicate_str, .inverse = 1 },
		{ 0 }
	};
	struct spindle_predicatemap_struct maps[] = {
		{ .target = "target", .matches = matches },
		{ 0 }
	};
	struct spindle_predicatemap_struct predicate_map = {
		.expected = RAPTOR_TERM_TYPE_LITERAL
	};
	struct propmatch_struct prop_matches[] = {
		{ .map = &predicate_map },
		{ 0 }
	};
	struct propdata_struct data = {
		.spindle = &spindle,
		.entry = &entry,
		.source = model,
		.maps = maps,
		.matches = prop_matches
	};

	expect(librdf_stream_end, will_return(0));
	expect(librdf_statement_get_subject);
	expect(librdf_statement_get_predicate);
	expect(librdf_statement_get_object);
	expect(librdf_uri_as_string, will_return(predicate_str));
	expect(librdf_uri_as_string, will_return(subject_str));
	expect(librdf_uri_as_string, will_return(object_str));
	expect(librdf_statement_get_subject, will_return(node), when(statement, is_equal_to(statement)));
	expect(librdf_node_is_literal, when(node, is_equal_to(node)));

	always_expect(librdf_new_statement);
	always_expect(librdf_model_find_statements);
	always_expect(librdf_stream_get_object, will_return(statement));
	always_expect(librdf_node_is_resource, will_return(1));
	always_expect(librdf_node_get_uri, will_return(uri));
	always_expect(librdf_stream_next);
	always_expect(librdf_stream_end, will_return(1));
	always_expect(librdf_free_stream);
	always_expect(librdf_free_statement);

	int r = spindle_prop_loop_(&data);
	assert_that(r, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_prop_cleanup_

/*
	spindle_prop_cleanup_ always returns zero.
*/

Ensure(spindle_generate_props, prop_cleanup_frees_match_resource_and_match_literals_if_match_map_has_a_target) {
	librdf_node *node_1 = (librdf_node *) 0xA01;
	librdf_node *node_2 = (librdf_node *) 0xA02;
	librdf_node *node_3 = (librdf_node *) 0xA03;
	librdf_node *node_4 = (librdf_node *) 0xA04;
	librdf_node *node_5 = (librdf_node *) 0xA05;
	librdf_node *resource_1 = (librdf_node *) 0xA06;
	librdf_node *resource_2 = (librdf_node *) 0xA07;
	struct spindle_predicatemap_struct map_1 = { .target = "target 1" };
	struct spindle_predicatemap_struct map_2 = { .target = "target 2" };
	struct spindle_predicatemap_struct map_3 = { .target = "target 3" };
	struct literal_struct literals_1_data[] = {
		{ .node = node_1 },
		{ .node = node_2 }
	};
	struct literal_struct literals_2_data[] = {
		{ .node = node_3 },
		{ .node = node_4 },
		{ .node = node_5 }
	};
	struct propmatch_struct prop_match_data[] = {
		{ .map = &map_1, .nliterals = 2, .resource = resource_1 },
		{ .map = &map_2, .resource = resource_2 },
		{ .map = &map_3, .nliterals = 3 },
		{ 0 }
	};
	struct propdata_struct data = { 0 };

	prop_match_data[0].literals = (struct literal_struct *) malloc(sizeof literals_1_data);
	prop_match_data[2].literals = (struct literal_struct *) malloc(sizeof literals_2_data);
	memcpy(prop_match_data[0].literals, literals_1_data, sizeof literals_1_data);
	memcpy(prop_match_data[2].literals, literals_2_data, sizeof literals_2_data);

	data.matches = (struct propmatch_struct *) malloc(sizeof prop_match_data);
	memcpy(data.matches, prop_match_data, sizeof prop_match_data);

	expect(librdf_free_node, when(node, is_equal_to(resource_1)));
	expect(librdf_free_node, when(node, is_equal_to(node_1)));
	expect(librdf_free_node, when(node, is_equal_to(node_2)));
	expect(librdf_free_node, when(node, is_equal_to(resource_2)));
	expect(librdf_free_node, when(node, is_equal_to(node_3)));
	expect(librdf_free_node, when(node, is_equal_to(node_4)));
	expect(librdf_free_node, when(node, is_equal_to(node_5)));

	int r = spindle_prop_cleanup_(&data);
	assert_that(r, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_prop_init_

Ensure(spindle_generate_props, prop_init_initialises_the_property_data_structure) {
	struct spindle_predicatemap_struct predicates[] = {
		{ 0 },
		{ 0 },
		{ 0 }
	};
	SPINDLERULES rules = {
		.predicates = predicates,
		.predcount = 3
	};
	SPINDLEENTRY entry = {
		.spindle = (SPINDLE *) 0xA00,
		.rules = &rules,
		.localname = "local name",
		.classname = "class name",
		.rootdata = (librdf_model *) 0xA01,
		.proxydata = (librdf_model *) 0xA02,
		.sourcedata = (librdf_model *) 0xA03,
		.extradata = (librdf_model *) 0xA04,
		.graph = (librdf_node *) 0xA05,
		.doc = (librdf_node *) 0xA06,
		.self = (librdf_node *) 0xA07,
		.sameas = (librdf_node *) 0xA08
	};
	struct propmatch_struct expected_matches[4] = {
		{ .map = &predicates[0] },
		{ .map = &predicates[1] },
		{ .map = &predicates[2] },
		{ 0 }
	};
	struct propdata_struct data, expected_data = {
		.entry = &entry,
		.spindle = entry.spindle,
		.source = entry.sourcedata,
		.localname = entry.localname,
		.classname = entry.classname,
		.proxymodel = entry.proxydata,
		.rootmodel = entry.rootdata,
		.context = entry.graph,
		.maps = rules.predicates
	};

	memset(&data, 0x55, sizeof (struct propdata_struct));

	int r = spindle_prop_init_(&data, &entry);
	assert_that(r, is_equal_to(0));
	assert_that(data.matches, is_equal_to_contents_of(expected_matches, sizeof expected_matches));
	expected_data.matches = data.matches; // fill in expected field with value of allocated pointer
	assert_that(&data, is_equal_to_contents_of(&expected_data, sizeof expected_data));
}

#pragma mark -
#pragma mark spindle_prop_update_entry

Ensure(spindle_generate_props, prop_update_entry_ignores_unknown_predicates) {
	char *object_str = "object";
	char *predicate_str = "predicate";
	char *subject_str = "subject";
	char *references[] = {
		subject_str,
		NULL
	};
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	struct spindle_predicatemap_struct predicates[] = {
		{ .target = "target", .expected = RAPTOR_TERM_TYPE_URI },
		{ 0 }
	};
	SPINDLE spindle = { .root = "root" };
	SPINDLERULES rules = {
		.predicates = predicates,
		.predcount = 1
	};
	SPINDLEGENERATE generate = { 0 };
	SPINDLEENTRY entry = {
		.spindle = &spindle,
		.rules = &rules,
		.generate = &generate,
		.refs = references,
		.self = node,
		.localname = "proxy local name"
	};

	expect(librdf_stream_end, will_return(0));
	expect(librdf_uri_as_string, will_return(predicate_str));
	expect(librdf_uri_as_string, will_return(subject_str));
	expect(librdf_uri_as_string, will_return(object_str));

	always_expect(librdf_free_statement);
	always_expect(librdf_free_stream);
	always_expect(librdf_model_find_statements);
	always_expect(librdf_new_statement, will_return(1));
	always_expect(librdf_node_get_uri, will_return(1));
	always_expect(librdf_node_is_resource, will_return(1));
	always_expect(librdf_statement_get_object, will_return(1));
	always_expect(librdf_statement_get_subject, will_return(1));
	always_expect(librdf_statement_get_predicate, will_return(1));
	always_expect(librdf_statement_set_predicate);
	always_expect(librdf_statement_set_subject);
	always_expect(librdf_stream_end, will_return(1));
	always_expect(librdf_stream_get_object, will_return(1));
	always_expect(librdf_stream_next);
	always_expect(twine_rdf_node_clone, will_return(node));
	always_expect(twine_rdf_node_createuri, will_return(node));
	always_expect(twine_rdf_st_clone, will_return(statement));
	always_expect(twine_rdf_st_create, will_return(statement));

	int r = spindle_prop_update_entry(&entry);
	assert_that(r, is_equal_to(0));
}

/*
	This test is basically an integration test, but only asserts that the work
	done directly in this function is correct for the supplied sample data,
	i.e. from after the call to spindle_prop_apply_() up to before the call
	to spindle_prop_cleanup_().
*/

Ensure(spindle_generate_props, prop_update_entry_updates_the_language_agnostic_title_of_a_proxy_cache_entry_using_data_from_the_source_model_properties) {
	char *label_mapped_predicate_1 = "skos:prefLabel";
	char *label_mapped_predicate_2 = "foaf:name";
	char *object_str = "object";
	char *subject_str = "subject";
	char *references[] = {
		subject_str,
		NULL
	};
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	struct spindle_predicatematch_struct label_matches[] = {
		{ .predicate = label_mapped_predicate_1 },
		{ .predicate = label_mapped_predicate_2 },
		{ 0 }
	};
	struct spindle_predicatemap_struct predicates[] = {
		{ .target = "target", .expected = RAPTOR_TERM_TYPE_URI },
		{ .target = NS_RDFS "label", .expected = RAPTOR_TERM_TYPE_LITERAL, .matches = label_matches },
		{ 0 }
	};
	SPINDLE spindle = { .root = "root" };
	SPINDLERULES rules = {
		.predicates = predicates,
		.predcount = 2
	};
	SPINDLEGENERATE generate = { .titlepred = NS_RDFS "label" };
	SPINDLEENTRY entry = {
		.spindle = &spindle,
		.rules = &rules,
		.generate = &generate,
		.refs = references,
		.self = node,
		.localname = "proxy local name"
	};

	struct spindle_literalstring_struct expected_titleset_literals = {
		.lang = "",
		.str = "title@"
	};
	SPINDLEENTRY expected = {
		.title = expected_titleset_literals.str,
		.titleset.nliterals = 1,
		.titleset.literals = &expected_titleset_literals
	};

	expect(librdf_stream_end, will_return(0));
	expect(librdf_uri_as_string, will_return(label_mapped_predicate_1));
	expect(librdf_uri_as_string, will_return(subject_str));
	expect(librdf_uri_as_string, will_return(object_str));
	expect(librdf_node_get_literal_value_language, will_return(NULL));
	expect(librdf_node_get_literal_value, will_return(expected_titleset_literals.str));
	expect(librdf_node_get_literal_value, will_return(expected_titleset_literals.str));

	// LIBRARY CALLS WHOSE RETURN VALUES ARE NOT INTERESTING FROM THIS POINT ON

	always_expect(librdf_free_node);
	always_expect(librdf_free_statement);
	always_expect(librdf_free_stream);
	always_expect(librdf_free_uri);
	always_expect(librdf_model_find_statements);
	always_expect(librdf_new_node_from_typed_literal, will_return(1));
	always_expect(librdf_new_statement, will_return(1));
	always_expect(librdf_new_uri, will_return(1));
	always_expect(librdf_node_get_literal_value_language);
	always_expect(librdf_node_get_uri, will_return(1));
	always_expect(librdf_node_is_literal, will_return(1));
	always_expect(librdf_node_is_resource, will_return(1));
	always_expect(librdf_statement_get_object, will_return(1));
	always_expect(librdf_statement_get_subject, will_return(1));
	always_expect(librdf_statement_get_predicate, will_return(1));
	always_expect(librdf_statement_set_object);
	always_expect(librdf_statement_set_predicate);
	always_expect(librdf_statement_set_subject);
	always_expect(librdf_stream_end, will_return(1));
	always_expect(librdf_stream_get_object, will_return(1));
	always_expect(librdf_stream_next);
	always_expect(twine_rdf_model_add_st);
	always_expect(twine_rdf_node_clone, will_return(node));
	always_expect(twine_rdf_node_createuri, will_return(node));
	always_expect(twine_rdf_node_destroy);
	always_expect(twine_rdf_st_clone, will_return(statement));
	always_expect(twine_rdf_st_create, will_return(statement));

	int r = spindle_prop_update_entry(&entry);
	assert_that(r, is_equal_to(0));

	// check spindle_prop_copystrings_ gets called as expected
	assert_that(entry.titleset.nliterals, is_equal_to(expected.titleset.nliterals));
	assert_that(entry.titleset.literals[0].lang, is_equal_to_string(expected.titleset.literals[0].lang));
	assert_that(entry.titleset.literals[0].str, is_equal_to_string(expected.titleset.literals[0].str));

	// check that the winning title is cached under the correct language field
	assert_that(entry.title, is_equal_to_string(expected.title));
	assert_that(entry.title_en, is_null);
}

Ensure(spindle_generate_props, prop_update_entry_updates_the_english_title_of_a_proxy_cache_entry_using_data_from_the_source_model_properties) {
	char *label_mapped_predicate_1 = "skos:prefLabel";
	char *label_mapped_predicate_2 = "foaf:name";
	char *object_str = "object";
	char *subject_str = "subject";
	char *references[] = {
		subject_str,
		NULL
	};
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	struct spindle_predicatematch_struct label_matches[] = {
		{ .predicate = label_mapped_predicate_1 },
		{ .predicate = label_mapped_predicate_2 },
		{ 0 }
	};
	struct spindle_predicatemap_struct predicates[] = {
		{ .target = NS_RDFS "label", .expected = RAPTOR_TERM_TYPE_LITERAL, .matches = label_matches },
		{ 0 }
	};
	SPINDLE spindle = { .root = "root" };
	SPINDLERULES rules = {
		.predicates = predicates,
		.predcount = 1
	};
	SPINDLEGENERATE generate = { .titlepred = NS_RDFS "label" };
	SPINDLEENTRY entry = {
		.spindle = &spindle,
		.rules = &rules,
		.generate = &generate,
		.refs = references,
		.self = node,
		.localname = "proxy local name"
	};

	struct spindle_literalstring_struct expected_titleset_literals = {
		.lang = "en",
		.str = "title@en"
	};
	SPINDLEENTRY expected = {
		.title_en = expected_titleset_literals.str,
		.titleset.nliterals = 1,
		.titleset.literals = &expected_titleset_literals
	};

	expect(librdf_stream_end, will_return(0));
	expect(librdf_uri_as_string, will_return(label_mapped_predicate_1));
	expect(librdf_uri_as_string, will_return(subject_str));
	expect(librdf_uri_as_string, will_return(object_str));
	expect(librdf_node_get_literal_value_language, will_return(expected_titleset_literals.lang));
	expect(librdf_node_get_literal_value, will_return(expected_titleset_literals.str));
	expect(librdf_node_get_literal_value, will_return(expected_titleset_literals.str));

	always_expect(librdf_free_node);
	always_expect(librdf_free_statement);
	always_expect(librdf_free_stream);
	always_expect(librdf_free_uri);
	always_expect(librdf_model_find_statements);
	always_expect(librdf_new_node_from_typed_literal, will_return(1));
	always_expect(librdf_new_statement, will_return(1));
	always_expect(librdf_new_uri, will_return(1));
	always_expect(librdf_node_get_literal_value_language);
	always_expect(librdf_node_get_uri, will_return(1));
	always_expect(librdf_node_is_literal, will_return(1));
	always_expect(librdf_node_is_resource, will_return(1));
	always_expect(librdf_statement_get_object, will_return(1));
	always_expect(librdf_statement_get_predicate, will_return(1));
	always_expect(librdf_statement_get_subject, will_return(1));
	always_expect(librdf_statement_set_object);
	always_expect(librdf_statement_set_predicate);
	always_expect(librdf_statement_set_subject);
	always_expect(librdf_stream_end, will_return(1));
	always_expect(librdf_stream_get_object, will_return(1));
	always_expect(librdf_stream_next);
	always_expect(twine_rdf_model_add_st);
	always_expect(twine_rdf_node_clone, will_return(node));
	always_expect(twine_rdf_node_createuri, will_return(node));
	always_expect(twine_rdf_node_destroy);
	always_expect(twine_rdf_st_clone, will_return(statement));
	always_expect(twine_rdf_st_create, will_return(statement));

	int r = spindle_prop_update_entry(&entry);
	assert_that(r, is_equal_to(0));
	assert_that(entry.title, is_null);
	assert_that(entry.title_en, is_equal_to_string(expected.title_en));
}

Ensure(spindle_generate_props, prop_update_entry_updates_the_description_literals_using_data_from_the_source_model_properties) {
	char *description_mapped_predicate_1 = "po:synopsis";
	char *description_mapped_predicate_2 = "rdfs:comment";
	char *object_str = "object";
	char *subject_str = "subject";
	char *references[] = {
		subject_str,
		NULL
	};
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	struct spindle_predicatematch_struct desc_matches[] = {
		{ .predicate = description_mapped_predicate_1 },
		{ .predicate = description_mapped_predicate_2 },
		{ 0 }
	};
	struct spindle_predicatemap_struct predicates[] = {
		{ .target = NS_DCTERMS "description", .expected = RAPTOR_TERM_TYPE_LITERAL, .matches = desc_matches },
		{ 0 }
	};
	SPINDLE spindle = { .root = "root" };
	SPINDLERULES rules = {
		.predicates = predicates,
		.predcount = 1
	};
	SPINDLEGENERATE generate = { 0 };
	SPINDLEENTRY entry = {
		.spindle = &spindle,
		.rules = &rules,
		.generate = &generate,
		.refs = references,
		.self = node,
		.localname = "proxy local name"
	};

	struct spindle_literalstring_struct expected_descset_literals = {
		.lang = "fr",
		.str = "description@fr"
	};
	SPINDLEENTRY expected = {
		.descset.nliterals = 1,
		.descset.literals = &expected_descset_literals
	};

	expect(librdf_stream_end, will_return(0));
	expect(librdf_uri_as_string, will_return(description_mapped_predicate_2));
	expect(librdf_uri_as_string, will_return(subject_str));
	expect(librdf_uri_as_string, will_return(object_str));
	expect(librdf_node_get_literal_value_language, will_return(expected_descset_literals.lang));
	expect(librdf_node_get_literal_value, will_return(expected_descset_literals.str));

	always_expect(librdf_free_node);
	always_expect(librdf_free_statement);
	always_expect(librdf_free_stream);
	always_expect(librdf_model_find_statements);
	always_expect(librdf_new_statement, will_return(1));
	always_expect(librdf_node_get_uri, will_return(1));
	always_expect(librdf_node_is_literal, will_return(1));
	always_expect(librdf_node_is_resource, will_return(1));
	always_expect(librdf_statement_get_object, will_return(1));
	always_expect(librdf_statement_get_subject, will_return(1));
	always_expect(librdf_statement_get_predicate, will_return(1));
	always_expect(librdf_statement_set_object);
	always_expect(librdf_statement_set_predicate);
	always_expect(librdf_statement_set_subject);
	always_expect(librdf_stream_end, will_return(1));
	always_expect(librdf_stream_get_object, will_return(1));
	always_expect(librdf_stream_next);
	always_expect(twine_rdf_model_add_st);
	always_expect(twine_rdf_node_clone, will_return(node));
	always_expect(twine_rdf_node_createuri, will_return(node));
	always_expect(twine_rdf_node_destroy);
	always_expect(twine_rdf_st_clone, will_return(statement));
	always_expect(twine_rdf_st_create, will_return(statement));

	int r = spindle_prop_update_entry(&entry);
	assert_that(r, is_equal_to(0));
	assert_that(entry.descset.nliterals, is_equal_to(expected.descset.nliterals));
	assert_that(entry.descset.literals[0].lang, is_equal_to_string(expected.descset.literals[0].lang));
	assert_that(entry.descset.literals[0].str, is_equal_to_string(expected.descset.literals[0].str));
}

Ensure(spindle_generate_props, prop_update_entry_updates_the_geographic_details_of_a_proxy_cache_entry_using_data_from_the_source_model_properties) {
	char *lat_mapped_predicate_1 = "geo:lat";
	char *long_mapped_predicate_1 = "geo:long";
	char *object_str = "object";
	char *subject_str = "subject";
	char *references[] = {
		subject_str,
		NULL
	};
	librdf_node *node = (librdf_node *) 0xA01;
	librdf_statement *statement = (librdf_statement *) 0xA02;
	struct spindle_predicatematch_struct lat_matches[] = {
		{ .predicate = lat_mapped_predicate_1 },
		{ 0 }
	};
	struct spindle_predicatematch_struct long_matches[] = {
		{ .predicate = long_mapped_predicate_1 },
		{ 0 }
	};
	struct spindle_predicatemap_struct predicates[] = {
		{ .target = NS_GEO "lat", .expected = RAPTOR_TERM_TYPE_LITERAL, .matches = lat_matches, .datatype = NS_XSD "decimal" },
		{ .target = NS_GEO "long", .expected = RAPTOR_TERM_TYPE_LITERAL, .matches = long_matches, .datatype = NS_XSD "decimal" },
		{ 0 }
	};
	SPINDLE spindle = { .root = "root" };
	SPINDLERULES rules = {
		.predicates = predicates,
		.predcount = 2
	};
	SPINDLEGENERATE generate = { 0 };
	SPINDLEENTRY entry = {
		.spindle = &spindle,
		.rules = &rules,
		.generate = &generate,
		.refs = references,
		.self = node,
		.localname = "proxy local name"
	};

	char *expected_longitude_literal_str = "123.45";
	char *expected_latitude_literal_str = "67.89";
	SPINDLEENTRY expected = {
		.has_geo = 1,
		.lon = 123.45,
		.lat = 67.89
	};

	// identify latitude
	char *latitude_type_str = NS_XSD "decimal";
	librdf_node *latitude_candidate_node = (librdf_node *) 0xC01;
	librdf_node *latitude_typed_literal_node = (librdf_node *) 0xC02;
	librdf_uri *latitude_type_uri = (librdf_uri *) 0xC03;
	expect(librdf_stream_end, will_return(0));
	expect(librdf_uri_as_string, will_return(lat_mapped_predicate_1));
	expect(librdf_uri_as_string, will_return(subject_str));
	expect(librdf_uri_as_string, will_return(object_str));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(latitude_type_uri));
	expect(librdf_uri_as_string, will_return(latitude_type_str), when(uri, is_equal_to(latitude_type_uri)));
	expect(librdf_node_get_literal_value);
	expect(librdf_new_node_from_typed_literal, will_return(latitude_typed_literal_node));

	// identify longitude
	char *longitude_type_str = NS_XSD "decimal";
	librdf_node *longitude_candidate_node = (librdf_node *) 0xD01;
	librdf_node *longitude_typed_literal_node = (librdf_node *) 0xD02;
	librdf_uri *longitude_type_uri = (librdf_uri *) 0xD03;
	expect(librdf_stream_end, will_return(0));
	expect(librdf_uri_as_string, will_return(long_mapped_predicate_1));
	expect(librdf_uri_as_string, will_return(subject_str));
	expect(librdf_uri_as_string, will_return(object_str));
	expect(librdf_node_get_literal_value_datatype_uri, will_return(longitude_type_uri));
	expect(librdf_uri_as_string, will_return(longitude_type_str), when(uri, is_equal_to(longitude_type_uri)));
	expect(librdf_node_get_literal_value);
	expect(librdf_new_node_from_typed_literal, will_return(longitude_typed_literal_node));

	// match is geo:lat
	expect(librdf_node_get_literal_value_datatype_uri, will_return(latitude_type_uri), when(node, is_equal_to(latitude_typed_literal_node)));
	expect(librdf_uri_as_string, will_return(latitude_type_str), when(uri, is_equal_to(latitude_type_uri)));
	expect(librdf_node_get_literal_value, will_return(expected_latitude_literal_str));

	// match is geo:long
	expect(librdf_node_get_literal_value_datatype_uri, will_return(longitude_type_uri), when(node, is_equal_to(longitude_typed_literal_node)));
	expect(librdf_uri_as_string, will_return(longitude_type_str), when(uri, is_equal_to(longitude_type_uri)));
	expect(librdf_node_get_literal_value, will_return(expected_longitude_literal_str));

	always_expect(librdf_free_node);
	always_expect(librdf_free_statement);
	always_expect(librdf_free_stream);
	always_expect(librdf_free_uri);
	always_expect(librdf_model_find_statements);
	always_expect(librdf_new_node_from_typed_literal, will_return(1));
	always_expect(librdf_new_statement, will_return(1));
	always_expect(librdf_new_uri, will_return(1));
	always_expect(librdf_node_get_literal_value_language);
	always_expect(librdf_node_get_uri, will_return(1));
	always_expect(librdf_node_is_literal, will_return(1));
	always_expect(librdf_node_is_resource, will_return(1));
	always_expect(librdf_statement_get_object, will_return(1));
	always_expect(librdf_statement_get_subject, will_return(1));
	always_expect(librdf_statement_get_predicate, will_return(1));
	always_expect(librdf_statement_set_object);
	always_expect(librdf_statement_set_predicate);
	always_expect(librdf_statement_set_subject);
	always_expect(librdf_stream_end, will_return(1));
	always_expect(librdf_stream_get_object, will_return(1));
	always_expect(librdf_stream_next);
	always_expect(twine_rdf_model_add_st);
	always_expect(twine_rdf_node_clone, will_return(node));
	always_expect(twine_rdf_node_createuri, will_return(node));
	always_expect(twine_rdf_node_destroy);
	always_expect(twine_rdf_st_clone, will_return(statement));
	always_expect(twine_rdf_st_create, will_return(statement));

	int r = spindle_prop_update_entry(&entry);
	assert_that(r, is_equal_to(0));
	assert_that(entry.has_geo, is_equal_to(expected.has_geo));
	assert_that_double(entry.lat, is_equal_to_double(expected.lat));
	assert_that_double(entry.lon, is_equal_to_double(expected.lon));
}

Ensure(spindle_generate_props, prop_update_entry_returns_error_when_prop_apply_fails) {
	librdf_node *node = (librdf_node *) 0xA01;
	SPINDLE spindle = { .root = "root" };
	SPINDLERULES rules = { 0 };
	SPINDLEENTRY cache = {
		.spindle = &spindle,
		.rules = &rules,
		.localname = "local"
	};

	expect(librdf_new_statement);
	expect(librdf_model_find_statements);
	expect(librdf_stream_end, will_return(1));
	expect(librdf_free_stream);
	expect(librdf_free_statement);
	expect(twine_rdf_node_clone, will_return(node));
	expect(twine_rdf_st_create, will_return(NULL));
	expect(librdf_free_node, when(node, is_equal_to(node)));

	int r = spindle_prop_update_entry(&cache);
	assert_that(r, is_equal_to(-1));
}

#pragma mark -

TestSuite *create_props_test_suite(void) {
	TestSuite *suite = create_test_suite();
	add_test_with_context(suite, spindle_generate_props, literal_copy_with_NULL_source_does_not_copy_and_returns_no_error);
	add_test_with_context(suite, spindle_generate_props, literal_copy_with_no_source_literals_does_not_copy_and_returns_no_error);
	add_test_with_context(suite, spindle_generate_props, literal_copy_copies_all_source_literals_and_returns_no_error);
	add_test_with_context(suite, spindle_generate_props, datatype_is_int_returns_true_iff_type_is_derived_from_xsd_decimal);
	add_test_with_context(suite, spindle_generate_props, candidate_lang_returns_error_on_unsupported_value);
	add_test_with_context(suite, spindle_generate_props, candidate_lang_returns_error_on_too_short_string_length);
	add_test_with_context(suite, spindle_generate_props, candidate_lang_returns_error_on_too_long_string_length);
	add_test_with_context(suite, spindle_generate_props, candidate_lang_returns_false_if_matching_language_has_lower_priority_than_criterion);
	add_test_with_context(suite, spindle_generate_props, candidate_lang_returns_false_if_matching_language_has_equal_priority_to_criterion);
	add_test_with_context(suite, spindle_generate_props, candidate_lang_returns_error_if_twine_rdf_node_clone_fails);
	add_test_with_context(suite, spindle_generate_props, candidate_lang_returns_true_on_success);
	add_test_with_context(suite, spindle_generate_props, candidate_lang_converts_language_tag_to_canonical_form_for_comparison);
	add_test_with_context(suite, spindle_generate_props, candidate_lang_uses_map_prominence_if_criterion_prominence_is_zero);
	add_test_with_context(suite, spindle_generate_props, candidate_lang_appends_candidate_language_to_propmatch_literals_if_no_match_found);
	add_test_with_context(suite, spindle_generate_props, candidate_lang_sets_the_english_title);
	add_test_with_context(suite, spindle_generate_props, candidate_lang_sets_the_base_title_only_when_input_lang_is_NULL);
	add_test_with_context(suite, spindle_generate_props, candidate_lang_does_not_set_a_title_title_when_input_lang_is_not_english);
	add_test_with_context(suite, spindle_generate_props, candidate_literal_matches_by_language_if_no_datatype_is_specified);
	add_test_with_context(suite, spindle_generate_props, candidate_literal_returns_false_if_matching_literal_has_lower_priority_than_criterion);
	add_test_with_context(suite, spindle_generate_props, candidate_literal_returns_false_if_matching_literal_has_equal_priority_to_criterion);
	add_test_with_context(suite, spindle_generate_props, candidate_literal_returns_false_if_object_datatype_has_no_uri_and_language_is_not_NULL);
	add_test_with_context(suite, spindle_generate_props, candidate_literal_returns_false_if_object_datatype_has_different_uri_to_predicate_map_datatype_and_predicate_map_datatype_is_not_xsd_decimal);
	add_test_with_context(suite, spindle_generate_props, candidate_literal_returns_false_if_predicate_map_datatype_is_xsd_decimal_and_object_datatype_cannot_be_coerced_to_xsd_decimal);
	add_test_with_context(suite, spindle_generate_props, candidate_literal_returns_error_if_creating_a_new_node_from_a_typed_literal_fails);
	add_test_with_context(suite, spindle_generate_props, candidate_literal_returns_true_if_object_datatype_has_no_uri_and_language_is_NULL);
	add_test_with_context(suite, spindle_generate_props, candidate_literal_returns_true_if_object_datatype_matches_predicate_map_datatype);
	add_test_with_context(suite, spindle_generate_props, candidate_literal_returns_true_if_predicate_map_datatype_is_xsd_decimal_and_object_datatype_can_be_coerced_to_xsd_decimal);
	add_test_with_context(suite, spindle_generate_props, candidate_literal_when_successful_sets_match_resource);
	add_test_with_context(suite, spindle_generate_props, candidate_literal_when_successful_sets_match_priority);
	add_test_with_context(suite, spindle_generate_props, candidate_literal_when_successful_sets_match_prominence_to_criterion_prominence_if_criterion_prominence_is_non_zero);
	add_test_with_context(suite, spindle_generate_props, candidate_literal_when_successful_sets_match_prominence_to_predicate_map_prominence_if_criterion_prominence_is_zero);
	add_test_with_context(suite, spindle_generate_props, candidate_uri_returns_false_if_earlier_match_has_lower_priority_than_criterion);
	add_test_with_context(suite, spindle_generate_props, candidate_uri_returns_false_if_earlier_match_has_equal_priority_to_criterion);
	add_test_with_context(suite, spindle_generate_props, candidate_uri_returns_false_if_the_required_proxy_does_not_exist);
	add_test_with_context(suite, spindle_generate_props, candidate_uri_returns_false_if_the_required_proxy_matches_localname);
	add_test_with_context(suite, spindle_generate_props, candidate_uri_returns_error_if_cannot_create_node_for_proxy_uri);
	add_test_with_context(suite, spindle_generate_props, candidate_uri_returns_error_if_cloning_object_node_fails);
	add_test_with_context(suite, spindle_generate_props, candidate_uri_returns_true_on_success);
	add_test_with_context(suite, spindle_generate_props, candidate_uri_when_successful_sets_match_resource);
	add_test_with_context(suite, spindle_generate_props, candidate_uri_when_successful_sets_match_priority);
	add_test_with_context(suite, spindle_generate_props, candidate_uri_when_successful_sets_match_prominence_to_criterion_prominence_if_criterion_prominence_is_non_zero);
	add_test_with_context(suite, spindle_generate_props, candidate_uri_when_successful_sets_match_prominence_to_predicate_map_prominence_if_criterion_prominence_is_zero);
	add_test_with_context(suite, spindle_generate_props, candidate_returns_false_for_raptor_term_type_unknown);
	add_test_with_context(suite, spindle_generate_props, candidate_returns_false_for_raptor_term_type_blank);
	add_test_with_context(suite, spindle_generate_props, candidate_returns_false_for_raptor_term_type_uri_if_object_is_not_a_resource);
	add_test_with_context(suite, spindle_generate_props, candidate_returns_false_for_raptor_term_type_literal_if_object_is_not_a_literal);
	add_test_with_context(suite, spindle_generate_props, candidate_returns_result_from_candidate_uri_for_raptor_term_type_uri_if_object_is_a_resource);
	add_test_with_context(suite, spindle_generate_props, candidate_returns_result_from_candidate_literal_for_raptor_term_type_literal_if_object_is_a_literal);
	add_test_with_context(suite, spindle_generate_props, prop_test_returns_no_error_for_empty_predicate_map_list);
	add_test_with_context(suite, spindle_generate_props, prop_test_skips_predicate_map_if_predicate_map_has_no_matches_list);
	add_test_with_context(suite, spindle_generate_props, prop_test_stops_iterating_predicate_map_matches_list_when_at_end);
	add_test_with_context(suite, spindle_generate_props, prop_test_skips_predicate_map_match_if_match_inversion_property_is_true_and_inverse_parameter_is_false);
	add_test_with_context(suite, spindle_generate_props, prop_test_skips_predicate_map_match_if_match_inversion_property_is_false_and_inverse_parameter_is_true);
	add_test_with_context(suite, spindle_generate_props, prop_test_skips_predicate_map_match_if_match_onlyfor_property_is_not_NULL_and_propdata_classname_is_NULL);
	add_test_with_context(suite, spindle_generate_props, prop_test_skips_predicate_map_match_if_match_onlyfor_property_is_not_equal_to_propdata_classname);
	add_test_with_context(suite, spindle_generate_props, prop_test_skips_predicate_map_match_if_predicate_parameter_differs_from_match_predicate);
	add_test_with_context(suite, spindle_generate_props, prop_test_processes_statement_object_if_predicate_parameter_is_equal_to_predicate_map_match_predicate_and_inverse_parameter_is_false);
	add_test_with_context(suite, spindle_generate_props, prop_test_processes_statement_subject_if_predicate_parameter_is_equal_to_predicate_map_match_predicate_and_inverse_parameter_is_true);
	add_test_with_context(suite, spindle_generate_props, prop_test_can_match_the_same_predicate_from_match_lists_corresponding_to_different_targets);
	add_test_with_context(suite, spindle_generate_props, prop_apply_returns_error_if_self_node_cannot_be_cloned);
	add_test_with_context(suite, spindle_generate_props, prop_apply_returns_error_if_cannot_create_empty_statement);
	add_test_with_context(suite, spindle_generate_props, prop_apply_returns_no_error_if_predicate_match_list_map_is_empty);
	add_test_with_context(suite, spindle_generate_props, prop_apply_returns_error_if_cloning_the_statement_contining_only_a_subject_fails);
	add_test_with_context(suite, spindle_generate_props, prop_apply_returns_error_if_creating_a_node_for_the_matchs_map_target_fails);
	add_test_with_context(suite, spindle_generate_props, prop_apply_returns_no_error_if_the_match_has_no_resource_or_literals);
	add_test_with_context(suite, spindle_generate_props, prop_apply_builds_and_adds_a_target_statement_to_the_proxy_model_if_the_match_has_a_resource);
	add_test_with_context(suite, spindle_generate_props, prop_apply_returns_error_if_the_match_has_a_resource_and_adding_statement_to_proxy_model_fails);
	add_test_with_context(suite, spindle_generate_props, prop_apply_clears_the_resource_of_a_successful_match);
	add_test_with_context(suite, spindle_generate_props, prop_apply_when_the_match_has_a_resource_and_is_successful_adds_the_target_statement_to_the_root_model_if_the_matched_map_is_indexed_and_not_inverted_and_if_multigraph_is_true);
	add_test_with_context(suite, spindle_generate_props, prop_apply_returns_error_if_the_match_has_a_resource_and_adding_the_target_statement_to_the_root_model_fails);
	add_test_with_context(suite, spindle_generate_props, prop_apply_sets_the_title_match_to_the_match_struct_mapping_to_rdfs_label);
	add_test_with_context(suite, spindle_generate_props, prop_apply_sets_the_description_match_to_the_match_struct_mapping_to_dct_description);
	add_test_with_context(suite, spindle_generate_props, prop_apply_decrements_the_proxy_entry_score_by_the_prominance_of_each_match);
	add_test_with_context(suite, spindle_generate_props, prop_apply_does_not_set_longitude_and_geo_flag_if_the_match_has_a_resource_and_map_target_is_geo_long_and_the_literal_has_no_datatype_uri);
	add_test_with_context(suite, spindle_generate_props, prop_apply_does_not_set_latitude_and_geo_flag_if_the_match_has_a_resource_and_map_target_is_geo_lat_and_the_literal_has_no_datatype_uri);
	add_test_with_context(suite, spindle_generate_props, prop_apply_does_not_set_longitude_and_geo_flag_if_the_match_has_a_resource_and_map_target_is_geo_long_and_the_datatype_uri_string_is_NULL);
	add_test_with_context(suite, spindle_generate_props, prop_apply_does_not_set_latitude_and_geo_flag_if_the_match_has_a_resource_and_map_target_is_geo_lat_and_the_datatype_uri_string_is_NULL);
	add_test_with_context(suite, spindle_generate_props, prop_apply_does_not_set_longitude_and_geo_flag_if_the_match_has_a_resource_and_map_target_is_geo_long_and_the_datatype_uri_is_not_xsd_decimal);
	add_test_with_context(suite, spindle_generate_props, prop_apply_does_not_set_latitude_and_geo_flag_if_the_match_has_a_resource_and_map_target_is_geo_lat_and_the_datatype_uri_is_not_xsd_decimal);
	add_test_with_context(suite, spindle_generate_props, prop_apply_sets_longitude_and_geo_flag_if_the_match_has_a_resource_and_map_target_is_geo_long_and_the_datatype_uri_is_xsd_decimal);
	add_test_with_context(suite, spindle_generate_props, prop_apply_sets_latitude_and_geo_flag_if_the_match_has_a_resource_and_map_target_is_geo_lat_and_the_datatype_uri_is_xsd_decimal);
	add_test_with_context(suite, spindle_generate_props, prop_apply_adds_the_statement_to_the_proxy_model_and_returns_no_error_if_the_match_has_a_literal_and_no_resource);
	add_test_with_context(suite, spindle_generate_props, prop_apply_returns_error_if_the_match_has_a_literal_and_no_resource_and_cloning_the_predicate_statement_fails);
	add_test_with_context(suite, spindle_generate_props, prop_apply_returns_error_if_the_match_has_a_literal_and_no_resource_and_adding_the_statement_to_the_proxy_model_fails);
	add_test_with_context(suite, spindle_generate_props, prop_apply_when_the_match_has_no_resource_and_is_successful_adds_the_statement_to_the_root_model_and_returns_no_error_if_the_matched_map_is_indexed_and_not_inverted_and_if_multigraph_is_true);
	add_test_with_context(suite, spindle_generate_props, prop_apply_returns_error_if_the_match_has_no_resource_and_adding_the_target_statement_to_the_root_model_fails);
	add_test_with_context(suite, spindle_generate_props, prop_loop_returns_no_error_if_there_are_no_statements_in_source_model);
	add_test_with_context(suite, spindle_generate_props, prop_loop_returns_no_error_if_the_predicate_is_not_a_resource);
	add_test_with_context(suite, spindle_generate_props, prop_loop_returns_no_error_if_the_predicate_uri_is_NULL);
	add_test_with_context(suite, spindle_generate_props, prop_loop_returns_no_error_if_the_predicate_uri_as_string_is_NULL);
	add_test_with_context(suite, spindle_generate_props, prop_loop_returns_no_error_if_no_entry_reference_is_found_matching_the_subject_or_object);
	add_test_with_context(suite, spindle_generate_props, prop_loop_returns_result_from_prop_test_if_subject_from_a_source_model_triple_matches_an_entry_reference);
	add_test_with_context(suite, spindle_generate_props, prop_loop_returns_result_from_prop_test_if_object_from_a_source_model_triple_matches_an_entry_reference);
	add_test_with_context(suite, spindle_generate_props, prop_cleanup_frees_match_resource_and_match_literals_if_match_map_has_a_target);
	add_test_with_context(suite, spindle_generate_props, prop_init_initialises_the_property_data_structure);
	add_test_with_context(suite, spindle_generate_props, prop_update_entry_ignores_unknown_predicates);
	add_test_with_context(suite, spindle_generate_props, prop_update_entry_updates_the_language_agnostic_title_of_a_proxy_cache_entry_using_data_from_the_source_model_properties);
	add_test_with_context(suite, spindle_generate_props, prop_update_entry_updates_the_english_title_of_a_proxy_cache_entry_using_data_from_the_source_model_properties);
	add_test_with_context(suite, spindle_generate_props, prop_update_entry_updates_the_description_literals_using_data_from_the_source_model_properties);
	add_test_with_context(suite, spindle_generate_props, prop_update_entry_updates_the_geographic_details_of_a_proxy_cache_entry_using_data_from_the_source_model_properties);
	add_test_with_context(suite, spindle_generate_props, prop_update_entry_returns_error_when_prop_apply_fails);
	return suite;
}

int props_test(char *test) {
	if(test) {
		return run_single_test(create_props_test_suite(), test, create_text_reporter());
	} else {
		return run_test_suite(create_props_test_suite(), create_text_reporter());
	}
}

int main(int argc, char **argv) {
	return props_test(argc > 1 ? argv[1] : NULL);
}
