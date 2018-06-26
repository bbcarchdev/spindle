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
#include "../../t/mock_libtwine.h"
#include "../../t/mock_strset.h"

#define SET_BLOCKSIZE 4

#define P_SPINDLE_H_
#define PLUGIN_NAME "spindle-rulebase"
int spindle_strset_add_flags(struct spindle_strset_struct *set, const char *str, unsigned flags);
#include "../strset.c"

Describe(spindle_common_strset);
BeforeEach(spindle_common_strset) { always_expect(twine_logf); die_in(1); }
AfterEach(spindle_common_strset) {}

/*
	This suite does not test any failure paths triggered by libc calls such as malloc.
*/

#pragma mark -
#pragma mark spindle_strset_create

Ensure(spindle_common_strset, create_succeeds) {
	struct spindle_strset_struct *p = spindle_strset_create();
	assert_that(p, is_non_null);
}

#pragma mark -
#pragma mark spindle_strset_destroy

Ensure(spindle_common_strset, destroy_returns_no_error_and_only_has_side_effects) {
	struct spindle_strset_struct *set = malloc(sizeof (struct spindle_strset_struct));
	set->size = SET_BLOCKSIZE;
	set->count = SET_BLOCKSIZE;
	set->strings = malloc(SET_BLOCKSIZE * sizeof (char *));
	set->flags = malloc(SET_BLOCKSIZE * sizeof (unsigned));
	for(int c = 0; c < SET_BLOCKSIZE; c++) {
		set->strings[c] = strdup("");
	}

	int r = spindle_strset_destroy(set);
	assert_that(r, is_equal_to(0));
}

#pragma mark -
#pragma mark spindle_strset_add

Ensure(spindle_common_strset, add_creates_the_string_list_if_necessary_and_adds_the_string) {
	const char *item = "item";
	struct spindle_strset_struct set = { 0 };

	int r = spindle_strset_add(&set, item);
	assert_that(r, is_equal_to(0));
	assert_that(set.size, is_greater_than(0));
	assert_that(set.count, is_equal_to(1));
	assert_that(set.strings, is_non_null);
	assert_that(set.strings[0], is_equal_to_string(item));
	assert_that(set.flags[0], is_equal_to(0));
}

Ensure(spindle_common_strset, add_expands_the_string_list_if_necessary_and_appends_the_string) {
	const char *item = "item";
	struct spindle_strset_struct set = { 0 };
	set.size = SET_BLOCKSIZE;
	set.count = SET_BLOCKSIZE;
	set.strings = malloc(SET_BLOCKSIZE * sizeof (char *));
	set.flags = malloc(SET_BLOCKSIZE * sizeof (unsigned));
	for(int c = 0; c < SET_BLOCKSIZE; c++) {
		set.strings[c] = strdup("");
	}

	int r = spindle_strset_add(&set, item);
	assert_that(r, is_equal_to(0));
	assert_that(set.size, is_greater_than(SET_BLOCKSIZE));
	assert_that(set.count, is_equal_to(SET_BLOCKSIZE + 1));
	assert_that(set.strings, is_non_null);
	assert_that(set.strings[SET_BLOCKSIZE], is_equal_to_string(item));
}

#pragma mark spindle_strset_add_flags

Ensure(spindle_common_strset, add_flags_creates_the_string_list_if_necessary_and_adds_the_string_and_its_flags) {
	const char *item = "item";
	unsigned item_flags = 0x89ABCDEF;
	struct spindle_strset_struct set = { 0 };

	int r = spindle_strset_add_flags(&set, item, item_flags);
	assert_that(r, is_equal_to(0));
	assert_that(set.size, is_greater_than(0));
	assert_that(set.count, is_equal_to(1));
	assert_that(set.strings, is_non_null);
	assert_that(set.strings[0], is_equal_to_string(item));
	assert_that(set.flags[0], is_equal_to(item_flags));
}

Ensure(spindle_common_strset, add_flags_expands_the_string_list_if_necessary_and_appends_the_string_and_its_flags) {
	const char *item = "item";
	unsigned item_flags = 0x89ABCDEF;
	struct spindle_strset_struct set = { 0 };
	set.size = SET_BLOCKSIZE;
	set.count = SET_BLOCKSIZE;
	set.strings = malloc(SET_BLOCKSIZE * sizeof (char *));
	set.flags = malloc(SET_BLOCKSIZE * sizeof (unsigned));
	for(int c = 0; c < SET_BLOCKSIZE; c++) {
		set.strings[c] = strdup("");
	}

	int r = spindle_strset_add_flags(&set, item, item_flags);
	assert_that(r, is_equal_to(0));
	assert_that(set.size, is_greater_than(SET_BLOCKSIZE));
	assert_that(set.count, is_equal_to(SET_BLOCKSIZE + 1));
	assert_that(set.strings, is_non_null);
	assert_that(set.strings[SET_BLOCKSIZE], is_equal_to_string(item));
	assert_that(set.flags[SET_BLOCKSIZE], is_equal_to(item_flags));
}

#pragma mark -

TestSuite *strset_test_suite(void) {
	TestSuite *suite = create_test_suite();
	add_test_with_context(suite, spindle_common_strset, create_succeeds);
	add_test_with_context(suite, spindle_common_strset, destroy_returns_no_error_and_only_has_side_effects);
	add_test_with_context(suite, spindle_common_strset, add_creates_the_string_list_if_necessary_and_adds_the_string);
	add_test_with_context(suite, spindle_common_strset, add_expands_the_string_list_if_necessary_and_appends_the_string);
	add_test_with_context(suite, spindle_common_strset, add_flags_creates_the_string_list_if_necessary_and_adds_the_string_and_its_flags);
	add_test_with_context(suite, spindle_common_strset, add_flags_expands_the_string_list_if_necessary_and_appends_the_string_and_its_flags);
	return suite;
}

int run(char *test) {
	if(test) {
		return run_single_test(strset_test_suite(), test, create_text_reporter());
	} else {
		return run_test_suite(strset_test_suite(), create_text_reporter());
	}
}

int main(int argc, char **argv) {
	return run(argc > 1 ? argv[1] : NULL);
}
