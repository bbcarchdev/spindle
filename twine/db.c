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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_spindle.h"

#if SPINDLE_DB_INDEX || SPINDLE_DB_PROXIES
static int spindle_db_local_(SPINDLE *spindle, const char *localname);
static char *spindle_db_id_(const char *localname);
static char *spindle_db_literalset_(struct spindle_literalset_struct *set);
static char *spindle_db_strset_(struct spindle_strset_struct *set);
static size_t spindle_db_esclen_(const char *src);
static char *spindle_db_escstr_(char *dest, const char *src);
static char *spindle_db_escstr_lower_(char *dest, const char *src);
#endif

#if SPINDLE_DB_INDEX
static int spindle_db_remove_(SQL *sql, const char *id);
static int spindle_db_add_(SQL *sql, const char *id, SPINDLECACHE *data);
static int spindle_db_langindex_(SQL *sql, const char *id, const char *target, const char *specific, const char *generic);
static int spindle_db_topics_(SQL *sql, const char *id, SPINDLECACHE *data);
static int spindle_db_media_(SQL *sql, const char *id, SPINDLECACHE *data);
static int spindle_db_media_refs_(SQL *sql, const char *id, SPINDLECACHE *data);
static char *spindle_db_media_kind_(SPINDLECACHE *data);
static char *spindle_db_media_type_(SPINDLECACHE *data);
static char *spindle_db_media_license_(SPINDLECACHE *data);
static int spindle_db_audiences_(SPINDLE *spindle, const char *license, char ***audiences);
static int spindle_db_audiences_interp_(SPINDLE *spindle, librdf_model *model, librdf_node *subject, char ***audiences);
static int spindle_db_audiences_permission_(SPINDLE *spindle, librdf_model *model, librdf_node *subject, char ***audiences);
static int spindle_db_audiences_action_(SPINDLE *spindle, librdf_model *model, librdf_node *subject);
static int spindle_db_audiences_assignee_(SPINDLE *spindle, librdf_model *model, librdf_node *subject, char ***audiences);
#endif

#if SPINDLE_DB_PROXIES
struct relate_struct
{
	SPINDLE *spindle;
	const char *id;
	const char *uri;
};


static int spindle_db_perform_proxy_relate_(SQL *restrict db, void *restrict userdata);
#endif

#if SPINDLE_DB_INDEX

/* Store an index entry in a PostgreSQL (or compatible) database for a proxy.
 *
 * The 'index' table has a core set of fields:-
 *
 *  id       The proxy's UUID
 *  version  The index version (SPINDLE_DB_INDEX_VERSION)
 *  modified The last-modification date
 *  classes  A list of class URIs for this proxy
 *  score    The calculated prominence score for this proxy
 *
 * In addition, the index stores:
 *
 *  title        (v1) An hstore of language/title pairs
 *  description  (v1) An hstore of language/description pairs
 *  coordinates  (v1) A point specifying latitude/longitude
 *  index_en_gb  (v1) The en-gb (British English) full-text index
 *  index_cy_gb  (v1) The cy-gb (Welsh) full-text index
 *  index_gd_gb  (v1) The gd-gb (Scottish Gaelic) full-text index
 *  index_ga_gb  (v1) The ga-gb (Irish Gaelic) full-text index
 *
 * The 'about' table tracks topicality references:
 *
 *  id       The UUID of an entity being cached
 *  about    The UUID of another reference this entity is about
 *
 * The 'media' table tracks information about media items (digital assets):
 *
 *  id       The UUID of the digital asset
 *  uri      The complete URI of the digital asset
 *  class    The DCMIT member for this asset (:Image, :Sound, etc.)
 *  type     The MIME type of the asset
 *  audience If known to be restricted, the URI of the audience it's
 *           restricted to.
 */

int
spindle_db_cache_store(SPINDLECACHE *data)
{
	SQL *sql;
	char *id;

	sql = data->spindle->db;
	id = spindle_db_id_(data->localname);
	if(!id)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": DB: ID is '%s'\n", id);
	if(spindle_db_remove_(sql, id))
	{
		free(id);
		return -1;
	}
	if(spindle_db_add_(sql, id, data))
	{
		free(id);
		return -1;
	}
	if(spindle_db_langindex_(sql, id, "en_gb", "en-gb", "en"))
	{
		free(id);
		return -1;
	}
	if(spindle_db_langindex_(sql, id, "cy_gb", "cy-gb", "cy"))
	{
		free(id);
		return -1;
	}
	if(spindle_db_langindex_(sql, id, "ga_gb", "ga-gb", "ga"))
	{
		free(id);
		return -1;
	}
	if(spindle_db_langindex_(sql, id, "gd_gb", "gd-gb", "gd"))
	{
		free(id);
		return -1;
	}
	if(spindle_db_topics_(sql, id, data))
	{
		free(id);
		return -1;
	}
	if(spindle_db_media_(sql, id, data))
	{
		free(id);
		return -1;
	}
	free(id);
	return 0;
}

