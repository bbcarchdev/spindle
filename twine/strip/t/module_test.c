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
#include <syslog.h>

/* declarations of functions to be tested */
int twine_plugin_init(void);
int twine_plugin_done(void);

/* mocks of dependancies */
#include "../../t/mock_libsql.h"
#include "../../t/mock_librdf.h"
#include "../../t/mock_libtwine.h"
#include "../../t/mock_spindle_core.h"
#include "../../t/mock_spindle_rulebase.h"

#define P_SPINDLE_STRIP_H_
#define PLUGIN_NAME "spindle-strip"
int spindle_strip(twine_graph *graph, void *data) { return 0; }
#include "../strip-module.c"

Describe(spindle_strip_module);
BeforeEach(spindle_strip_module) { always_expect(twine_logf); die_in(1); }
AfterEach(spindle_strip_module) {}

Ensure(spindle_strip_module, initalisation_succeeds) {
	expect(spindle_rulebase_create, will_return(555));
	expect(twine_config_get_bool, will_return(666), when(name, is_equal_to_string("spindle:dumprules")), when(fallback, is_equal_to(0)));
	expect(twine_config_get_bool, will_return(0), when(name, is_equal_to_string("spindle-strip:dumprules")), when(fallback, is_equal_to(666)));
	expect(twine_preproc_register, when(plugin, is_equal_to_string("spindle")), when(f, is_equal_to(spindle_strip)), when(data, is_equal_to(555)));
	expect(twine_graph_register, when(name, is_equal_to("spindle-strip")), when(fn, is_equal_to(spindle_strip)), when(data, is_equal_to(555)));
	assert_that(twine_plugin_init(), is_equal_to(0));
}

Ensure(spindle_strip_module, initalisation_fails_if_rulebase_creation_fails) {
	expect(spindle_rulebase_create, will_return(0));
	assert_that(twine_plugin_init(), is_equal_to(-1));
}

/* for when commit ada1846 "Refactor rulebase…" is deployed:
Ensure(spindle_strip_module, initalisation_fails_if_rulebase_configuration_fails) {
	expect(spindle_rulebase_create, will_return(555));
	expect(spindle_rulebase_add_config, will_return(-1), when(rulebase, is_equal_to(555)));
	expect(spindle_rulebase_destroy, when(rulebase, is_equal_to(555)));
	assert_that(twine_plugin_init(), is_equal_to(-1));
}
*/

Ensure(spindle_strip_module, dumps_the_rulebase_during_initalisation_if_the_spindledumprules_config_is_set) {
	expect(spindle_rulebase_create, will_return(555));
	expect(twine_config_get_bool, will_return(1), when(name, is_equal_to_string("spindle:dumprules")), when(fallback, is_equal_to(0)));
	expect(twine_config_get_bool, will_return(1), when(name, is_equal_to_string("spindle-strip:dumprules")), when(fallback, is_equal_to(1)));
	expect(spindle_rulebase_dump, when(rules, is_equal_to(555)));
	always_expect(twine_preproc_register);
	always_expect(twine_graph_register);
	twine_plugin_init();
}

Ensure(spindle_strip_module, dumps_the_rulebase_during_initalisation_if_the_spindlestripdumprules_config_is_set) {
	expect(spindle_rulebase_create, will_return(555));
	expect(twine_config_get_bool, will_return(0), when(name, is_equal_to_string("spindle:dumprules")), when(fallback, is_equal_to(0)));
	expect(twine_config_get_bool, will_return(1), when(name, is_equal_to_string("spindle-strip:dumprules")), when(fallback, is_equal_to(0)));
	expect(spindle_rulebase_dump, when(rules, is_equal_to(555)));
	always_expect(twine_preproc_register);
	always_expect(twine_graph_register);
	twine_plugin_init();
}

Ensure(spindle_strip_module, spindlestripdumprules_overrides_spindledumprules) {
	expect(spindle_rulebase_create, will_return(555));
	expect(twine_config_get_bool, will_return(1), when(name, is_equal_to_string("spindle:dumprules")), when(fallback, is_equal_to(0)));
	expect(twine_config_get_bool, will_return(0), when(name, is_equal_to_string("spindle-strip:dumprules")), when(fallback, is_equal_to(1)));
	never_expect(spindle_rulebase_dump);
	always_expect(twine_preproc_register);
	always_expect(twine_graph_register);
	twine_plugin_init();
}

Ensure(spindle_strip_module, disposes_gracefully) {
	expect(spindle_rulebase_destroy, when(rules, is_equal_to(555)));
	always_expect(spindle_rulebase_create, will_return(555));
	always_expect(twine_config_get_bool);
	always_expect(twine_preproc_register);
	always_expect(twine_graph_register);
	twine_plugin_init();
	assert_that(twine_plugin_done(), is_equal_to(0));
}

Ensure(spindle_strip_module, disposes_gracefully_even_if_never_initalised) {
	never_expect(spindle_rulebase_destroy);
	assert_that(twine_plugin_done(), is_equal_to(0));
}

int main(int argc, char **argv) {
	TestSuite *suite = create_test_suite();
	add_test_with_context(suite, spindle_strip_module, initalisation_succeeds);
	add_test_with_context(suite, spindle_strip_module, initalisation_fails_if_rulebase_creation_fails);
/*	add_test_with_context(suite, spindle_strip_module, initalisation_fails_if_rulebase_configuration_fails);
*/	add_test_with_context(suite, spindle_strip_module, dumps_the_rulebase_during_initalisation_if_the_spindledumprules_config_is_set);
	add_test_with_context(suite, spindle_strip_module, dumps_the_rulebase_during_initalisation_if_the_spindlestripdumprules_config_is_set);
	add_test_with_context(suite, spindle_strip_module, spindlestripdumprules_overrides_spindledumprules);
	add_test_with_context(suite, spindle_strip_module, disposes_gracefully);
	add_test_with_context(suite, spindle_strip_module, disposes_gracefully_even_if_never_initalised);
	return run_test_suite(suite, create_text_reporter());
}
