/*
 *  Wellcome Trust Sanger Institute
 *  Copyright (C) 2013  Wellcome Trust Sanger Institute
 *  
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 3
 *  of the License, or (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <regex.h>
#include <sys/types.h>
#include "kseq.h"
#include "vcf.h"
#include "alignment-file.h"
#include "snp-sites.h"

KSEQ_INIT(gzFile, gzread)

// Given a file handle, return the length of the current line
int line_length(FILE * alignment_file_pointer)
{
	char szBuffer[MAX_READ_BUFFER] = {0};  
	char *pcRes         = NULL; 
	int  length_of_line    = 0;    
	int total_length_of_line = 0;
	
	while((pcRes = fgets(szBuffer, sizeof(szBuffer), alignment_file_pointer))  != NULL){
		length_of_line = strlen(szBuffer) - 1;
		total_length_of_line = total_length_of_line + length_of_line;
		if((szBuffer)[length_of_line] == '\n'){
			break;
		}
	}
	return total_length_of_line;
}

void advance_to_sequence(FILE * alignment_file_pointer)
{
	// Skip first line since its a comment, ToDo make this better by doing a regex on the line
	line_length(alignment_file_pointer);
}

void advance_to_sequence_name(FILE * alignment_file_pointer)
{
	// Skip sequence line, TODO make this work properly
	line_length(alignment_file_pointer);
}


void get_bases_for_each_snp(char filename[], int snp_locations[], char ** bases_for_snps, size_t length_of_genome, int number_of_snps)
{
  int l;
  int i = 0;
  int sequence_number = 0;
  size_t length_of_genome_found =0;
	
  gzFile fp;
  kseq_t *seq;
	
  fp = gzopen(filename, "r");
  seq = kseq_init(fp);

  while ((l = kseq_read(seq)) >= 0) 
    {
      if(sequence_number == 0)
	{
	  length_of_genome_found = seq->seq.l;
	}
      for(i = 0; i< number_of_snps; i++)
	{
	  bases_for_snps[i][sequence_number] = toupper(((char *) seq->seq.s)[snp_locations[i]]);
	}
      
      if(seq->seq.l != length_of_genome_found)
	{
	  fprintf(stderr, "Alignment %s contains sequences of unequal length. Expected length is %d but got %d in sequence %s\n\n",filename, length_of_genome_found, seq->seq.l,seq->name.s);
	  fflush(stderr);
	  exit(EXIT_FAILURE);
	}
		
      sequence_number++;
    }

  kseq_destroy(seq);
  gzclose(fp);
}

int genome_length(char filename[])
{
	size_t length_of_genome;
	
	if( access( filename, F_OK ) == -1 ) {
		printf("Cannot calculate genome_length because file '%s' doesnt exist\n",filename);
		exit(0);
  }

	gzFile fp;
	kseq_t *seq;
	
	fp = gzopen(filename, "r");
	seq = kseq_init(fp);
  kseq_read(seq);

  length_of_genome = seq->seq.l;

	kseq_destroy(seq);
	gzclose(fp);
	return length_of_genome;
}


int number_of_sequences_in_file(char filename[])
{
  int number_of_sequences = 0;
  int l;
	
	gzFile fp;
	kseq_t *seq;
	
	fp = gzopen(filename, "r");
	seq = kseq_init(fp);
  
	while ((l = kseq_read(seq)) >= 0) {
    number_of_sequences++;
	}
	kseq_destroy(seq);
	gzclose(fp);
	return number_of_sequences;
}

int build_reference_sequence(char reference_sequence[], char filename[])
{
  return build_reference_sequence_and_truncate(reference_sequence,filename,0);
}


int build_reference_sequence_and_truncate(char reference_sequence[], char filename[], size_t buffer_length)
{
  int i;
  
  size_t length_of_genome;
  
  gzFile fp;
  kseq_t *seq;
  
  fp = gzopen(filename, "r");
  seq = kseq_init(fp);
  kseq_read(seq);

  length_of_genome = seq->seq.l;
  //printf("%zd\n", length_of_genome);
  //printf("%zd\n", buffer_length);


  for(i = 0; i < seq->seq.l ; i++)
    {
      if( buffer_length != 0  && i >= buffer_length - 1 )
	  break;

      reference_sequence[i] = toupper(seq->seq.s[i]);
    }
  reference_sequence[i] = '\0';
  //printf("%s\n", reference_sequence);
  kseq_destroy(seq);
  gzclose(fp);
  return 1;
}

int is_unknown(char base)
{
  switch (toupper(base)) {
    case 'N':
    case '-':
    case '*':
      return 1;
    default:
      return 0;
  }
}

int detect_snps(char reference_sequence[], char filename[], size_t length_of_genome)
{
  int i;
  int l;
  int number_of_snps = 0;
  
  gzFile fp;
  kseq_t *seq;
  
  fp = gzopen(filename, "r");
  seq = kseq_init(fp);
  // First sequence is the reference sequence so skip it
  kseq_read(seq);
  
  while ((l = kseq_read(seq)) >= 0) {
    for(i = 0; i < length_of_genome; i++)
      {
	// If there is an indel in the reference sequence, replace with the first proper base you find
	if(is_unknown(reference_sequence[i]) && !is_unknown(seq->seq.s[i]))
	  {
	    reference_sequence[i] = toupper(seq->seq.s[i]);
	  }
	
	if(! is_unknown(reference_sequence[i]) && ! is_unknown(seq->seq.s[i]) && (reference_sequence[i] != toupper(seq->seq.s[i])))
	  {
	    reference_sequence[i] = '*';
	    number_of_snps++;
	  }
      } 
  }
  kseq_destroy(seq);
  gzclose(fp);

  return number_of_snps;
}


char * read_line(char sequence[], FILE * pFilePtr)
{
  char *pcRes         = NULL;  
  long   lineLength    = 0; 
  char current_line_buffer[MAX_READ_BUFFER] = {0};
	
	
  while((pcRes = fgets(current_line_buffer, sizeof(current_line_buffer), pFilePtr))  != NULL){
    if(strlen(sequence) > 0)
      {
	sequence = realloc(sequence, sizeof(char)*(strlen(sequence) + strlen(current_line_buffer) + 2) );
      }
    strcat(sequence,current_line_buffer);
    current_line_buffer[0] = '\0';
    lineLength = strlen(sequence);
    //if end of line character is found then exit from loop
		
    if((sequence)[lineLength] == '\n' || (sequence)[lineLength] == '\0'){
      break;
    }
  }
	 
	 
  return sequence;
}


void get_sample_names_for_header(char filename[], char ** sequence_names, int number_of_samples)
{
  int l;
  int i = 0;
	
	gzFile fp;
	kseq_t *seq;
	
	fp = gzopen(filename, "r");
	seq = kseq_init(fp);
  
	while ((l = kseq_read(seq)) >= 0) {
		strcpy(sequence_names[i], seq->name.s);
    i++;
	}
	kseq_destroy(seq);
	gzclose(fp);

}


char filter_invalid_characters(char input_char)
{
	regex_t regex;
	int reti;
	char  input_chars[10];
	input_chars[0] =input_char;
	input_chars[1] = '\0';
	
	/* Compile regular expression */
	reti = regcomp(&regex, "^[[:alnum:]_.]", 0);

	/* Execute regular expression */
	reti = regexec(&regex, input_chars, 0, NULL, 0);
	if( !reti ){
		return input_char;
	}
	else if( reti == REG_NOMATCH ){
		return '\0';
	}
	return '\0';

	regfree(&regex);
}