static int
spindle_db_remove_(SQL *sql, const char *id)
{
	if(sql_executef(sql, "DELETE FROM \"index\" WHERE \"id\" = %Q",
					id))
	{
		return -1;
	}
	if(sql_executef(sql, "DELETE FROM \"about\" WHERE \"id\" = %Q",
					id))
	{
		return -1;
	}
	if(sql_executef(sql, "DELETE FROM \"media\" WHERE \"id\" = %Q",
					id))
	{
		return -1;
	}
	if(sql_executef(sql, "DELETE FROM \"index_media\" WHERE \"id\" = %Q",
					id))
	{
		return -1;
	}
	if(sql_executef(sql, "DELETE FROM \"membership\" WHERE \"id\" = %Q",
					id))
	{
		return -1;
	}
	return 0;
}

static int
spindle_db_add_(SQL *sql, const char *id, SPINDLECACHE *data)
{
	const char *t;
	char *title, *desc, *classes;
	char lbuf[64];
	int r;

	title = spindle_db_literalset_(&(data->titleset));
	desc = spindle_db_literalset_(&(data->descset));
	classes = spindle_db_strset_(data->classes);
	if(data->has_geo)
	{
		snprintf(lbuf, sizeof(lbuf), "(%f, %f)", data->lat, data->lon);
		t = lbuf;
	}
	else
	{
		t = NULL;
	}
	r = sql_executef(sql, "INSERT INTO \"index\" (\"id\", \"version\", \"modified\", \"score\", \"title\", \"description\", \"coordinates\", \"classes\") VALUES (%Q, %d, now(), %d, %Q, %Q, %Q, %Q)",
					 id, SPINDLE_DB_INDEX_VERSION, data->score, title, desc, t, classes);
	
	free(title);
	free(desc);
	free(classes);
	if(r)
	{
		return -1;
	}
	return 0;
}

/* Add information about foaf:topic relationships from this entity to
 * others.
 */
