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

#pragma mark -

int props_test(void) {
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
	return run_test_suite(suite, create_text_reporter());
}

int main(int argc, char **argv) {
	return props_test();
}
