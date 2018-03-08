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

# define SPINDLE_DB_INDEX_VERSION       2

typedef struct spindle_generate_struct SPINDLEGENERATE;
typedef struct spindle_entry_struct SPINDLEENTRY;

struct spindle_generate_struct
{
	SPINDLE *spindle;
	RULEBASE *rules;
	/* Connection objects from the Spindle context */
	SQL *db;
	SPARQL *sparql;
	/* The bucket that precomposed N-Quads should be stored in */
	AWSS3BUCKET *bucket;
	int s3_verbose;
	/* The filesystem paths that precomposed N-Quads should be stored in */
	char *cachepath;
	/* Names of specific predicates */
	char *titlepred;
	struct rulebase_predicatemap_struct *licensepred;
	struct spindle_license_struct *licenses;
	size_t nlicenses;
	/* Should creative works be 'about' themselves? */
	int aboutself;
	/* Should we add POWDER describedby statements to the proxy graph? */
	int describedby;
	/* Should we consider references to be descriptive?
	 * i.e., do we treat graphs containing statements whose objects are
	 * one of our corefs to be 'source data'?
	 */
	int describeinbound;
};

/* State used while generating a single proxy entry */
struct spindle_entry_struct
{
	SPINDLEGENERATE *generate;
	SPINDLE *spindle;
	PROXYENTRY *proxy;
	SPARQL *sparql;
	SQL *db;
	char *graphname;
	char *docname;
	char *id;
	time_t modified;
	int flags;

	/* The name of the information resource which contains the proxy (will
	 * be the same as 'graph' if multigraph is true)
	 */
	librdf_node *doc;
	/* A precomposed owl:sameAs node */
	librdf_node *sameas;
	/* List of URIs which trigger updates to this entity */
	size_t ntriggers;
	struct spindle_trigger_struct *triggers;
	/* List of URIs which describe this entity */
	struct strset_struct *sources;
};

struct spindle_trigger_struct
{
	char *uri;
	unsigned int kind;
	char *id;
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
int spindle_entry_reset(SPINDLEENTRY *data);
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
int spindle_index_audiences_licence(SQL *sql, const char *id, SPINDLEENTRY *data);
int spindle_index_audiences(SPINDLEGENERATE *generate, const char *license, const char *mediaid, const char *mediauri, const char *mediakind, const char *mediatype, const char *duration);

/* Cached N-Quads handling */
int spindle_cache_init(SPINDLEGENERATE *spindle);
int spindle_cache_store(SPINDLEENTRY *data, const char *suffix, librdf_model *model);
int spindle_cache_store_buf(SPINDLEENTRY *data, const char *suffix, char *quadbuf, size_t bufsize);
int spindle_cache_fetch(SPINDLEENTRY *data, const char *suffix, librdf_model *destmodel);

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

/* Add a new trigger */
int spindle_trigger_add(SPINDLEENTRY *cache, const char *uri, unsigned int kind, const char *id);
/* Apply triggers referring to this entity to any others */
int spindle_trigger_apply(SPINDLEENTRY *entry);
/* Add triggers to the database */
int spindle_triggers_index(SQL *sql, const char *id, SPINDLEENTRY *data);
/* Update any triggers which have one of our URIs */
int spindle_triggers_update(SPINDLEENTRY *data);

#endif /*!P_SPINDLE_GENERATE_H_*/
