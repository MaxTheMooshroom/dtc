// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright 2007 Jon Loeliger, Freescale Semiconductor, Inc.
 */

#define _GNU_SOURCE

#include <stdio.h>

#include "dtc.h"
#include "srcpos.h"

/* A node in our list of directories to search for source/include files */
struct search_path {
	search_path_t *next;		/* next node in list, NULL for end */
	const char *dirname;		/* name of directory to search */
};

/* Detect infinite include recursion. */
#define MAX_SRCFILE_DEPTH     (200)

static char *get_dirname(const char *path)
{
	const char *slash = strrchr(path, '/');

	if (slash) {
		int len = slash - path;
		char *dir = xmalloc(len + 1);

		memcpy(dir, path, len);
		dir[len] = '\0';
		return dir;
	}
	return NULL;
}


static void set_initial_path(dt_info_t *dti, char *fname)
{
	int i, len = strlen(fname);

	xasprintf(&dti->src_info.initial_path, "%s", fname);
	dti->src_info.initial_pathlen = 0;
	for (i = 0; i != len; i++)
		if (dti->src_info.initial_path[i] == '/')
			dti->src_info.initial_pathlen++;
}

static char *shorten_to_initial_path(dt_info_t *dti, char *fname)
{
	char *p1, *p2, *prevslash1 = NULL;
	int slashes = 0;

	for (p1 = fname, p2 = dti->src_info.initial_path; *p1 && *p2; p1++, p2++) {
		if (*p1 != *p2)
			break;
		if (*p1 == '/') {
			prevslash1 = p1;
			slashes++;
		}
	}
	p1 = prevslash1 + 1;
	if (prevslash1) {
		int diff = dti->src_info.initial_pathlen - slashes, i, j;
		int restlen = strlen(fname) - (p1 - fname);
		char *res;

		res = xmalloc((3 * diff) + restlen + 1);
		for (i = 0, j = 0; i != diff; i++) {
			res[j++] = '.';
			res[j++] = '.';
			res[j++] = '/';
		}
		strcpy(res + j, p1);
		return res;
	}
	return NULL;
}

/**
 * Try to open a file in a given directory.
 *
 * If the filename is an absolute path, then dirname is ignored. If it is a
 * relative path, then we look in that directory for the file.
 *
 * @param dirname	Directory to look in, or NULL for none
 * @param fname		Filename to look for
 * @param fp		Set to NULL if file did not open
 * @return allocated filename on success (caller must free), NULL on failure
 */
static char *try_open(const char *dirname, const char *fname, FILE **fp)
{
	char *fullname;

	if (!dirname || fname[0] == '/')
		fullname = xstrdup(fname);
	else
		fullname = join_path(dirname, fname);

	*fp = fopen(fullname, "rb");
	if (!*fp) {
		free(fullname);
		fullname = NULL;
	}

	return fullname;
}

/**
 * Open a file for read access
 *
 * If it is a relative filename, we search the full search path for it.
 *
 * @param search_path_head 	Root search_path_t node to search through
 * @param fname				Filename to open
 * @param fp				Returns pointer to opened FILE, or NULL on failure
 * @return pointer to allocated filename, which caller must free
 */
static char *fopen_any_on_path(dt_info_t *dti, const char *fname,
							   FILE **fp)
{
	const char *cur_dir = NULL;
	search_path_t *node;
	char *fullname;

	/* Try current directory first */
	assert(fp);
	if (dti->src_info.current_srcfile)
		cur_dir = dti->src_info.current_srcfile->dir;
	fullname = try_open(cur_dir, fname, fp);

	/* Failing that, try each search path in turn */
	for (node = dti->src_info.search_path_head; !*fp && node; node = node->next)
		fullname = try_open(node->dirname, fname, fp);

	return fullname;
}

FILE *srcfile_relative_open(dt_info_t *dti, const char *fname,
							char **fullnamep)
{
	FILE *f;
	char *fullname;

	if (streq(fname, "-")) {
		f = stdin;
		fullname = xstrdup("<stdin>");
	} else {
		fullname = fopen_any_on_path(dti, fname, &f);
		if (!f)
			die("Couldn't open \"%s\": %s\n", fname,
			    strerror(errno));
	}

	if (dti->src_info.depfile)
		fprintf(dti->src_info.depfile, " %s", fullname);

	if (fullnamep)
		*fullnamep = fullname;
	else
		free(fullname);

	return f;
}

void srcfile_push(dt_info_t *dti, const char *fname)
{
	srcfile_state_t *srcfile;

	if (dti->src_info.srcfile_depth++ >= MAX_SRCFILE_DEPTH)
		die("Includes nested too deeply");

	srcfile = xmalloc(sizeof(*srcfile));

	srcfile->f = srcfile_relative_open(dti, fname, &srcfile->name);
	srcfile->dir = get_dirname(srcfile->name);
	srcfile->prev = dti->src_info.current_srcfile;

	srcfile->lineno = 1;
	srcfile->colno = 1;

	dti->src_info.current_srcfile = srcfile;

	if (dti->src_info.srcfile_depth == 1)
		set_initial_path(dti, srcfile->name);
}

bool srcfile_pop(dt_info_t *dti)
{
	srcfile_state_t *srcfile = dti->src_info.current_srcfile;

	assert(srcfile);

	dti->src_info.current_srcfile = srcfile->prev;

	if (fclose(srcfile->f))
		die("Error closing \"%s\": %s\n", srcfile->name,
		    strerror(errno));

	/* FIXME: We allow the srcfile_state structure to leak,
	 * because it could still be referenced from a location
	 * variable being carried through the parser somewhere.  To
	 * fix this we could either allocate all the files from a
	 * table, or use a pool allocator. */

	return dti->src_info.current_srcfile ? true : false;
}

