// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/*
 * Copyright (c) 2002-2007, Communications and Remote Sensing Laboratory, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2007, Professor Benoit Macq
 * Copyright (c) 2003-2007, Francois-Olivier Devaux 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openjpeg.h"
#include "j2k.h"
#include "jp2.h"
#include "cio.h"
#include "mj2.h"

static int int_ceildiv(int a, int b) {
	return (a + b - 1) / b;
}

/**
Size of memory first allocated for MOOV box
*/
#define TEMP_BUF 10000 

#define J2K_CODESTREAM_MAGIC "\xff\x4f\xff\x51"

/* -------------------------------------------------------------------------- */

static int test_image(const char *fname, mj2_cparameters_t *cp)
{
	FILE *reader;
	opj_image_t *image;
	unsigned char *src;
	opj_dinfo_t *dinfo;
	opj_cio_t *cio;
	opj_dparameters_t dparameters;
	int success;
	long src_len;

	success = 0;

	if((reader = fopen(fname, "rb")) == NULL) return success;

	fseek(reader, 0, SEEK_END);
	src_len = ftell(reader);
	fseek(reader, 0, SEEK_SET);
	src = (unsigned char*) malloc(src_len);
	fread(src, 1, src_len, reader);
	fclose(reader);

	if(memcmp(src, J2K_CODESTREAM_MAGIC, 4) != 0)
   {
	fprintf(stderr,"%s:%d:\n\tONLY J2K files are accepted. Quitting.\n",
	__FILE__,__LINE__);
	free(src); return success;
   }
	memset(&dparameters, 0, sizeof(opj_dparameters_t));

	opj_set_default_decoder_parameters(&dparameters);

	dinfo = opj_create_decompress(CODEC_J2K);

	opj_setup_decoder(dinfo, &dparameters);

	cio = opj_cio_open((opj_common_ptr)dinfo, src, src_len);

	if(cio == NULL) goto fin;

	image = opj_decode(dinfo, cio);

	if(image == NULL) goto fin;

	free(src); cio->buffer = NULL;
	opj_cio_close(cio);

	if(image == NULL) goto fin;

	cp->numcomps = image->numcomps;
	cp->w = image->comps[0].w;
	cp->h = image->comps[0].h;
	cp->prec = image->comps[0].prec;

	if(image->numcomps > 2)/* RGB or RGBA */
   {
	cp->enumcs = ENUMCS_SRGB;
/*  cp->CbCr_subsampling_dx = 0; */
/*  cp->CbCr_subsampling_dy = 0; */
   }
	else /* GRAY or GRAYA */
   {
	cp->enumcs = ENUMCS_GRAY;
/*  cp->CbCr_subsampling_dx = 0; */
/*  cp->CbCr_subsampling_dy = 0; */
   }
	if(image->icc_profile_buf)
   {
	cp->meth = 2;
	free(image->icc_profile_buf); image->icc_profile_buf = NULL;
   }
	else cp->meth = 1;

	success = 1;
fin:
	if(dinfo)
	 opj_destroy_decompress(dinfo);

	if(image)
	 opj_image_destroy(image);

	return success;
}

/**
sample error callback expecting a FILE* client object
*/
void error_callback(const char *msg, void *client_data) {
	FILE *stream = (FILE*)client_data;
	fprintf(stream, "[ERROR] %s", msg);
}
/**
sample warning callback expecting a FILE* client object
*/
void warning_callback(const char *msg, void *client_data) {
	FILE *stream = (FILE*)client_data;
	fprintf(stream, "[WARNING] %s", msg);
}
/**
sample debug callback expecting a FILE* client object
*/
void info_callback(const char *msg, void *client_data) {
	FILE *stream = (FILE*)client_data;
	fprintf(stream, "[INFO] %s", msg);
}

/* -------------------------------------------------------------------------- */