static int
spindle_db_topics_(SQL *sql, const char *id, SPINDLECACHE *data)
{
	librdf_statement *query;
	librdf_statement *st;
	librdf_stream *stream;
	librdf_node *node;
	librdf_uri *uri;
	const char *uristr;
	char *tid;
	int r;
	size_t c;

	const char *predlist[] = 
	{
		NS_FOAF "topic",
		NS_EVENT "factor",
		NULL
	};
	
	if(!(query = librdf_new_statement(data->spindle->world)))
	{
		return -1;
	}
	if(!(node = librdf_new_node_from_node(data->self)))
	{
		librdf_free_statement(query);
		return -1;
	}
	librdf_statement_set_subject(query, node);
	if(!(node = librdf_new_node_from_uri_string(data->spindle->world, (const unsigned char *) NS_FOAF "topic")))
	{
		librdf_free_statement(query);
		return -1;
	}
	librdf_statement_set_predicate(query, node);
	r = 0;
	for(stream = librdf_model_find_statements_in_context(data->proxydata, query, data->graph); !librdf_stream_end(stream); librdf_stream_next(stream))
	{
		st = librdf_stream_get_object(stream);
		if(!(node = librdf_statement_get_predicate(st)) ||
		   !(uri = librdf_node_get_uri(node)) ||
		   !(uristr = (const char *) librdf_uri_as_string(uri)))
		{
			continue;
		}
		/* XXX use rulebase */
		for(c = 0; predlist[c]; c++)
		{
			if(!strcmp(predlist[c], uristr))
			{
				break;
			}
		}
		if(!predlist[c])
		{
			continue;
		}			 
		if((node = librdf_statement_get_object(st)) &&
		   librdf_node_is_resource(node) &&
		   (uri = librdf_node_get_uri(node)) &&
		   (uristr = (const char *) librdf_uri_as_string(uri)))
		{
			tid = spindle_db_id_(uristr);
			if(!tid)
			{
				continue;
			}
			if(sql_executef(sql, "INSERT INTO \"about\" (\"id\", \"about\") VALUES (%Q, %Q)", id, tid))
			{
				free(tid);
				r = -1;
				break;
			}
			free(tid);			
		}
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	return r;
}

/* Add information about references from this entity to related digital objects
 */
static int
spindle_db_media_refs_(SQL *sql, const char *id, SPINDLECACHE *data)
{
	librdf_statement *query;
	librdf_statement *st;
	librdf_stream *stream;
	librdf_node *node;
	librdf_uri *uri;
	const char *uristr;
	char *tid, *localid;
	int r;

	if(!(query = librdf_new_statement(data->spindle->world)))
	{
		return -1;
	}
	if(!(node = librdf_new_node_from_node(data->self)))
	{
		librdf_free_statement(query);
		return -1;
	}
	librdf_statement_set_subject(query, node);
	r = 0;
	for(stream = librdf_model_find_statements_in_context(data->proxydata, query, data->graph); !librdf_stream_end(stream); librdf_stream_next(stream))
	{
		if(!(st = librdf_stream_get_object(stream))) continue;
		if(!(node = librdf_statement_get_predicate(st))) continue;
		if(!librdf_node_is_resource(node)) continue;
		if(!(uri = librdf_node_get_uri(node))) continue;
		if(!(uristr = (const char *) librdf_uri_as_string(uri))) continue;
		/* XXX this should be controlled by the rulebase or config */
		if(strcmp(uristr, NS_FOAF "page") &&
		   strcmp(uristr, NS_MRSS "player") &&
		   strcmp(uristr, NS_MRSS "content"))
		{
			continue;
		}
		if(!(node = librdf_statement_get_object(st))) continue;
		if(!librdf_node_is_resource(node)) continue;
		if(!(uri = librdf_node_get_uri(node))) continue;
		if(!(uristr = (const char *) librdf_uri_as_string(uri))) continue;
		localid = NULL;
		if(!spindle_db_local_(data->spindle, uristr))
		{
			localid = spindle_proxy_locate(data->spindle, uristr);
			if(!localid)
			{
				continue;
			}
			uristr = localid;
		}
		if(!(tid = spindle_db_id_(uristr)))
		{
			free(localid);
			continue;
		}
		if(sql_executef(sql, "INSERT INTO \"index_media\" (\"id\", \"media\") VALUES (%Q, %Q)", id, tid))
		{
			free(tid);
			free(localid);
			r = -1;
			break;
		}
		free(tid);
		free(localid);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	return r;
}

/* If this entity is a digital object, store information about it the "media"
 * database table.
 */
static int
spindle_db_media_(SQL *sql, const char *id, SPINDLECACHE *data)
{
	size_t c;
	char **refs, **audiences;
	char *kind, *type, *license;   
	int r;

	if(!data->classname || strcmp(data->classname, NS_FOAF "Document"))
	{
		/* This isn't a digital asset */
		return spindle_db_media_refs_(sql, id, data);
	}
	/* Find the source URI (there can be only one) */
	refs = spindle_proxy_refs(data->spindle, data->localname);
	if(!refs)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to retrieve co-reference set for digital object\n");
		return -1;
	}
	for(c = 0; refs[c]; c++) { }
	if(c != 1)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": will not track media for a digital object with multiple co-references\n");
		for(c = 0; refs[c]; c++)
		{
			free(refs[c]);
		}
		free(refs[c]);
		return 0;
	}
	kind = spindle_db_media_kind_(data);
	if(!kind)
	{
		free(refs[0]);
		free(refs);
		return -1;
	}
	type = spindle_db_media_type_(data);
	
	license = spindle_db_media_license_(data);
	if(license)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": license URI for <%s> is <%s>\n", refs[0], license);
		if(spindle_db_audiences_(data->spindle, license, &audiences))
		{
			free(license);
			free(type);
			free(refs[0]);
			free(refs);
			return -1;
		}
	}
	else
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": media object <%s> has no license\n", refs[0]);
		audiences = NULL;
	}
	if(!audiences || audiences[0])
	{
		r = sql_executef(sql, "INSERT INTO \"index_media\" (\"id\", \"media\") VALUES (%Q, %Q)", id, id);
	}
	if(!audiences)
	{		
		sql_executef(sql, "INSERT INTO \"media\" (\"id\", \"uri\", \"class\", \"type\", \"audience\") VALUES (%Q, %Q, %Q, %Q, %Q)",
					 id, refs[0], kind, type, NULL);
	}
	else
	{
		for(c = 0; audiences[c]; c++)
		{
			/* For each target audience (based upon ODRL descriptions), add
			 * a row to the media table.
			 */
			r = sql_executef(sql, "INSERT INTO \"media\" (\"id\", \"uri\", \"class\", \"type\", \"audience\") VALUES (%Q, %Q, %Q, %Q, %Q)",
							 id, refs[0], kind, type, audiences[c]);
			free(audiences[c]);
		}
		free(audiences);
	}
	free(refs[0]);
	free(refs);
	free(kind);
	free(type);
	free(license);
	return r;
}

/* Determine the kind of object we're dealing with
 * XXX should be driven by the rulebase
 */
static char *
spindle_db_media_kind_(SPINDLECACHE *data)
{
	librdf_statement *query;
	librdf_statement *st;
	librdf_stream *stream;
	librdf_node *node;
	librdf_uri *uri;
	const char *uristr;
	char *kind;

	kind = NULL;
	if(!(query = librdf_new_statement(data->spindle->world)))
	{
		return NULL;
	}
	if(!(node = librdf_new_node_from_node(data->self)))
	{
		librdf_free_statement(query);
		return NULL;
	}
	librdf_statement_set_subject(query, node);
	if(!(node = librdf_new_node_from_uri_string(data->spindle->world, (const unsigned char *) NS_RDF "type")))
	{
		librdf_free_statement(query);
		return NULL;
	}
	librdf_statement_set_predicate(query, node);
	for(stream = librdf_model_find_statements_in_context(data->proxydata, query, data->graph); !librdf_stream_end(stream); librdf_stream_next(stream))
	{
		st = librdf_stream_get_object(stream);
		if((node = librdf_statement_get_object(st)) &&
		   librdf_node_is_resource(node) &&
		   (uri = librdf_node_get_uri(node)) &&
		   (uristr = (const char *) librdf_uri_as_string(uri)))
		{
			if(!strncmp(uristr, NS_DCMITYPE, strlen(NS_DCMITYPE)))
			{
				kind = strdup(uristr);
				if(!kind)
				{
					librdf_free_stream(stream);
					librdf_free_statement(query);
					return NULL;
				}
				break;
			}
		}
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);	
	if(!kind)
	{
		kind = strdup(NS_FOAF "Document");
		if(!kind)
		{
			return NULL;
		}
	}
	return kind;
}

