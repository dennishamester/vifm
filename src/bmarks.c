/* vifm
 * Copyright (C) 2015 xaizek.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "bmarks.h"

#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* free() realloc() */
#include <string.h> /* strcmp() strdup() strlen() strncmp() strstr() */
#include <time.h> /* time_t time() */

#include "engine/completion.h"
#include "utils/str.h"
#include "utils/string_array.h"

/* Single bookmark representation. */
typedef struct
{
	char *path;       /* Path to the directory. */
	char *tags;       /* Comma-seperated list of tags. */
	time_t timestamp; /* Last bookmark update time (-1 means "never"). */
}
bmark_t;

static int validate_tags(const char tags[]);
static int change_bmark(const char path[], const char tags[], time_t timestamp,
		int *ret);
static int add_bmark(const char path[], const char tags[], time_t timestamp);

/* Array of the bookmarks. */
static bmark_t *bmarks;
/* Current number of bookmarks. */
static size_t bmark_count;

int
bmarks_set(const char path[], const char tags[])
{
	return bmarks_setup(path, tags, time(NULL));
}

int
bmarks_setup(const char path[], const char tags[], time_t timestamp)
{
	int ret;

	if(validate_tags(tags) != 0)
	{
		return 1;
	}

	if(change_bmark(path, tags, timestamp, &ret) == 0)
	{
		return ret;
	}

	return add_bmark(path, tags, timestamp);
}

/* Validates list of tags.  Returns zero if tags are well-formed and non-zero
 * otherwise. */
static int
validate_tags(const char tags[])
{
	return tags[0] == '\0'
	    || starts_with_lit(tags, ",") || strstr(tags, ",,") != NULL
	    || ends_with(tags, ",");
}

void
bmarks_remove(const char path[])
{
	int ret;
	(void)change_bmark(path, "", time(NULL), &ret);
}

/* Changes value of existing bookmark.  When value was found *ret is set to
 * error code.  Returns non-zero if value was found and zero otherwise. */
static int
change_bmark(const char path[], const char tags[], time_t timestamp, int *ret)
{
	size_t i;

	/* Try to update tags of an existing bookmark. */
	for(i = 0U; i < bmark_count; ++i)
	{
		if(strcmp(path, bmarks[i].path) == 0)
		{
			*ret = replace_string(&bmarks[i].tags, tags);
			if(*ret == 0)
			{
				bmarks[i].timestamp = timestamp;
			}
			return 0;
		}
	}

	return 1;
}

/* Adds new bookmark.  Returns zero on success and non-zero otherwise. */
static int
add_bmark(const char path[], const char tags[], time_t timestamp)
{
	bmark_t *bm;

	void *p = realloc(bmarks, sizeof(*bmarks)*(bmark_count + 1));
	if(p == NULL)
	{
		return 1;
	}
	bmarks = p;

	bm = &bmarks[bmark_count];
	bm->path = strdup(path);
	bm->tags = strdup(tags);
	bm->timestamp = timestamp;
	if(bm->path == NULL || bm->tags == NULL)
	{
		free(bm->path);
		free(bm->tags);
		return 1;
	}

	++bmark_count;
	return 0;
}

void
bmarks_list(bmarks_find_cb cb, void *arg)
{
	size_t i;
	for(i = 0U; i < bmark_count; ++i)
	{
		if(bmarks[i].tags[0] != '\0')
		{
			cb(bmarks[i].path, bmarks[i].tags, bmarks[i].timestamp, arg);
		}
	}
}

void
bmarks_find(const char tags[], bmarks_find_cb cb, void *arg)
{
	char *clone = strdup(tags);
	size_t i;
	for(i = 0U; i < bmark_count; ++i)
	{
		int nmatches = 0;
		int nmismatches = 0;

		char *tag = clone, *state = NULL;
		while((tag = split_and_get(tag, ',', &state)) != NULL)
		{
			int match = 0;

			char *bmark_tag = bmarks[i].tags, *state = NULL;
			while((bmark_tag = split_and_get(bmark_tag, ',', &state)) != NULL)
			{
				if(strcmp(tag, bmark_tag) == 0)
				{
					match = 1;
				}
			}

			nmatches += match;
			nmismatches += !match;
		}

		if(nmatches != 0 && nmismatches == 0)
		{
			cb(bmarks[i].path, bmarks[i].tags, bmarks[i].timestamp, arg);
		}
	}
	free(clone);
}

void
bmarks_clear(void)
{
	size_t i;
	for(i = 0U; i < bmark_count; ++i)
	{
		free(bmarks[i].path);
		free(bmarks[i].tags);
	}
	free(bmarks);

	bmarks = NULL;
	bmark_count = 0U;
}

int
bmark_is_older(const char path[], time_t than)
{
	size_t i;

	for(i = 0U; i < bmark_count; ++i)
	{
		if(strcmp(path, bmarks[i].path) == 0)
		{
			return bmarks[i].timestamp < than;
		}
	}

	return 1;
}

void
bmarks_complete(int n, char *tags[], const char str[])
{
	const size_t len = strlen(str);
	size_t i;
	for(i = 0U; i < bmark_count; ++i)
	{
		char *tag = bmarks[i].tags, *state = NULL;
		while((tag = split_and_get(tag, ',', &state)) != NULL)
		{
			if(strncmp(tag, str, len) == 0 && !is_in_string_array(tags, n, tag))
			{
				vle_compl_add_match(tag);
			}
		}
	}

	vle_compl_finish_group();
	vle_compl_add_last_match(str);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
