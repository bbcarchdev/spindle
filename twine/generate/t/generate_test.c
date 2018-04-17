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
#include <time.h>

/* mocks of dependancies */
#include "../../t/mock_librdf.h"
#include "../../t/mock_spindle_core.h"

int twine_logf(int level, char *msg, ...) {
	return (int) mock(level, msg);
}

/* compile SUT inline due to static functions */
#define P_SPINDLE_GENERATE_H_
#define PLUGIN_NAME "spindle-generate"
#include "../generate.c"

Describe(spindle_generate_generator);
BeforeEach(spindle_generate_generator) { always_expect(twine_logf); die_in(1); }
AfterEach(spindle_generate_generator) {}

/*
	This suite does not test the static timestamp functions (which are only used
	for logging), nor any failure paths triggered by libc calls such as malloc.
*/

#pragma mark spindle_generate

Ensure(spindle_generate_generator, generates_a_proxy_entry_and_invokes_an_SQL_transaction_to_store_it_if_a_db_connection_exists) {
	SQL *db = (SQL *) 0x333;
	SPINDLE spindle = { .root = "root" };
	SPINDLEGENERATE generate = { .spindle = &spindle };
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };
	const char *ident = "identifier";
	void *idref = malloc(128); // normally allocated by spindle_generate_uri_() and freed at the end of spindle_generate()

	expect(spindle_proxy_locate, will_return(idref), when(spindle, is_equal_to(&spindle)), when(uri, is_equal_to(ident)));
	expect(spindle_entry_init, will_return(0), will_set_contents_of_parameter(entry, &entry, sizeof entry), when(generate, is_equal_to(&generate)), when(localname, is_equal_to(idref)));
	expect(sql_perform, will_return(0), when(sql, is_equal_to(db)), when(fn, is_equal_to(spindle_generate_txn_)));
	expect(spindle_entry_cleanup);
	never_expect(sql_queryf);

	assert_that(spindle_generate(&generate, ident, 0x222), is_equal_to(0));
}

Ensure(spindle_generate_generator, generates_and_attempts_to_store_a_proxy_entry_even_when_db_is_unavailable_MAYBE_BUG) {
	SQL *db = (SQL *) NULL;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLE spindle = { .root = "root" };
	SPINDLEGENERATE generate = { .spindle = &spindle };
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };
	const char *ident = "identifier";
	void *idref = malloc(128); // normally allocated by spindle_proxy_locate() and freed at the end of spindle_generate()

	expect(spindle_proxy_locate, will_return(idref), when(spindle, is_equal_to(&spindle)), when(uri, is_equal_to(ident)));
	expect(spindle_entry_init, will_return(0), will_set_contents_of_parameter(entry, &entry, sizeof entry), when(generate, is_equal_to(&generate)), when(localname, is_equal_to(idref)));
	expect(sql_queryf, will_return(stmt), when(sql, is_equal_to(db)), when(format, begins_with_string("SELECT ")));
	expect(sql_stmt_eof, when(stmt, is_equal_to(stmt)));
	expect(sql_stmt_str, when(stmt, is_equal_to(stmt)), when(col, is_equal_to(0)));
	expect(sql_stmt_str, will_return(""), when(stmt, is_equal_to(stmt)), when(col, is_equal_to(1)));
	expect(sql_stmt_long, when(stmt, is_equal_to(stmt)), when(col, is_equal_to(2)));
	expect(sql_stmt_destroy, when(stmt, is_equal_to(stmt)));
	expect(spindle_source_fetch_entry, will_return(0));
	expect(spindle_triggers_update, will_return(0));
	expect(spindle_class_update_entry, will_return(0));
	expect(spindle_prop_update_entry, will_return(0));
	expect(spindle_describe_entry, will_return(0));
	expect(spindle_doc_apply, will_return(0));
	expect(spindle_license_apply, will_return(0));
	expect(spindle_related_fetch_entry, will_return(0));
	expect(spindle_index_entry, will_return(0));
	expect(spindle_store_entry, will_return(0));
	expect(sql_executef, when(sql, is_equal_to(db)), when(format, begins_with_string("UPDATE ")));
	expect(spindle_trigger_apply, will_return(0));
	expect(spindle_entry_cleanup);

	assert_that(spindle_generate(&generate, ident, 0x222), is_equal_to(0));
}