/* Determine the MIME type of object we're dealing with */
static char *
spindle_db_media_type_(SPINDLECACHE *data)
{
	librdf_statement *query;
	librdf_statement *st;
	librdf_stream *stream;
	librdf_node *node;
	librdf_uri *uri;
	const char *uristr;
	char *type;
	size_t l;

	type = NULL;
	if(!(query = librdf_new_statement(data->spindle->world)))
	{
		return NULL;
	}
	if(!(node = librdf_new_node_from_node(data->self)))
	{
		librdf_free_statement(query);
		return NULL;
	}
	librdf_statement_set_subject(query, node);
	if(!(node = librdf_new_node_from_uri_string(data->spindle->world, (const unsigned char *) NS_DCTERMS "format")))
	{
		librdf_free_statement(query);
		return NULL;
	}
	librdf_statement_set_predicate(query, node);
	l = strlen(NS_MIME);
	for(stream = librdf_model_find_statements_in_context(data->proxydata, query, data->graph); !librdf_stream_end(stream); librdf_stream_next(stream))
	{
		st = librdf_stream_get_object(stream);
		if((node = librdf_statement_get_object(st)) &&
		   librdf_node_is_resource(node) &&
		   (uri = librdf_node_get_uri(node)) &&
		   (uristr = (const char *) librdf_uri_as_string(uri)))
		{
			if(!strncmp(uristr, NS_MIME, strlen(NS_MIME)) &&
			   strlen(uristr) > l)
			{
				type = strdup(uristr + l);
				librdf_free_stream(stream);
				librdf_free_statement(query);
				return type;
			}
		}
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);	
	return NULL;
}

/* Determine the URI of the licensing statement used by this object */
static char *
spindle_db_media_license_(SPINDLECACHE *data)
{
	librdf_statement *query;
	librdf_statement *st;
	librdf_stream *stream;
	librdf_node *node;
	librdf_uri *uri;
	const char *uristr;
	char *license;
	size_t l;

	license = NULL;
	if(!(query = librdf_new_statement(data->spindle->world)))
	{
		return NULL;
	}
	if(!(node = librdf_new_node_from_node(data->self)))
	{
		librdf_free_statement(query);
		return NULL;
	}
	librdf_statement_set_subject(query, node);
	if(!(node = librdf_new_node_from_uri_string(data->spindle->world, (const unsigned char *) NS_DCTERMS "rights")))
	{
		librdf_free_statement(query);
		return NULL;
	}
	librdf_statement_set_predicate(query, node);
	l = strlen(NS_MIME);
	for(stream = librdf_model_find_statements_in_context(data->proxydata, query, data->graph); !librdf_stream_end(stream); librdf_stream_next(stream))
	{
		st = librdf_stream_get_object(stream);
		if((node = librdf_statement_get_object(st)) &&
		   librdf_node_is_resource(node) &&
		   (uri = librdf_node_get_uri(node)) &&
		   (uristr = (const char *) librdf_uri_as_string(uri)))
		{
			license = strdup(uristr);
			return license;
		}
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);	
	return NULL;
}

/* Determine who can access a digital object based upon an ODRL description */
static int
spindle_db_audiences_(SPINDLE *spindle, const char *license, char ***audiences)
{
	librdf_node *node;
	librdf_model *model;
	URI *uri;
	URI_INFO *info;
	char *base, *p;
	int r;

	*audiences = NULL;
	model = twine_rdf_model_create();
	if(!model)
	{
		return -1;
	}
	node = twine_rdf_node_createuri(license);
	if(!node)
	{
		twine_rdf_model_destroy(model);
		return -1;
	}
	uri = uri_create_str(license, NULL);
	if(!uri)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to parse <%s> as a URI\n", license);
		twine_rdf_node_destroy(node);
		twine_rdf_model_destroy(model);
		return -1;
	}
	info = uri_info(uri);
	if(!info)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to retrieve URI information about <%s>\n", license);
		uri_destroy(uri);
		twine_rdf_node_destroy(node);
		twine_rdf_model_destroy(model);
		return -1;
	}
	base = (char *) calloc(1, (info->scheme ? strlen(info->scheme) + 3 : 0) +
						   (info->auth ? strlen(info->auth) + 1 : 0) +
						   (info->host ? strlen(info->host) : 0) + 1 +
						   (info->port ? 64 : 0) + 2);
	if(!base)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to allocate buffer for base URI\n", license);
		uri_info_destroy(info);
		uri_destroy(uri);
		twine_rdf_node_destroy(node);
		twine_rdf_model_destroy(model);
		return -1;
	}		
	p = base;
	if(info->scheme)
	{
		strcpy(p, info->scheme);
		p = strchr(p, 0);
		*p = ':';
		p++;
		*p = '/';
		p++;
		*p = '/';
		p++;
	}
	if(info->auth)
	{
		strcpy(p, info->auth);
		p = strchr(p, 0);
		*p = '@';
		p++;
	}
	if(info->host)
	{
		strcpy(p, info->host);
		p = strchr(p, 0);
		if(info->port)
		{
			*p = ':';
			p++;
			p += snprintf(p, 64, "%d", info->port);		   
		}

	}
	*p = '/';
	p++;
	*p = 0;
	uri_info_destroy(info);
	uri_destroy(uri);
	r = sparql_queryf_model(spindle->sparql, model,
							"SELECT DISTINCT ?s ?p ?o\n"
							" WHERE {\n"
							"  GRAPH ?g {\n"
							"   %V ?p1 ?o1 .\n"
							"   ?s ?p ?o .\n"
							"  }\n"
							"  FILTER(REGEX(STR(?g), \"^%s\", \"i\"))\n"
							"}",
							node, base);
	if(!r)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": retrieved licensing triples, will interpret\n");
		r = spindle_db_audiences_interp_(spindle, model, node, audiences);
		if(r)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to interepret licensing triples\n");
		}
	}
	twine_rdf_node_destroy(node);
	twine_rdf_model_destroy(model);
	free(base);
	return r;
}

