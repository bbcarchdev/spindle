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

static int spindle_index_membership_add_uri_(SPINDLEENTRY *data, SQL *sql, const char *id, const char *uristr);
static int spindle_index_membership_add_(SQL *sql, const char *id, const char *collid);
static int spindle_index_membership_query_(SQL *sql, const char *id, SPINDLEENTRY *data, librdf_model *model, librdf_node *graph, const char *predicate, int inverse, int matchrefs);
static int spindle_index_membership_strset_(SQL *sql, const char *id, SPINDLEENTRY *data, struct spindle_strset_struct *set);

/* Add information to the database about this entities membership in
 * collections.
 *
 * XXX The actual predicates evaluated here should be configurable via the
 * rulebase.
 */
int
spindle_index_membership(SQL *sql, const char *id, SPINDLEENTRY *data)
{
	/* Find the statements within the proxy model which explicity express
	 * the fact that our proxy is a member of some collection.
	 */
	if(spindle_index_membership_query_(sql, id, data, data->proxydata, data->graph, NS_DCTERMS "isPartOf", 0, 0))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to query proxy for dct:isPartOf statements\n");
		return -1;
	}
	/* Find the statements within the source data which express the fact
	 * that the document describing our subject is a member of a collection
	 */
	if(spindle_index_membership_query_(sql, id, data, data->sourcedata, NULL, NS_FOAF "primaryTopic", 1, 1))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to query proxy for foaf:primaryTopic statements\n");
		return -1;
	}
	/* Attempt to recursively add this proxy to a collection corresponding to
	 * the source graph URI (which may also be a member of other collections).
	 */
	if(spindle_index_membership_strset_(sql, id, data, data->sources))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to recursively add proxy to collections\n");
		return -1;
	}
	return 0;
}

/* Query the model for collection membership, where the objects of the
 * triples are proxy URIs (in which case they're translated to UUIDs),
 * or remote URIs which need to be looked up.
 *
 * - 'inverse' means the query will be in the form ?s <predicate> <proxy-uri>
 *
 * - 'matchrefs' means any subject or object (depending upon 'inverse') which
 *   is in data->refs (i.e., all of the source data URIs for the entity being
 *   processed) will be matched in place of <proxy-uri>
 */