#pragma mark spindle_generate_uri_

Ensure(spindle_generate_generator, URI_generator_creates_a_valid_URI_given_a_valid_UUID) {
	SPINDLE spindle = { .root = "root" };
	SPINDLEGENERATE generate = { .spindle = &spindle };
	const char *uuid = "ABCD-EF12-3456-7890-abcd-ef12-3456-7890";
	char *idref = spindle_generate_uri_(&generate, uuid);

	assert_that(idref, is_equal_to_string("root/abcdef1234567890abcdef1234567890#id"));
	free(idref);
}

Ensure(spindle_generate_generator, URI_generator_creates_a_valid_URI_given_a_valid_UUID_and_a_root_ending_with_a_slash) {
	SPINDLE spindle = { .root = "root/" };
	SPINDLEGENERATE generate = { .spindle = &spindle };
	const char *uuid = "ABCD-EF12-3456-7890-abcd-ef12-3456-7890";
	char *idref = spindle_generate_uri_(&generate, uuid);

	assert_that(idref, is_equal_to_string("root/abcdef1234567890abcdef1234567890#id"));
	free(idref);
}

Ensure(spindle_generate_generator, URI_generator_creates_a_server_relative_URI_given_a_valid_UUID_and_an_empty_root) {
	SPINDLE spindle = { .root = "" };
	SPINDLEGENERATE generate = { .spindle = &spindle };
	const char *uuid = "ABCD-EF12-3456-7890-abcd-ef12-3456-7890";
	char *idref = spindle_generate_uri_(&generate, uuid);

	assert_that(idref, is_equal_to_string("/abcdef1234567890abcdef1234567890#id"));
	free(idref);
}

Ensure(spindle_generate_generator, URI_generator_attempts_to_locate_a_proxy_when_the_identifier_does_not_begin_with_the_root_URI) {
	SPINDLE spindle = { .root = "root" };
	SPINDLEGENERATE generate = { .spindle = &spindle };
	const char *ident = "identifier";
	void *idref = malloc(128); // normally allocated by spindle_proxy_locate() and freed at the end of spindle_generate()

	expect(spindle_proxy_locate, will_return(idref), when(spindle, is_equal_to(&spindle)), when(uri, is_equal_to(ident)));

	assert_that(spindle_generate_uri_(&generate, ident), is_equal_to(idref));
	free(idref);
}

Ensure(spindle_generate_generator, URI_generator_copies_the_identifier_to_a_new_string_when_the_identifier_begins_with_the_root_URI) {
	SPINDLE spindle = { .root = "root" };
	SPINDLEGENERATE generate = { .spindle = &spindle };
	const char *ident = "root/uuid#id";
	char *idref = spindle_generate_uri_(&generate, ident);

	// strings are equal but pointers differ
	assert_that(idref, is_not_equal_to(ident));
	assert_that(idref, is_equal_to_string(ident));
	free(idref);
}

#pragma mark spindle_generate_txn_

Ensure(spindle_generate_generator, transaction_generates_and_stores_a_proxy_entry) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	expect(sql_queryf, will_return(stmt), when(sql, is_equal_to(db)), when(format, begins_with_string("SELECT ")));
	expect(sql_stmt_eof, when(stmt, is_equal_to(stmt)));
	expect(sql_stmt_str, when(stmt, is_equal_to(stmt)), when(col, is_equal_to(0)));
	expect(sql_stmt_str, will_return(""), when(stmt, is_equal_to(stmt)), when(col, is_equal_to(1)));
	expect(sql_stmt_long, when(stmt, is_equal_to(stmt)), when(col, is_equal_to(2)));
	expect(sql_stmt_destroy, when(stmt, is_equal_to(stmt)));
	expect(spindle_source_fetch_entry, will_return(0));
	expect(spindle_triggers_update, will_return(0));
	expect(spindle_class_update_entry, will_return(0));
	expect(spindle_prop_update_entry, will_return(0));
	expect(spindle_describe_entry, will_return(0));
	expect(spindle_doc_apply, will_return(0));
	expect(spindle_license_apply, will_return(0));
	expect(spindle_related_fetch_entry, will_return(0));
	expect(spindle_index_entry, will_return(0));
	expect(spindle_store_entry, will_return(0));
	expect(sql_executef, when(sql, is_equal_to(db)), when(format, begins_with_string("UPDATE ")));
	expect(spindle_trigger_apply, will_return(0));

	assert_that(spindle_generate_txn_(db, &entry), is_equal_to(1));
}

