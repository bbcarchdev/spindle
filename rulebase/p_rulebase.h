/* Spindle: Co-reference aggregation engine
 *
 * Copyright (c) 2018 BBC
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

#ifndef P_RULEBASE_H_
# define P_RULEBASE_H_                  1

# include <liburi.h>
# include <libsql.h>

# include "libtwine.h"

/* Internal rule-base processing */
int spindle_rulebase_class_add_node(SPINDLERULES *rules, librdf_model *model, const char *uri, librdf_node *node);
int spindle_rulebase_class_add_matchnode(SPINDLERULES *rules, librdf_model *model, const char *matchuri, librdf_node *node);
int spindle_rulebase_class_finalise(SPINDLERULES *rules);
int spindle_rulebase_class_cleanup(SPINDLERULES *rules);
int spindle_rulebase_class_dump(SPINDLERULES *rules);

int spindle_rulebase_pred_add_node(SPINDLERULES *rules, librdf_model *model, const char *uri, librdf_node *node);
int spindle_rulebase_pred_add_matchnode(SPINDLERULES *rules, librdf_model *model, const char *matchuri, librdf_node *matchnode, int inverse);
int spindle_rulebase_pred_finalise(SPINDLERULES *rules);
int spindle_rulebase_pred_cleanup(SPINDLERULES *rules);
int spindle_rulebase_pred_dump(SPINDLERULES *rules);

int spindle_rulebase_cachepred_add(SPINDLERULES *rules, const char *uri);
int spindle_rulebase_cachepred_finalise(SPINDLERULES *rules);
int spindle_rulebase_cachepred_cleanup(SPINDLERULES *rules);
int spindle_rulebase_cachepred_dump(SPINDLERULES *rules);

int spindle_rulebase_coref_add_node(SPINDLERULES *rules, const char *predicate, librdf_node *node);
int spindle_rulebase_coref_dump(SPINDLERULES *rules);

#endif /*!P_RULEBASE_H_*/
