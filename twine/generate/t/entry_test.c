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

/* compile SUT inline due to static functions */
#define P_SPINDLE_GENERATE_H_
#define PLUGIN_NAME "spindle-generate"
#include "../entry.c"

Describe(spindle_generate_entry);
BeforeEach(spindle_generate_entry) { always_expect(twine_logf); die_in(1); }
AfterEach(spindle_generate_entry) {}

/*
	This suite does not test any failure paths triggered by libc calls such as malloc.
*/

#pragma mark -
#pragma mark spindle_entry_init

Ensure(spindle_generate_entry, can_be_constructed) {
	char *id = "id";
	const char *local_entity = "local#id";
	const char *local_resource = "local";
	librdf_world *world = (librdf_world *) 0x333;
	librdf_node *sameas = (librdf_node *) 0xB01;
	SPINDLE spindle = {
		.root = "root",
		.world = world,
		.sameas = sameas,
		.multigraph = 1
	};
	SPINDLEGENERATE generate = {
		.spindle = &spindle,
		.rules = (SPINDLERULES *) 0xA01,
		.sparql = (SPARQL *) 0xA02,
		.db = (SQL *) 0xA03
	};
	SPINDLEENTRY entry, expected = {
		.generate = &generate,
		.spindle = &spindle,
		.rules = generate.rules,
		.sparql = generate.sparql,
		.db = generate.db,
		.id = id,
		.localname = local_entity,
		.score = 50,
		.sameas = sameas,
		.self = (librdf_node *) 0x444,
		.doc = (librdf_node *) 0x555,
		.graph = (librdf_node *) 0x555, /* same as .doc */
		.rootdata = (librdf_model *) 0x666,
		.proxydata = (librdf_model *) 0x777,
		.sourcedata = (librdf_model *) 0x888,
		.extradata = (librdf_model *) 0x999,
		.sources = (struct spindle_strset_struct *) 0xAAA
	};

	expect(spindle_db_id, will_return(id), when(localname, is_equal_to(local_entity)));
	expect(librdf_new_node_from_uri_string, will_return(0x444), when(world, is_equal_to(world)), when(uri, is_equal_to(local_entity)));
	expect(librdf_new_node_from_uri_string, will_return(0x555), when(world, is_equal_to(world)), when(uri, is_equal_to_string(local_resource)));
	expect(twine_rdf_model_create, will_return(0x666));
	expect(twine_rdf_model_create, will_return(0x888));
	expect(twine_rdf_model_create, will_return(0x777));
	expect(twine_rdf_model_create, will_return(0x999));
	expect(spindle_strset_create, will_return(0xAAA));

	int r = spindle_entry_init(&entry, &generate, local_entity);
	assert_that(r, is_equal_to(0));
	assert_that(entry.docname, is_equal_to_string(local_resource));
	assert_that(entry.graphname, is_equal_to_string(local_resource));
	expected.docname = entry.docname;
	expected.graphname = entry.graphname;
	assert_that(&entry, is_equal_to_contents_of(&expected, sizeof expected));
}

Ensure(spindle_generate_entry, fails_construction_if_source_string_set_cannot_be_allocated) {
	const char *local_entity = "local#id";
	const char *local_resource = "local";
	librdf_world *world = (librdf_world *) 0x333;
	SPINDLE spindle = { .root = "root", .world = world };
	SPINDLEGENERATE generate = { .spindle = &spindle };
	SPINDLEENTRY entry;

	expect(librdf_new_node_from_uri_string, will_return(0x444), when(world, is_equal_to(world)), when(uri, is_equal_to(local_entity)));
	expect(librdf_new_node_from_uri_string, will_return(0x555), when(world, is_equal_to(world)), when(uri, is_equal_to_string(local_resource)));
	expect(twine_rdf_model_create, will_return(0x666));
	expect(twine_rdf_model_create, will_return(0x888));
	expect(twine_rdf_model_create, will_return(0x777));
	expect(twine_rdf_model_create, will_return(0x999));
	expect(spindle_strset_create, will_return(NULL));

	assert_that(spindle_entry_init(&entry, &generate, local_entity), is_equal_to(-1));
}

