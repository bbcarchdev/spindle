/* This engine processes requests for coreference graphs populated
 * by Twine's "spindle" post-processing module.
 *
 * Author: Prashanth devarajappa <prashanth.devarajappa.ext@bbc.co.uk>
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

#include "p_spindle.h"

struct index_struct *spindle_partitions;

static int spindle_part_read_cb_(const char *key, const char *value, void *data);
static int spindle_part_add_(const char* slug);
static int spindle_part_read_class_cb_(const char *key, const char *value, void *data);
static size_t part_size;
static const char *part_key = "spindle:partitions:";

// helpers
static int spindle_part_not_seen_(const char* slug);
static struct index_struct* locate_partition_(const char* uri);
static struct index_struct* allocate_partition_(const char* uri);
static void spindle_partition_print_(int status);

int
spindle_partition_init(void)
{
	// Read partition section(s) defined in quilt.conf into spindle_partitions
	int status = quilt_config_get_all(NULL, NULL, spindle_part_read_cb_, NULL);

	spindle_partition_print_(status);

	// TODO : Why is status 37/0x25 ?
	return 0;
}


static int
spindle_part_read_cb_(const char *key, const char *value, void *data)
{
	(void)value;
	(void)data;

	// look for partition section(s) is [spindle:partitions:$slug] with in config.
	size_t len = strlen(part_key);

	if(strncmp(key, part_key, len))
	{
		return 0;
	}

	key += len;

	if(strchr(key, ':'))
	{
		// it must be partition's data members.
		return 0;
	}
	else
	{
		// we must have found the start of partition section
		return spindle_part_add_(key);
	}
}

static int
spindle_part_add_(const char* slug)
{
	if(spindle_part_not_seen_(slug) != 0)
	{
		quilt_logf(LOG_INFO, "spindle_part_add_ already seen slug %s\n", slug);
		return 0;
	}

	size_t mem_len = strlen(part_key) + strlen(slug) + 1;

	char *section = (char*) malloc(mem_len);

	if(!section)
	{
		quilt_logf(LOG_CRIT, "failed to allocate %lu bytes spindle partition string buffer\n", (unsigned long) mem_len);
		return -1;
	}

	strcpy(section, part_key);
	strcat(section, slug);
	section[mem_len-1] = 0;

	quilt_config_get_all(section, NULL, spindle_part_read_class_cb_, section);

	free(section);
	return 0;
}

static int
spindle_part_read_class_cb_(const char *key, const char *value, void *data)
{
	char *t, *uri;
	struct index_struct *index;

	//quilt_logf(LOG_CRIT, "SPI : spindle_part_read_class_cb_ %s = %s\n",key, value);

	if(data)
	{
		char *part = (char*)data;

		if(strstr(key, part))
		{
			if((t = strrchr(part, ':')))
			{
				uri = ++t;

				// Include for ':' as well
				key += (strlen(part) + 1);

				index = locate_partition_(uri);

				if(!index)
				{
					if(!(index = allocate_partition_(uri)))
					{
						return -1;
					}
				}

				if(!strcmp(key, "title"))
				{
					// Assign only when its not already
					if(!index->title)
					{
						index->title = strdup(value);
					}
				}
				else if(!strcmp(key, "class"))
				{
					if(!index->qclass)
					{
						index->qclass = strdup(value);
					}
				}
			}
		}
	}

	return 0;
}

static int
spindle_part_not_seen_(const char* slug)
{
	if(locate_partition_(slug))
	{
		//quilt_logf(LOG_INFO, "spindle_part_not_seen_ already seen %s\n",slug);
		return -1;
	}
	return 0;
}

static struct index_struct*
locate_partition_(const char* uri)
{
	struct index_struct *index = NULL;

	for(size_t i=0; i < part_size; ++i)
	{
		// uri starts with '/'
		if(!strcmp(&spindle_partitions[i].uri[1], uri))
		{
			index = &spindle_partitions[i];
		}
	}

	return index;
}

static struct index_struct*
allocate_partition_(const char* uri)
{
	char *uri_local;
	struct index_struct *p = (struct index_struct *) realloc(spindle_partitions, sizeof(struct index_struct) * (part_size + 1));

	if(!p)
	{
		return p;
	}

	// 1 byte for '/' and 1 for terminator
	uri_local = (char*) malloc(strlen(uri) + 2);

	if(!uri_local)
	{
		quilt_logf(LOG_CRIT, "Failed to allocate %ul bytes for string buffer\n", (unsigned long) (strlen(uri) + 2));

		// TODO : what about the realloc above ?
		return NULL;
	}

	strcpy(uri_local, "/");
	strcat(uri_local, uri);

	spindle_partitions = p;
	spindle_partitions[part_size].uri = strdup(uri_local);
	spindle_partitions[part_size].title = NULL;
	spindle_partitions[part_size].qclass = NULL;
	free(uri_local);

	p = &spindle_partitions[part_size];
	part_size++;

	return p;
}

static void
spindle_partition_print_(int status)
{
	quilt_logf(LOG_CRIT, "\n+++++++++++++++++++++++++++++++++\n");
	quilt_logf(LOG_CRIT, "status %d\n", status);
	for(size_t i = 0; i < part_size; ++i)
	{
		quilt_logf(LOG_CRIT, "uri = %s;\t title = %s;\t qclass = %s\n",
				spindle_partitions[i].uri, spindle_partitions[i].title, spindle_partitions[i].qclass);
	}
	quilt_logf(LOG_CRIT, "\n+++++++++++++++++++++++++++++++++\n");
}
