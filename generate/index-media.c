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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_spindle-generate.h"

/*
 * The 'media' table tracks information about media items (digital assets):
 *
 *  id       The UUID of the digital asset
 *  uri      The complete URI of the digital asset
 *  class    The DCMIT member for this asset (:Image, :Sound, etc.)
 *  type     The MIME type of the asset
 *  audience If known to be restricted, the URI of the audience it's
 *           restricted to.
 *  duration For timed media, the length in seconds
 */

static int spindle_index_media_refs_(SQL *sql, const char *id, SPINDLEENTRY *data);
static char *spindle_index_media_kind_(SPINDLEENTRY *data);
static char *spindle_index_media_type_(SPINDLEENTRY *data);
static char *spindle_index_media_duration_(SPINDLEENTRY *data);
static char *spindle_index_media_license_(SPINDLEENTRY *data);

/* If this entity is a digital object, store information about it the "media"
 * database table.
 *
 * Invoked when TK_MEDIA is set
 */
int
spindle_index_media(SQL *sql, const char *id, SPINDLEENTRY *data)
{
	size_t c;
	char **refs;
	char *kind, *type, *license, *duration;
	int r;

	r = 0;
	if(!data->classname || strcmp(data->classname, NS_FOAF "Document"))
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": item is not a digital asset\n");
		/* This isn't a digital asset */
		return spindle_index_media_refs_(sql, id, data);
	}
	/* Find the source URI (there can be only one) */
	refs = spindle_proxy_refs(data->spindle, data->localname);
	if(!refs)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to retrieve co-reference set for digital object\n");
		return -1;
	}
	if(!refs[0] || refs[1])
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": will not track media for a digital object with multiple co-references\n");
		for(c = 0; refs[c]; c++)
		{
			free(refs[c]);
		}
		free(refs);
		return 0;
	}
	kind = spindle_index_media_kind_(data);
	if(!kind)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": media: cannot determine media kind\n");
		free(refs[0]);
		free(refs);
		return -1;
	}
	type = spindle_index_media_type_(data);
	duration = spindle_index_media_duration_(data);
	
	/* XXX TODO: handle multiple licenses */
	license = spindle_index_media_license_(data);
	if(license)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": media: license URI for <%s> is <%s>\n", refs[0], license);
		spindle_trigger_add(data, license, TK_MEDIA, NULL);
		r = spindle_index_audiences(data->generate, license, id, refs[0], kind, type, duration);
	}
	else
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": media: <%s> has no license associated with it\n", refs[0]);
		r = sql_executef(sql, "INSERT INTO \"media\" (\"id\", \"uri\", \"class\", \"type\", \"audience\", \"duration\") VALUES (%Q, %Q, %Q, %Q, %Q, %Q)",
						 id, refs[0], kind, type, NULL, duration);
	}
#if SPINDLE_ENABLE_ABOUT_SELF
	if(r >= 0)
	{
		r = sql_executef(sql, "INSERT INTO \"index_media\" (\"id\", \"media\") VALUES (%Q, %Q)", id, id);
	}
#endif
	free(refs[0]);
	free(refs);
	free(kind);
	free(type);
	free(license);
	free(duration);
	return r;
}

/* Add information about references from this entity to related digital objects
 */
static int
spindle_index_media_refs_(SQL *sql, const char *id, SPINDLEENTRY *data)
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
		if(!spindle_db_local(data->spindle, uristr))
		{
			localid = spindle_proxy_locate(data->spindle, uristr);
			if(!localid)
			{
				spindle_trigger_add(data, uristr, TK_MEDIA, NULL);
				continue;
			}
		}
		if(!(tid = spindle_db_id(localid ? localid : uristr)))
		{
			free(localid);
			spindle_trigger_add(data, uristr, TK_MEDIA, NULL);
			continue;
		}
		spindle_trigger_add(data, uristr, TK_MEDIA, tid);
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

/* Determine the kind of object we're dealing with
 * XXX should be driven by the rulebase
 */
static char *
spindle_index_media_kind_(SPINDLEENTRY *data)
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

/* Determine the duration, in seconds, of the media we're dealing with
 * XXX should be driven by the rulebase
 */
static char *
spindle_index_media_duration_(SPINDLEENTRY *data)
{
	librdf_statement *query;
	librdf_statement *st;
	librdf_stream *stream;
	librdf_node *node;
	librdf_uri *uri;
	const char *uristr, *value;
	char *endp;
	char duration[32];
	long long val;

	duration[0] = 0;
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
	if(!(node = librdf_new_node_from_uri_string(data->spindle->world, (const unsigned char *) NS_PO "duration")))
	{
		librdf_free_statement(query);
		return NULL;
	}
	librdf_statement_set_predicate(query, node);
	for(stream = librdf_model_find_statements_in_context(data->proxydata, query, data->graph); !librdf_stream_end(stream); librdf_stream_next(stream))
	{
		st = librdf_stream_get_object(stream);
		if((node = librdf_statement_get_object(st)) &&
		   (uri = librdf_node_get_literal_value_datatype_uri(node)) &&
		   (uristr = (const char *) librdf_uri_as_string(uri)))
		{
			/* We don't really perform type-checking here, we just want to know if
			 * it's an ordinal value
			 */
			if(!strcmp(uristr, NS_XSD "integer") ||
			   !strcmp(uristr, NS_XSD "long") ||
			   !strcmp(uristr, NS_XSD "unsignedLong") ||
			   !strcmp(uristr, NS_XSD "int") ||
			   !strcmp(uristr, NS_XSD "unsignedInt") ||
			   !strcmp(uristr, NS_XSD "short") ||
			   !strcmp(uristr, NS_XSD "unsignedShort") ||
			   !strcmp(uristr, NS_XSD "byte") ||
			   !strcmp(uristr, NS_XSD "unsignedByte") ||
			   !strcmp(uristr, NS_XSD "nonPositiveInteger") ||
			   !strcmp(uristr, NS_XSD "negativeInteger") ||
			   !strcmp(uristr, NS_XSD "nonNegativeInteger") ||
			   !strcmp(uristr, NS_XSD "positiveInteger"))
			{
				value = (const char *) librdf_node_get_literal_value(node);
				if(value)
				{
					endp = NULL;
					val = strtoll(value, &endp, 10);
					if((!endp || !endp[0]) && val >= 0)
					{
						snprintf(duration, sizeof(duration), "%llu", (unsigned long long) val);
						break;
					}
				}
			}
		}
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	if(duration[0])
	{
		return strdup(duration);
	}
	return NULL;
}

/* Determine the MIME type of object we're dealing with */
static char *
spindle_index_media_type_(SPINDLEENTRY *data)
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
	/* XXX The predicate here should be specified via the rulebase */
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
spindle_index_media_license_(SPINDLEENTRY *data)
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
	/* XXX The predicate used here should be configurable via the rulebase */
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
			break;
		}
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);	
	return license;
}
