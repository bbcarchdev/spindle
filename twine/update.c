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

int
spindle_update(const char *name, const char *identifier, void *data)
{
	SPINDLE *spindle;
	char uuid[40];
	char *idbuf, *p;
	int c, r;
	const char *t;

	(void) name;

	spindle = (SPINDLE *) data;
	
	/* If identifier is a string of 32 hex-digits, possibly including hyphens,
	 * then we can prefix it with the root and suffix it with #id to form
	 * a real identifer.
	 */
	c = 0;
	for(t = identifier; *t && c < 32; t++)
	{
		if(isxdigit(*t))
		{
			uuid[c] = tolower(*t);
			c++;
		}
		else if(*t == '-')
		{
			continue;
		}
		else
		{
			break;
		}
	}
	uuid[c] = 0;
	if(!*t && c == 32)
	{
		/* XXX the fragment should be configurable */
		idbuf = (char *) malloc(strlen(spindle->root) + 1 + 32 + 3 + 1);
		if(!idbuf)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate buffer for local identifier\n");
			return -1;
		}
		strcpy(idbuf, spindle->root);
		p = strchr(idbuf, 0);
		if(p > idbuf)
		{
			p--;
			if(*p == '/')
			{
				p++;
			}
			else
			{
				p++;
				*p = '/';
				p++;
			}
		}
		else
		{
			*p = '/';
			p++;
		}
		strcpy(p, uuid);
		p += 32;
		*p = '#';
		p++;
		*p = 'i';
		p++;
		*p = 'd';
		p++;
		*p = 0;
		identifier = idbuf;
	}
	r = spindle_cache_update(spindle, identifier, NULL);	
	if(r)
	{
		twine_logf(LOG_ERR, PLUGIN_NAME ": update failed for <%s>\n", identifier);
	}
	else
	{
		twine_logf(LOG_NOTICE, PLUGIN_NAME ": update complete for <%s>\n", identifier);
	}
	free(idbuf);
	return r;
}
