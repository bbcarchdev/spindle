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

#include <sys/time.h>

int spindle_class_update_entry(SPINDLEENTRY *entry) {
	return (int) mock(entry);
}

int spindle_doc_apply(SPINDLEENTRY *entry) {
	return (int) mock(entry);
}

int spindle_describe_entry(SPINDLEENTRY *entry) {
	return (int) mock(entry);
}

int spindle_index_entry(SPINDLEENTRY *entry) {
	return (int) mock(entry);
}

int spindle_entry_init(SPINDLEENTRY *entry, SPINDLEGENERATE *generate, const char *localname) {
	return (int) mock(entry, generate, localname);
}

int spindle_entry_reset(SPINDLEENTRY *entry) {
	return (int) mock(entry);
}

int spindle_entry_cleanup(SPINDLEENTRY *entry) {
	return (int) mock(entry);
}

int spindle_license_apply(SPINDLEENTRY *entry) {
	return (int) mock(entry);
}

int spindle_prop_update_entry(SPINDLEENTRY *entry) {
	return (int) mock(entry);
}

char *spindle_proxy_locate(SPINDLE *spindle, const char *uri) {
	return (char *) mock(spindle, uri);
}

int spindle_related_fetch_entry(SPINDLEENTRY *entry) {
	return (int) mock(entry);
}

int spindle_source_fetch_entry(SPINDLEENTRY *entry) {
	return (int) mock(entry);
}

int spindle_store_entry(SPINDLEENTRY *entry) {
	return (int) mock(entry);
}

int spindle_trigger_apply(SPINDLEENTRY *entry) {
	return (int) mock(entry);
}

int spindle_triggers_update(SPINDLEENTRY *entry) {
	return (int) mock(entry);
}
