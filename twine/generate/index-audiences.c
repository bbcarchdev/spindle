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

#include "p_spindle-generate.h"

static char *spindle_index_audiences_origin_(const char *uristr);
static int spindle_index_audiences_interp_(SPINDLEGENERATE *generate, librdf_model *model, librdf_node *subject, struct spindle_strset_struct *audiences);
static int spindle_index_audiences_permission_(SPINDLEGENERATE *generate, librdf_model *model, librdf_node *subject, struct spindle_strset_struct *audiences);
static int spindle_index_audiences_action_(SPINDLEGENERATE *generate, librdf_model *model, librdf_node *subject);
static int spindle_index_audiences_assignee_(SPINDLEGENERATE *generate, librdf_model *model, librdf_node *subject, struct spindle_strset_struct *audiences);

/* Determine who can access a digital object based upon data about licenses */
int
spindle_index_audiences(SPINDLEGENERATE *generate, const char *license, const char *mediaid, const char *mediauri, const char *mediakind, const char *mediatype)
{
	SQL_STATEMENT *rs;
	char *uri, *id;
	
	uri = spindle_proxy_locate(generate->spindle, license);
	if(!uri)
	{
		/* No data held for this URI */
		return 0;
	}
	id = spindle_db_id(uri);
	if(!id)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to obtain UUID from <%s>\n", uri);
		free(uri);
		return -1;
	}
	free(uri);
	rs = sql_queryf(generate->spindle->db, "SELECT \"uri\", \"audienceid\" FROM \"licenses_audiences\" WHERE \"id\" = %Q", id);
	free(id);
	if(sql_stmt_eof(rs))
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": audiences: license <%s> has no associated audiences\n", license);
		sql_stmt_destroy(rs);
		return 0;
	}
	for(; !sql_stmt_eof(rs); sql_stmt_next(rs))
	{
		sql_executef(generate->spindle->db, "INSERT INTO \"media\" (\"id\", \"uri\", \"class\", \"type\", \"audience\", \"audienceid\") VALUES (%Q, %Q, %Q, %Q, %Q, %Q)",
			mediaid, mediauri, mediakind, mediatype, sql_stmt_str(rs, 0), sql_stmt_str(rs, 1));
	}
	sql_stmt_destroy(rs);
	return 1;
}

/* Interpet ODRL descriptions in source data to determine who can access
 * digital objects associated with this licence
 */
