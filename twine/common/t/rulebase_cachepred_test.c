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
#include "../../t/mock_spindle_rulebase_class.h"
#include "../../t/mock_spindle_rulebase_coref.h"
#include "../../t/mock_spindle_rulebase_pred.h"

/* compile SUT inline due to static functions */
#define P_SPINDLE_H_
#define PLUGIN_NAME "spindle-rulebase"
int spindle_rulebase_destroy(SPINDLERULES *rules);
#include "../rulebase-cachepred.c"

Describe(spindle_common_rulebase);
BeforeEach(spindle_common_rulebase) { cgreen_mocks_are(learning_mocks); always_expect(twine_logf); die_in(1); }
AfterEach(spindle_common_rulebase) {}

/*
	This suite does not test any failure paths triggered by libc calls such as malloc.
*/

#pragma mark -
#pragma mark spindle_rulebase_cachepred_add

Ensure(spindle_common_rulebase, cachepred_add_adds_the_uri_to_the_rulebase_cachepred_list) {
	SPINDLERULES rules = { 0 };
	const char *uri = "uri";

	int r = spindle_rulebase_cachepred_add(&rules, uri);
	assert_that(r, is_equal_to(0));
	assert_that(rules.cpsize, is_equal_to(8));
	assert_that(rules.cpcount, is_equal_to(1));
	assert_that(rules.cachepreds, is_non_null);
	assert_that(rules.cachepreds[0], is_equal_to_string(uri));
}

#pragma mark -

TestSuite *create_rulebase_cachepred_test_suite(void) {
	TestSuite *suite = create_test_suite();
	add_test_with_context(suite, spindle_common_rulebase, cachepred_add_adds_the_uri_to_the_rulebase_cachepred_list);
	return suite;
}

int rulebase_cachepred_test(char *test) {
	if(test) {
		return run_single_test(create_rulebase_cachepred_test_suite(), test, create_text_reporter());
	} else {
		return run_test_suite(create_rulebase_cachepred_test_suite(), create_text_reporter());
	}
}

int main(int argc, char **argv) {
	return rulebase_cachepred_test(argc > 1 ? argv[1] : NULL);
}
