/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright 2007 Jon Loeliger, Freescale Semiconductor, Inc.
 */

#ifndef SRCPOS_H
#define SRCPOS_H

#include <stdio.h>
#include <stdbool.h>
#include "util.h"

typedef struct srcfile_state_ srcfile_state_t;
struct srcfile_state_ {
	FILE *f;
	char *name;
	char *dir;
	int lineno, colno;
	srcfile_state_t *prev;
};

extern FILE *depfile; /* = NULL */
extern srcfile_state_t *current_srcfile; /* = NULL */

/**
 * Open a source file.
 *
 * If the source file is a relative pathname, then it is searched for in the
 * current directory (the directory of the last source file read) and after
 * that in the search path.
 *
 * We work through the search path in order from the first path specified to
 * the last.
 *
 * If the file is not found, then this function does not return, but calls
 * die().
 *
 * @param fname		Filename to search
 * @param fullnamep	If non-NULL, it is set to the allocated filename of the
 *			file that was opened. The caller is then responsible
 *			for freeing the pointer.
 * @return pointer to opened FILE
 */
FILE *srcfile_relative_open(const char *fname, char **fullnamep);

void srcfile_push(const char *fname);
bool srcfile_pop(void);

/**
 * Add a new directory to the search path for input files
 *
 * The new path is added at the end of the list.
 *
 * @param dirname	Directory to add
 */
void srcfile_add_search_path(const char *dirname);

typedef struct srcpos srcpos_t;
struct srcpos {
    int first_line;
    int first_column;
    int last_line;
    int last_column;
    srcfile_state_t *file;
    srcpos_t *next;
};

#define YYLTYPE srcpos_t

#define YYLLOC_DEFAULT(Current, Rhs, N)						\
	do {									\
		if (N) {							\
			(Current).first_line = YYRHSLOC(Rhs, 1).first_line;	\
			(Current).first_column = YYRHSLOC(Rhs, 1).first_column;	\
			(Current).last_line = YYRHSLOC(Rhs, N).last_line;	\
			(Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
			(Current).file = YYRHSLOC(Rhs, N).file;			\
		} else {							\
			(Current).first_line = (Current).last_line =		\
				YYRHSLOC(Rhs, 0).last_line;			\
			(Current).first_column = (Current).last_column =	\
				YYRHSLOC(Rhs, 0).last_column;			\
			(Current).file = YYRHSLOC (Rhs, 0).file;		\
		}								\
		(Current).next = NULL;						\
	} while (0)


extern void srcpos_update(srcpos_t *pos, const char *text, int len);
extern srcpos_t *srcpos_copy(srcpos_t *pos);
extern srcpos_t *srcpos_extend(srcpos_t *new_srcpos,
				    srcpos_t *old_srcpos);
extern char *srcpos_string(srcpos_t *pos);
extern char *srcpos_string_first(srcpos_t *pos, int level);
extern char *srcpos_string_last(srcpos_t *pos, int level);


extern void PRINTF(3, 0) srcpos_verror(srcpos_t *pos, const char *prefix,
					const char *fmt, va_list va);
extern void PRINTF(3, 4) srcpos_error(srcpos_t *pos, const char *prefix,
				      const char *fmt, ...);

extern void srcpos_set_line(char *f, int l);

#endif /* SRCPOS_H */
