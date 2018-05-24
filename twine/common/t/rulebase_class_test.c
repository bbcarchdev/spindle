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
#include "../../t/mock_spindle_rulebase_coref.h"
#include "../../t/mock_spindle_rulebase_pred.h"

#define NS_OLO "NS_OLO "
#define NS_SPINDLE "NS_SPINDLE "

#define P_SPINDLE_H_
#define PLUGIN_NAME "spindle-rulebase"
#include "../rulebase-class.c"

Describe(spindle_common_rulebase);
BeforeEach(spindle_common_rulebase) { cgreen_mocks_are(learning_mocks); always_expect(twine_logf); die_in(1); }
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

TestSuite *create_rulebase_class_test_suite(void) {
	TestSuite *suite = create_test_suite();
	add_test_with_context(suite, spindle_common_rulebase, class_dump_only_has_side_effects);
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
