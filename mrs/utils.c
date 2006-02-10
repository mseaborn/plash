
#include <stdio.h>

#include "region.h"


void print_data(seqf_t b)
{
  fprint_data(stdout, b);
}

void fprint_data(FILE *fp, seqf_t b)
{
  fprintf(fp, "data of length %i:\n", b.size);
  while(b.size > 0) {
    int i = 0;
    fputc(' ', fp); fputc(' ', fp);
    if(b.size >= 4) {
      fprintf(fp, "[%i]", *(int *) b.data);
    }
    fputc('"', fp);
    while(i < 4 && i < b.size) {
      char c = b.data[i];
      if(c < 32) { fprintf(fp, "\\%03i", (unsigned char) c); }
      else { fputc(c, fp); }
      i++;
    }
    fputc('"', fp);
    fputc('\n', fp);
    b.data += i;
    b.size -= i;
  }
}

void fprint_d(FILE *fp, seqf_t b)
{
  fwrite(b.data, b.size, 1, fp);
}