static int
spindle_db_audiences_interp_(SPINDLE *spindle, librdf_model *model, librdf_node *subject, char ***audiences)
{
	librdf_statement *query, *st;
	librdf_stream *stream;
	librdf_node *node;
	int r;
	size_t c;

	/* Find all odrl:permission nodes and interpret them */
	query = twine_rdf_st_create();
	if(!query)
	{
		return -1;
	}
	node = twine_rdf_node_clone(subject);
	if(!node)
	{
		twine_rdf_st_destroy(query);
		return -1;
	}
	librdf_statement_set_subject(query, node);
	/* node is now owned by query */
	node = twine_rdf_node_createuri(NS_ODRL "permission");
	if(!node)
	{
		twine_rdf_st_destroy(query);
		return -1;
	}
	librdf_statement_set_predicate(query, node);
	/* node is now owned by query */
	r = 0;
	for(stream = librdf_model_find_statements(model, query);
		!librdf_stream_end(stream);
		librdf_stream_next(stream))
	{
		st = librdf_stream_get_object(stream);
		node = librdf_statement_get_object(st);
		/* XXX bnodes */
		if(!librdf_node_is_resource(node))
		{
			twine_logf(LOG_DEBUG, PLUGIN_NAME ": odrl:permission's object is not a resource\n");
			continue;
		}
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": found odrl:permission\n");
		r = spindle_db_audiences_permission_(spindle, model, node, audiences);
		if(r)
		{
			break;
		}		
	}
	librdf_free_stream(stream);
	twine_rdf_st_destroy(query);
	if(r == 1 && *audiences)
	{
		/* spindle_db_audiences_permission_() indicated that anybody can play
		 * items under these terms, so we should discard any audience
		 * statements that we have.
		 */
		for(c = 0; (*audiences)[c]; c++)
		{
			free((*audiences)[c]);
		}
		free(*audiences);	   
		*audiences = NULL;
	}
	return r < 0 ? -1 : 0;
}

/* Interpret an individual odrl:Permission instance */
static int
spindle_db_audiences_permission_(SPINDLE *spindle, librdf_model *model, librdf_node *subject, char ***audiences)
{
	int r;

	/* We care about two things:
	 * - that there's an odrl:action and that it includes odrl:play,
	 *   odrl:present, odrl:display, or odrl:use
	 *
	 * - there's an odrl:assignee
	 *
	 * If there's no suitable action, then we don't care about this
	 * particular permission.
	 *
	 * If there's an suitable action but no assignee, then anybody can
	 * play and we can bail out early.
	 */
	r = spindle_db_audiences_action_(spindle, model, subject);
	if(r < 0)
	{
		return -1;
	}
	if(!r)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": odrl:Permission does not include a suitable action; skipping\n");
		return 0;
	}
	r = spindle_db_audiences_assignee_(spindle, model, subject, audiences);
	if(r < 0)
	{
		return -1;
	}
	if(!r)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": permission is granted to everyone\n");
		return 1;
	}
	return 0;
}

/* Check for a suitable odrl:action in a permission; returns 1 if found,
 * 0 if not found, -1 on error.
 */
