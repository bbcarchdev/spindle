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

#ifndef P_SPINDLE_GENERATE_H_
# define P_SPINDLE_GENERATE_H_          1

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <ctype.h>
# include <time.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/time.h>
# include <sys/param.h>
# include <sys/stat.h>
# include <errno.h>
# include <libawsclient.h>
# include <libmq-engine.h>

# include "spindle-common.h"

# undef PLUGIN_NAME
# define PLUGIN_NAME                    "spindle-generate"

# define SPINDLE_URI_MIME               "application/x-spindle-uri"

# define SPINDLE_DB_INDEX_VERSION       1

/* The number of entries in the graph cache */
# define SPINDLE_GRAPHCACHE_SIZE        16

typedef struct spindle_generate_struct SPINDLEGENERATE;
typedef struct spindle_entry_struct SPINDLEENTRY;

struct spindle_generate_struct
{
	SPINDLE *spindle;
	SPINDLERULES *rules;
	/* Connection objects from the Spindle context */
	SQL *db;
	SPARQL *sparql;
	/* The bucket that precomposed N-Quads should be stored in */
	AWSS3BUCKET *bucket;
	int s3_verbose;
	/* The filesystem paths that precomposed N-Quads should be stored in */
	char *cachepath;
	/* Cached information about graphs */
	struct spindle_graphcache_struct *graphcache;
	/* Names of specific predicates */
	char *titlepred;
	struct spindle_predicatemap_struct *licensepred;
	struct spindle_license_struct *licenses;
	size_t nlicenses;
};

/* State used while generating a single proxy entry */
struct spindle_entry_struct
{
	SPINDLEGENERATE *generate;
	SPINDLE *spindle;
	SPINDLERULES *rules;
	SPARQL *sparql;
	SQL *db;
	char *graphname;
	char *docname;
	char *title;
	char *title_en;
	char *id;
	const char *localname;
	const char *classname;
	char **refs;
	time_t modified;
	int flags;
	
	/* Data which will be inserted into the root graph, always in the form
	 * <proxy> pred obj
	 */
	librdf_model *rootdata;
	/* Proxy data: all of the generated information about the proxy entity;
	 * stored in the proxy graph.
	 */
	librdf_model *proxydata;
	/* Data about the source graphs - only populated (and used) if we're
	 * stashing data in an S3 bucket
	 */
	librdf_model *sourcedata;
	/* Extra data: information about related entities, only used when caching
	 * to an S3 bucket.
	 */
	librdf_model *extradata;
	/* The name of the graph we store information in */
	librdf_node *graph;
	/* The name of the information resource which contains the proxy (will
	 * be the same as 'graph' if multigraph is true)
	 */
	librdf_node *doc;
	/* The name of the proxy, including the fragment */
	librdf_node *self;
	/* A precomposed owl:sameAs node */
	librdf_node *sameas;
	/* The proxy's prominence score */
	int score;
	/* Copies of literal predicates we need to keep */
	struct spindle_literalset_struct titleset;
	struct spindle_literalset_struct descset;
	struct spindle_strset_struct *classes;	
	/* Geographical co-ordinates */
	int has_geo;
	double lat, lon;
	/* List of URIs which trigger updates to this entity */
	size_t ntriggers;
	struct spindle_trigger_struct *triggers;
	/* List of URIs which describe this entity */
	struct spindle_strset_struct *sources;
};

struct spindle_trigger_struct
{
	char *uri;
	unsigned int kind;
};

struct spindle_graphcache_struct
{
	char *uri;
	librdf_model *model;
};

/* Information about a license */
struct spindle_license_struct
{
	char *name;
	char *title;
	char **uris;
	size_t uricount;
	int score;
};

/* Generate and index data about an entity */
int spindle_generate(SPINDLEGENERATE *generate, const char *identifier, int mode);
int spindle_generate_graph(twine_graph *graph, void *data);
int spindle_generate_message(const char *mime, const unsigned char *buf, size_t buflen, void *data);
int spindle_generate_update(const char *name, const char *identifier, void *data);

/* Initialise and release an entry's data structure */
int spindle_entry_init(SPINDLEENTRY *data, SPINDLEGENERATE *generate, const char *localname);
int spindle_entry_cleanup(SPINDLEENTRY *data);

/* Fetch source data about an entry */
int spindle_source_fetch_entry(SPINDLEENTRY *data);

/* Store information about the digital objects describing an entry */
int spindle_describe_entry(SPINDLEENTRY *data);

/* Fetch extra, related, information about an entry */
int spindle_related_fetch_entry(SPINDLEENTRY *data);

/* Store the generated data */
int spindle_store_entry(SPINDLEENTRY *entry);

/* Index an entry in a database */
int spindle_index_entry(SPINDLEENTRY *data);
int spindle_index_core(SQL *sql, const char *id, SPINDLEENTRY *data);
int spindle_index_about(SQL *sql, const char *id, SPINDLEENTRY *data);
int spindle_index_media(SQL *sql, const char *id, SPINDLEENTRY *data);
int spindle_index_membership(SQL *sql, const char *id, SPINDLEENTRY *data);
int spindle_index_audiences(SPINDLEGENERATE *generate, const char *license, char ***audiences);

/* Add a trigger URI */
int spindle_cache_trigger(SPINDLEENTRY *cache, const char *uri, unsigned int kind);

/* Generate and store pre-composed N-Quads */
int spindle_precompose_init(SPINDLEGENERATE *spindle);
int spindle_precompose_s3(SPINDLEENTRY *data, char *quadbuf, size_t bufsize);
int spindle_precompose_file(SPINDLEENTRY *data, char *quadbuf, size_t bufsize);

/* Determine the class of something (storing in cache->classname) */
int spindle_class_match(SPINDLEENTRY *cache, struct spindle_strset_struct *classes);
/* Update the classes of a proxy (updates cache->classname) */
int spindle_class_update_entry(SPINDLEENTRY *cache);

/* Update the properties of a proxy */
int spindle_prop_update_entry(SPINDLEENTRY *cache);

/* Graph cache */
int spindle_graph_description_node(SPINDLEGENERATE *generate, librdf_model *target, librdf_node *graph);

/* Update the information resource describing the proxy */
int spindle_doc_init(SPINDLEGENERATE *spindle);
int spindle_doc_apply(SPINDLEENTRY *cache);

/* Relay information about licensing */
int spindle_license_init(SPINDLEGENERATE *spindle);
int spindle_license_apply(SPINDLEENTRY *spindle);

/* SQL index */
int spindle_db_cache_store(SPINDLEENTRY *data);
int spindle_db_cache_source(SPINDLEENTRY *data);

/* MQ engine */
int spindle_mq_init(void *handle);

#endif /*!P_SPINDLE_GENERATE_H_*/