int
spindle_index_audiences_licence(SQL *sql, const char *id, SPINDLEENTRY *data)
{
	librdf_iterator *iter;
	librdf_node *graph, *node;
	librdf_uri *graphuri;
	librdf_model *model;
	const char *graphuristr;
	char *base, *audienceuri, *audienceid;
	int r, match;
	char **bases;
	size_t c, d;
	struct spindle_strset_struct *audiences;
	SQL_STATEMENT *rs;
	
	(void) id;
	
	bases = (char **) calloc(sizeof(char *), data->refcount + 1);
	if(!bases)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": faled to allocate buffer for base URI strings\n");
		return -1;
	}
	audiences = spindle_strset_create();
	if(!audiences)
	{
		free(bases);
		twine_logf(LOG_CRIT, PLUGIN_NAME ": faled to allocate string-set for audience URIs\n");
		return -1;
	}
	
	r = 0;
	/* For each of our external co-referenced URIs, determine an origin (base)
	 * URI for it for same-origin comparison purposes
	 */
	for(c = 0; c < data->refcount; c++)
	{
		bases[c] = spindle_index_audiences_origin_(data->refs[c]);
		if(!bases[c])
		{
			continue;
		}
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": added same-origin base URI <%s>\n", bases[c]);
	}
	if(r)
	{
		for(c = 0; c < data->refcount; c++)
		{
			free(bases[c]);
		}
		free(bases);
		spindle_strset_destroy(audiences);
		return -1;
	}
	/* Next, iterate all of the graphs in the source data we have */
	for(iter = librdf_model_get_contexts(data->sourcedata);
		!librdf_iterator_end(iter);
		librdf_iterator_next(iter))
	{
		graph = librdf_iterator_get_object(iter);
		if(!graph)
		{
			continue;
		}
		graphuri = librdf_node_get_uri(graph);
		graphuristr = (const char *) librdf_uri_as_string(graphuri);
		base = spindle_index_audiences_origin_(graphuristr);
		if(!base)
		{
			continue;
		}
		match = 0;
		/* Check that the supplied graph URI's origin matches one that we
		 * are interested in (i.e., it passes a same-origin test)
		 */
		for(c = 0; c < data->refcount; c++)
		{
			if(!bases[c])
			{
				continue;
			}
			if(!strcmp(base, bases[c]))
			{
				match = 1;
				break;
			}
		}
		if(!match)
		{
			free(base);
			continue;
		}
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": graph <%s> passes same-origin check\n", graphuristr);
		/* Fetch the contents of the graph so that we can walk it */
		if((model = spindle_graphcache_fetch_node(data->spindle, graph)))
		{
			twine_logf(LOG_DEBUG, PLUGIN_NAME ": processing graph <%s> for licensing data\n", graphuristr);
			/* For each subject that this entity is co-referenced to
			 * that this candidate graph authoratitively describes,
			 * attempt to extract audience permission data from it
			 */
			for(d = 0; d < data->refcount; d++)
			{
				if(bases[d] && !strcmp(bases[d], base))
				{
					/* The subject's (data->refs[d]) base URI (bases[d])
					 * matches this graph's URI (base), so we can
					 * walk the graph starting from this subject node.
					 */
					twine_logf(LOG_DEBUG, PLUGIN_NAME ": walking graph, starting at <%s>\n", data->refs[d]);
					node = twine_rdf_node_createuri(data->refs[d]);
					if(spindle_index_audiences_interp_(data->generate, model, node, audiences) > 0)
					{
						/* Available to everyone */
						r = 1;
						twine_logf(LOG_DEBUG, PLUGIN_NAME ": licensing data indicates open access\n");
					}
					twine_rdf_node_destroy(node);
					break;
				}
			}
		}
		else
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed fetch graph <%s>\n", graphuristr);
			r = -1;
		}
		free(base);
		if(r == 1)
		{
			break;
		}
	}
	if(r == 0)
	{
		for(c = 0; c < audiences->count; c++)
		{
			audienceuri = spindle_proxy_locate(data->spindle, audiences->strings[c]);
			if(audienceuri)
			{
				audienceid = spindle_db_id(audienceuri);
			}
			else
			{
				audienceid = NULL;
			}
			free(audienceuri);
			if(audienceid)
			{
				rs = sql_queryf(sql, "SELECT \"id\" FROM \"audiences\" WHERE \"id\" = %Q", audienceid);
				if(rs)
				{
					if(sql_stmt_eof(rs))
					{
						sql_executef(sql, "INSERT INTO \"audiences\" (\"id\", \"uri\") VALUES (%Q, %Q)", audienceid, audiences->strings[c]);
					}
					sql_stmt_destroy(rs);
				}
				else
				{
					r = -1;
				}
			}
			if(sql_executef(data->generate->db, "INSERT INTO \"licenses_audiences\" (\"id\", \"uri\", \"audienceid\") VALUES (%Q, %Q, %Q)",
				data->id, audiences->strings[c], audienceid))
			{
				r = -1;
			}
			free(audienceid);
		}
	}
/*	if(!r)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": retrieved licensing triples, will interpret\n");
		r = spindle_index_audiences_interp_(generate, model, node, audiences);
		if(r)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to interepret licensing triples\n");
		}
	}
	twine_rdf_node_destroy(node);
	twine_rdf_model_destroy(model);
	free(base); */
	spindle_strset_destroy(audiences);
	for(c = 0; c < data->refcount; c++)
	{
		free(bases[c]);
	}
	free(bases);
	return r;
}