Ensure(spindle_generate_generator, transaction_rollsback_and_retries_storing_a_proxy_entry_on_deadlock) {
	SQL *db = (SQL *) 0x333;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	expect(sql_queryf, will_return(NULL), when(sql, is_equal_to(db))); /* first call we can fail on */
	expect(sql_deadlocked, will_return(1), when(sql, is_equal_to(db)));
	expect(spindle_entry_reset, will_return(0), when(entry, is_equal_to(&entry)));

	assert_that(spindle_generate_txn_(db, &entry), is_equal_to(-1));
}

Ensure(spindle_generate_generator, transaction_rollsback_and_aborts_storing_a_proxy_entry_on_non_deadlock_failure) {
	SQL *db = (SQL *) 0x333;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	expect(sql_queryf, will_return(NULL), when(sql, is_equal_to(db))); /* first call we can fail on */
	expect(sql_deadlocked, will_return(0), when(sql, is_equal_to(db)));

	assert_that(spindle_generate_txn_(db, &entry), is_equal_to(-2));
}

Ensure(spindle_generate_generator, transaction_rollsback_and_aborts_storing_a_proxy_entry_on_deadlock_with_reset_failure) {
	SQL *db = (SQL *) 0x333;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	expect(sql_queryf, will_return(NULL), when(sql, is_equal_to(db))); /* first call we can fail on */
	expect(sql_deadlocked, will_return(1), when(sql, is_equal_to(db)));
	expect(spindle_entry_reset, will_return(-1), when(entry, is_equal_to(&entry)));

	assert_that(spindle_generate_txn_(db, &entry), is_equal_to(-2));
}

#pragma mark spindle_generate_entry_

Ensure(spindle_generate_generator, core_function_generates_and_stores_a_proxy_entry) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	expect(sql_queryf, will_return(stmt), when(sql, is_equal_to(db)), when(format, begins_with_string("SELECT ")));
	expect(sql_stmt_eof, when(stmt, is_equal_to(stmt)));
	expect(sql_stmt_str, when(stmt, is_equal_to(stmt)), when(col, is_equal_to(0)));
	expect(sql_stmt_str, will_return(""), when(stmt, is_equal_to(stmt)), when(col, is_equal_to(1)));
	expect(sql_stmt_long, when(stmt, is_equal_to(stmt)), when(col, is_equal_to(2)));
	expect(sql_stmt_destroy, when(stmt, is_equal_to(stmt)));
	expect(spindle_source_fetch_entry, will_return(0));
	expect(spindle_triggers_update, will_return(0));
	expect(spindle_class_update_entry, will_return(0));
	expect(spindle_prop_update_entry, will_return(0));
	expect(spindle_describe_entry, will_return(0));
	expect(spindle_doc_apply, will_return(0));
	expect(spindle_license_apply, will_return(0));
	expect(spindle_related_fetch_entry, will_return(0));
	expect(spindle_index_entry, will_return(0));
	expect(spindle_store_entry, will_return(0));
	expect(sql_executef, when(sql, is_equal_to(db)), when(format, begins_with_string("UPDATE ")));
	expect(spindle_trigger_apply, will_return(0));

	assert_that(spindle_generate_entry_(&entry), is_equal_to(0));
}

Ensure(spindle_generate_generator, core_function_succeeds_if_no_proxy_exists_in_db) {
	SQL *db = (SQL *) 0x333;
	SPINDLEENTRY entry = { .id = 0, .db = db };

	never_expect(sql_queryf);
	always_expect(spindle_source_fetch_entry);
	always_expect(spindle_triggers_update);
	always_expect(spindle_class_update_entry);
	always_expect(spindle_prop_update_entry);
	always_expect(spindle_describe_entry);
	always_expect(spindle_doc_apply);
	always_expect(spindle_license_apply);
	always_expect(spindle_related_fetch_entry);
	always_expect(spindle_index_entry);
	always_expect(spindle_store_entry);
	always_expect(sql_executef);
	always_expect(spindle_trigger_apply);

	assert_that(spindle_generate_entry_(&entry), is_equal_to(0));
	assert_that(entry.flags, is_equal_to(-1));
}

