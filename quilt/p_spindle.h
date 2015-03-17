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

#ifndef P_SPINDLE_H_
# define P_SPINDLE_H_                  1

# include <stdlib.h>
# include <string.h>
# include <libsparqlclient.h>
# include <libs3client.h>

# include "libquilt.h"

# define QUILT_PLUGIN_NAME              "spindle"

struct index_struct
{
	const char *uri;
	const char *title;
	const char *qclass;
};

extern S3BUCKET *spindle_bucket;
extern int spindle_s3_verbose;
extern struct index_struct spindle_indices[];

int spindle_process(QUILTREQ *request);
int spindle_index(QUILTREQ *req, const char *qclass);
int spindle_home(QUILTREQ *req);
int spindle_item(QUILTREQ *req);
int spindle_item_s3(QUILTREQ *req);
int spindle_lookup(QUILTREQ *req, const char *uri);

#endif /*!P_SPINDLE_H_*/
