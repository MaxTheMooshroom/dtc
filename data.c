// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * (C) Copyright David Gibson <dwg@au1.ibm.com>, IBM Corporation.  2005.
 */

#include "dtc.h"

void data_free(data_t d)
{
	marker_t *m;
	marker_t *nm;

	m = d.markers;
	while (m) {
		nm = m->next;
		free(m->ref);
		free(m);
		m = nm;
	}

	if (d.val)
		free(d.val);
}

data_t data_grow_for(data_t d, unsigned int xlen)
{
	data_t nd;
	unsigned int newsize;

	if (xlen == 0)
		return d;

	nd = d;

	newsize = xlen;

	while ((d.len + xlen) > newsize)
		newsize *= 2;

	nd.val = xrealloc(d.val, newsize);

	return nd;
}

data_t data_copy_mem(const char *mem, int len)
{
	data_t d;

	d = data_grow_for(empty_data, len);

	d.len = len;
	memcpy(d.val, mem, len);

	return d;
}

data_t data_copy_escape_string(const char *s, int len)
{
	int i = 0;
	data_t d;
	char *q;

	d = data_add_marker(empty_data, TYPE_STRING, NULL);
	d = data_grow_for(d, len + 1);

	q = d.val;
	while (i < len) {
		char c = s[i++];

		if (c == '\\')
			c = get_escape_char(s, &i);

		q[d.len++] = c;
	}

	q[d.len++] = '\0';
	return d;
}

data_t data_copy_file(FILE *f, size_t maxlen)
{
	data_t d = empty_data;

	d = data_add_marker(d, TYPE_NONE, NULL);
	while (!feof(f) && (d.len < maxlen)) {
		size_t chunksize, ret;

		if (maxlen == (size_t)-1)
			chunksize = 4096;
		else
			chunksize = maxlen - d.len;

		d = data_grow_for(d, chunksize);
		ret = fread(d.val + d.len, 1, chunksize, f);

		if (ferror(f))
			die("Error reading file into data: %s", strerror(errno));

		if (d.len + ret < d.len)
			die("Overflow reading file into data\n");

		d.len += ret;
	}

	return d;
}

data_t data_append_data(data_t d, const void *p, int len)
{
	d = data_grow_for(d, len);
	memcpy(d.val + d.len, p, len);
	d.len += len;
	return d;
}

data_t data_insert_at_marker(data_t d, marker_t *m,
				  const void *p, int len)
{
	d = data_grow_for(d, len);
	memmove(d.val + m->offset + len, d.val + m->offset, d.len - m->offset);
	memcpy(d.val + m->offset, p, len);
	d.len += len;

	/* Adjust all markers after the one we're inserting at */
	m = m->next;
	for_each_marker(m)
		m->offset += len;
	return d;
}

static data_t data_append_markers(data_t d, marker_t *m)
{
	marker_t **mp = &d.markers;

	/* Find the end of the markerlist */
	while (*mp)
		mp = &((*mp)->next);
	*mp = m;
	return d;
}

data_t data_merge(data_t d1, data_t d2)
{
	data_t d;
	marker_t *m2 = d2.markers;

	d = data_append_markers(data_append_data(d1, d2.val, d2.len), m2);

	/* Adjust for the length of d1 */
	for_each_marker(m2)
		m2->offset += d1.len;

	d2.markers = NULL; /* So data_free() doesn't clobber them */
	data_free(d2);

	return d;
}

data_t data_append_integer(data_t d, uint64_t value, int bits)
{
	uint8_t value_8;
	fdt16_t value_16;
	fdt32_t value_32;
	fdt64_t value_64;

	switch (bits) {
	case 8:
		value_8 = value;
		return data_append_data(d, &value_8, 1);

	case 16:
		value_16 = cpu_to_fdt16(value);
		return data_append_data(d, &value_16, 2);

	case 32:
		value_32 = cpu_to_fdt32(value);
		return data_append_data(d, &value_32, 4);

	case 64:
		value_64 = cpu_to_fdt64(value);
		return data_append_data(d, &value_64, 8);

	default:
		die("Invalid literal size (%d)\n", bits);
	}
}

data_t data_append_re(data_t d, uint64_t address, uint64_t size)
{
	struct fdt_reserve_entry re;

	re.address = cpu_to_fdt64(address);
	re.size = cpu_to_fdt64(size);

	return data_append_data(d, &re, sizeof(re));
}

data_t data_append_cell(data_t d, cell_t word)
{
	return data_append_integer(d, word, sizeof(word) * 8);
}

data_t data_append_addr(data_t d, uint64_t addr)
{
	return data_append_integer(d, addr, sizeof(addr) * 8);
}

data_t data_append_byte(data_t d, uint8_t byte)
{
	return data_append_data(d, &byte, 1);
}

data_t data_append_zeroes(data_t d, int len)
{
	d = data_grow_for(d, len);

	memset(d.val + d.len, 0, len);
	d.len += len;
	return d;
}

data_t data_append_align(data_t d, int align)
{
	int newlen = ALIGN(d.len, align);
	return data_append_zeroes(d, newlen - d.len);
}

data_t data_add_marker(data_t d, enum markertype type, char *ref)
{
	marker_t *m;

	m = xmalloc(sizeof(*m));
	m->offset = d.len;
	m->type = type;
	m->ref = ref;
	m->next = NULL;

	return data_append_markers(d, m);
}

bool data_is_one_string(data_t d)
{
	int i;
	int len = d.len;

	if (len == 0)
		return false;

	for (i = 0; i < len-1; i++)
		if (d.val[i] == '\0')
			return false;

	if (d.val[len-1] != '\0')
		return false;

	return true;
}
