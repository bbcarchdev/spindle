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

typedef struct sql_struct SQL;
typedef struct sql_statement_struct SQL_STATEMENT;

/* Return values for SQL_PERFORM_TXN */
#define SQL_TXN_COMMIT 1
#define SQL_TXN_ROLLBACK 0
#define SQL_TXN_RETRY -1
#define SQL_TXN_ABORT -2
#define SQL_TXN_FAIL -3

typedef enum {
	SQL_TXN_DEFAULT,
	SQL_TXN_CONSISTENT
} SQL_TXN_MODE;

int sql_deadlocked(SQL *sql) {
	return (int) mock(sql);
}

int sql_executef(SQL *sql, const char *format, ...) {
	return (int) mock(sql, format);
}

int sql_perform(SQL *sql, int (*fn)(SQL *, void *), void *userdata, int maxretries, SQL_TXN_MODE mode) {
	return (int) mock(sql, fn, userdata, maxretries, mode);
}

SQL_STATEMENT *sql_queryf(SQL *sql, const char *format, ...) {
	return (SQL_STATEMENT *) mock(sql, format);
}

int sql_stmt_destroy(SQL_STATEMENT *stmt) {
	return (int) mock(stmt);
}

int sql_stmt_eof(SQL_STATEMENT *stmt) {
	return (int) mock(stmt);
}

long sql_stmt_long(SQL_STATEMENT *stmt, unsigned int col) {
	return (long) mock(stmt, col);
}

const char *sql_stmt_str(SQL_STATEMENT *stmt, unsigned int col) {
	return (const char *) mock(stmt, col);
}