Ensure(spindle_generate_entry, fails_construction_if_rdf_model_for_extra_data_cannot_be_allocated) {
	const char *local_entity = "local#id";
	const char *local_resource = "local";
	librdf_world *world = (librdf_world *) 0x333;
	SPINDLE spindle = { .root = "root", .world = world };
	SPINDLEGENERATE generate = { .spindle = &spindle };
	SPINDLEENTRY entry;

	expect(librdf_new_node_from_uri_string, will_return(0x444), when(world, is_equal_to(world)), when(uri, is_equal_to(local_entity)));
	expect(librdf_new_node_from_uri_string, will_return(0x555), when(world, is_equal_to(world)), when(uri, is_equal_to_string(local_resource)));
	expect(twine_rdf_model_create, will_return(0x666));
	expect(twine_rdf_model_create, will_return(0x888));
	expect(twine_rdf_model_create, will_return(0x777));
	expect(twine_rdf_model_create, will_return(NULL));

	assert_that(spindle_entry_init(&entry, &generate, local_entity), is_equal_to(-1));
}

Ensure(spindle_generate_entry, fails_construction_if_rdf_model_for_proxy_data_cannot_be_allocated) {
	const char *local_entity = "local#id";
	const char *local_resource = "local";
	librdf_world *world = (librdf_world *) 0x333;
	SPINDLE spindle = { .root = "root", .world = world };
	SPINDLEGENERATE generate = { .spindle = &spindle };
	SPINDLEENTRY entry;

	expect(librdf_new_node_from_uri_string, will_return(0x444), when(world, is_equal_to(world)), when(uri, is_equal_to(local_entity)));
	expect(librdf_new_node_from_uri_string, will_return(0x555), when(world, is_equal_to(world)), when(uri, is_equal_to_string(local_resource)));
	expect(twine_rdf_model_create, will_return(0x666));
	expect(twine_rdf_model_create, will_return(0x888));
	expect(twine_rdf_model_create, will_return(NULL));

	assert_that(spindle_entry_init(&entry, &generate, local_entity), is_equal_to(-1));
}

Ensure(spindle_generate_entry, fails_construction_if_rdf_model_for_source_data_cannot_be_allocated) {
	const char *local_entity = "local#id";
	const char *local_resource = "local";
	librdf_world *world = (librdf_world *) 0x333;
	SPINDLE spindle = { .root = "root", .world = world };
	SPINDLEGENERATE generate = { .spindle = &spindle };
	SPINDLEENTRY entry;

	expect(librdf_new_node_from_uri_string, will_return(0x444), when(world, is_equal_to(world)), when(uri, is_equal_to(local_entity)));
	expect(librdf_new_node_from_uri_string, will_return(0x555), when(world, is_equal_to(world)), when(uri, is_equal_to_string(local_resource)));
	expect(twine_rdf_model_create, will_return(0x666));
	expect(twine_rdf_model_create, will_return(NULL));

	assert_that(spindle_entry_init(&entry, &generate, local_entity), is_equal_to(-1));
}

Ensure(spindle_generate_entry, fails_construction_if_rdf_model_for_root_data_cannot_be_allocated) {
	const char *local_entity = "local#id";
	const char *local_resource = "local";
	librdf_world *world = (librdf_world *) 0x333;
	SPINDLE spindle = { .root = "root", .world = world };
	SPINDLEGENERATE generate = { .spindle = &spindle };
	SPINDLEENTRY entry;

	expect(librdf_new_node_from_uri_string, will_return(0x444), when(world, is_equal_to(world)), when(uri, is_equal_to(local_entity)));
	expect(librdf_new_node_from_uri_string, will_return(0x555), when(world, is_equal_to(world)), when(uri, is_equal_to_string(local_resource)));
	expect(twine_rdf_model_create, will_return(NULL));

	assert_that(spindle_entry_init(&entry, &generate, local_entity), is_equal_to(-1));
}

