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


/* Fetch an item by retrieving triples or quads from the on-disk cache */
int
spindle_item_file(QUILTREQ *request)
{
	char *p;
	char *buf, *buffer;
	FILE *f;
	ssize_t r;
	size_t bufsize, buflen;
	
	quilt_canon_add_path(request->canonical, request->path);
	quilt_canon_set_fragment(request->canonical, "id");
	/* Perform a basic sanity-check on the path */
	if(request->path[0] != '/' ||
		strchr(request->path, '.') ||
		strchr(request->path, '%'))
	{
		return 404;
	}
	for(p = request->path; *p && *p == '/'; p++);
	if(!*p)
	{
		return 404;
	}
	if(strchr(p, '/'))
	{
		return 404;
	}
	buf = (char *) calloc(1, strlen(spindle_cachepath) + strlen(p) + 16);
	if(!buf)
	{
		return 500;
	}
	strcpy(buf, spindle_cachepath);
	strcat(buf, p);
	f = fopen(buf, "rb");
	if(!f)
	{
		quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to open cache file for reading: %s: %s\n", buf, strerror(errno));
		free(buf);
		return 404;
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
				quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": failed to reallocate buffer from %u bytes to %u bytes\n", (unsigned) bufsize, (unsigned) bufsize + 1024);
				free(buffer);
				free(buf);
				fclose(f);
				return 500;
			}
			buffer = p;
			bufsize += 1024;
		}
		r = fread(&(buffer[buflen]), 1, 1023, f);
		if(r < 0)
		{
			quilt_logf(LOG_CRIT, QUILT_PLUGIN_NAME ": error reading from '%s': %s\n", buf, strerror(errno));
			free(buffer);
			free(buf);
			fclose(f);
			return 500;
		}
		buflen += r;
		buffer[buflen] = 0;
	}
	fclose(f);
	if(quilt_model_parse(request->model, MIME_NQUADS, buffer, buflen))
	{
		quilt_logf(LOG_ERR, QUILT_PLUGIN_NAME ": file: failed to parse buffer from %s as '%s'\n", buf, MIME_NQUADS);
		free(buf);
		free(buffer);
		return 500;
	}
	free(buffer);
	free(buf);
	return 200;
}