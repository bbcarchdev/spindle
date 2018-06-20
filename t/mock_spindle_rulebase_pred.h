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

int spindle_rulebase_pred_add_matchnode(SPINDLERULES *rules, librdf_model *model, const char *matchuri, librdf_node *matchnode, int inverse) {
	return mock(rules, model, matchuri, matchnode, inverse);
}

int spindle_rulebase_pred_add_node(SPINDLERULES *rules, librdf_model *model, const char *uri, librdf_node *node) {
	return mock(rules, model, uri, node);
}

int spindle_rulebase_pred_cleanup(SPINDLERULES *rules) {
	return mock(rules);
}

int spindle_rulebase_pred_dump(SPINDLERULES *rules) {
	return mock(rules);
}

int spindle_rulebase_pred_finalise(SPINDLERULES *rules) {
	return mock(rules);
}
