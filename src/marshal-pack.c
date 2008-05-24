/* Copyright (C) 2006 Mark Seaborn

   This file is part of Plash.

   Plash is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   Plash is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Plash; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
   USA.  */

#include <stdarg.h>

#include "filesysobj.h"


struct cap_args pl_pack(region_t r, int method, const char *fmt, ...)
{
  va_list list;
  struct cap_args args;
  const char *p;
  char *data, *data_start;
  int data_size = 0;
  cap_t *caps;
  int *fds;
  
  data_size = sizeof(int); /* Include method ID to start with */
  args.caps.size = 0;
  args.fds.count = 0;

  /* Calculate size of data, caps and FDs portions so that we can
     allocate blocks. */
  va_start(list, fmt);
  for(p = fmt; *p; p++) {
    switch(*p) {
      case 'i':
	va_arg(list, int);
	data_size += sizeof(int);
	break;

      case 's': {
	seqf_t arg = va_arg(list, seqf_t);
	data_size += sizeof(int);
	data_size += arg.size;
	break;
      }

      case 'S': {
	seqf_t arg = va_arg(list, seqf_t);
	data_size += arg.size;
	break;
      }

      case 'c':
	va_arg(list, cap_t);
	args.caps.size++;
	break;

      case 'C': {
	cap_seq_t arg = va_arg(list, cap_seq_t);
	args.caps.size += arg.size;
	break;
      }

      case 'd':
	data_size += sizeof(int);
	if(va_arg(list, cap_t) != NULL) {
	  args.caps.size++;
	}
	break;

      case 'f':
	va_arg(list, int);
	args.fds.count++;
	break;

      default:
	assert(!"Unrecognised character format string");
    }
  }
  va_end(list);

  data = data_start = region_alloc(r, data_size);
  caps = region_alloc(r, args.caps.size * sizeof(cap_t));
  fds = region_alloc(r, args.fds.count * sizeof(int));
  args.data = mk_leaf2(r, data, data_size);
  args.caps.caps = caps;
  args.fds.fds = fds;

  *(int *) data = method;
  data += sizeof(int);

  va_start(list, fmt);
  for(p = fmt; *p; p++) {
    switch(*p) {
      case 'i':
	*(int *) data = va_arg(list, int);
	data += sizeof(int);
	break;

      case 's': {
	seqf_t arg = va_arg(list, seqf_t);
	*(int *) data = arg.size;
	data += sizeof(int);
	memcpy(data, arg.data, arg.size);
	data += arg.size;
	break;
      }

      case 'S': {
	seqf_t arg = va_arg(list, seqf_t);
	memcpy(data, arg.data, arg.size);
	data += arg.size; /* Only necessary for assertion check later */
	break;
      }

      case 'c':
	*caps++ = va_arg(list, cap_t);
	break;

      case 'C': {
	cap_seq_t arg = va_arg(list, cap_seq_t);
	memcpy(caps, arg.caps, arg.size * sizeof(cap_t));
	caps += arg.size;
	break;
      }

      case 'd': {
	cap_t arg = va_arg(list, cap_t);
	*(int *) data = arg != NULL;
	data += sizeof(int);
	if(arg != NULL) {
	  *caps++ = arg;
	}
	break;
      }

      case 'f':
	*fds++ = va_arg(list, int);
	break;

      default:
	assert(!"Unrecognised character format string");
    }
  }
  va_end(list);

  assert(data == data_start + data_size);
  assert(caps == args.caps.caps + args.caps.size);
  assert(fds == args.fds.fds + args.fds.count);
  
  return args;
}

int pl_unpack(region_t r, struct cap_args args, int method,
	      const char *fmt, ...)
{
  seqf_t args_data = flatten_reuse(r, args.data);
  int data_pos;
  int caps_pos = 0;
  int fds_pos = 0;
  int i;
  va_list list;
  va_start(list, fmt);

  /* Check method ID. */
  if(args_data.size < sizeof(int)) { return FALSE; }
  if(*(int *) args_data.data != method) { return FALSE; }
  data_pos = sizeof(int);
  
  for(i = 0; fmt[i]; i++) {
    switch(fmt[i]) {
      case 'i': {
	int *ptr = va_arg(list, int *);
	if(data_pos + sizeof(int) > args_data.size) { goto mismatch; }
	*ptr = *(int *) (args_data.data + data_pos);
	data_pos += sizeof(int);
	break;
      }
      case 's': {
	seqf_t *ptr = va_arg(list, seqf_t *);
	/* Read size */
	if(data_pos + sizeof(int) > args_data.size)
	  goto mismatch;
	int size = *(int *) (args_data.data + data_pos);
	data_pos += sizeof(int);
	/* Get data */
	if(data_pos + size > args_data.size)
	  goto mismatch;
	ptr->data = args_data.data + data_pos;
	ptr->size = size;
	data_pos += size;
	break;
      }
      case 'S': {
	seqf_t *ptr = va_arg(list, seqf_t *);
	ptr->data = args_data.data + data_pos;
	ptr->size = args_data.size - data_pos;
	data_pos = args_data.size;
	break;
      }
      case 'c': {
	cap_t *ptr = va_arg(list, cap_t *);
	if(caps_pos >= args.caps.size) { goto mismatch; }
	*ptr = args.caps.caps[caps_pos++];
	break;
      }
      case 'C': {
	cap_seq_t *ptr = va_arg(list, cap_seq_t *);
	ptr->caps = args.caps.caps + caps_pos;
	ptr->size = args.caps.size - caps_pos;
	caps_pos = args.caps.size;
	break;
      }
      case 'd': {
	cap_t *ptr = va_arg(list, cap_t *);
	if(data_pos + sizeof(int) > args_data.size) { goto mismatch; }
	if(*(int *) (args_data.data + data_pos)) {
	  if(caps_pos >= args.caps.size) { goto mismatch; }
	  *ptr = args.caps.caps[caps_pos++];
	}
	else {
	  *ptr = NULL;
	}
	data_pos += sizeof(int);
	break;
      }
      case 'f': {
        int *ptr = va_arg(list, int *);
	if(fds_pos >= args.fds.count) { goto mismatch; }
	*ptr = args.fds.fds[fds_pos++];
	break;
      }
      case 'F': {
	fds_t *ptr = va_arg(list, fds_t *);
	ptr->fds = args.fds.fds + fds_pos;
	ptr->count = args.fds.count - fds_pos;
	fds_pos = args.fds.count;
	break;
      }
      default:
	assert(!"Unrecognised character format string");
	return FALSE;
    }
  }
  va_end(list);
  /* There must be no arguments left over. */
  if(data_pos == args_data.size &&
     caps_pos == args.caps.size &&
     fds_pos == args.fds.count) {
    return TRUE;
  }
 mismatch:
  // printf("mismatch on arg %i ('%c')\n", i, fmt[i]);
  return FALSE;
}