static int
spindle_db_audiences_action_(SPINDLE *spindle, librdf_model *model, librdf_node *subject)
{
	librdf_node *node;
	librdf_statement *query, *st;
	librdf_stream *stream;
	librdf_uri *uri;
	const char *uristr;
	int r;

	query = twine_rdf_st_create();
	if(!query)
	{
		return -1;
	}
	node = twine_rdf_node_clone(subject);
	if(!node)
	{
		twine_rdf_st_destroy(query);
		return -1;
	}
	librdf_statement_set_subject(query, node);
	/* node is now owned by query */
	node = twine_rdf_node_createuri(NS_ODRL "action");
	if(!node)
	{
		twine_rdf_st_destroy(query);
		return -1;
	}
	librdf_statement_set_predicate(query, node);
	/* node is now owned by query */
	r = 0;
	for(stream = librdf_model_find_statements(model, query);
		!librdf_stream_end(stream);
		librdf_stream_next(stream))
	{
		st = librdf_stream_get_object(stream);
		node = librdf_statement_get_object(st);
		if(!librdf_node_is_resource(node))
		{
			continue;
		}
		if(!(uri = librdf_node_get_uri(node)) ||
		   !(uristr = (const char *) librdf_uri_as_string(uri)))
		{
			r = -1;
			break;
		}
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": found odrl:action <%s>\n", uristr);
		if(!strcmp(uristr, NS_ODRL "play") ||
		   !strcmp(uristr, NS_ODRL "present") ||
		   !strcmp(uristr, NS_ODRL "display") ||
		   !strcmp(uristr, NS_ODRL "use"))
		{
			r = 1;
			break;
		}
	}
	librdf_free_stream(stream);
	twine_rdf_st_destroy(query);	
	return r;
}

/* Check for a suitable odrl:assignee in a permission; returns 1 if found,
 * 0 if not found, -1 on error.
 */
static int
spindle_db_audiences_assignee_(SPINDLE *spindle, librdf_model *model, librdf_node *subject, char ***audiences)
{
	librdf_node *node;
	librdf_statement *query, *st;
	librdf_stream *stream;
	librdf_uri *uri;
	const char *uristr;
	int r;
	char **p;
	size_t c;

	query = twine_rdf_st_create();
	if(!query)
	{
		return -1;
	}
	node = twine_rdf_node_clone(subject);
	if(!node)
	{
		twine_rdf_st_destroy(query);
		return -1;
	}
	librdf_statement_set_subject(query, node);
	/* node is now owned by query */
	node = twine_rdf_node_createuri(NS_ODRL "assignee");
	if(!node)
	{
		twine_rdf_st_destroy(query);
		return -1;
	}
	librdf_statement_set_predicate(query, node);
	/* node is now owned by query */
	r = 0;
	for(stream = librdf_model_find_statements(model, query);
		!librdf_stream_end(stream);
		librdf_stream_next(stream))
	{
		st = librdf_stream_get_object(stream);
		node = librdf_statement_get_object(st);
		if(!librdf_node_is_resource(node))
		{
			continue;
		}
		if(!(uri = librdf_node_get_uri(node)) ||
		   !(uristr = (const char *) librdf_uri_as_string(uri)))
		{
			r = -1;
			break;
		}
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": found odrl:assignee <%s>\n", uristr);
		r = 1;
		for(c = 0; *audiences && (*audiences)[c]; c++) {}
		p = (char **) realloc(*audiences, sizeof(char *) * (c + 2));
		if(!p)
		{
			r = -1;
			break;
		}
		*audiences = p;
		p[c] = strdup(uristr);
		if(!p[c])
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate assignee URI string\n");
			r = -1;
			break;
		}
		p[c + 1] = NULL;
		r = 1;
	}
	librdf_free_stream(stream);
	twine_rdf_st_destroy(query);	
	return r;
}

/* Update the language-specific index "index_<target>" using (in order of
 * preference), <specific>, <generic>, (none).
 * e.g., target="en_gb", specific="en-gb", generic="en"
 */
static int
spindle_db_langindex_(SQL *sql, const char *id, const char *target, const char *specific, const char *generic)
{
	if(specific)
	{
		if(sql_executef(sql, "UPDATE \"index\" SET "
						"\"index_%s\" = setweight(to_tsvector(coalesce(\"title\" -> '%s', \"title\" -> '%s', \"title\" -> '_', '')), 'A') || "
						" setweight(to_tsvector(coalesce(\"description\" -> '%s', \"description\" -> '%s', \"description\" -> '_', '')), 'B')  "
						"WHERE \"id\" = %Q", target, specific, generic, specific, generic, id))
		{
			return -1;
		}
		return 0;
	}
	if(sql_executef(sql, "UPDATE \"index\" SET "
					"\"index_%s\" = setweight(to_tsvector(coalesce(\"title\" -> '%s', \"title\" -> '_', '')), 'A') || "
					" setweight(to_tsvector(coalesce(\"description\" -> '%s', \"description\" -> '_', '')), 'B')  "
					"WHERE \"id\" = %Q", target, generic, generic, id))
	{
		return -1;
	}
	return 0;
}

#endif


#if SPINDLE_DB_PROXIES

