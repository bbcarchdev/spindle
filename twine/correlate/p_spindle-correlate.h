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

#ifndef P_SPINDLE_CORRELATE_H_
# define P_SPINDLE_CORRELATE_H_         1

# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# include "spindle-common.h"

# undef PLUGIN_NAME
# define PLUGIN_NAME                    "spindle-correlate"

# define SET_BLOCKSIZE                  4

/* Perform coreference correlation on a graph */
int spindle_correlate(twine_graph *graph, void *data);

/* Matching callbacks */
int spindle_match_sameas(struct spindle_corefset_struct *set, const char *subject, const char *object);
int spindle_match_wikipedia(struct spindle_corefset_struct *set, const char *subject, const char *object);

/* Extract a list of co-references from a librdf model */
struct spindle_corefset_struct *spindle_coref_extract(SPINDLE *spindle, librdf_model *model, const char *graphuri);
/* Free the resources used by a co-reference set */
int spindle_coref_destroy(struct spindle_corefset_struct *set);

int spindle_graph_discard(SPINDLE *spindle, const char *uri);
int spindle_cache_update_set(SPINDLE *spindle, struct spindle_strset_struct *set);

#endif /*!P_SPINDLE_CORRELATE_H_*/
