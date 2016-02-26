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

#include "p_spindle-correlate.h"

static int spindle_coref_add_(struct spindle_corefset_struct *set, const char *l, const char *r);

/* Extract a list of co-references from a librdf model */
struct spindle_corefset_struct *
spindle_coref_extract(SPINDLE *spindle, librdf_model *model, const char *graphuri)
{
	struct spindle_corefset_struct *set;
	librdf_statement *query, *st;
	librdf_stream *stream;
	librdf_node *subj, *obj, *pred;
	librdf_uri *uri;
	unsigned char *l, *r;
	size_t c;

	(void) graphuri;

	set = (struct spindle_corefset_struct *) calloc(1, sizeof(struct spindle_corefset_struct));
	if(!set)
	{
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to allocate memory for new coreference set\n");
		return NULL;
	}
	/* Loop through each of the co-referencing predicates */
	for(c = 0; c < spindle->rules->corefcount; c++)
	{
		query = librdf_new_statement(spindle->world);
		pred = librdf_new_node_from_uri_string(spindle->world, (const unsigned char *) spindle->rules->coref[c].predicate);
		librdf_statement_set_predicate(query, pred);
		/* pred is now owned by query */
		pred = NULL;
		stream = librdf_model_find_statements(model, query);
		while(!librdf_stream_end(stream))
		{
			st = librdf_stream_get_object(stream);
			subj = librdf_statement_get_subject(st);
			obj = librdf_statement_get_object(st);
			if(librdf_node_is_resource(subj) && librdf_node_is_resource(obj))
			{
				uri = librdf_node_get_uri(subj);
				l = librdf_uri_as_string(uri);
				uri = librdf_node_get_uri(obj);
				r = librdf_uri_as_string(uri);
/*				twine_logf(LOG_DEBUG, PLUGIN_NAME ": found coref (<%s>, <%s>) with <%s>\n", (const char *) l, (const char *) r, spindle->coref[c].predicate); */
				if(spindle->rules->coref[c].callback(set, (const char *) l, (const char *) r))
				{
					spindle_coref_destroy(set);
					set = NULL;
					break;
				}
			}
			librdf_stream_next(stream);
		}
		librdf_free_stream(stream);
		librdf_free_statement(query);
		if(!set)
		{
			return NULL;
		}
	}
	/* Now, find all of the subjects in the graph and add them as dangling
	 * references if they don't already exist: this ensures that they'll
	 * still get processed even there aren't actually any co-references
	 */
	query = librdf_new_statement(spindle->world);
	stream = librdf_model_find_statements(model, query);
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		subj = librdf_statement_get_subject(st);
		if(librdf_node_is_resource(subj) &&
		   (uri = librdf_node_get_uri(subj)) &&
		   (l = librdf_uri_as_string(uri)))
		{
/*			twine_logf(LOG_DEBUG, PLUGIN_NAME ": adding subject <%s>\n", (const char *) l); */
			if(spindle_coref_add_(set, (const char *) l, NULL))
			{
				spindle_coref_destroy(set);
				set = NULL;
				break;
			}
		}
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	librdf_free_statement(query);
	return set;
}

/* Free the resources used by a co-reference set */
int
spindle_coref_destroy(struct spindle_corefset_struct *set)
{
	size_t c;

	for(c = 0; c < set->refcount; c++)
	{
		free(set->refs[c].left);
		free(set->refs[c].right);
	}
	free(set->refs);
	free(set);
	return 0;
}

/* Callback invoked when owl:sameAs references are found */
int
spindle_match_sameas(struct spindle_corefset_struct *set, const char *subject, const char *object)
{
	if(spindle_coref_add_(set, subject, object))
	{
		return -1;
	}
	return 0;		   	
}

int
spindle_match_wikipedia(struct spindle_corefset_struct *set, const char *subject, const char *object)
{
	char *buf;

	if(strncmp(object, "http://en.wikipedia.org/wiki/", 29))
	{
		return 0;
	}
	object += 29;
	buf = (char *) malloc(strlen(object) + 29);
	if(!buf)
	{
		return -1;
	}
	strcpy(buf, "http://dbpedia.org/resource/");
	strcat(buf, object);
	if(spindle_coref_add_(set, subject, buf))
	{
		free(buf);
		return -1;
	}
	free(buf);
	return 0;
}

/* Add a single co-reference to a set (or if r is NULL, a lone subject) */
static int
spindle_coref_add_(struct spindle_corefset_struct *set, const char *l, const char *r)
{
	struct spindle_coref_struct *p;
	size_t c;

	for(c = 0; c < set->refcount; c++)
	{
		if(!strcmp(l, set->refs[c].left) &&
		   (!r || !strcmp(r, set->refs[c].right)))
		{
/*			twine_logf(LOG_DEBUG, "(reference already exists)\n"); */
			return 0;
		}
	}
	if(set->refcount >= set->size)
	{
		p = (struct spindle_coref_struct *) realloc(set->refs, sizeof(struct spindle_coref_struct) * (set->size + SET_BLOCKSIZE));
		if(!p)
		{
			twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to expand size of coreference set\n");
			return -1;
		}
		set->refs = p;
		set->size += SET_BLOCKSIZE;
	}
	p = &(set->refs[set->refcount]);
	memset(p, 0, sizeof(struct spindle_coref_struct));
	p->left = strdup(l);
	if(r)
	{
		p->right = strdup(r);
	}
	if(!p->left || (r && !p->right))
	{
		free(p->left);
		free(p->right);
		twine_logf(LOG_CRIT, PLUGIN_NAME ": failed to duplicate URIs when adding coreference to set\n");
		return -1;
	}
	set->refcount++;
/*	twine_logf(LOG_DEBUG, "refcount=%d, (added %s = %s)\n", (int) set->refcount, p->left, p->right); */
	return 0;
}