Ensure(spindle_generate_generator, core_function_succeeds_if_no_proxy_state_exists_in_db) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	expect(sql_queryf, will_return(stmt));
	expect(sql_stmt_eof, will_return(1));
	expect(sql_stmt_destroy);
	always_expect(spindle_source_fetch_entry);
	always_expect(spindle_triggers_update);
	always_expect(spindle_class_update_entry);
	always_expect(spindle_prop_update_entry);
	always_expect(spindle_describe_entry);
	always_expect(spindle_doc_apply);
	always_expect(spindle_license_apply);
	always_expect(spindle_related_fetch_entry);
	always_expect(spindle_index_entry);
	always_expect(spindle_store_entry);
	always_expect(sql_executef);
	always_expect(spindle_trigger_apply);

	assert_that(spindle_generate_entry_(&entry), is_equal_to(0));
}

Ensure(spindle_generate_generator, core_function_fails_if_fetching_proxy_state_from_db_fails) {
	SQL *db = (SQL *) 0x333;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	expect(sql_queryf, will_return(NULL));

	assert_that(spindle_generate_entry_(&entry), is_equal_to(-1));
}

Ensure(spindle_generate_generator, core_function_fails_if_fetching_proxy_source_data_fails) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	always_expect(sql_queryf, will_return(stmt));
	always_expect(sql_stmt_eof, will_return(1));
	always_expect(sql_stmt_destroy);
	expect(spindle_source_fetch_entry, will_return(-1));

	assert_that(spindle_generate_entry_(&entry), is_equal_to(-1));
}

Ensure(spindle_generate_generator, core_function_fails_if_updating_proxy_triggers_fails) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	always_expect(sql_queryf, will_return(stmt));
	always_expect(sql_stmt_eof, will_return(1));
	always_expect(sql_stmt_destroy);
	always_expect(spindle_source_fetch_entry);
	expect(spindle_triggers_update, will_return(-1));

	assert_that(spindle_generate_entry_(&entry), is_equal_to(-1));
}

Ensure(spindle_generate_generator, core_function_fails_if_updating_proxy_classes_fails) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	always_expect(sql_queryf, will_return(stmt));
	always_expect(sql_stmt_eof, will_return(1));
	always_expect(sql_stmt_destroy);
	always_expect(spindle_source_fetch_entry);
	always_expect(spindle_triggers_update);
	expect(spindle_class_update_entry, will_return(-1));

	assert_that(spindle_generate_entry_(&entry), is_equal_to(-1));
}

Ensure(spindle_generate_generator, core_function_fails_if_updating_proxy_properties_fails) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	always_expect(sql_queryf, will_return(stmt));
	always_expect(sql_stmt_eof, will_return(1));
	always_expect(sql_stmt_destroy);
	always_expect(spindle_source_fetch_entry);
	always_expect(spindle_triggers_update);
	always_expect(spindle_class_update_entry);
	expect(spindle_prop_update_entry, will_return(-1));

	assert_that(spindle_generate_entry_(&entry), is_equal_to(-1));
}

Ensure(spindle_generate_generator, core_function_fails_if_fetching_information_about_the_documents_describing_the_proxy_fails) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	always_expect(sql_queryf, will_return(stmt));
	always_expect(sql_stmt_eof, will_return(1));
	always_expect(sql_stmt_destroy);
	always_expect(spindle_source_fetch_entry);
	always_expect(spindle_triggers_update);
	always_expect(spindle_class_update_entry);
	always_expect(spindle_prop_update_entry);
	expect(spindle_describe_entry, will_return(-1));

	assert_that(spindle_generate_entry_(&entry), is_equal_to(-1));
}

Ensure(spindle_generate_generator, core_function_fails_if_describing_the_document_itself_fails) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	always_expect(sql_queryf, will_return(stmt));
	always_expect(sql_stmt_eof, will_return(1));
	always_expect(sql_stmt_destroy);
	always_expect(spindle_source_fetch_entry);
	always_expect(spindle_triggers_update);
	always_expect(spindle_class_update_entry);
	always_expect(spindle_prop_update_entry);
	always_expect(spindle_describe_entry);
	expect(spindle_doc_apply, will_return(-1));

	assert_that(spindle_generate_entry_(&entry), is_equal_to(-1));
}