static int
spindle_index_membership_query_(SQL *sql, const char *id, SPINDLEENTRY *data, librdf_model *model, librdf_node *graph, const char *predicate, int inverse, int matchrefs)
{
	librdf_iterator *i;
	librdf_statement *query, *st;
	librdf_node *node;
	librdf_stream *stream;
	librdf_uri *uri;
	const char *uristr;
	size_t c;
	int r, match;	

	if(!graph)
	{
		for(i = librdf_model_get_contexts(model); !librdf_iterator_end(i); librdf_iterator_next(i))
		{
			graph = librdf_iterator_get_object(i);
			if(!graph)
			{
				return -1;
			}
			if(spindle_index_membership_query_(sql, id, data, model, graph, predicate, inverse, matchrefs))
			{
				return -1;
			}
		}
		librdf_free_iterator(i);
		return 0;
	}
	if(!(query = librdf_new_statement(data->spindle->world)))
	{
		return -1;
	}
	if(!matchrefs)
	{
		if(!(node = librdf_new_node_from_node(data->self)))
		{
			librdf_free_statement(query);
			return -1;
		}
		if(inverse)
		{
			librdf_statement_set_object(query, node);
		}
		else
		{
			librdf_statement_set_subject(query, node);
		}
		/* the above 'node' is now owned by 'query' */
	}
	if(!(node = librdf_new_node_from_uri_string(data->spindle->world, (const unsigned char *) predicate)))
	{
		librdf_free_statement(query);
		return -1;
	}
	librdf_statement_set_predicate(query, node);
	/* the above 'node' is now owned by 'query' */
	r = 0;
	stream = librdf_model_find_statements_in_context(model, query, graph);
	for(; !librdf_stream_end(stream); librdf_stream_next(stream))
	{
		r = 0;
		st = librdf_stream_get_object(stream);
		if(matchrefs)
		{
			/* If 'matchrefs' is set, then the subject (inverse=0) or object
			 * (inverse=1) of the query is unset, and we should compare its
			 * value against the external URIs for the entity we're updating,
			 * as listed in data->refs[]. If we get a match, then the triple
			 * refers to this entity, and we can proceed with the membership
			 * addition.
			 */
			if(inverse)
			{
				node = librdf_statement_get_object(st);
			}
			else
			{
				node = librdf_statement_get_subject(st);
			}
			if(node &&
			   librdf_node_is_resource(node) &&
			   (uri = librdf_node_get_uri(node)) &&
			   (uristr = (const char *) librdf_uri_as_string(uri)))
			{
				match = 0;
				for(c = 0; data->refs[c]; c++)
				{
					if(!strcmp(uristr, data->refs[c]))
					{
						match = 1;
						break;
					}
				}
				if(!match)
				{
					continue;
				}
			}
			else
			{
				/* Not a URI node */
				continue;
			}
		}
		if(inverse)
		{
			node = librdf_statement_get_subject(st);
		}
		else
		{
			node = librdf_statement_get_object(st);
		}
		if(node &&
		   librdf_node_is_resource(node) &&
		   (uri = librdf_node_get_uri(node)) &&
		   (uristr = (const char *) librdf_uri_as_string(uri)))
		{
			if(spindle_index_membership_add_uri_(data, sql, id, uristr))
			{
				r = -1;
				break;
			}
		}
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	
	return r;
}

/* Iterate a set of URIs, where each is either a proxy URI (in which case
 * it's translated to a UUID), or a remote URI which needs to be looked up.
 */
static int
spindle_index_membership_strset_(SQL *sql, const char *id, SPINDLEENTRY *data, struct spindle_strset_struct *set)
{
	int r;
	size_t c;

	r = 0;
	for(c = 0; c < set->count; c++)
	{
		if((r = spindle_index_membership_add_uri_(data, sql, id, set->strings[c])))
		{
			break;
		}
	}
	return r;
}

static int
spindle_index_membership_add_uri_(SPINDLEENTRY *data, SQL *sql, const char *id, const char *uristr)
{
	char *collid, *localuri;
	int r;

	r = 0;
	localuri = NULL;
	collid = NULL;
	if(spindle_db_local(data->spindle, uristr))
	{
		collid = spindle_db_id(uristr);
		if(!collid)
		{
			twine_logf(LOG_WARNING, PLUGIN_NAME ": no UUID found for collection <%s>\n", uristr);
			spindle_trigger_add(data, uristr, TK_MEMBERSHIP, NULL);
			return 0;
		}
		spindle_trigger_add(data, uristr, TK_MEMBERSHIP, collid);
		r = spindle_index_membership_add_(sql, id, collid);
		free(collid);
		return r;
	}
	localuri = spindle_proxy_locate(data->spindle, uristr);
	if(!localuri)
	{
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": membership: no proxy found for <%s>\n", uristr);
		spindle_trigger_add(data, uristr, TK_MEMBERSHIP, NULL);
		return 0;
	}
	collid = spindle_db_id(localuri);
	if(!collid)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": membership: failed to locate local UUID for <%s>\n", localuri);
		free(localuri);
		spindle_trigger_add(data, uristr, TK_MEMBERSHIP, NULL);
		return 0;
	}
	spindle_trigger_add(data, uristr, TK_MEMBERSHIP, collid);
	r = spindle_index_membership_add_(sql, id, collid);
	free(collid);
	free(localuri);
	return r;
}

static int
spindle_index_membership_add_(SQL *sql, const char *id, const char *collid)
{
	SQL_STATEMENT *rs;

	rs = sql_queryf(sql, "SELECT \"id\" FROM \"membership\" WHERE \"id\" = %Q AND \"collection\" = %Q", id, collid);
	if(!rs)
	{
		return -1;
	}
	if(!sql_stmt_eof(rs))
	{
		sql_stmt_destroy(rs);
		return 0;
	}
	sql_stmt_destroy(rs);
	if(sql_executef(sql, "INSERT INTO \"membership\" (\"id\", \"collection\") VALUES (%Q, %Q)", id, collid))
	{
		return -1;
	}
	rs = sql_queryf(sql, "SELECT \"collection\" FROM \"membership\" WHERE \"id\" = %Q", collid);
	if(!rs)
	{
		return -1;
	}
	for(; !sql_stmt_eof(rs); sql_stmt_next(rs))
	{
		spindle_index_membership_add_(sql, id, sql_stmt_str(rs, 0));
	}
	sql_stmt_destroy(rs);
	return 0;
}