void srcfile_add_search_path(dt_info_t *dti, const char *dirname)
{
	search_path_t *node;

	/* Create the node */
	node = xmalloc(sizeof(*node));
	node->next = NULL;
	node->dirname = xstrdup(dirname);

	/* Add to the end of our list */
	if (dti->src_info.search_path_tail)
		*dti->src_info.search_path_tail = node;
	else
		dti->src_info.search_path_head = node;
	dti->src_info.search_path_tail = &node->next;
}

void srcpos_update(dt_info_t *dti, srcpos_t *pos, const char *text, int len)
{
	int i;

	pos->file = dti->src_info.current_srcfile;

	pos->first_line = dti->src_info.current_srcfile->lineno;
	pos->first_column = dti->src_info.current_srcfile->colno;

	for (i = 0; i < len; i++)
		if (text[i] == '\n') {
			dti->src_info.current_srcfile->lineno++;
			dti->src_info.current_srcfile->colno = 1;
		} else {
			dti->src_info.current_srcfile->colno++;
		}

	pos->last_line = dti->src_info.current_srcfile->lineno;
	pos->last_column = dti->src_info.current_srcfile->colno;
}

srcpos_t *
srcpos_copy(srcpos_t *pos)
{
	srcpos_t *pos_new;
	srcfile_state_t *srcfile_state;

	if (!pos)
		return NULL;

	pos_new = xmalloc(sizeof(srcpos_t));
	assert(pos->next == NULL);
	memcpy(pos_new, pos, sizeof(srcpos_t));

	/* allocate without free */
	srcfile_state = xmalloc(sizeof(srcfile_state_t));
	memcpy(srcfile_state, pos->file, sizeof(srcfile_state_t));
	pos_new->file = srcfile_state;

	return pos_new;
}

srcpos_t *srcpos_extend(srcpos_t *pos, srcpos_t *newtail)
{
	srcpos_t *p;

	if (!pos)
		return newtail;

	for (p = pos; p->next != NULL; p = p->next);
	p->next = newtail;
	return pos;
}

char *
srcpos_string(srcpos_t *pos)
{
	const char *fname = "<no-file>";
	char *pos_str;

	if (pos->file && pos->file->name)
		fname = pos->file->name;


	if (pos->first_line != pos->last_line)
		xasprintf(&pos_str, "%s:%d.%d-%d.%d", fname,
			  pos->first_line, pos->first_column,
			  pos->last_line, pos->last_column);
	else if (pos->first_column != pos->last_column)
		xasprintf(&pos_str, "%s:%d.%d-%d", fname,
			  pos->first_line, pos->first_column,
			  pos->last_column);
	else
		xasprintf(&pos_str, "%s:%d.%d", fname,
			  pos->first_line, pos->first_column);

	return pos_str;
}

static char *
srcpos_string_comment(dt_info_t *dti, srcpos_t *pos, bool first_line, int level)
{
	char *pos_str, *fname, *first, *rest;
	bool fresh_fname = false;

	if (!pos) {
		if (level > 1) {
			xasprintf(&pos_str, "<no-file>:<no-line>");
			return pos_str;
		} else {
			return NULL;
		}
	}

	if (!pos->file)
		fname = "<no-file>";
	else if (!pos->file->name)
		fname = "<no-filename>";
	else if (level > 1)
		fname = pos->file->name;
	else {
		fname = shorten_to_initial_path(dti, pos->file->name);
		if (fname)
			fresh_fname = true;
		else
			fname = pos->file->name;
	}

	if (level > 1)
		xasprintf(&first, "%s:%d:%d-%d:%d", fname,
			  pos->first_line, pos->first_column,
			  pos->last_line, pos->last_column);
	else
		xasprintf(&first, "%s:%d", fname,
			  first_line ? pos->first_line : pos->last_line);

	if (fresh_fname)
		free(fname);

	if (pos->next != NULL) {
		rest = srcpos_string_comment(dti, pos->next, first_line, level);
		xasprintf(&pos_str, "%s, %s", first, rest);
		free(first);
		free(rest);
	} else {
		pos_str = first;
	}

	return pos_str;
}

char *srcpos_string_first(dt_info_t *dti, srcpos_t *pos, int level)
{
	return srcpos_string_comment(dti, pos, true, level);
}

char *srcpos_string_last(dt_info_t *dti, srcpos_t *pos, int level)
{
	return srcpos_string_comment(dti, pos, false, level);
}

void srcpos_verror(srcpos_t *pos, const char *prefix,
		   const char *fmt, va_list va)
{
	char *srcstr;

	srcstr = srcpos_string(pos);

	fprintf(stderr, "%s: %s ", prefix, srcstr);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");

	free(srcstr);
}

void srcpos_error(srcpos_t *pos, const char *prefix,
		  const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	srcpos_verror(pos, prefix, fmt, va);
	va_end(va);
}

void srcpos_set_line(dt_info_t *dti, char *f, int l)
{
	dti->src_info.current_srcfile->name = f;
	dti->src_info.current_srcfile->lineno = l;

	if (dti->src_info.initial_cpp) {
		dti->src_info.initial_cpp = false;
		set_initial_path(dti, f);
	}
}