Ensure(spindle_generate_generator, core_function_fails_if_describing_licensing_information_fails) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	always_expect(sql_queryf, will_return(stmt));
	always_expect(sql_stmt_eof, will_return(1));
	always_expect(sql_stmt_destroy);
	always_expect(spindle_source_fetch_entry);
	always_expect(spindle_triggers_update);
	always_expect(spindle_class_update_entry);
	always_expect(spindle_prop_update_entry);
	always_expect(spindle_describe_entry);
	always_expect(spindle_doc_apply);
	expect(spindle_license_apply, will_return(-1));

	assert_that(spindle_generate_entry_(&entry), is_equal_to(-1));
}

Ensure(spindle_generate_generator, core_function_fails_if_fetching_data_about_related_resources_fails) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	always_expect(sql_queryf, will_return(stmt));
	always_expect(sql_stmt_eof, will_return(1));
	always_expect(sql_stmt_destroy);
	always_expect(spindle_source_fetch_entry);
	always_expect(spindle_triggers_update);
	always_expect(spindle_class_update_entry);
	always_expect(spindle_prop_update_entry);
	always_expect(spindle_describe_entry);
	always_expect(spindle_doc_apply);
	always_expect(spindle_license_apply);
	expect(spindle_related_fetch_entry, will_return(-1));

	assert_that(spindle_generate_entry_(&entry), is_equal_to(-1));
}

Ensure(spindle_generate_generator, core_function_fails_if_indexing_the_resulting_model_fails) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	always_expect(sql_queryf, will_return(stmt));
	always_expect(sql_stmt_eof, will_return(1));
	always_expect(sql_stmt_destroy);
	always_expect(spindle_source_fetch_entry);
	always_expect(spindle_triggers_update);
	always_expect(spindle_class_update_entry);
	always_expect(spindle_prop_update_entry);
	always_expect(spindle_describe_entry);
	always_expect(spindle_doc_apply);
	always_expect(spindle_license_apply);
	always_expect(spindle_related_fetch_entry);
	expect(spindle_index_entry, will_return(-1));

	assert_that(spindle_generate_entry_(&entry), is_equal_to(-1));
}

Ensure(spindle_generate_generator, core_function_fails_if_storing_the_resulting_model_fails) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	always_expect(sql_queryf, will_return(stmt));
	always_expect(sql_stmt_eof, will_return(1));
	always_expect(sql_stmt_destroy);
	always_expect(spindle_source_fetch_entry);
	always_expect(spindle_triggers_update);
	always_expect(spindle_class_update_entry);
	always_expect(spindle_prop_update_entry);
	always_expect(spindle_describe_entry);
	always_expect(spindle_doc_apply);
	always_expect(spindle_license_apply);
	always_expect(spindle_related_fetch_entry);
	always_expect(spindle_index_entry);
	expect(spindle_store_entry, will_return(-1));

	assert_that(spindle_generate_entry_(&entry), is_equal_to(-1));
}

Ensure(spindle_generate_generator, core_function_fails_if_update_the_state_of_the_db_entry_fails) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	always_expect(sql_queryf, will_return(stmt));
	always_expect(sql_stmt_eof, will_return(1));
	always_expect(sql_stmt_destroy);
	always_expect(spindle_source_fetch_entry);
	always_expect(spindle_triggers_update);
	always_expect(spindle_class_update_entry);
	always_expect(spindle_prop_update_entry);
	always_expect(spindle_describe_entry);
	always_expect(spindle_doc_apply);
	always_expect(spindle_license_apply);
	always_expect(spindle_related_fetch_entry);
	always_expect(spindle_index_entry);
	always_expect(spindle_store_entry);
	expect(sql_executef, will_return(-1), when(sql, is_equal_to(db)), when(format, begins_with_string("UPDATE ")));

	assert_that(spindle_generate_entry_(&entry), is_equal_to(-1));
}

Ensure(spindle_generate_generator, core_function_fails_if_application_of_the_triggers_fails) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	always_expect(sql_queryf, will_return(stmt));
	always_expect(sql_stmt_eof, will_return(1));
	always_expect(sql_stmt_destroy);
	always_expect(spindle_source_fetch_entry);
	always_expect(spindle_triggers_update);
	always_expect(spindle_class_update_entry);
	always_expect(spindle_prop_update_entry);
	always_expect(spindle_describe_entry);
	always_expect(spindle_doc_apply);
	always_expect(spindle_license_apply);
	always_expect(spindle_related_fetch_entry);
	always_expect(spindle_index_entry);
	always_expect(spindle_store_entry);
	always_expect(sql_executef);
	expect(spindle_trigger_apply, will_return(-1));

	assert_that(spindle_generate_entry_(&entry), is_equal_to(-1));
}