char *
spindle_db_proxy_locate(SPINDLE *spindle, const char *uri)
{
	SQL_STATEMENT *rs;
	char *buf, *p;
	const char *s;
	
	rs = sql_queryf(spindle->db, "SELECT \"id\" FROM \"proxy\" WHERE %Q = ANY(\"sameas\")", uri);
	if(!rs)
	{
		return NULL;
	}
	if(sql_stmt_eof(rs))
	{
		return NULL;
	}
    /* root + '/' + uuid + '#id' + NUL */
	/* XXX the fragment should be configurable */
	buf = (char *) malloc(strlen(spindle->root) + 1 + 32 + 3 + 1);
	if(!buf)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for proxy URI\n");
		sql_stmt_destroy(rs);
		return NULL;
	}
	strcpy(buf, spindle->root);
	p = strchr(buf, 0);
	if(p > buf)
	{
		p--;
		if(*p == '/')
		{
			p++;
		}
		else
		{
			p++;
			*p = '/';
			p++;
		}
	}
	else
	{
		*p = '/';
		p++;
	}
	for(s = sql_stmt_str(rs, 0); *s; s++)
	{
		if(isalnum(*s))
		{
			*p = tolower(*s);
			p++;
		}
	}
	strcpy(p, "#id");
	return buf;
}

/* Store a relationship between a proxy and a processed entity */
int
spindle_db_proxy_relate(SPINDLE *spindle, const char *remote, const char *local)
{
	char *id;
	struct relate_struct data;

	id = spindle_db_id_(local);
	if(!id)
	{
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": DB: adding %s = <%s>\n", id, remote);
	data.spindle = spindle;
	data.id = id;
	data.uri = remote;
	if(sql_perform(spindle->db, spindle_db_perform_proxy_relate_, (void *) &data, -1, SQL_TXN_CONSISTENT))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to relate <%s> to %s\n", remote, id);
		free(id);
		return -1;
	}

	free(id);
	return 0;
}

/* Obtain all of the outbound references from a proxy */
char **
spindle_db_proxy_refs(SPINDLE *spindle, const char *uri)
{
	SQL_STATEMENT *rs;
	char *id;
	char **refset;
	size_t count;

	id = spindle_db_id_(uri);
	if(!id)
	{
		return NULL;
	}
	
	rs = sql_queryf(spindle->db, "SELECT unnest(\"sameas\") AS \"uri\" FROM \"proxy\" WHERE \"id\" = %Q", id);
	free(id);
	if(!rs)
	{
		return NULL;
	}
	for(count = 0; !sql_stmt_eof(rs); sql_stmt_next(rs))
	{
		count++;
	}
	refset = (char **) calloc(count + 1, sizeof(char *));
	if(!refset)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for reference-set\n");		
		sql_stmt_destroy(rs);
		return NULL;
	}
	sql_stmt_rewind(rs);
	for(count = 0; !sql_stmt_eof(rs); sql_stmt_next(rs))
	{
		refset[count] = strdup(sql_stmt_str(rs, 0));
		if(!refset[count])
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate reference <%s>\n", sql_stmt_str(rs, 0));
			for(count--; ; count--)
			{
				free(refset[count]);
				if(!count)
				{
					break;
				}
			}
			free(refset);
			sql_stmt_destroy(rs);
			return NULL;
		}
		count++;
	}
	sql_stmt_destroy(rs);
	return refset;
}

/* Move a set of references from one proxy to another */
int
spindle_db_proxy_migrate(SPINDLE *spindle, const char *from, const char *to, char **refs)
{
	char *oldid, *newid;

	(void) refs;

	oldid = spindle_db_id_(from);
	newid = spindle_db_id_(to);
	if(!oldid || !newid)
	{
		free(oldid);
		free(newid);
		return -1;
	}
	/* Transaction */
	sql_executef(spindle->db, "UPDATE \"proxy\" SET \"sameas\" = \"sameas\" || ( SELECT \"sameas\" FROM \"proxy\" WHERE \"id\" = %Q ) WHERE \"id\" = %Q", oldid, newid); 
	sql_executef(spindle->db, "DELETE FROM \"proxy\" WHERE \"id\" = %Q", oldid);
	free(oldid);
	free(newid);
	return 0;
}

static int
spindle_db_perform_proxy_relate_(SQL *restrict db, void *restrict userdata)
{
	struct relate_struct *data;
	SQL_STATEMENT *rs;

	data = (struct relate_struct *) userdata;
	rs = sql_queryf(db, "SELECT \"id\" FROM \"proxy\" WHERE \"id\" = %Q", data->id);
	if(!rs)
	{
		return -2;
	}
	if(sql_stmt_eof(rs))
	{
		if(sql_executef(db, "INSERT INTO \"proxy\" (\"id\", \"sameas\") VALUES (%Q, ARRAY[]::text[])", data->id))
		{
			sql_stmt_destroy(rs);
			return -2;
		}
	}
	sql_stmt_destroy(rs);
	if(sql_executef(db, "UPDATE \"proxy\" SET \"sameas\" = array_append(\"sameas\", %Q) WHERE \"id\" = %Q", data->uri, data->id))
	{
		return -2;
	}
	return 1;
}

/* Fetch all of the source data about the entities that relate to this
 * proxy
 */
