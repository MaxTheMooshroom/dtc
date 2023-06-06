/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef DTC_H
#define DTC_H

/*
 * (C) Copyright David Gibson <dwg@au1.ibm.com>, IBM Corporation.  2005.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>

#include <libfdt_env.h>
#include <fdt.h>

#include "srcpos.h"
#include "util.h"

#ifdef DEBUG
#define debug(...)	printf(__VA_ARGS__)
#else
#define debug(...)
#endif

#define DEFAULT_FDT_VERSION	17

#define PHANDLE_LEGACY	0x1
#define PHANDLE_EPAPR	0x2
#define PHANDLE_BOTH	0x3

typedef uint32_t cell_t;

static inline bool phandle_is_valid(cell_t phandle)
{
	return phandle != 0 && phandle != ~0U;
}

static inline uint16_t dtb_ld16(const void *p)
{
	const uint8_t *bp = (const uint8_t *)p;

	return ((uint16_t)bp[0] << 8)
		| bp[1];
}

static inline uint32_t dtb_ld32(const void *p)
{
	const uint8_t *bp = (const uint8_t *)p;

	return ((uint32_t)bp[0] << 24)
		| ((uint32_t)bp[1] << 16)
		| ((uint32_t)bp[2] << 8)
		| bp[3];
}

static inline uint64_t dtb_ld64(const void *p)
{
	const uint8_t *bp = (const uint8_t *)p;

	return ((uint64_t)bp[0] << 56)
		| ((uint64_t)bp[1] << 48)
		| ((uint64_t)bp[2] << 40)
		| ((uint64_t)bp[3] << 32)
		| ((uint64_t)bp[4] << 24)
		| ((uint64_t)bp[5] << 16)
		| ((uint64_t)bp[6] << 8)
		| bp[7];
}

#define streq(a, b)	(strcmp((a), (b)) == 0)
#define strstarts(s, prefix)	(strncmp((s), (prefix), strlen(prefix)) == 0)
#define strprefixeq(a, n, b)	(strlen(b) == (n) && (memcmp(a, b, n) == 0))
static inline bool strends(const char *str, const char *suffix)
{
	unsigned int len, suffix_len;

	len = strlen(str);
	suffix_len = strlen(suffix);
	if (len < suffix_len)
		return false;
	return streq(str + len - suffix_len, suffix);
}

#define ALIGN(x, a)	(((x) + (a) - 1) & ~((a) - 1))

/* Data blobs */
enum markertype {
	TYPE_NONE,
	REF_PHANDLE,
	REF_PATH,
	LABEL,
	TYPE_UINT8,
	TYPE_UINT16,
	TYPE_UINT32,
	TYPE_UINT64,
	TYPE_STRING,
};

static inline bool is_type_marker(enum markertype type)
{
	return type >= TYPE_UINT8;
}

extern const char *markername(enum markertype markertype);

typedef struct marker marker_t;
struct marker {
	enum markertype type;
	unsigned int offset;
	char *ref;
	marker_t *next;
};


typedef struct data {
	unsigned int len;
	char *val;
	marker_t *markers;
} data_t;


#define empty_data ((data_t){ 0 /* all .members = 0 or NULL */ })

#define for_each_marker(m) \
	for (; (m); (m) = (m)->next)
#define for_each_marker_of_type(m, t) \
	for_each_marker(m) \
		if ((m)->type == (t))

static inline marker_t *next_type_marker(marker_t *m)
{
	for_each_marker(m)
		if (is_type_marker(m->type))
			break;
	return m;
}

static inline size_t type_marker_length(marker_t *m)
{
	marker_t *next = next_type_marker(m->next);

	if (next)
		return next->offset - m->offset;
	return 0;
}

void data_free(data_t d);

data_t data_grow_for(data_t d, unsigned int xlen);

data_t data_copy_mem(const char *mem, int len);
data_t data_copy_escape_string(const char *s, int len);
data_t data_copy_file(FILE *f, size_t len);

