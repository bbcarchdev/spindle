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

int
spindle_strset_add(struct spindle_strset_struct *set, const char *str) {
	return (int) mock(set, str);
}

struct spindle_strset_struct *spindle_strset_create(void) {
	return (struct spindle_strset_struct *) mock();
}

int spindle_strset_destroy(struct spindle_strset_struct *set) {
	return (int) mock(set);
}
