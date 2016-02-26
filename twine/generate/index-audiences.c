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

static int spindle_index_audiences_interp_(SPINDLEGENERATE *generate, librdf_model *model, librdf_node *subject, char ***audiences);
static int spindle_index_audiences_permission_(SPINDLEGENERATE *generate, librdf_model *model, librdf_node *subject, char ***audiences);
static int spindle_index_audiences_action_(SPINDLEGENERATE *generate, librdf_model *model, librdf_node *subject);
static int spindle_index_audiences_assignee_(SPINDLEGENERATE *generate, librdf_model *model, librdf_node *subject, char ***audiences);

/* Determine who can access a digital object based upon an ODRL description */
int
spindle_index_audiences(SPINDLEGENERATE *generate, const char *license, char ***audiences)
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
	r = sparql_queryf_model(generate->sparql, model,
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
		r = spindle_index_audiences_interp_(generate, model, node, audiences);
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
spindle_index_audiences_interp_(SPINDLEGENERATE *generate, librdf_model *model, librdf_node *subject, char ***audiences)
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
		r = spindle_index_audiences_permission_(generate, model, node, audiences);
		if(r)
		{
			break;
		}		
	}
	librdf_free_stream(stream);
	twine_rdf_st_destroy(query);
	if(r == 1 && *audiences)
	{
		/* spindle_index_audiences_permission_() indicated that anybody can play
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
spindle_index_audiences_permission_(SPINDLEGENERATE *generate, librdf_model *model, librdf_node *subject, char ***audiences)
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
spindle_index_audiences_assignee_(SPINDLEGENERATE *generate, librdf_model *model, librdf_node *subject, char ***audiences)
{
	librdf_node *node;
	librdf_statement *query, *st;
	librdf_stream *stream;
	librdf_uri *uri;
	const char *uristr;
	int r;
	char **p;
	size_t c;

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
