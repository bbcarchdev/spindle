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

typedef struct {
	char *uri;
	void *reserved;
	void *store;
	void *old;
} twine_graph;

char *twine_config_geta(const char *key, const char *defval) {
	return (char *) mock(key, defval);
}

int twine_logf(int level, char *msg, ...) {
	return (int) mock(level, msg);
}

int twine_plugin_init(void) {
	return (int) mock();
}

int twine_rdf_model_add_st(librdf_model *model, librdf_statement *statement, librdf_node *ctx) {
	return (int) mock(model, statement, ctx);
}

librdf_model *twine_rdf_model_create(void) {
	return (librdf_model *) mock();
}

int twine_rdf_model_destroy(librdf_model *model) {
	return (int) mock(model);
}

int twine_rdf_model_parse(librdf_model *model, const char *mime, const char *buf, size_t buflen) {
	return (int) mock(model, mime, buf, buflen);
}

librdf_node *twine_rdf_node_createuri(const char *uri) {
	return (librdf_node *) mock(uri);
}

librdf_node *twine_rdf_node_clone(librdf_node *node) {
	return (librdf_node *) mock(node);
}

int twine_rdf_node_destroy(librdf_node *node) {
	return (int) mock(node);
}

librdf_statement *twine_rdf_st_create(void) {
	return (librdf_statement *) mock();
}

librdf_statement *twine_rdf_st_clone(librdf_statement *src) {
	return (librdf_statement *) mock(src);
}

int twine_rdf_st_destroy(librdf_statement *statement) {
	return (int) mock(statement);
}

int twine_rdf_st_obj_intval(librdf_statement *statement, long *value) {
	return (int) mock(statement, value);
}

librdf_world *twine_rdf_world(void) {
	return (librdf_world *) mock();
}