Ensure(spindle_generate_entry, does_not_fail_construction_if_obtaining_the_node_for_the_proxy_resource_fails_MAY_BE_BUG) {
	char *id = "id";
	const char *local_entity = "local#id";
	const char *local_resource = "local";
	librdf_world *world = (librdf_world *) 0x333;
	librdf_node *sameas = (librdf_node *) 0xB01;
	SPINDLE spindle = {
		.root = "root",
		.world = world,
		.sameas = sameas,
		.multigraph = 1
	};
	SPINDLEGENERATE generate = {
		.spindle = &spindle,
		.rules = (SPINDLERULES *) 0xA01,
		.sparql = (SPARQL *) 0xA02,
		.db = (SQL *) 0xA03
	};
	SPINDLEENTRY entry, expected = {
		.generate = &generate,
		.spindle = &spindle,
		.rules = generate.rules,
		.sparql = generate.sparql,
		.db = generate.db,
		.id = id,
		.localname = local_entity,
		.score = 50,
		.sameas = sameas,
		.self = (librdf_node *) 0x444,
		.doc = (librdf_node *) NULL,
		.graph = (librdf_node *) NULL, /* same as .doc */
		.rootdata = (librdf_model *) 0x666,
		.proxydata = (librdf_model *) 0x777,
		.sourcedata = (librdf_model *) 0x888,
		.extradata = (librdf_model *) 0x999,
		.sources = (struct spindle_strset_struct *) 0xAAA
	};

	expect(spindle_db_id, will_return(id), when(localname, is_equal_to(local_entity)));
	expect(librdf_new_node_from_uri_string, will_return(0x444), when(world, is_equal_to(world)), when(uri, is_equal_to(local_entity)));
	expect(librdf_new_node_from_uri_string, will_return(NULL), when(world, is_equal_to(world)), when(uri, is_equal_to_string(local_resource)));
	expect(twine_rdf_model_create, will_return(0x666));
	expect(twine_rdf_model_create, will_return(0x888));
	expect(twine_rdf_model_create, will_return(0x777));
	expect(twine_rdf_model_create, will_return(0x999));
	expect(spindle_strset_create, will_return(0xAAA));

	int r = spindle_entry_init(&entry, &generate, local_entity);
	assert_that(r, is_equal_to(0));
	assert_that(entry.docname, is_equal_to_string(local_resource));
	assert_that(entry.graphname, is_equal_to_string(local_resource));
	expected.docname = entry.docname;
	expected.graphname = entry.graphname;
	assert_that(&entry, is_equal_to_contents_of(&expected, sizeof expected));
}

Ensure(spindle_generate_entry, fails_construction_if_obtaining_the_node_for_the_proxy_entity_fails) {
	const char *local_entity = "local#id";
	const char *local_resource = "local";
	librdf_world *world = (librdf_world *) 0x333;
	SPINDLE spindle = { .root = "root", .world = world };
	SPINDLEGENERATE generate = { .spindle = &spindle };
	SPINDLEENTRY entry;

	expect(librdf_new_node_from_uri_string, will_return(NULL), when(world, is_equal_to(world)), when(uri, is_equal_to(local_entity)));

	assert_that(spindle_entry_init(&entry, &generate, local_entity), is_equal_to(-1));
}

#pragma mark -
#pragma mark spindle_entry_init_models_
/*	I realise these cover the same code as some of the earlier tests, i.e.
	fails_construction_if_rdf_model_for_extra_data_cannot_be_allocated et al,
	but these spec the response code of the internal method, for completeness */

Ensure(spindle_generate_entry, rdf_models_can_be_constructed) {
	SPINDLEENTRY entry = { 0 }, expected = {
		.rootdata = (librdf_model *) 0x666,
		.proxydata = (librdf_model *) 0x777,
		.sourcedata = (librdf_model *) 0x888,
		.extradata = (librdf_model *) 0x999
	};

	expect(twine_rdf_model_create, will_return(0x666));
	expect(twine_rdf_model_create, will_return(0x888));
	expect(twine_rdf_model_create, will_return(0x777));
	expect(twine_rdf_model_create, will_return(0x999));

	int r = spindle_entry_init_models_(&entry);
	assert_that(r, is_equal_to(0));
	assert_that(&entry, is_equal_to_contents_of(&expected, sizeof expected));
}

