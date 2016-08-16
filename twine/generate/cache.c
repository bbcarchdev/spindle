/* Spindle: Co-reference aggregation engine
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2016 BBC
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

/* Functions for storing and retrieving sets of quads in a cache */

struct s3_upload_struct
{
	char *buf;
	size_t bufsize;
	size_t pos;
};

static int spindle_cache_init_s3_(SPINDLEGENERATE *generate, const char *bucketname);
static int spindle_cache_init_file_(SPINDLEGENERATE *generate, const char *path);
static int spindle_cache_store_s3_buf_(SPINDLEENTRY *data, const char *suffix, char *quadbuf, size_t bufsize);
static int spindle_cache_store_file_buf_(SPINDLEENTRY *data, const char *suffix, char *quadbuf, size_t bufsize);
static int spindle_cache_fetch_s3_(SPINDLEENTRY *data, const char *suffix, char **quadbuf, size_t *bufsize);
static int spindle_cache_fetch_file_(SPINDLEENTRY *data, const char *suffix, char **quadbuf, size_t *bufsize);
static size_t spindle_cache_s3_upload_(char *buffer, size_t size, size_t nitems, void *userdata);
static size_t spindle_cache_s3_download_(char *buffer, size_t size, size_t nitems, void *userdata);
static char *spindle_cache_filename_(SPINDLEENTRY *data, const char *suffix);
static char *spindle_cache_s3path_(SPINDLEENTRY *data, const char *suffix);

int
spindle_cache_init(SPINDLEGENERATE *generate)
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
			r = spindle_cache_init_s3_(generate, info->host);
		}
		else if(!strcmp(info->scheme, "file"))
		{
			r = spindle_cache_init_file_(generate, info->path);
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
		r = spindle_cache_init_s3_(generate, t);
		free(t);
		return r;
	}
	/* No cache configured */
	return 0;
}

/* Store an RDF model as N-Quads in the cache (if available) */
int
spindle_cache_store(SPINDLEENTRY *data, const char *suffix, librdf_model *model)
{
	char *buf;
	size_t bufsize;
	int r;
	
	if(!data->generate->bucket && !data->generate->cachepath)
	{
		/* No cache available */
		return 0;
	}
	buf = twine_rdf_model_nquads(model, &bufsize);
	if(!buf)
	{
		return -1;
	}
	r = spindle_cache_store_buf(data, suffix, buf, bufsize);
	librdf_free_memory(buf);
	return r;
}

/* Store a pre-composed buffer of N-Quads in the cache (if available) */
int
spindle_cache_store_buf(SPINDLEENTRY *data, const char *suffix, char *quadbuf, size_t bufsize)
{
	if(data->generate->bucket)
	{
		return spindle_cache_store_s3_buf_(data, suffix, quadbuf, bufsize);
	}
	if(data->generate->cachepath)
	{
		return spindle_cache_store_file_buf_(data, suffix, quadbuf, bufsize);
	}
	/* No cache available */
	return 0;
}

/* Retrieve N-Quads from the cache, if available */
int
spindle_cache_fetch(SPINDLEENTRY *data, const char *suffix, librdf_model *destmodel)
{
	char *buf;
	size_t bufsize;
	int r;
	
	buf = NULL;
	bufsize = 0;
	if(data->generate->bucket)
	{
		r = spindle_cache_fetch_s3_(data, suffix, &buf, &bufsize);
	}
	else if(data->generate->cachepath)
	{
		r = spindle_cache_fetch_file_(data, suffix, &buf, &bufsize);
	}
	else
	{
		/* No cache available */
		return 0;
	}
	if(r < 0)
	{
		free(buf);
		return -1;
	}
	if(bufsize)
	{
		/* Parse N-Quads */
		r = twine_rdf_model_parse(destmodel, MIME_NQUADS, buf, bufsize);
	}
	else
	{
		return 0;
	}
	free(buf);
	if(r)
	{
		return -1;
	}
	return 1;
}

static char *
spindle_cache_s3path_(SPINDLEENTRY *data, const char *suffix)
{
	char *t, *urlbuf;
	size_t l;
	
	l = 1 + strlen(data->localname) + 4 + 1;
	if(suffix)
	{
		l += strlen(suffix) + 1;
	}
	urlbuf = (char *) malloc(l);
	if(!urlbuf)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate memory for URL\n");
		return NULL;
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
	else
	{
		t = strchr(urlbuf, 0);
	}
	if(suffix)
	{
		*t = '.';
		t++;
		strcpy(t, suffix);
	}
	return urlbuf;
}

/* Store pre-composed N-Quads in an S3 (or RADOS) bucket */
static int
spindle_cache_store_s3_buf_(SPINDLEENTRY *data, const char *suffix, char *quadbuf, size_t bufsize)
{
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
	urlbuf = spindle_cache_s3path_(data, suffix);
	if(!urlbuf)
	{
		return -1;
	}
	req = aws_s3_request_create(data->generate->bucket, urlbuf, "PUT");
	ch = aws_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, data->generate->s3_verbose);
	curl_easy_setopt(ch, CURLOPT_READFUNCTION, spindle_cache_s3_upload_);
	curl_easy_setopt(ch, CURLOPT_READDATA, &s3data);
	curl_easy_setopt(ch, CURLOPT_INFILESIZE, (long) s3data.bufsize);
	curl_easy_setopt(ch, CURLOPT_UPLOAD, 1);
	headers = curl_slist_append(aws_request_headers(req), "Expect: 100-continue");
	headers = curl_slist_append(headers, "Content-Type: " MIME_NQUADS);
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

