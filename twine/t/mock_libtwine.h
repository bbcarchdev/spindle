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

int twine_logf(int level, char *msg, ...) {
	return (int) mock(level, msg);
}

int twine_plugin_init(void) {
	return (int) mock();
}

librdf_model *twine_rdf_model_create(void) {
	return (librdf_model *) mock();
}

int twine_rdf_model_destroy(librdf_model *model) {
	return (int) mock(model);
}