Ensure(spindle_generate_entry, rdf_models_fail_construction_if_model_for_extra_data_cannot_be_allocated) {
	SPINDLEENTRY entry;

	expect(twine_rdf_model_create, will_return(0x666));
	expect(twine_rdf_model_create, will_return(0x888));
	expect(twine_rdf_model_create, will_return(0x777));
	expect(twine_rdf_model_create, will_return(NULL));

	assert_that(spindle_entry_init_models_(&entry), is_equal_to(-1));
}

Ensure(spindle_generate_entry, rdf_models_fail_construction_if_model_for_proxy_data_cannot_be_allocated) {
	SPINDLEENTRY entry;

	expect(twine_rdf_model_create, will_return(0x666));
	expect(twine_rdf_model_create, will_return(0x888));
	expect(twine_rdf_model_create, will_return(NULL));

	assert_that(spindle_entry_init_models_(&entry), is_equal_to(-1));
}

Ensure(spindle_generate_entry, rdf_models_fail_construction_if_model_for_source_data_cannot_be_allocated) {
	SPINDLEENTRY entry;

	expect(twine_rdf_model_create, will_return(0x666));
	expect(twine_rdf_model_create, will_return(NULL));

	assert_that(spindle_entry_init_models_(&entry), is_equal_to(-1));
}

Ensure(spindle_generate_entry, rdf_models_fail_construction_if_model_for_root_data_cannot_be_allocated) {
	SPINDLEENTRY entry;

	expect(twine_rdf_model_create, will_return(NULL));

	assert_that(spindle_entry_init_models_(&entry), is_equal_to(-1));
}

#pragma mark -
#pragma mark spindle_entry_reset

Ensure(spindle_generate_entry, can_be_reset) {
	SPINDLEENTRY entry = {
		.rootdata = (librdf_model *) 0x333,
		.proxydata = (librdf_model *) 0x444,
		.sourcedata = (librdf_model *) 0x555,
		.extradata = (librdf_model *) 0x666,
		.classes = (struct spindle_strset_struct *) 0x777
	};
	SPINDLEENTRY expected = {
		.rootdata = (librdf_model *) 0x888,
		.proxydata = (librdf_model *) 0x999,
		.sourcedata = (librdf_model *) 0xAAA,
		.extradata = (librdf_model *) 0xBBB
	};

	expect(twine_rdf_model_destroy, when(model, is_equal_to(0x333)));
	expect(twine_rdf_model_destroy, when(model, is_equal_to(0x444)));
	expect(twine_rdf_model_destroy, when(model, is_equal_to(0x555)));
	expect(twine_rdf_model_destroy, when(model, is_equal_to(0x666)));
	expect(spindle_strset_destroy, when(set, is_equal_to(0x777)));
	expect(twine_rdf_model_create, will_return(0x888));
	expect(twine_rdf_model_create, will_return(0xAAA));
	expect(twine_rdf_model_create, will_return(0x999));
	expect(twine_rdf_model_create, will_return(0xBBB));

	int r = spindle_entry_reset(&entry);
	assert_that(r, is_equal_to(0));
	assert_that(&entry, is_equal_to_contents_of(&expected, sizeof expected));
}

#pragma mark -
#pragma mark spindle_entry_cleanup

