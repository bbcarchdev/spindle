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

Ensure(spindle_generate_props, does_a_thing) {
	struct spindle_literalset_struct dest = { 0 };
	struct propmatch_struct source = { 0 };
	int r = spindle_prop_copystrings_(&dest, &source);
	assert_that(r, is_equal_to(0));
}

#pragma mark -

int props_test(void) {
	TestSuite *suite = create_test_suite();
	add_test_with_context(suite, spindle_generate_props, does_a_thing);
	return run_test_suite(suite, create_text_reporter());
}

int main(int argc, char **argv) {
	return props_test();
}
