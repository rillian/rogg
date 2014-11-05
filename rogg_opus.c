/*
   Copyright (C) 2014 Xiph.org Foundation

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

/* opus header modification script using the rogg library */

/* compile with
   gcc -O2 -g -Wall -I. -o rogg_opus rogg.c rogg_opus.c -lm
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

#include <math.h>

#include <rogg.h>

int verbose = 0;
int gain_set = 0;
int gain;

/* little endian accessors for the opus header data */
int get16(unsigned char *data)
{
  return data[0] | (data[1] << 8);
}
int get32(unsigned char *data)
{
  return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}
void put16(unsigned char *data, int v)
{
  data[0] = v & 0xFF;
  data[1] = (v >> 8) & 0xFF;
}
void put32(unsigned char *data, int v)
{
  data[0] = v & 0xFF;
  data[1] = (v >>  8) & 0xFF;
  data[2] = (v >> 16) & 0xFF;
  data[3] = (v >> 24) & 0xFF;
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

void print_opus_info(FILE *out, unsigned char *data)
{
  int version = data[8];
  int channels = data[9];
  int preskip = get16(data + 10);
  int input = get32(data + 12);
  int gain = get16(data + 16);
  int mapping = data[18];
  fprintf(out, "  Opus info header version %d (%d.%d)\n",
	version, version >> 4, version & 0xf);
  fprintf(out, "    channels %d%s\n", channels,
      channels == 0 ? " INVALID!" :
      channels == 1 && mapping == 0 ? " (mono)" :
      channels == 2 && mapping == 0 ? " (stereo)" :
      channels == 1 && mapping == 1 ? " (mono)" :
      channels == 2 && mapping == 1 ? " (stereo)" :
      channels == 3 && mapping == 1 ? " (wide stereo)" :
      channels == 4 && mapping == 1 ? " (quadraphonic)" :
      channels == 5 && mapping == 1 ? " (5.0 surround)" :
      channels == 6 && mapping == 1 ? " (5.1 surround)" :
      channels == 7 && mapping == 1 ? " (6.1 surround)" :
      channels == 8 && mapping == 1 ? " (7.1 surround)" :
      channels > 8 && mapping == 1 ? " INVALID!" :
      mapping == 255 ?  "discrete" :
      " UNDEFINED");
  fprintf(out, "    preskip %d samples\n", preskip);
  fprintf(out, "    original sample rate %d Hz\n", input);
  fprintf(out, "    output gain %d (%+.3lf dB)\n", gain, (double)gain/256.0);
  fprintf(out, "    channel mapping %d\n", mapping);
}

void print_usage(FILE *out, char *name)
{
  fprintf(stderr, "Script for editing opus headers, in place.\n");
  fprintf(stderr, "%s [-v] [-a num:den] [-f num:den] <file1.ogg> [<file2.ogg>...]\n",
	name);
  fprintf(stderr, "    -v          print more information\n"
		  "    -g gain     set the EBU R128 output gain\n");
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
	case 'g':
	  gain_set = 1;
	  /* read gain from the next arg */
	  if (sscanf(argv[arg+1], "%d", &gain) != 1) {
	    fprintf(stderr, "Could not parse gain '%s'.\n", argv[arg+1]);
	    gain_set = 0;
	  }
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
	if (!memcmp(header.data, "OpusHead", 8)) {
	  print_opus_info(stdout, header.data);
	  if (gain_set) {
            fprintf(stderr, "Setting gain isn't yet supported.\n");
	  }
	  if (gain_set) {
	    rogg_page_update_crc(q);
	    fprintf(stdout, "New settings:\n");
	    print_opus_info(stdout, header.data);
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