/* Obtain an origin (base) URI for a given absolute URI string */
static char *
spindle_index_audiences_origin_(const char *uristr)
{
	URI *uri;
	URI_INFO *info;
	char *base, *p;
	
	uri = uri_create_str(uristr, NULL);
	if(!uri)
	{
		twine_logf(LOG_WARNING, PLUGIN_NAME ": failed to parse <%s> as a URI\n", uristr);
		uri_destroy(uri);
		return NULL;
	}
	if(!uri_absolute(uri))
	{
		twine_logf(LOG_WARNING, PLUGIN_NAME ": URI <%s> is not absolute\n", uristr);
		uri_destroy(uri);
		return NULL;
	}
	info = uri_info(uri);
	if(!info)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to retrieve URI information about <%s>\n", uristr);
		uri_destroy(uri);
		return NULL;
	}
	base = (char *) calloc(1, (info->scheme ? strlen(info->scheme) + 3 : 0) +
		(info->auth ? strlen(info->auth) + 1 : 0) +
		(info->host ? strlen(info->host) : 0) + 1 +
		(info->port ? 64 : 0) + 2);
	p = base;
	if(!base)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to allocate buffer for base URI for <%s>\n", uristr);
		uri_info_destroy(info);
		uri_destroy(uri);
		return NULL;
	}
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
	return base;
}

/* Find all statements in the form:
 *
 * <subject> odrl:permission <object> .
 *
 * ...and interrogate the permissions being granted in each.
 *
 * Note that the <object> must appear in the same graph.
 */
static int
spindle_index_audiences_interp_(SPINDLEGENERATE *generate, librdf_model *model, librdf_node *subject, struct spindle_strset_struct *audiences)
{
	librdf_statement *query, *st;
	librdf_stream *stream;
	librdf_node *node;
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
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": found statement matching query\n");
		st = librdf_stream_get_object(stream);
		node = librdf_statement_get_object(st);
		/* XXX bnodes */
		if(!librdf_node_is_resource(node))
		{
			twine_logf(LOG_DEBUG, PLUGIN_NAME ": odrl:permission's object is not a resource\n");
			continue;
		}
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": found odrl:permission\n");
		r = spindle_index_audiences_permission_(generate, model, node, audiences);
		if(r)
		{
			break;
		}
	}
	librdf_free_stream(stream);
	twine_rdf_st_destroy(query);
	if(r == 1)
	{
		return 1;
	}
	return r < 0 ? -1 : 0;
}

/* Interpret an individual odrl:Permission instance */
static int
spindle_index_audiences_permission_(SPINDLEGENERATE *generate, librdf_model *model, librdf_node *subject, struct spindle_strset_struct *audiences)
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
	r = spindle_index_audiences_action_(generate, model, subject);
	if(r < 0)
	{
		return -1;
	}
	if(!r)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": odrl:Permission does not include a suitable action; skipping\n");
		return 0;
	}
	r = spindle_index_audiences_assignee_(generate, model, subject, audiences);
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
spindle_index_audiences_action_(SPINDLEGENERATE *generate, librdf_model *model, librdf_node *subject)
{
	librdf_node *node;
	librdf_statement *query, *st;
	librdf_stream *stream;
	librdf_uri *uri;
	const char *uristr;
	int r;

	(void) generate;

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
spindle_index_audiences_assignee_(SPINDLEGENERATE *generate, librdf_model *model, librdf_node *subject, struct spindle_strset_struct *audiences)
{
	librdf_node *node;
	librdf_statement *query, *st;
	librdf_stream *stream;
	librdf_uri *uri;
	const char *uristr;
	int r;

	(void) generate;

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
		if(spindle_strset_add(audiences, uristr))
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to add assignee URI string to set\n");
			r = -1;
			break;
		}
	}
	librdf_free_stream(stream);
	twine_rdf_st_destroy(query);
	return r;
}
