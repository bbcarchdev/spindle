/* This engine processes requests for coreference graphs populated
 * by Twine's "spindle" post-processing module.
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

struct data_struct
{
	char *buf;
	size_t size;
	size_t pos;
};

static int spindle_item_post_(QUILTREQ *request);
static size_t spindle_s3_write_(char *ptr, size_t size, size_t nemb, void *userdata);

int
spindle_lookup(QUILTREQ *request, const char *target)
{
	SPARQL *sparql;
	SPARQLRES *res;
	SPARQLROW *row;
	librdf_node *node;
	librdf_uri *uri;
	const char *uristr;
	size_t l;
	char *buf;

	sparql = quilt_sparql();
	if(!sparql)
	{
		return 500;
	}	
	res = sparql_queryf(sparql, "SELECT ?s\n"
						"WHERE {\n"
						" GRAPH %V {\n"
						"  <%s> <http://www.w3.org/2002/07/owl#sameAs> ?s .\n"
						" }\n"
						"}\n",
						request->basegraph, target);
	if(!res)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": SPARQL query for coreference failed\n");
		return 500;
	}
	row = sparqlres_next(res);
	if(!row)
	{
		sparqlres_destroy(res);
		return 404;
	}
	node = sparqlrow_binding(row, 0);
	if(!node)
	{
		sparqlres_destroy(res);
		return 404;
	}
	if(!librdf_node_is_resource(node) ||
	   !(uri = librdf_node_get_uri(node)) ||
	   !(uristr = (const char *) librdf_uri_as_string(uri)))
	{
		sparqlres_destroy(res);
		return 404;
	}
	l = strlen(request->base);
	if(!strncmp(uristr, request->base, l))
	{
		buf = (char *) malloc(strlen(uristr) + 32);
		buf[0] = '/';
		strcpy(&(buf[1]), &(uristr[l]));
	}
	else
	{
		buf = strdup(uristr);
	}
	sparqlres_destroy(res);
	quilt_request_printf(request, "Status: 303 See other\n"
				 "Server: Quilt/" PACKAGE_VERSION "\n"
				 "Location: %s\n"
				 "\n", buf);
	free(buf);
	/* Return 0 to supress output */
	return 0;
}

int
spindle_item(QUILTREQ *request)
{
	char *query;

	query = (char *) malloc(strlen(request->subject) + 1024);
	if(!query)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to allocate %u bytes\n", (unsigned) strlen(request->subject) + 128);
		return 500;
	}
	/* Fetch the contents of the graph named in the request */
	sprintf(query, "SELECT DISTINCT * WHERE {\n"
			"GRAPH ?g {\n"
			"  ?s ?p ?o . \n"
			"  FILTER( ?g = <%s> )\n"
			"}\n"
			"}", request->subject);
	if(quilt_sparql_query_rdf(query, request->model))
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to create model from query\n");
		free(query);
		return 500;
	}
	/* If the model is completely empty, consider the graph to be Not Found */
	if(quilt_model_isempty(request->model))
	{
		free(query);
		return 404;
	}
	free(query);
	return spindle_item_post_(request);
}

/* Fetch an item by retrieving triples or quads from an S3 bucket */
int
spindle_item_s3(QUILTREQ *request)
{
	S3REQUEST *req;
	CURL *ch;
	struct data_struct data;
	long status;
	char *mime;

	/* Perform a basic sanity-check on the path */
	if(request->path[0] != '/' ||
	   strchr(request->path, '.') ||
	   strchr(request->path, '%'))
	{
		return 404;
	}
	quilt_logf(LOG_DEBUG, QUILT_PLUGIN_NAME ": S3: request path is %s\n", request->path);
	memset(&data, 0, sizeof(struct data_struct));
	req = s3_request_create(spindle_bucket, request->path, "GET");
	if(!req)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": S3: failed to create S3 request\n");
		return 500;
	}
	ch = s3_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_HEADER, 0);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, spindle_s3_verbose);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *) &data);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, spindle_s3_write_);
	if(s3_request_perform(req))
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": S3: request failed\n");
		free(data.buf);
		s3_request_destroy(req);
		return 500;
	}
	curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
	if(status != 200)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": S3: request failed with HTTP status %d\n", (int) status);
		free(data.buf);
		s3_request_destroy(req);
		return (int) status;
	}
	mime = NULL;
	curl_easy_getinfo(ch, CURLINFO_CONTENT_TYPE, &mime);
	if(!mime)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": S3: server did not send a Content-Type\n");
		free(data.buf);
		s3_request_destroy(req);
		return 500;
	}	
	if(quilt_model_parse(request->model, mime, data.buf, data.pos))
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": S3: failed to parse buffer as '%s'\n", mime);
		free(data.buf);
		s3_request_destroy(req);
		return 500;
	}
	free(data.buf);
	s3_request_destroy(req);
	return spindle_item_post_(request);
}

/* Fetch additional information about an item */
static int
spindle_item_post_(QUILTREQ *request)
{
	SPARQL *sparql;
	
	sparql = quilt_sparql();
	
	if(sparql_queryf_model(sparql, request->model, "SELECT DISTINCT ?s ?p ?o ?g WHERE {\n"
						   "  GRAPH ?g {\n"
						   "    ?s <http://xmlns.com/foaf/0.1/topic> <%s#id>\n"
						   "  }"
						   "  GRAPH ?g {\n"
						   "    ?s ?p ?o\n"
						   "  }\n"
						   "  FILTER regex(str(?g), \"^%s\", \"i\")\n"
						   "}",
						   request->subject, request->base))
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": failed to retrieve related items\n");
		return 500;
	}
	
	/* Return 200, rather than 0, to auto-serialise the model */
	return 200;
}

static size_t
spindle_s3_write_(char *ptr, size_t size, size_t nemb, void *userdata)
{
	struct data_struct *data;
	char *p;

	data = (struct data_struct *) userdata;

	size *= nemb;
	if(data->pos + size >= data->size)
	{
		p = (char *) realloc(data->buf, data->size + size + 1);
		if(!p)
		{
			quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": S3: failed to expand receive buffer\n");
			return 0;
		}
		data->buf = p;
		data->size += size;
	}
	memcpy(&(data->buf[data->pos]), ptr, size);
	data->pos += size;
	data->buf[data->pos] = 0;
	return size;
}
