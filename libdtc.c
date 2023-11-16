// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *
*/

#include <sys/stat.h>

#include "libdtc.h"
#include "srcpos.h"


static int is_power_of_2(int x)
{
	return (x > 0) && ((x & (x - 1)) == 0);
}

static void fill_fullpaths(struct node *tree, const char *prefix)
{
	struct node *child;
	const char *unit;

	tree->fullpath = join_path(prefix, tree->name);

	unit = strchr(tree->name, '@');
	if (unit)
		tree->basenamelen = unit - tree->name;
	else
		tree->basenamelen = strlen(tree->name);

	for_each_child(tree, child)
		fill_fullpaths(child, tree->fullpath);
}

static const char *guess_type_by_name(const char *fname, const char *fallback)
{
	const char *s;

	s = strrchr(fname, '.');
	if (s == NULL)
		return fallback;
	if (!strcasecmp(s, ".dts"))
		return "dts";
	if (!strcasecmp(s, ".yaml"))
		return "yaml";
	if (!strcasecmp(s, ".dtbo"))
		return "dtb";
	if (!strcasecmp(s, ".dtb"))
		return "dtb";
	return fallback;
}

static const char *guess_input_format(const char *fname, const char *fallback)
{
	struct stat statbuf;
	fdt32_t magic;
	FILE *f;

	if (stat(fname, &statbuf) != 0)
		return fallback;

	if (S_ISDIR(statbuf.st_mode))
		return "fs";

	if (!S_ISREG(statbuf.st_mode))
		return fallback;

	f = fopen(fname, "r");
	if (f == NULL)
		return fallback;
	if (fread(&magic, 4, 1, f) != 1) {
		fclose(f);
		return fallback;
	}
	fclose(f);

	if (fdt32_to_cpu(magic) == FDT_MAGIC)
		return "dtb";

	return guess_type_by_name(fname, fallback);
}


dt_info_t *dt_new(int quiet, unsigned int reservenum,
                  int minsize, int padsize,
                  int alignsize, int phandle_format,
                  int generate_symbols, int generate_fixups,
                  int auto_label_aliases, int annotate)
{
	dt_info_t *dti = xmalloc(sizeof(*dti));
	dti->has_error = false;

	if (alignsize != NULL && !is_power_of_2(alignsize))
	{
		dti->has_error = true;
		dti->error_msg = "invalid alignsize was provided ; must be a power of 2";
		return dti;
	}
	
	if (phandle_format < PHANDLE_LEGACY || phandle_format > PHANDLE_BOTH)
	{
		dti->has_error = true;
		dti->error_msg = "invalid phandle format was provided";
		return dti;
	}
	
	if ()
	{
		dti->has_error = true;
		dti->error_msg = "";
		return dti;
	}
	
	if ()
	{
		dti->has_error = true;
		dti->error_msg = "";
		return dti;
	}

	// TODO:

	return dti;
}