/* Fetch a set of N-Quads from an S3/RADOS bucket */
static int
spindle_cache_fetch_s3_(SPINDLEENTRY *entry, const char *suffix, char **quadbuf, size_t *quadbuflen)
{
	AWSREQUEST *req;
	CURL *ch;
	struct curl_slist *headers;
	struct s3_upload_struct s3data;
	int r, e;
	long status;
	char *urlbuf;
	
	*quadbuf = NULL;
	*quadbuflen = 0;
	memset(&s3data, 0, sizeof(struct s3_upload_struct));
	urlbuf = spindle_cache_s3path_(entry, suffix);
	if(!urlbuf)
	{
		return -1;
	}
	req = aws_s3_request_create(entry->generate->bucket, urlbuf, "GET");
	ch = aws_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, entry->generate->s3_verbose);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, spindle_cache_s3_download_);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, &s3data);
	headers = curl_slist_append(aws_request_headers(req), "Expect: 100-continue");
	headers = curl_slist_append(headers, "Accept: " MIME_NQUADS);
	aws_request_set_headers(req, headers);
	r = 1;
	if((e = aws_request_perform(req)))
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": failed to download N-Quads from bucket at <%s>: %s\n", urlbuf, curl_easy_strerror(e));
		r = -1;
	}
	else
	{
		curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
		if(status == 404 || status == 403)
		{
			r = 0;
		}
		else if(status != 200)
		{
			twine_logf(LOG_ERR, PLUGIN_NAME ": failed to download N-Quads from bucket at <%s> (HTTP status %ld)\n", urlbuf, status);
			r = -1;
		}
	}
	aws_request_destroy(req);
	free(urlbuf);
	if(r > 0)
	{
		*quadbuf = s3data.buf;
		*quadbuflen = s3data.pos;
	}
	else
	{
		free(s3data.buf);
	}
	return r;
}

static char *
spindle_cache_filename_(SPINDLEENTRY *data, const char *suffix)
{
	size_t l;
	const char *s;
	char *path, *t;
	
	l = strlen(data->generate->cachepath) + strlen(data->localname) + 8;
	path = (char *) calloc(1, l);
	if(!path)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate memory for local path buffer\n");
		return NULL;
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
	else
	{
		t = strchr(path, 0);
	}
	if(suffix)
	{
		*t = '.';
		t++;
		strcpy(t, suffix);
	}
	twine_logf(LOG_DEBUG, "cache filename for %s + %s is <%s>\n", data->localname, suffix, path);
	return path;
}

/* Store pre-composed N-Quads in a file */
static int
spindle_cache_store_file_buf_(SPINDLEENTRY *data, const char *suffix, char *quadbuf, size_t bufsize)
{
	FILE *f;
	int r;
	char *path;
	
	path = spindle_cache_filename_(data, suffix);
	if(!path)
	{
		return -1;
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

/* Fetch a set of N-Quads from a file on disk */
static int
spindle_cache_fetch_file_(SPINDLEENTRY *entry, const char *suffix, char **quadbuf, size_t *quadbuflen)
{
	char *path, *buffer, *p;
	FILE *f;
	size_t bufsize, buflen;
	ssize_t r;
	
	path = spindle_cache_filename_(entry, suffix);
	if(!path)
	{
		return -1;
	}
	f = fopen(path, "rb");
	if(!f)
	{
		if(errno == ENOENT)
		{
			free(path);
			return 0;
		}
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to open cache file for reading: %s: %s\n", path, strerror(errno));
		free(path);
		return -1;
	}
	r = 0;
	buffer = NULL;
	bufsize = 0;
	buflen = 0;
	while(!feof(f))
	{
		if(bufsize - buflen < 1024)
		{
			p = (char *) realloc(buffer, bufsize + 1024);
			if(!p)
			{
				twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to reallocate buffer from %u bytes to %u bytes\n", (unsigned) bufsize, (unsigned) bufsize + 1024);
				free(buffer);
				return -1;
			}
			buffer = p;
			bufsize += 1024;
		}
		r = fread(&(buffer[buflen]), 1, 1023, f);
		if(r < 0)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": error reading from '%s': %s\n", path, strerror(errno));
			free(buffer);
			free(path);
			return -1;
		}
		buflen += r;
		buffer[buflen] = 0;
	}
	fclose(f);
	*quadbuf = buffer;
	*quadbuflen = buflen;
	return 1;
}

/* Initialise an S3-based cache for pre-composed N-Quads */
static int
spindle_cache_init_s3_(SPINDLEGENERATE *generate, const char *bucketname)
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
spindle_cache_init_file_(SPINDLEGENERATE *generate, const char *path)
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
spindle_cache_s3_upload_(char *buffer, size_t size, size_t nitems, void *userdata)
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

static size_t
spindle_cache_s3_download_(char *buffer, size_t size, size_t nitems, void *userdata)
{
	struct s3_upload_struct *data;
	char *p;
	
	data = (struct s3_upload_struct *) userdata;
	size *= nitems;
	if(data->pos + size >= data->bufsize)
	{
		p = (char *) realloc(data->buf, data->bufsize + size + 1);
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": S3: failed to expand receive buffer\n");
			return 0;
		}
		data->buf = p;
		data->bufsize += size;
	}
	memcpy(&(data->buf[data->pos]), buffer, size);
	data->pos += size;
	data->buf[data->pos] = 0;
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": S3: written %lu bytes\n", (unsigned long) size);
	return size;
}
