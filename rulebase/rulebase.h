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

#ifndef SPINDLE_RULEBASE_H_
# define SPINDLE_RULEBASE_H_            1

# include <liburi.h>
# include <libsql.h>

# include "libtwine.h"

typedef struct spindle_rulebase_struct SPINDLERULES;

/* The rule-base object */
struct spindle_rulebase_struct
{
	/* Class-matching data */
	struct spindle_classmap_struct *classes;
	size_t classcount;
	size_t classsize;
	/* Predicate-matching data */
	struct spindle_predicatemap_struct *predicates;
	size_t predcount;
	size_t predsize;
	/* Predicates which are cached */
	char **cachepreds;
	size_t cpcount;
	size_t cpsize;
	/* Co-reference match types */
	const struct coref_match_struct *match_types;
	struct coref_match_struct *coref;
	size_t corefcount;
	size_t corefsize;
};

struct spindle_strset_struct
{
	char **strings;
	unsigned *flags;
	size_t count;
	size_t size;
};

/* Mapping data for a class. 'uri' is the full class URI which will be
 * applied to the proxy; 'match' is a list of other classes which
 * when encountered will map to this one; 'score' is the matching priority
 * for this rule; and 'prominence' specifies the prominence which will be
 * subtracted from the proxy's score if the proxy is an instance of this
 * class.
 */
struct spindle_classmap_struct
{
	char *uri;
	struct spindle_classmatch_struct *match;
	size_t matchcount;
	size_t matchsize;
	int score;
	int prominence;
	struct spindle_strset_struct *roots;
};

struct spindle_classmatch_struct
{
	char *uri;
	int prominence;
};

/* Mapping data for a predicate. 'target' is the predicate which should be
 * used in the proxy data. If 'expected' is RAPTOR_TERM_TYPE_LITERAL, then
 * 'datatype' can optionally specify a datatype which literals must conform
 * to (candidate literals must either have no datatype and language, or
 * be of the specified datatype).
 *
 * If 'expected' is RAPTOR_TERM_TYPE_URI and proxyonly is nonzero, then
 * only those candidate properties whose objects have existing proxy
 * objects within the store will be used (and the triple stored in the
 * proxy will point to the corresponding proxy instead of the original
 * URI).
 */
struct spindle_predicatemap_struct
{
	char *target;
	struct spindle_predicatematch_struct *matches;
	size_t matchcount;
	size_t matchsize;
	raptor_term_type expected;
	char *datatype;
	int indexed;
	int proxyonly;
	int score;
	int prominence;
	int inverse;
};

/* A single predicate which should be matched; optionally matching is restricted
 * to members of a particular class (the class must be defined in classes.c)
 * Priority values are 0 for 'always add', or 1..n, where 1 is highest-priority.
 */
struct spindle_predicatematch_struct
{
	int priority;
	char *predicate;
	char *onlyfor;
	int prominence;
	int inverse;
};

struct spindle_coref_struct
{
	char *left;
	char *right;
};

struct spindle_corefset_struct
{
	struct spindle_coref_struct *refs;
	size_t refcount;
	size_t size;
};

struct coref_match_struct
{
	const char *predicate;
	int (*callback)(struct spindle_corefset_struct *set, const char *subject, const char *object);
};

/* Load and parse the rulebase */
SPINDLERULES *spindle_rulebase_create(void);
/* Set the list of matching callbacks used by the correlation engine */
int spindle_rulebase_set_matchtypes(SPINDLERULES *rules, const struct coref_match_struct *match_types);
/* Add the statements from a librdf_model to a rulebase */
int spindle_rulebase_add_model(SPINDLERULES *rules, librdf_model *model, const char *sourcefile);
/* Add a buffer containing Turtle to the rulebase */
int spindle_rulebase_add_turtle(SPINDLERULES *rules, const char *buf, const char *sourcefile);
/* Add the statements from a Turtle file to the rulebase */
int spindle_rulebase_add_file(SPINDLERULES *rules, const char *path);
/* Add all of the rules from the default file(s) */
int spindle_rulebase_add_config(SPINDLERULES *rules);
/* Once all rules have been loaded, finalise the rulebase so that it can be
 * used to process source graphs.
 */
int spindle_rulebase_finalise(SPINDLERULES *rules);
/* Free resources used by the rulebase */
int spindle_rulebase_destroy(SPINDLERULES *rules);
/* Dump the contents of the loaded rulebase */
int spindle_rulebase_dump(SPINDLERULES *rules);

/* Create an empty string-set */
struct spindle_strset_struct *spindle_strset_create(void);
/* Add a string to a string-set */
int spindle_strset_add(struct spindle_strset_struct *set, const char *str);
int spindle_strset_add_flags(struct spindle_strset_struct *set, const char *str, unsigned flags);
/* Free the resources used by a string set */
int spindle_strset_destroy(struct spindle_strset_struct *set);

#endif /*!SPINDLE_RULEBASE_H_*/
