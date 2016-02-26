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

struct s3_upload_struct
{
	char *buf;
	size_t bufsize;
	size_t pos;
};

static int spindle_precompose_init_s3_(SPINDLEGENERATE *generate, const char *bucketname);
static int spindle_precompose_init_file_(SPINDLEGENERATE *generate, const char *path);
static size_t spindle_precompose_s3_read_(char *buffer, size_t size, size_t nitems, void *userdata);

int
spindle_precompose_init(SPINDLEGENERATE *generate)
{
	char *t;
	URI *base, *uri;
	URI_INFO *info;
	int r;
	
	t = twine_config_geta("spindle:cache", NULL);
	if(t)
	{
		base = uri_create_cwd();
		uri = uri_create_str(t, base);
		free(t);
		info = uri_info(uri);
		uri_destroy(uri);
		uri_destroy(base);
		if(!strcmp(info->scheme, "s3"))
		{
			r = spindle_precompose_init_s3_(generate, info->host);
		}
		else if(!strcmp(info->scheme, "file"))
		{
			r = spindle_precompose_init_file_(generate, info->path);
		}
		else
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": cache type '%s' is not supported\n", info->scheme);
			r = -1;
		}
		uri_info_destroy(info);
		return r;
	}
	/* For compatibility, specifying spindle:bucket=NAME is equivalent to
	 * spindle:cache=s3://NAME
	 */
	t = twine_config_geta("spindle:bucket", NULL);
	if(t)
	{
		r = spindle_precompose_init_s3_(generate, t);
		free(t);
		return r;
	}
	/* No cache configured */
	return 0;
}

/* Store pre-composed N-Quads in an S3 (or RADOS) bucket */
int
spindle_precompose_s3(SPINDLEENTRY *data, char *quadbuf, size_t bufsize)
{
	char *t;
	char *urlbuf;
	char nqlenstr[256];
	AWSREQUEST *req;
	CURL *ch;
	struct curl_slist *headers;
	struct s3_upload_struct s3data;
	int r, e;
	long status;
	
	s3data.buf = quadbuf;
	s3data.bufsize = bufsize;
	s3data.pos = 0;
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
	req = aws_s3_request_create(data->generate->bucket, urlbuf, "PUT");
	ch = aws_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, data->generate->s3_verbose);
	curl_easy_setopt(ch, CURLOPT_READFUNCTION, spindle_precompose_s3_read_);
	curl_easy_setopt(ch, CURLOPT_READDATA, &s3data);
	curl_easy_setopt(ch, CURLOPT_INFILESIZE, (long) s3data.bufsize);
	curl_easy_setopt(ch, CURLOPT_UPLOAD, 1);
	headers = curl_slist_append(aws_request_headers(req), "Expect: 100-continue");
	headers = curl_slist_append(headers, "Content-Type: application/nquads");
	headers = curl_slist_append(headers, "x-amz-acl: public-read");
	sprintf(nqlenstr, "Content-Length: %u", (unsigned) s3data.bufsize);
	headers = curl_slist_append(headers, nqlenstr);
	aws_request_set_headers(req, headers);
	r = 0;
	if((e = aws_request_perform(req)))
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
	aws_request_destroy(req);
	free(urlbuf);
	return r;
}

/* Store pre-composed N-Quads in a file */
int
spindle_precompose_file(SPINDLEENTRY *data, char *quadbuf, size_t bufsize)
{
	const char *s;
	char *path, *t;
	FILE *f;
	int r;
	
	path = (char *) calloc(1, strlen(data->generate->cachepath) + strlen(data->localname) + 8);
	if(!path)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate memory for local path buffer\n");
		return -1;
	}
	strcpy(path, data->generate->cachepath);
	if((s = strrchr(data->localname, '/')))
	{
		s++;
	}
	else
	{
		s = (char *) data->localname;
	}
	strcat(path, s);
	t = strchr(path, '#');
	if(t)
	{
		*t = 0;
	}
	f = fopen(path, "wb");
	if(!f)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to open cache file for writing: %s: %s\n", path, strerror(errno));
		free(path);
		return -1;
	}
	r = 0;
	while(bufsize)
	{
		if(bufsize > 1024)
		{
			if(fwrite(quadbuf, 1024, 1, f) != 1)
			{
				r = -1;
				break;
			}
			quadbuf += 1024;
			bufsize -= 1024;
		}
		else
		{
			if(fwrite(quadbuf, bufsize, 1, f) != 1)
			{
				r = -1;
				break;
			}
			bufsize = 0;
		}
	}
	if(r)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to write to cache file: %s: %s\n", path, strerror(errno));
		fclose(f);
		free(path);
		return -1;
	}
	fclose(f);
	free(path);
	return 0;
}

/* Initialise an S3-based cache for pre-composed N-Quads */
static int
spindle_precompose_init_s3_(SPINDLEGENERATE *generate, const char *bucketname)
{
	char *t;
	
	generate->bucket = aws_s3_create(bucketname);
	if(!generate->bucket)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create S3 bucket object for <s3://%s>\n", bucketname);
		return -1;
	}
	aws_s3_set_logger(generate->bucket, twine_vlogf);
	if((t = twine_config_geta("s3:endpoint", NULL)))
	{
		aws_s3_set_endpoint(generate->bucket, t);
		free(t);
	}
	if((t = twine_config_geta("s3:access", NULL)))
	{
		aws_s3_set_access(generate->bucket, t);
		free(t);
	}
	if((t = twine_config_geta("s3:secret", NULL)))
	{
		aws_s3_set_secret(generate->bucket, t);
		free(t);
	}
	generate->s3_verbose = twine_config_get_bool("s3:verbose", 0);
	return 0;
}

/* Initialise a filesystem-based cache for pre-composed N-Quads */
static int
spindle_precompose_init_file_(SPINDLEGENERATE *generate, const char *path)
{
	char *t;
	
	if(mkdir(path, 0777))
	{
		if(errno != EEXIST)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to create cache directory: %s: %s\n", path, strerror(errno));
			return -1;
		}
	}
	generate->cachepath = (char *) calloc(1, strlen(path) + 2);
	if(!generate->cachepath)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate memory for path buffer\n");
		return -1;
	}
	strcpy(generate->cachepath, path);
	if(path[0])
	{
		t = strchr(generate->cachepath, 0);
		t--;
		if(*t != '/')
		{
			t++;
			*t = '/';
			t++;
			*t = 0;
		}
	}
	return 0;
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
