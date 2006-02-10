
#ifndef parse_filename_h
#define parse_filename_h


#define FILENAME_ERROR 0
#define FILENAME_CWD 1
#define FILENAME_ROOT 2

int filename_parse_start(seqf_t filename, int *end, seqf_t *rest);
void filename_parse_component(seqf_t filename, seqf_t *name, int *end,
			      seqf_t *rest, int *trailing_slash);
void print_filename(seqf_t filename);

static inline int filename_parent(seqf_t name) {
  return name.size == 2 && name.data[0] == '.' && name.data[1] == '.';
}
static inline int filename_samedir(seqf_t name) {
  return name.size == 1 && name.data[0] == '.';
}


#endif