int
spindle_db_cache_source(SPINDLECACHE *data)
{
	char **refs;
	int r;
	size_t c;
	librdf_statement *st;

	refs = spindle_db_proxy_refs(data->spindle, data->localname);
	if(!refs)
	{
		return -1;
	}
	r = 0;
	for(c = 0; refs[c]; c++)
	{
		/* Add <ref> owl:sameAs <localname> triples to the proxy model */
		st = twine_rdf_st_create();
		librdf_statement_set_subject(st, twine_rdf_node_createuri(refs[c]));
		librdf_statement_set_predicate(st, twine_rdf_node_clone(data->spindle->sameas));
		librdf_statement_set_object(st, twine_rdf_node_createuri(data->localname));
		librdf_model_context_add_statement(data->proxydata, data->graph, st);
		twine_rdf_st_destroy(st);
		/* Fetch the source data into the source model */
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": DB: fetching data for <%s>\n", refs[c]);
		if(sparql_queryf_model(data->spindle->sparql, data->sourcedata,
							   "SELECT DISTINCT ?s ?p ?o ?g\n"
							   " WHERE {\n"
							   "  GRAPH ?g {\n"
							   "   ?s ?p ?o .\n"
							   "   FILTER(?s = <%s>)\n"
							   "  }\n"
							   "}",
							   refs[c]))
		{
			r = -1;
			break;
		}
	}
	
	for(c = 0; refs[c]; c++)
	{
		free(refs[c]);
	}
	free(refs);
	return r;
}

#endif /* SPINDLE_DB_PROXIES */

#if SPINDLE_DB_INDEX || SPINDLE_DB_PROXIES

static char *
spindle_db_literalset_(struct spindle_literalset_struct *set)
{
	size_t c, nbytes;
	char *str, *p;

	nbytes = 1;
	for(c = 0; c < set->nliterals; c++)
	{
		/* "lang"=>"string", */
		nbytes += 7;
		if(set->literals[c].lang[0])
		{
			nbytes += spindle_db_esclen_(set->literals[c].lang);
		}
		else
		{
			/* We use an underscore to indicate no language */
			nbytes++;
		}
		nbytes += spindle_db_esclen_(set->literals[c].str);
	}
	str = (char *) malloc(nbytes);
	if(!str)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate %lu bytes for literal set\n", (unsigned long) nbytes);
		return NULL;
	}
	p = str;
	for(c = 0; c < set->nliterals; c++)
	{
		if(c)
		{
			*p = ',';
			p++;
		}
		*p = '"';
		p++;
		if(set->literals[c].lang[0])
		{
			p = spindle_db_escstr_lower_(p, set->literals[c].lang);
		}
		else
		{
			*p = '_';
			p++;
		}
		strcpy(p, "\"=>\"");
		p += 4;
		p = spindle_db_escstr_(p, set->literals[c].str);
		*p = '"';
		p++;
	}
	*p = 0;
	return str;
}

static char *
spindle_db_strset_(struct spindle_strset_struct *set)
{
	size_t c, nbytes;
	char *str, *p;

	nbytes = 3;
	for(c = 0; c < set->count; c++)
	{
		/* "string", */
		nbytes += 3;
		nbytes += spindle_db_esclen_(set->strings[c]);
	}
	str = (char *) malloc(nbytes);
	if(!str)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate %lu bytes for string set\n", (unsigned long) nbytes);
		return NULL;
	}
	p = str;
	*p = '{';
	p++;
	for(c = 0; c < set->count; c++)
	{
		if(c)
		{
			*p = ',';
			p++;
		}
		*p = '"';
		p++;
		p = spindle_db_escstr_(p, set->strings[c]);
		*p = '"';
		p++;
	}
	*p = '}';
	p++;
	*p = 0;
	return str;
}

static size_t
spindle_db_esclen_(const char *src)
{
	size_t len;

	for(len = 0; *src; src++)
	{
		if(*src == '"' || *src == '\\')
		{
			/* Characters which require escaping need an extra byte */
			len++;
		}
		len++;
	}
	return len;
}

static char *
spindle_db_escstr_(char *dest, const char *src)
{
	for(; *src; src++)
	{
		if(*src == '"' || *src == '\\')
		{
			*dest = '\\';
			dest++;
		}
		*dest = *src;
		dest++;
	}
	return dest;
}

static char *
spindle_db_escstr_lower_(char *dest, const char *src)
{
	for(; *src; src++)
	{
		if(*src == '"' || *src == '\\')
		{
			*dest = '\\';
			dest++;
		}
		*dest = tolower(*src);
		dest++;
	}
	return dest;
}

static int
spindle_db_local_(SPINDLE *spindle, const char *localname)
{
	if(strncmp(localname, spindle->root, strlen(spindle->root)))
	{
		return 0;
	}
	return 1;
}

static char *
spindle_db_id_(const char *localname)
{
	char *id, *p;
	const char *t;

	id = (char *) calloc(1, strlen(localname) + 1);
	if(!id)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for ID\n");
		return NULL;
	}
	if(!(t = strrchr(localname, '/')))
	{
		t = id;
	}
	for(p = id; *t; t++)
	{
		if(isalnum(*t))
		{
			*p = *t;
			p++;
		}
		else if(*t == '#')
		{
			break;
		}
	}
	*p = 0;
	return id;
}

#endif /* SPINDLE_DB_INDEX || SPINDLE_DB_PROXIES */