data_t data_append_data(data_t d, const void *p, int len);
data_t data_insert_at_marker(data_t d, marker_t *m,
				  const void *p, int len);
data_t data_merge(data_t d1, data_t d2);
data_t data_append_cell(data_t d, cell_t word);
data_t data_append_integer(data_t d, uint64_t word, int bits);
data_t data_append_re(data_t d, uint64_t address, uint64_t size);
data_t data_append_addr(data_t d, uint64_t addr);
data_t data_append_byte(data_t d, uint8_t byte);
data_t data_append_zeroes(data_t d, int len);
data_t data_append_align(data_t d, int align);

data_t data_add_marker(data_t d, enum markertype type, char *ref);

bool data_is_one_string(data_t d);

/* DT constraints */

#define MAX_PROPNAME_LEN	31
#define MAX_NODENAME_LEN	31

/* Live trees */
typedef struct label label_t;
struct label {
	bool deleted;
	char *label;
	label_t *next;
};

typedef struct bus_type {
	const char *name;
} bus_type_t;

typedef struct property property_t;
struct property {
	bool deleted;
	char *name;
	data_t val;

	property_t *next;

	label_t *labels;
	srcpos_t *srcpos;
};

typedef struct node node_t;
struct node {
	bool deleted;
	char *name;
	property_t *proplist;
	node_t *children;

	node_t *parent;
	node_t *next_sibling;

	char *fullpath;
	int basenamelen;

	cell_t phandle;
	int addr_cells, size_cells;

	label_t *labels;
	const bus_type_t *bus;
	srcpos_t *srcpos;

	bool omit_if_unused, is_referenced;
};

#define for_each_label_withdel(l0, l) \
	for ((l) = (l0); (l); (l) = (l)->next)

#define for_each_label(l0, l) \
	for_each_label_withdel(l0, l) \
		if (!(l)->deleted)

#define for_each_property_withdel(n, p) \
	for ((p) = (n)->proplist; (p); (p) = (p)->next)

#define for_each_property(n, p) \
	for_each_property_withdel(n, p) \
		if (!(p)->deleted)

#define for_each_child_withdel(n, c) \
	for ((c) = (n)->children; (c); (c) = (c)->next_sibling)

#define for_each_child(n, c) \
	for_each_child_withdel(n, c) \
		if (!(c)->deleted)

void add_label(label_t **labels, char *label);
void delete_labels(label_t **labels);

property_t *build_property(char *name, data_t val,
				srcpos_t *srcpos);
property_t *build_property_delete(char *name);
property_t *chain_property(property_t *first, property_t *list);
property_t *reverse_properties(property_t *first);

node_t *build_node(property_t *proplist, node_t *children,
			srcpos_t *srcpos);
node_t *build_node_delete(srcpos_t *srcpos);
node_t *name_node(node_t *node, char *name);
node_t *omit_node_if_unused(node_t *node);
node_t *reference_node(node_t *node);
node_t *chain_node(node_t *first, node_t *list);
node_t *merge_nodes(node_t *old_node, node_t *new_node);
node_t *add_orphan_node(node_t *old_node, node_t *new_node, char *ref);

void add_property(node_t *node, property_t *prop);
void delete_property_by_name(node_t *node, char *name);
void delete_property(property_t *prop);
void add_child(node_t *parent, node_t *child);
void delete_node_by_name(node_t *parent, char *name);
void delete_node(node_t *node);
void append_to_property(node_t *node,
			char *name, const void *data, int len,
			enum markertype type);

const char *get_unitname(node_t *node);
property_t *get_property(node_t *node, const char *propname);
cell_t propval_cell(property_t *prop);
cell_t propval_cell_n(property_t *prop, unsigned int n);
property_t *get_property_by_label(node_t *tree, const char *label,
				       node_t **node);