static void read_siz_marker(FILE *file, opj_image_t *image)
{
  int len,i;
  char buf, buf2[2];
  unsigned char *siz_buffer;
	opj_cio_t *cio;
  
  fseek(file, 0, SEEK_SET);
  do {
    fread(&buf,1,1, file);
    if (buf==(char)0xff)
      fread(&buf,1,1, file);
  }
  while (!(buf==(char)0x51));
  
  fread(buf2,2,1,file);		/* Lsiz                */
  len = ((buf2[0])<<8) + buf2[1];
  
  siz_buffer = (unsigned char*) malloc(len * sizeof(unsigned char));
  fread(siz_buffer,len, 1, file);
  cio = opj_cio_open(NULL, siz_buffer, len);

  if(cio == NULL) goto fin;
 
  cio_read(cio, 2);			/* Rsiz (capabilities) */
  image->x1 = cio_read(cio, 4);	/* Xsiz                */
  image->y1 = cio_read(cio, 4);	/* Ysiz                */
  image->x0 = cio_read(cio, 4);	/* X0siz               */
  image->y0 = cio_read(cio, 4);	/* Y0siz               */
  cio_skip(cio, 16);			/* XTsiz, YTsiz, XT0siz, YT0siz        */
  
  image->numcomps = cio_read(cio,2);	/* Csiz                */
  image->comps =
    (opj_image_comp_t *) malloc(image->numcomps * sizeof(opj_image_comp_t));
	
  for (i = 0; i < image->numcomps; i++) {
    int tmp;
    tmp = cio_read(cio,1);		/* Ssiz_i          */
    image->comps[i].prec = (tmp & 0x7f) + 1;
    image->comps[i].sgnd = tmp >> 7;
    image->comps[i].dx = cio_read(cio,1);	/* XRsiz_i         */
    image->comps[i].dy = cio_read(cio,1);	/* YRsiz_i         */
    image->comps[i].resno_decoded = 0;	/* number of resolution decoded */
    image->comps[i].factor = 0;	/* reducing factor by component */
  }
  fseek(file, 0, SEEK_SET);
  opj_cio_close(cio);

fin:
  free(siz_buffer);
}

static void setparams(opj_mj2_t *movie, opj_image_t *image) {
  int i, depth_0, depth, sign;
  
  movie->tk[0].w = int_ceildiv(image->x1 - image->x0, image->comps[0].dx);
  movie->tk[0].h = int_ceildiv(image->y1 - image->y0, image->comps[0].dy);

  mj2_init_stdmovie(movie);
  
  movie->tk[0].depth = image->comps[0].prec;

  movie->tk[0].CbCr_subsampling_dx = 0;
  movie->tk[0].CbCr_subsampling_dy = 0;  
  movie->tk[0].sample_rate = 25;
  
  movie->tk[0].jp2_struct.numcomps = image->numcomps;/* NC */
	
	/* Init Standard jp2 structure */
	
  movie->tk[0].jp2_struct.comps = (opj_jp2_comps_t *)
   malloc(movie->tk[0].jp2_struct.numcomps * sizeof(opj_jp2_comps_t));
  movie->tk[0].jp2_struct.precedence = 0;   /* PRECEDENCE*/
  movie->tk[0].jp2_struct.approx = 0;   /* APPROX*/
  movie->tk[0].jp2_struct.brand = JP2_JP2;	/* BR         */
  movie->tk[0].jp2_struct.minversion = 0;	/* MinV       */
  movie->tk[0].jp2_struct.numcl = 1;
  movie->tk[0].jp2_struct.cl = (unsigned int *) 
   malloc(movie->tk[0].jp2_struct.numcl * sizeof(int));
  movie->tk[0].jp2_struct.cl[0] = JP2_JP2;	/* CL0 : JP2  */
  movie->tk[0].jp2_struct.C = 7;      /* C : Always 7*/
  movie->tk[0].jp2_struct.UnkC = 0;      /* UnkC, colorspace specified in colr box*/
  movie->tk[0].jp2_struct.IPR = 0;      /* IPR, no intellectual property*/
  movie->tk[0].jp2_struct.w = int_ceildiv(image->x1 - image->x0, image->comps[0].dx);
  movie->tk[0].jp2_struct.h = int_ceildiv(image->y1 - image->y0, image->comps[0].dy);
  
  depth_0 = image->comps[0].prec - 1;
  sign = image->comps[0].sgnd;
  movie->tk[0].jp2_struct.bpc = depth_0 + (sign << 7);
  
  for (i = 1; i < image->numcomps; i++) {
    depth = image->comps[i].prec - 1;
    sign = image->comps[i].sgnd;
    if (depth_0 != depth)
      movie->tk[0].jp2_struct.bpc = 255;
  }
  
  for (i = 0; i < image->numcomps; i++)
    movie->tk[0].jp2_struct.comps[i].bpcc =
    image->comps[i].prec - 1 + (image->comps[i].sgnd << 7);
  
  if ((image->numcomps == 1 || image->numcomps == 3)
    && (movie->tk[0].jp2_struct.bpc != 255))
    movie->tk[0].jp2_struct.meth = 1;
  else
    movie->tk[0].jp2_struct.meth = 2;
	
}