Ensure(spindle_generate_entry, can_be_destroyed_when_initalised) {
	const char *local_entity = "local#id";
	const char *local_resource = "local";
	librdf_world *world = (librdf_world *) 0x333;
	librdf_node *sameas = (librdf_node *) 0xB01;
	SPINDLE spindle = {
		.root = "root",
		.world = world,
		.sameas = sameas,
		.multigraph = 1
	};
	SPINDLEGENERATE generate = {
		.spindle = &spindle,
		.rules = (SPINDLERULES *) 0xA01,
		.sparql = (SPARQL *) 0xA02,
		.db = (SQL *) 0xA03
	};
	SPINDLEENTRY entry = {
		.generate = &generate,
		.spindle = &spindle,
		.rules = generate.rules,
		.sparql = generate.sparql,
		.db = generate.db,
		.localname = local_entity,
		.score = 50,
		.sameas = sameas,
		.self = (librdf_node *) 0xA04,
		.doc = (librdf_node *) 0xA05,
		.refs = (librdf_node *) 0xA06,
		.rootdata = (librdf_model *) 0xA07,
		.proxydata = (librdf_model *) 0xA08,
		.sourcedata = (librdf_model *) 0xA09,
		.extradata = (librdf_model *) 0xA10,
		.sources = (struct spindle_strset_struct *) 0xA11
	};

	expect(spindle_strset_destroy, when(set, is_equal_to(entry.sources)));
	expect(spindle_proxy_refs_destroy, when(refs, is_equal_to(entry.refs)));
	expect(librdf_free_node, when(node, is_equal_to(entry.doc)));
	expect(librdf_free_node, when(node, is_equal_to(entry.self)));
	expect(twine_rdf_model_destroy, when(model, is_equal_to(entry.rootdata)));
	expect(twine_rdf_model_destroy, when(model, is_equal_to(entry.proxydata)));
	expect(twine_rdf_model_destroy, when(model, is_equal_to(entry.sourcedata)));
	expect(twine_rdf_model_destroy, when(model, is_equal_to(entry.extradata)));

	assert_that(spindle_entry_cleanup(&entry), is_equal_to(0));
}

Ensure(spindle_generate_entry, can_be_destroyed_when_empty) {
	SPINDLEENTRY entry = { 0 };

	never_expect(spindle_strset_destroy);
	never_expect(spindle_proxy_refs_destroy);
	never_expect(librdf_free_node);
	never_expect(twine_rdf_model_destroy);

	assert_that(spindle_entry_cleanup(&entry), is_equal_to(0));
}

#pragma mark -

int entry_test(void) {
	TestSuite *suite = create_test_suite();
	add_test_with_context(suite, spindle_generate_entry, can_be_constructed);
	add_test_with_context(suite, spindle_generate_entry, fails_construction_if_source_string_set_cannot_be_allocated);
	add_test_with_context(suite, spindle_generate_entry, fails_construction_if_rdf_model_for_extra_data_cannot_be_allocated);
	add_test_with_context(suite, spindle_generate_entry, fails_construction_if_rdf_model_for_proxy_data_cannot_be_allocated);
	add_test_with_context(suite, spindle_generate_entry, fails_construction_if_rdf_model_for_source_data_cannot_be_allocated);
	add_test_with_context(suite, spindle_generate_entry, fails_construction_if_rdf_model_for_root_data_cannot_be_allocated);
	add_test_with_context(suite, spindle_generate_entry, does_not_fail_construction_if_obtaining_the_node_for_the_proxy_resource_fails_MAY_BE_BUG);
	add_test_with_context(suite, spindle_generate_entry, fails_construction_if_obtaining_the_node_for_the_proxy_entity_fails);
	add_test_with_context(suite, spindle_generate_entry, rdf_models_can_be_constructed);
	add_test_with_context(suite, spindle_generate_entry, rdf_models_fail_construction_if_model_for_extra_data_cannot_be_allocated);
	add_test_with_context(suite, spindle_generate_entry, rdf_models_fail_construction_if_model_for_proxy_data_cannot_be_allocated);
	add_test_with_context(suite, spindle_generate_entry, rdf_models_fail_construction_if_model_for_source_data_cannot_be_allocated);
	add_test_with_context(suite, spindle_generate_entry, rdf_models_fail_construction_if_model_for_root_data_cannot_be_allocated);
	add_test_with_context(suite, spindle_generate_entry, can_be_reset);
	add_test_with_context(suite, spindle_generate_entry, can_be_destroyed_when_initalised);
	add_test_with_context(suite, spindle_generate_entry, can_be_destroyed_when_empty);
	return run_test_suite(suite, create_text_reporter());
}

int main(int argc, char **argv) {
	return entry_test();
}
