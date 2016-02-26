/* Spindle: Co-reference aggregation engine
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2015 BBC
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

#ifndef P_SPINDLE_H_
# define P_SPINDLE_H_                   1

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <sys/stat.h>
# include <ctype.h>
# include <errno.h>
# include <uuid/uuid.h>

# include "spindle-common.h"

/* The name of this plug-in */
# define PLUGIN_NAME                    "spindle"

/* The number of co-references allocated at a time when extending a set */
# define SET_BLOCKSIZE                  4

/* Internal rule-base processing */
int spindle_rulebase_class_add_node(SPINDLERULES *rules, librdf_model *model, const char *uri, librdf_node *node);
int spindle_rulebase_class_add_matchnode(SPINDLERULES *rules, librdf_model *model, const char *matchuri, librdf_node *node);
int spindle_rulebase_class_finalise(SPINDLERULES *rules);
int spindle_rulebase_class_cleanup(SPINDLERULES *rules);

int spindle_rulebase_pred_add_node(SPINDLERULES *rules, librdf_model *model, const char *uri, librdf_node *node);
int spindle_rulebase_pred_add_matchnode(SPINDLERULES *rules, librdf_model *model, const char *matchuri, librdf_node *matchnode, int inverse);
int spindle_rulebase_pred_finalise(SPINDLERULES *rules);
int spindle_rulebase_pred_cleanup(SPINDLERULES *rules);

int spindle_rulebase_cachepred_add(SPINDLERULES *rules, const char *uri);
int spindle_rulebase_cachepred_finalise(SPINDLERULES *rules);
int spindle_rulebase_cachepred_cleanup(SPINDLERULES *rules);

int spindle_rulebase_coref_add_node(SPINDLERULES *rules, const char *predicate, librdf_node *node);

/* Database schema update */
int spindle_db_schema_update_(SPINDLE *spindle);

/* RDBMS-based correlation */
char *spindle_db_proxy_locate(SPINDLE *spindle, const char *uri);
int spindle_db_proxy_relate(SPINDLE *spindle, const char *remote, const char *local);
char **spindle_db_proxy_refs(SPINDLE *spindle, const char *uri);
int spindle_db_proxy_migrate(SPINDLE *spindle, const char *from, const char *to, char **refs);

#endif /*!P_SPINDLE_H_*/
