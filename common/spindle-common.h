/* Spindle: Co-reference aggregation engine
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2017 BBC
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

# include <libsql.h>
# include <libsparqlclient.h>
# include <libtwine.h>
# include <rulebase/librulebase.h>

/* The number of co-references allocated at a time when extending a set */
# define REFLIST_BLOCKSIZE              4

/* Trigger kinds */
# define TK_PROXY                       (1<<0)
# define TK_TOPICS                      (1<<1)
# define TK_MEDIA                       (1<<2)
# define TK_MEMBERSHIP                  (1<<3)

typedef struct spindle_context_struct SPINDLE;

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
	RULEBASE *rules;
	/* Cached information about graphs */
	struct spindle_graphcache_struct *graphcache;
};

struct spindle_graphcache_struct
{
	char *uri;
	librdf_model *model;
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

/* Initialise a spindle context object */
int spindle_init(SPINDLE *spindle);
/* Free resources used by a spindle context object */
int spindle_cleanup(SPINDLE *spindle);

/* Utility functions used by SQL interaction code */
int spindle_db_init(SPINDLE *spindle);
int spindle_db_cleanup(SPINDLE *spindle);
int spindle_db_local(SPINDLE *spindle, const char *localname);
char *spindle_db_id(const char *localname);
int spindle_db_id_copy(char *dest, const char *localname);
char *spindle_db_literalset(struct spindle_literalset_struct *set);
char *spindle_db_strset(struct strset_struct *set);
size_t spindle_db_esclen(const char *src);
char *spindle_db_escstr(char *dest, const char *src);
char *spindle_db_escstr_lower(char *dest, const char *src);

/* Assert that two URIs are equivalent */
int spindle_proxy_create(SPINDLE *spindle, const char *uri1, const char *uri2, struct strset_struct *changeset);
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

/* Retrieve the contents of a graph */
librdf_model *spindle_graphcache_fetch_node(SPINDLE *spindle, librdf_node *graph);
/* Discard a graph */
int spindle_graphcache_discard(SPINDLE *spindle, const char *uri);
/* Copy a description of a graph */
int spindle_graphcache_description_node(SPINDLE *spindle, librdf_model *target, librdf_node *graph);

#endif /*!SPINDLE_COMMON_H_*/