#pragma mark spindle_generate_state_fetch_

Ensure(spindle_generate_generator, fetches_state_from_db) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	expect(sql_queryf, will_return(stmt), when(sql, is_equal_to(db)), when(format, begins_with_string("SELECT ")));
	expect(sql_stmt_eof, will_return(0), when(stmt, is_equal_to(stmt)));
	expect(sql_stmt_str, will_return(""), when(stmt, is_equal_to(stmt)), when(col, is_equal_to(0)));
	expect(sql_stmt_str, will_return(""), when(stmt, is_equal_to(stmt)), when(col, is_equal_to(1)));
	expect(sql_stmt_long, when(stmt, is_equal_to(stmt)), when(col, is_equal_to(2)));
	expect(sql_stmt_destroy, when(stmt, is_equal_to(stmt)));

	assert_that(spindle_generate_state_fetch_(&entry), is_equal_to(0));
}

Ensure(spindle_generate_generator, keeps_state_when_db_state_is_dirty) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	expect(sql_stmt_str, will_return("DIRTY"), when(col, is_equal_to(0)));
	expect(sql_stmt_str, will_return("2018-05-01 12:00:00"), when(col, is_equal_to(1)));
	expect(sql_stmt_long, will_return(0x555), when(col, is_equal_to(2)));
	always_expect(sql_queryf, will_return(stmt));
	always_expect(sql_stmt_eof, will_return(0));
	always_expect(sql_stmt_destroy);

	assert_that(spindle_generate_state_fetch_(&entry), is_equal_to(0));
	assert_that(entry.flags, is_equal_to(0x555));
	assert_that(entry.modified, is_equal_to(1525176000));
}

Ensure(spindle_generate_generator, keeps_state_and_sets_all_flags_when_db_state_is_missing) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	// (sql_stmt_str(0) == NULL) -> (entry.flags == -1) regardless of sql_stmt_long(2)
	expect(sql_stmt_str, will_return(NULL), when(col, is_equal_to(0)));
	expect(sql_stmt_long, will_return(0x555), when(col, is_equal_to(2)));
	always_expect(sql_queryf, will_return(stmt));
	always_expect(sql_stmt_eof, will_return(0));
	always_expect(sql_stmt_str, will_return(""));
	always_expect(sql_stmt_destroy);

	assert_that(spindle_generate_state_fetch_(&entry), is_equal_to(0));
	assert_that(entry.flags, is_equal_to(-1));
}

Ensure(spindle_generate_generator, keeps_state_and_sets_all_flags_when_db_state_is_clean) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	// (sql_stmt_str(0) == "CLEAN") -> (entry.flags == -1) regardless of sql_stmt_long(2)
	expect(sql_stmt_str, will_return("CLEAN"), when(col, is_equal_to(0)));
	expect(sql_stmt_long, will_return(0x555), when(col, is_equal_to(2)));
	always_expect(sql_queryf, will_return(stmt));
	always_expect(sql_stmt_eof, will_return(0));
	always_expect(sql_stmt_str, will_return(""));
	always_expect(sql_stmt_destroy);

	assert_that(spindle_generate_state_fetch_(&entry), is_equal_to(0));
	assert_that(entry.flags, is_equal_to(-1));
}

Ensure(spindle_generate_generator, keeps_state_and_sets_all_flags_when_db_flags_are_empty) {
	SQL *db = (SQL *) 0x333;
	SQL_STATEMENT *stmt = (SQL_STATEMENT *) 0x444;
	SPINDLEENTRY entry = { .id = "entry_id", .db = db };

	// (sql_stmt_long(2) == 0) -> (entry.flags == -1)
	expect(sql_stmt_long, will_return(0), when(col, is_equal_to(2)));
	always_expect(sql_queryf, will_return(stmt));
	always_expect(sql_stmt_eof, will_return(0));
	always_expect(sql_stmt_str, will_return(""));
	always_expect(sql_stmt_destroy);

	assert_that(spindle_generate_state_fetch_(&entry), is_equal_to(0));
	assert_that(entry.flags, is_equal_to(-1));
}

