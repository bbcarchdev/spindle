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

#ifndef SPINDLE_COMMON_H_
# define SPINDLE_COMMON_H_              1

# include <liburi.h>
# include <libsql.h>

# include "libtwine.h"

/* Flags on strsets */
# define SF_NONE                        0
# define SF_MOVED                       (1<<0)
# define SF_UPDATED                     (1<<1)
# define SF_REFRESHED                   (1<<2)
# define SF_DONE                        (1<<3)

/* Trigger kinds */
# define TK_PROXY                       (1<<0)
# define TK_TOPICS                      (1<<1)
# define TK_MEDIA                       (1<<2)
# define TK_MEMBERSHIP                  (1<<3)

/* Namespaces */
# define NS_RDF                         "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
# define NS_XSD                         "http://www.w3.org/2001/XMLSchema#"
# define NS_RDFS                        "http://www.w3.org/2000/01/rdf-schema#"
# define NS_FOAF                        "http://xmlns.com/foaf/0.1/"
# define NS_POWDER                      "http://www.w3.org/2007/05/powder-s#"
# define NS_MRSS                        "http://search.yahoo.com/mrss/"
# define NS_OWL                         "http://www.w3.org/2002/07/owl#"
# define NS_SPINDLE                     "http://bbcarchdev.github.io/ns/spindle#"
# define NS_OLO                         "http://purl.org/ontology/olo/core#"
# define NS_DCTERMS                     "http://purl.org/dc/terms/"
# define NS_GEO                         "http://www.w3.org/2003/01/geo/wgs84_pos#"
# define NS_DCMITYPE                    "http://purl.org/dc/dcmitype/"
# define NS_MIME                        "http://purl.org/NET/mediatypes/"
# define NS_ODRL                        "http://www.w3.org/ns/odrl/2/"
# define NS_EVENT                       "http://purl.org/NET/c4dm/event.owl#"

/* MIME types */
# define MIME_TURTLE                    "text/turtle"

typedef struct spindle_context_struct SPINDLE;
typedef struct spindle_rulebase_struct SPINDLERULES;

struct spindle_context_struct
{
	/* The librdf execution context from Twine */
	librdf_world *world;
	/* The URI of our root graph, and prefix for proxy entities */
	char *root;
	/* The SPARQL connection handle from Twine */
	SPARQL *sparql;
	/* The RDBMS connection */
	SQL *db;
	/* rdf:type */
	librdf_node *rdftype;
	/* owl:sameAs */
	librdf_node *sameas;
	/* dct:modified */
	librdf_node *modified;
	/* xsd:dateTime */
	librdf_uri *xsd_dateTime;
	/* The root URI as a librdf_node */
	librdf_node *rootgraph;
	/* Whether to store each proxy in its own graph */
	int multigraph;
	/* The rulebase */
	SPINDLERULES *rules;
};

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

/* A set of literal strings */
struct spindle_literalset_struct
{
	struct spindle_literalstring_struct *literals;
	size_t nliterals;
};

struct spindle_literalstring_struct
{
	char lang[8];
	char *str;
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

/* Initialise a spindle context object */
int spindle_init(SPINDLE *spindle);
/* Free resources used by a spindle context object */
int spindle_cleanup(SPINDLE *spindle);

/* Load and parse the rulebase */
SPINDLERULES *spindle_rulebase_create(const char *path, const struct coref_match_struct *match_types);
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

/* Utility functions used by SQL interaction code */
int spindle_db_init(SPINDLE *spindle);
int spindle_db_cleanup(SPINDLE *spindle);
int spindle_db_local(SPINDLE *spindle, const char *localname);
char *spindle_db_id(const char *localname);
char *spindle_db_literalset(struct spindle_literalset_struct *set);
char *spindle_db_strset(struct spindle_strset_struct *set);
size_t spindle_db_esclen(const char *src);
char *spindle_db_escstr(char *dest, const char *src);
char *spindle_db_escstr_lower(char *dest, const char *src);

/* Assert that two URIs are equivalent */
int spindle_proxy_create(SPINDLE *spindle, const char *uri1, const char *uri2, struct spindle_strset_struct *changeset);
/* Generate a new local URI for an external URI */
char *spindle_proxy_generate(SPINDLE *spindle, const char *uri);
/* Look up the local URI for an external URI in the store */
char *spindle_proxy_locate(SPINDLE *spindle, const char *uri);
/* Move a set of references from one proxy to another */
int spindle_proxy_migrate(SPINDLE *spindle, const char *from, const char *to, char **refs);
/* Store a relationship between a proxy and an external entity */
int spindle_proxy_relate(SPINDLE *spindle, const char *remote, const char *proxy);
/* Obtain all of the outbound references (related external entity URIs) from a proxy */
char **spindle_proxy_refs(SPINDLE *spindle, const char *uri);
/* Destroy a list of references */
void spindle_proxy_refs_destroy(char **refs);

#endif /*!SPINDLE_COMMON_H_*/