int main(int argc, char *argv[]) {
	opj_cinfo_t* cinfo; 
	opj_event_mgr_t event_mgr;		/* event manager */  
  unsigned int snum;
  opj_mj2_t *movie;
  mj2_sample_t *sample;
  unsigned char* frame_codestream;
  FILE *mj2file, *j2kfile;
  char *j2kfilename;
  unsigned char *buf;
  int offset, mdat_initpos, failed;
  opj_image_t img;
 	opj_cio_t *cio;
	mj2_cparameters_t parameters;
	
  if (argc != 3) {
    printf("\nUsage: %s source_location mj2_filename\n",argv[0]);
    printf("Example: %s input/input output.mj2\n\n",argv[0]);
    return 1;
  }
  
  mj2file = fopen(argv[2], "wb");
  
  if (!mj2file) {
    fprintf(stderr, "failed to open %s for writing\n", argv[2]);
    return 1;
  }
	failed = 1;
	memset(&img, 0, sizeof(opj_image_t));
	/*
	configure the event callbacks (not required)
	setting of each callback is optionnal
	*/
	memset(&event_mgr, 0, sizeof(opj_event_mgr_t));
	event_mgr.error_handler = error_callback;
	event_mgr.warning_handler = warning_callback;
	event_mgr.info_handler = info_callback;

	/* get a MJ2 decompressor handle */
	cinfo = mj2_create_compress();

	if(cinfo == NULL) goto fin;

	/* catch events using our callbacks and give a local context */
	opj_set_event_mgr((opj_common_ptr)cinfo, &event_mgr, stderr);	
	
	/* setup the decoder encoding parameters using user parameters */
	memset(&parameters, 0, sizeof(mj2_cparameters_t));
	movie = (opj_mj2_t*) cinfo->mj2_handle;

	j2kfilename = (char*)malloc(strlen(argv[1]) + 12);/* max. '%6d' */
	sprintf(j2kfilename, "%s_00000.j2k",argv[1]);

	if(test_image(j2kfilename, &parameters) == 0) goto fin;

	parameters.frame_rate = 25; /* DEFAULT */

	mj2_setup_encoder(movie, &parameters);

  
/* Writing JP, FTYP and MDAT boxes 
   Assuming that the JP and FTYP boxes won't be longer than 300 bytes 
*/
	
  buf = (unsigned char*) malloc (300 * sizeof(unsigned char)); 

  if(buf == NULL) goto fin;

  cio = opj_cio_open(movie->cinfo, buf, 300);

  if(cio == NULL) goto fin;

  mj2_write_jp(cio);
  mj2_write_ftyp(movie, cio);
  mdat_initpos = cio_tell(cio);
  cio_skip(cio, 4);
  cio_write(cio,MJ2_MDAT, 4);	
  fwrite(buf,cio_tell(cio),1,mj2file);
  free(buf);
	
/* Insert each j2k codestream in a JP2C box */
  snum=0;
  offset = 0;  
  while(1)
  {
    sample = &movie->tk[0].sample[snum];
    sprintf(j2kfilename,"%s_%05d.j2k",argv[1],snum);
    j2kfile = fopen(j2kfilename, "rb");
    if (!j2kfile) 
 {
/* No more files found: done. Leave while(1) loop: */
	fprintf(stdout,"%d frames are being added to the MJ2 file\n",snum);
	break;
 }

/* Calculating offset for samples and chunks: */
    offset += cio_tell(cio);     
    sample->offset = offset;
/* There will be one sample per chunk: */
    movie->tk[0].chunk[snum].offset = offset;  
    
/* Calculating sample size: */
    fseek(j2kfile,0,SEEK_END);	
/* Sample size is codestream + JP2C box header: */
    sample->sample_size = ftell(j2kfile) + 8;
    fseek(j2kfile,0,SEEK_SET);
    
/* Reading siz marker of j2k image for the first codestream */
    if (snum==0)	      
      read_siz_marker(j2kfile, &img);
    
    frame_codestream = (unsigned char*) 
	 malloc (sample->sample_size+8); 
	cio = opj_cio_open(movie->cinfo, frame_codestream, sample->sample_size);    

	if(cio == NULL) goto fin;

/* Writing JP2C box header: */
    cio_write(cio,sample->sample_size, 4);
    cio_write(cio,JP2_JP2C, 4);
    
/* Writing codestream from J2K file to MJ2 file */
    fread(frame_codestream+8,sample->sample_size-8,1,j2kfile);
    fwrite(frame_codestream,sample->sample_size,1,mj2file);
    cio_skip(cio, sample->sample_size-8);
    
    fclose(j2kfile);
    snum++;
    movie->tk[0].sample = (mj2_sample_t*)
		realloc(movie->tk[0].sample, (snum+1) * sizeof(mj2_sample_t));
    movie->tk[0].chunk = (mj2_chunk_t*)
		realloc(movie->tk[0].chunk, (snum+1) * sizeof(mj2_chunk_t));
    free(frame_codestream);
  }/* while(1) */
  
/* Writing the MDAT box length in header */
  offset += cio_tell(cio);
  buf = (unsigned char*) malloc (4 * sizeof(unsigned char));

  if(buf == NULL) goto fin;

  cio = opj_cio_open(movie->cinfo, buf, 4);

  if(cio == NULL) goto fin;

  cio_write(cio,offset-mdat_initpos,4); 
  fseek(mj2file,(long)mdat_initpos,SEEK_SET);
  fwrite(buf,4,1,mj2file);
  fseek(mj2file,0,SEEK_END);

/*  if(cio->openmode == OPJ_STREAM_READ) */ free(buf);
  opj_cio_close(cio);

/* Setting movie parameters: */
  movie->tk[0].num_samples=snum;
  movie->tk[0].num_chunks=snum;
  setparams(movie, &img);
	
/* Writing MOOV box */
  buf = (unsigned char*) malloc ((TEMP_BUF+snum*20) * sizeof(unsigned char));

  if(buf == NULL) goto fin;

  cio = opj_cio_open(movie->cinfo, buf, (TEMP_BUF+snum*20));

  if(cio == NULL) goto fin;

  mj2_write_moov(movie, cio);
  fwrite(buf,cio_tell(cio),1,mj2file);
	
  free(img.comps);

/*  if(cio->openmode == OPJ_STREAM_READ) */ free(buf);
  opj_cio_close(cio);

  failed = 0; buf = NULL;

fin:
  fclose(mj2file);

  if(failed) remove(argv[2]);

  if(buf) free(buf);

	if(cinfo)
   {
	mj2_destroy_compress(movie);
	free(cinfo);
   }
  free(j2kfilename);

  return failed;
}
