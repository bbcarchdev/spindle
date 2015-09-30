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

struct s3_upload_struct
{
	char *buf;
	size_t bufsize;
	size_t pos;
};

static int spindle_precompose_s3_(SPINDLECACHE *data, char *quadbuf, size_t bufsize);
static size_t spindle_precompose_s3_read_(char *buffer, size_t size, size_t nitems, void *userdata);


/* Generate a set of pre-composed N-Quads representing an entity we have
 * indexed and write it to a location for the Quilt module to be able to
 * read.
 */

int
spindle_precompose(SPINDLECACHE *data)
{
	char *proxy, *source, *extra, *buf;
	size_t bufsize, proxylen, sourcelen, extralen, l;
	int r;
	
	/* If there's no S3 bucket, this is a no-op */
	if(!data->spindle->bucket)
	{
		return 0;
	}
	if(data->spindle->multigraph)
	{
		/* Remove the root graph from the proxy data model, if it's present */
		librdf_model_context_remove_statements(data->proxydata, data->spindle->rootgraph);
	}
	proxy = twine_rdf_model_nquads(data->proxydata, &proxylen);
	if(!proxy)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to serialise proxy model as N-Quads\n");
		return -1;
	}
	source = twine_rdf_model_nquads(data->sourcedata, &sourcelen);
	if(!source)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to serialise source model as N-Quads\n");
		librdf_free_memory(proxy);
		return -1;
	}

	extra = twine_rdf_model_nquads(data->extradata, &extralen);
	if(!extra)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to serialise extra model as N-Quads\n");
		librdf_free_memory(proxy);
		librdf_free_memory(source);
		return -1;
	}
	bufsize = proxylen + sourcelen + extralen + 3 + 128;
	buf = (char *) calloc(1, bufsize + 1);
	if(!buf)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for consolidated N-Quads\n");
		librdf_free_memory(proxy);
		librdf_free_memory(source);
		librdf_free_memory(extra);
		return -1;
	}
	strcpy(buf, "## Proxy:\n");
	l = strlen(buf);

	if(proxylen)
	{
		memcpy(&(buf[l]), proxy, proxylen);
	}
	strcpy(&(buf[l + proxylen]), "\n## Source:\n");
	l = strlen(buf);

	if(sourcelen)
	{
		memcpy(&(buf[l]), source, sourcelen);
	}
	strcpy(&(buf[l + sourcelen]), "\n## Extra:\n");
	l = strlen(buf);
	
	if(extralen)
	{
		memcpy(&(buf[l]), extra, extralen);
	}
	strcpy(&(buf[l + extralen]), "\n## End\n");
	bufsize = strlen(buf);

	librdf_free_memory(proxy);
	librdf_free_memory(source);
	librdf_free_memory(extra);
	
	r = spindle_precompose_s3_(data, buf, bufsize);

	free(buf);
	return r;
}

/* Store pre-composed N-Quads in an S3 (or RADOS) bucket */
static int
spindle_precompose_s3_(SPINDLECACHE *data, char *quadbuf, size_t bufsize)
{
	char *t;
	char *urlbuf;
	char nqlenstr[256];
	S3REQUEST *req;
	CURL *ch;
	struct curl_slist *headers;
	struct s3_upload_struct s3data;
	int r, e;
	long status;
	
	s3data.buf = quadbuf;
	s3data.bufsize = bufsize;
	urlbuf = (char *) malloc(1 + strlen(data->localname) + 4 + 1);
	if(!urlbuf)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate memory for URL\n");
		return -1;
	}
	urlbuf[0] = '/';
	if((t = strrchr(data->localname, '/')))
	{
		t++;
	}
	else
	{
		t = (char *) data->localname;
	}
	strcpy(&(urlbuf[1]), t);
	if((t = strchr(urlbuf, '#')))
	{
		*t = 0;
	}
	req = s3_request_create(data->spindle->bucket, urlbuf, "PUT");
	ch = s3_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, data->spindle->s3_verbose);
	curl_easy_setopt(ch, CURLOPT_READFUNCTION, spindle_precompose_s3_read_);
	curl_easy_setopt(ch, CURLOPT_READDATA, &s3data);
	curl_easy_setopt(ch, CURLOPT_INFILESIZE, (long) s3data.bufsize);
	curl_easy_setopt(ch, CURLOPT_UPLOAD, 1);
	headers = curl_slist_append(s3_request_headers(req), "Expect: 100-continue");
	headers = curl_slist_append(headers, "Content-Type: application/nquads");
	headers = curl_slist_append(headers, "x-amz-acl: public-read");
	sprintf(nqlenstr, "Content-Length: %u", (unsigned) s3data.bufsize);
	headers = curl_slist_append(headers, nqlenstr);
	s3_request_set_headers(req, headers);
	r = 0;
	if((e = s3_request_perform(req)))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to upload N-Quads to bucket at <%s>: %s\n", urlbuf, curl_easy_strerror(e));
		r = -1;
	}
	else
	{
		curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
		if(status != 200)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to upload N-Quads to bucket at <%s> (HTTP status %ld)\n", urlbuf, status);
			r = -1;
		}
	}
	s3_request_destroy(req);
	free(urlbuf);
	return r;
}

static size_t
spindle_precompose_s3_read_(char *buffer, size_t size, size_t nitems, void *userdata)
{
	struct s3_upload_struct *data;

	data = (struct s3_upload_struct *) userdata;
	size *= nitems;
	if(size > data->bufsize - data->pos)
	{
		size = data->bufsize - data->pos;
	}
	memcpy(buffer, &(data->buf[data->pos]), size);
	data->pos += size;
	return size;
}