marker_t *get_marker_label(node_t *tree, const char *label,
				node_t **node, property_t **prop);
node_t *get_subnode(node_t *node, const char *nodename);
node_t *get_node_by_path(node_t *tree, const char *path);
node_t *get_node_by_label(node_t *tree, const char *label);
node_t *get_node_by_phandle(node_t *tree, cell_t phandle, int generate_fixups);
node_t *get_node_by_ref(node_t *tree, const char *ref);
cell_t get_node_phandle(node_t *root, node_t *node, int phandle_format);

uint32_t guess_boot_cpuid(node_t *tree);

/* Boot info (tree plus memreserve information */

typedef struct reserve_info reserve_info_t;
struct reserve_info {
	uint64_t address, size;

	reserve_info_t *next;

	label_t *labels;
};

reserve_info_t *build_reserve_entry(uint64_t start, uint64_t len);
reserve_info_t *chain_reserve_entry(reserve_info_t *first,
					 reserve_info_t *list);
reserve_info_t *add_reserve_entry(reserve_info_t *list,
				       reserve_info_t *new);


// srcpos.h -> typedef struct dt_info dt_info_t;

struct dt_info {
	unsigned int dtsflags;
	reserve_info_t *reservelist;
	uint32_t boot_cpuid_phys;
	node_t *dt;						/* the device tree */
	const char *outname;			/* filename being written to, "-" for stdout */

	bool has_error;
	const char *error_msg;

	struct {
		int quiet;					/* Level of quietness */
		unsigned int reservenum;	/* Number of memory reservation slots */
		int minsize;				/* Minimum blob size */
		int padsize;				/* Additional padding to blob */
		int alignsize;				/* Additional padding to blob accroding to the alignsize */
		int phandle_format;			/* Use linux,phandle or phandle properties */
		int generate_symbols;		/* generate symbols for nodes with labels */
		int generate_fixups;		/* generate fixups */
		int auto_label_aliases;		/* auto generate labels -> aliases */
		int annotate;				/* annotate .dts with input source location */
	} options;

	struct {
		FILE *depfile; 						/* = NULL 	*/
		int srcfile_depth; 					/* = 0 		*/
		srcfile_state_t *current_srcfile; 	/* = NULL 	*/
		char *initial_path; 				/* = NULL 	*/
		int initial_pathlen; 				/* = 0 		*/
		bool initial_cpp;					/* = true	*/

		FILE *yyin;
		YYLTYPE yylloc;

		/* This is the list of directories to search for source/include files */
		search_path_t *search_path_head, **search_path_tail;
	} src_info;
};

/* DTS version flags definitions */
#define DTSF_V1		0x0001	/* /dts-v1/ */
#define DTSF_PLUGIN	0x0002	/* /plugin/ */

void build_dt_info(dt_info_t *parser_output,
				  unsigned int dtsflags,
			      reserve_info_t *reservelist,
			      node_t *tree, uint32_t boot_cpuid_phys);
void sort_tree(dt_info_t *dti);
void generate_label_tree(dt_info_t *dti, char *name, bool allocph);
void generate_fixups_tree(dt_info_t *dti, char *name);
void generate_local_fixups_tree(dt_info_t *dti, char *name);

/* Checks */

void parse_checks_option(bool warn, bool error, const char *arg);
void process_checks(bool force, dt_info_t *dti);

/* Flattened trees */

void dt_to_blob(FILE *f, dt_info_t *dti, int version);
void dt_to_asm(FILE *f, dt_info_t *dti, int version);

void dt_from_blob(const char *fname, dt_info_t *dti);

/* Tree source */

void dt_to_source(FILE *f, dt_info_t *dti);
void dt_from_source(dt_info_t *dti, const char *f);

/* YAML source */

void dt_to_yaml(FILE *f, dt_info_t *dti);

/* FS trees */

void dt_from_fs(const char *dirname, dt_info_t *dti);

#endif /* DTC_H */
