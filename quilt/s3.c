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

extern long spindle_s3_fetch_limit;

struct data_struct
{
	char *buf;
	size_t size;
	size_t pos;
};

static size_t spindle_s3_write_(char *ptr, size_t size, size_t nemb, void *userdata);

/* Fetch an item by retrieving triples or quads from an S3 bucket */
int
spindle_item_s3(QUILTREQ *request)
{
	AWSREQUEST *req;
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
	quilt_canon_add_path(request->canonical, request->path);
	quilt_canon_set_fragment(request->canonical, "id");
	memset(&data, 0, sizeof(struct data_struct));
	req = aws_s3_request_create(spindle_bucket, request->path, "GET");
	if(!req)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": S3: failed to create S3 request\n");
		return 500;
	}
	ch = aws_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_HEADER, 0);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, spindle_s3_verbose);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *) &data);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, spindle_s3_write_);
	if(aws_request_perform(req) != CURLE_OK)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": S3: request failed\n");
		free(data.buf);
		aws_request_destroy(req);
		return 500;
	}
	if(curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status) != CURLE_OK)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": S3: failed to obtain HTTP status code\n");
		free(data.buf);
		aws_request_destroy(req);
		return -1;
	}		
	if(status != 200)
	{
		if(!status)
		{
			if(curl_easy_getinfo(ch, CURLINFO_OS_ERRNO, &status) != CURLE_OK)
			{		
				quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": S3: failed to obtain OS-level error code for request\n");
			}
			else
			{
				quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": S3: request failed: %s\n", strerror(status));
			}
			status = -1;
		}
		else
		{
			quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": S3: request failed with HTTP status %d\n", (int) status);
		}
		free(data.buf);
		aws_request_destroy(req);
		return (int) status;
	}
	mime = NULL;
	curl_easy_getinfo(ch, CURLINFO_CONTENT_TYPE, &mime);
	if(!mime)
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": S3: server did not send a Content-Type\n");
		free(data.buf);
		aws_request_destroy(req);
		return 500;
	}	
	if(quilt_model_parse(request->model, mime, data.buf, data.pos))
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": S3: failed to parse buffer as '%s'\n", mime);
		free(data.buf);
		aws_request_destroy(req);
		return 500;
	}
	free(data.buf);
	aws_request_destroy(req);
	return 200;
}

static size_t
spindle_s3_write_(char *ptr, size_t size, size_t nemb, void *userdata)
{
	struct data_struct *data;
	char *p;

	data = (struct data_struct *) userdata;

	size *= nemb;

	if(spindle_s3_fetch_limit && size > spindle_s3_fetch_limit)
	{
		quilt_logf(LOG_WARNING, QUILT_PLUGIN_NAME ": S3: failing write due to size exceeding fetch limit. input_size:%u > fetch_limit:%u \n", size, spindle_s3_fetch_limit);
		return 0;
	}

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
	quilt_logf(LOG_DEBUG, QUILT_PLUGIN_NAME ": S3: written %lu bytes\n", (unsigned long) size);
	return size;
}
