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

#include <sys/time.h>

#define AWSS3BUCKET void
#define SPARQL void

struct spindle_literalstring_struct {
	char lang[8];
	char *str;
};

struct spindle_literalset_struct {
	struct spindle_literalstring_struct *literals;
	size_t nliterals;
};

typedef struct {
	struct spindle_classmap_struct *classes;
	size_t classcount;
	size_t classsize;
	struct spindle_predicatemap_struct *predicates;
	size_t predcount;
	size_t predsize;
	char **cachepreds;
	size_t cpcount;
	size_t cpsize;
	const struct coref_match_struct *match_types;
	struct coref_match_struct *coref;
	size_t corefcount;
	size_t corefsize;
} SPINDLERULES;

typedef struct {
	librdf_world *world;
	char *root;
	SPARQL *sparql;
	SQL *db;
	librdf_node *rdftype;
	librdf_node *sameas;
	librdf_node *modified;
	librdf_uri *xsd_dateTime;
	librdf_node *rootgraph;
	int multigraph;
	SPINDLERULES *rules;
	struct spindle_graphcache_struct *graphcache;
} SPINDLE;

typedef struct {
	SPINDLE *spindle;
	SPINDLERULES *rules;
	SQL *db;
	SPARQL *sparql;
	AWSS3BUCKET *bucket;
	int s3_verbose;
	char *cachepath;
	char *titlepred;
	struct spindle_predicatemap_struct *licensepred;
	struct spindle_license_struct *licenses;
	size_t nlicenses;
} SPINDLEGENERATE;

typedef struct {
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
	size_t refcount;
	time_t modified;
	int flags;
	librdf_model *rootdata;
	librdf_model *proxydata;
	librdf_model *sourcedata;
	librdf_model *extradata;
	librdf_node *graph;
	librdf_node *doc;
	librdf_node *self;
	librdf_node *sameas;
	int score;
	struct spindle_literalset_struct titleset;
	struct spindle_literalset_struct descset;
	struct spindle_strset_struct *classes;
	int has_geo;
	double lat, lon;
	size_t ntriggers;
	struct spindle_trigger_struct *triggers;
	struct spindle_strset_struct *sources;
} SPINDLEENTRY;

struct spindle_trigger_struct {
	char *uri;
	unsigned int kind;
	char *id;
};

struct spindle_strset_struct
{
	char **strings;
	unsigned *flags;
	size_t count;
	size_t size;
};

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
