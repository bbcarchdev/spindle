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

SPINDLERULES *spindle_rulebase_create(void) {
	return (SPINDLERULES *) mock();
}

int spindle_rulebase_finalise(SPINDLERULES *rules) {
	return mock(rules);
}

int spindle_rulebase_destroy(SPINDLERULES *rules) {
	return mock(rules);
}

int spindle_rulebase_dump(SPINDLERULES *rules) {
	return mock(rules);
}

int spindle_rulebase_add_config(SPINDLERULES *rules) {
	return mock(rules);
}

int spindle_rulebase_set_matchtypes(SPINDLERULES *rules, const struct coref_match_struct *match_types) {
	return mock(rules, match_types);
}
