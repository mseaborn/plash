
#include <stdio.h>

#include "region.h"
#include "parse-filename.h"


/* Parse the start of a pathname.  Removes any preceding '/'s.
   Indicates whether pathname is relative to the root or the cwd.
   Returns FILENAME_ERROR if the pathname was empty (which isn't valid). */
int filename_parse_start(seqf_t filename, int *end, seqf_t *rest)
{
  if(filename.size > 0) {
    if(filename.data[0] == '/') {
      int i;
      for(i = 1; ; i++) {
	if(filename.size > i) {
	  if(filename.data[i] != '/') {
	    rest->data = filename.data + i;
	    rest->size = filename.size - i;
	    *end = 0;
	    return FILENAME_ROOT;
	  }
	}
	else {
	  *end = 1;
	  return FILENAME_ROOT;
	}
      }
    }
    else {
      *rest = filename;
      *end = 0;
      return FILENAME_CWD;
    }
  }
  else return FILENAME_ERROR;
}

/* Takes a pathname, assumes no leading slashes, assumes not empty.
   Returns: a component
    * if end, the rest of the pathname (also no leading slashes, not empty)
    * if not end, whether there were trailing slashes.
*/
void filename_parse_component(seqf_t filename, seqf_t *name, int *end,
			      seqf_t *rest, int *trailing_slash)
{
  int i;
  for(i = 0; ; i++) {
    if(filename.size > i) {
      if(filename.data[i] == '/') {
	name->data = filename.data;
	name->size = i;
	for(i++; ; i++) {
	  if(filename.size > i) {
	    if(filename.data[i] != '/') {
	      rest->data = filename.data + i;
	      rest->size = filename.size - i;
	      *end = 0;
	      return;
	    }
	  }
	  else {
	    *end = 1;
	    *trailing_slash = 1;
	    return;
	  }
	}
      }
    }
    else {
      name->data = filename.data;
      name->size = i;
      *end = 1;
      *trailing_slash = 0;
      return;
    }
  }
}

void print_filename(seqf_t filename)
{
  int end;
  int start = filename_parse_start(filename, &end, &filename);
  if(start == FILENAME_ROOT) { printf("ROOT\n"); }
  else if(start == FILENAME_CWD) { printf("CWD\n"); }
  else { printf("ERROR\n"); return; }

  while(!end) {
    seqf_t name;
    int trailing_slash;
    filename_parse_component(filename, &name, &end, &filename, &trailing_slash);
    if(name.size == 2 && name.data[0] == '.' && name.data[1] == '.') {
      printf("PARENT\n");
    }
    else if(name.size == 1 && name.data[0] == '.') {
      printf("SAMEDIR\n");
    }
    else {
      fwrite(name.data, name.size, 1, stdout);
      printf("\n");
    }
    if(end && trailing_slash) {
      printf("TRAILINGSLASH\n");
    }
  }
}
