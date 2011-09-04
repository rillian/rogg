/*
   Copyright (C) 2009,2011 Xiph.org Foundation

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* kate header modification script using the rogg library */

/* compile with
   gcc -O2 -g -Wall -I. -o rogg_kate rogg.c rogg_kate.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include <rogg.h>

int verbose = 0;
int language_set = 0;
const char * language = NULL;
int category_set = 0;
const char * category = NULL;
int canvas_size_set = 0;
int canvas_width = 0;
int canvas_height = 0;

/* little endian accessors for the kate header data */
int get32(unsigned char *data)
{
  return (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
}
void put32(unsigned char *data, int v)
{
  data[3] = (v >> 24) & 0xFF;
  data[2] = (v >> 16) & 0xFF;
  data[1] = (v >>  8) & 0xFF;
  data[0] = v & 0xFF;
}
void put15s(unsigned char *data,const char *string)
{
  if (strlen(string)>15) {
    fprintf(stderr ,"String must be less than 16 characters");
    return;
  }
  strcpy((char*)data, string);
  data[15] = 0;
}
void put_canvas_size(unsigned char *data,int size)
{
  size_t base=size;
  size_t shift=0;

  while (base&~((1<<12)-1)) {
    /* we have a high bit we can't fit, increase shift if we wouldn't lose low bits */
    if ((size>>shift)&1) {
      fprintf(stderr,"Canvas size out of range\n");
      return;
    }
    ++shift;
    base>>=1;
  }
  if (shift>=16) {
    fprintf(stderr,"Canvas size out of range\n");
    return;
  }
  data[0]=(shift<<4)|(base&0xf);
  data[1]=base>>4;
}

void print_header_info(FILE *out, rogg_page_header *header)
{
  fprintf(out, " Ogg page serial %08x seq %d (%5d bytes)",
	header->serialno, header->sequenceno, header->length);
  fprintf(out, (header->continued) ? " c" : "  ");
  fprintf(out, " %lld", (long long int)header->granulepos);
  fprintf(out, (header->bos) ? " bos" : "");
  fprintf(out, (header->eos) ? " eos" : "");
  fprintf(out, "\n");
}

void print_kate_info(FILE *out, unsigned char *data)
{
  char language[16], category[16];
  int canvas_width = ((data[16]&0xf)|(data[17]<<4))<<(data[16]>>4);
  int canvas_height = ((data[18]&0xf)|(data[19]<<4))<<(data[18]>>4);
  memcpy(language, data+32, 16);
  language[15] = 0;
  memcpy(category, data+48, 16);
  category[15] = 0;

  fprintf(out, "  Kate info header version %d.%d\n",
	data[9], data[10]);
  fprintf(out, "   language: %s\n", language);
  fprintf(out, "   category: %s\n", category);
  fprintf(out, "   original canvas size: %dx%d\n", 
	canvas_width, canvas_height);
  fprintf(out, "   frame rate %d:%d\n", get32(data+24), get32(data+28));
}

void print_usage(FILE *out, char *name)
{
  fprintf(stderr, "Script for editing kate headers, in place.\n");
  fprintf(stderr, "%s [-v] [-l language] [-c category] [-s wxh] <file1.ogg> [<file2.ogg>...]\n",
	name);
  fprintf(stderr, "    -v          print more information\n"
		  "    -s wxh      set original canvas size\n"
		  "                common values: \n"
		  "                        720x576, 1920x1080, etc \n"
		  "    -l language set the language (ISO 639-1, RFC 3066)\n"
		  "    -c category set the category (SUB, K-SPU, etc)\n");
}

int parse_args(int *argc, char *argv[])
{
  int arg = 1;
  int shift;

  while (arg < *argc) {
    /* parse recognized options */
    shift = 0;
    if (argv[arg][0] == '-') {
      switch (argv[arg][1]) {
	case 'v':
	  verbose = 1;
	  shift = 1;
	  break;
	case 's':
	  canvas_size_set = 1;
	  /* read canvas size from the next arg */
	  if (sscanf(argv[arg+1], "%dx%d", &canvas_width, &canvas_height) != 2) {
	    fprintf(stderr, "Could not parse canvas size '%s'.\n", argv[arg+1]);
	    canvas_size_set = 0;
	  }
	  shift = 2;
	  break;
	case 'l':
	  language_set = 1;
          language = argv[arg+1];
	  shift = 2;
          break;
	case 'c':
	  category_set = 1;
          category = argv[arg+1];
	  shift = 2;
          break;
      }
    }

    /* consume the arguments we've parsed */
    if (shift) {
      int left = *argc - arg - shift;
      if (left < 0) {
	fprintf(stderr, "Internal error parsing argument '%s'.\n", argv[arg]);
	exit(1);
      }
      memmove(&argv[arg], &argv[arg+shift], left*sizeof(*argv));
      *argc -= shift;
    } else {
      arg++;
    }
  }

  return 0;
}

int main(int argc, char *argv[])
{
  int f, i;
  unsigned char *p, *q, *o, *e;
  struct stat s;
  rogg_page_header header;
  int changed=0;

  parse_args(&argc, argv);
  if (argc < 2) {
    print_usage(stderr, argv[0]);
    exit(1);
  }

  for (i = 1; i < argc; i++) {
    f = open(argv[i], O_RDWR);
    if (f < 0) {
	fprintf(stderr, "couldn't open '%s'\n", argv[i]);
	continue;
    }
    if (fstat(f, &s) < 0) {
	fprintf(stderr, "couldn't stat '%s'\n", argv[i]);
	close(f);
	continue;
    }
    p = mmap(0, s.st_size, PROT_READ|PROT_WRITE,
	MAP_SHARED, f, 0);
    if (p == MAP_FAILED) {
	fprintf(stderr, "couldn't mmap '%s'\n", argv[i]);
	close(f);
	continue;
    }
    fprintf(stdout, "Checking Ogg file '%s'\n", argv[i]);
    e = p + s.st_size; /* pointer to the end of the file */
    q = rogg_scan(p, s.st_size); /* scan for an Ogg page */
    if (q == NULL) {
	fprintf(stdout, "couldn't find ogg data!\n");
    } else {
      if (q > p) {
	fprintf(stdout, "Skipped %d garbage bytes at the start\n", (int)(q-p));
      }
      while (q < e) {
	o = rogg_scan(q, e-q); /* find the next Ogg page */
	if (o > q) {
	  fprintf(stdout, "Hole in data! skipped %d bytes\n", (int)(o-q));
	   q = o;
	} else if (o == NULL) {
	  fprintf(stdout, "Skipped %d garbage bytes as the end\n", (int)(e-q));
	  break;
	}
	rogg_page_parse(q, &header);
	if (!header.bos) break; /* only look at the initial bos pages */
	if (verbose) {
	  print_header_info(stdout, &header);
	  int j;
	  for (j = 0; j < header.length; j++) {
	    fprintf(stdout, " %02x", header.data[j]);
	    if (!((j+1)%4)) fprintf(stdout, " ");
	    if (!((j+1)%16)) fprintf(stdout, "\n");
	  }
	  fprintf(stdout, "\n");
	}
	if (!memcmp(header.data, "\x80kate\0\0\0", 8)) {
	  print_kate_info(stdout, header.data);
	  if (canvas_size_set) {
	    fprintf(stdout, "Setting canvas size to %dx%d\n",
		canvas_width, canvas_height);
            put_canvas_size(header.data+16,canvas_width);
            put_canvas_size(header.data+18,canvas_height);
            changed = 1;
	  }
	  if (language_set) {
	    fprintf(stdout, "Setting language to %s\n",
		language);
            put15s(header.data+32, language);
            changed = 1;
	  }
	  if (category_set) {
	    fprintf(stdout, "Setting category to %s\n",
		category);
            put15s(header.data+48, category);
            changed = 1;
	  }
	  if (changed) {
	    rogg_page_update_crc(q);
	    fprintf(stdout, "New settings:\n");
	    print_kate_info(stdout, header.data);
	  }
        }
	q += header.length;
      }
    }
    munmap(p, s.st_size);
    close(f);
  }
  return 0;
}