int main(int argc, char **argv) {
	TestSuite *suite = create_test_suite();
	add_test_with_context(suite, spindle_generate_generator, generates_a_proxy_entry_and_invokes_an_SQL_transaction_to_store_it_if_a_db_connection_exists);
	add_test_with_context(suite, spindle_generate_generator, generates_and_attempts_to_store_a_proxy_entry_even_when_db_is_unavailable_MAYBE_BUG);
	add_test_with_context(suite, spindle_generate_generator, URI_generator_creates_a_valid_URI_given_a_valid_UUID);
	add_test_with_context(suite, spindle_generate_generator, URI_generator_creates_a_valid_URI_given_a_valid_UUID_and_a_root_ending_with_a_slash);
	add_test_with_context(suite, spindle_generate_generator, URI_generator_creates_a_server_relative_URI_given_a_valid_UUID_and_an_empty_root);
	add_test_with_context(suite, spindle_generate_generator, URI_generator_attempts_to_locate_a_proxy_when_the_identifier_does_not_begin_with_the_root_URI);
	add_test_with_context(suite, spindle_generate_generator, URI_generator_copies_the_identifier_to_a_new_string_when_the_identifier_begins_with_the_root_URI);
	add_test_with_context(suite, spindle_generate_generator, transaction_generates_and_stores_a_proxy_entry);
	add_test_with_context(suite, spindle_generate_generator, transaction_rollsback_and_retries_storing_a_proxy_entry_on_deadlock);
	add_test_with_context(suite, spindle_generate_generator, transaction_rollsback_and_aborts_storing_a_proxy_entry_on_non_deadlock_failure);
	add_test_with_context(suite, spindle_generate_generator, transaction_rollsback_and_aborts_storing_a_proxy_entry_on_deadlock_with_reset_failure);
	add_test_with_context(suite, spindle_generate_generator, core_function_generates_and_stores_a_proxy_entry);
	add_test_with_context(suite, spindle_generate_generator, core_function_succeeds_if_no_proxy_exists_in_db);
	add_test_with_context(suite, spindle_generate_generator, core_function_succeeds_if_no_proxy_state_exists_in_db);
	add_test_with_context(suite, spindle_generate_generator, core_function_fails_if_fetching_proxy_state_from_db_fails);
	add_test_with_context(suite, spindle_generate_generator, core_function_fails_if_fetching_proxy_source_data_fails);
	add_test_with_context(suite, spindle_generate_generator, core_function_fails_if_updating_proxy_triggers_fails);
	add_test_with_context(suite, spindle_generate_generator, core_function_fails_if_updating_proxy_classes_fails);
	add_test_with_context(suite, spindle_generate_generator, core_function_fails_if_updating_proxy_properties_fails);
	add_test_with_context(suite, spindle_generate_generator, core_function_fails_if_fetching_information_about_the_documents_describing_the_proxy_fails);
	add_test_with_context(suite, spindle_generate_generator, core_function_fails_if_describing_the_document_itself_fails);
	add_test_with_context(suite, spindle_generate_generator, core_function_fails_if_describing_licensing_information_fails);
	add_test_with_context(suite, spindle_generate_generator, core_function_fails_if_fetching_data_about_related_resources_fails);
	add_test_with_context(suite, spindle_generate_generator, core_function_fails_if_indexing_the_resulting_model_fails);
	add_test_with_context(suite, spindle_generate_generator, core_function_fails_if_storing_the_resulting_model_fails);
	add_test_with_context(suite, spindle_generate_generator, core_function_fails_if_update_the_state_of_the_db_entry_fails);
	add_test_with_context(suite, spindle_generate_generator, core_function_fails_if_application_of_the_triggers_fails);
	add_test_with_context(suite, spindle_generate_generator, fetches_state_from_db);
	add_test_with_context(suite, spindle_generate_generator, keeps_state_when_db_state_is_dirty);
	add_test_with_context(suite, spindle_generate_generator, keeps_state_and_sets_all_flags_when_db_state_is_missing);
	add_test_with_context(suite, spindle_generate_generator, keeps_state_and_sets_all_flags_when_db_state_is_clean);
	add_test_with_context(suite, spindle_generate_generator, keeps_state_and_sets_all_flags_when_db_flags_are_empty);
	return run_test_suite(suite, create_text_reporter());
}
