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

#define librdf_model void
#define librdf_node void
#define librdf_statement void
#define librdf_stream void
#define librdf_uri void
#define librdf_world void

int librdf_free_node(librdf_node *node) {
	return (int) mock(node);
}

void librdf_free_statement(librdf_statement *statement) {
	return (void) mock(statement);
}

int librdf_free_stream(librdf_stream *stream) {
	return (int) mock(stream);
}

int librdf_model_add_statement(librdf_model *model, librdf_statement *statement) {
	return (int) mock(model, statement);
}

int librdf_model_context_add_statement(librdf_model *model, librdf_node *context, librdf_statement *statement) {
	return (int) mock(model, context, statement);
}

librdf_stream *librdf_model_as_stream(librdf_model *model) {
	return (librdf_stream *) mock(model);
}

librdf_stream *librdf_model_find_statements(librdf_model *model, librdf_statement *statement) {
	return (librdf_stream *) mock(model, statement);
}

librdf_node *librdf_new_node_from_node(librdf_node *node) {
	return (librdf_node *) mock(node);
}

librdf_node *librdf_new_node_from_uri_string(librdf_world *world, const unsigned char *uri) {
	return (librdf_node *) mock(world, uri);
}

librdf_statement *librdf_new_statement(librdf_world *world) {
	return (librdf_statement *) mock(world);
}

librdf_statement *librdf_new_statement_from_statement(librdf_statement *statement) {
	return (librdf_statement *) mock(statement);
}

librdf_uri *librdf_node_get_uri(librdf_node *node) {
	return (librdf_uri *) mock(node);
}

int librdf_node_is_resource(librdf_node *node) {
	return (int) mock(node);
}

librdf_node *librdf_statement_get_object(librdf_statement *statement) {
	return (librdf_node *) mock(statement);
}

librdf_node *librdf_statement_get_predicate(librdf_statement *statement) {
	return (librdf_node *) mock(statement);
}

librdf_node *librdf_statement_get_subject(librdf_statement *statement) {
	return (librdf_node *) mock(statement);
}

void librdf_statement_set_object(librdf_statement *statement, librdf_node *node) {
	return (void) mock(statement, node);
}

void librdf_statement_set_predicate(librdf_statement *statement, librdf_node *node) {
	return (void) mock(statement, node);
}

void librdf_statement_set_subject(librdf_statement *statement, librdf_node *node) {
	return (void) mock(statement, node);
}

int librdf_stream_end(librdf_stream *stream) {
	return (int) mock(stream);
}

librdf_statement *librdf_stream_get_object(librdf_stream *stream) {
	return (librdf_statement *) mock(stream);
}

int librdf_stream_next(librdf_stream *stream) {
	return (int) mock(stream);
}

char *librdf_uri_as_string(librdf_uri *uri) {
	return (char *) mock(uri);
}
