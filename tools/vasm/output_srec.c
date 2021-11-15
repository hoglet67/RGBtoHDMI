/* output_srec.c Motorola S-record output driver for vasm
 (c) in 2015 by Joseph Zatarski
 
 jzatar2@illinois.edu
 
  modified 2021 by Grzegorz Mazur, g.mazur@ii.pw.edu.pl

NOTE: this file makes assumptions that your character set is at least ASCII
compatible. That is, the characters 0-9, A-F, and S are the same for your
character set as in ASCII, and your newline character at least contains CR.
If your character set does not meet these requirements, then the S record will
probably look correct if it is displayed using your character set, but will
likely need to be converted to ASCII to use it elsewhere.
*/

#include "vasm.h"

#ifdef OUTSREC
static char *copyright="vasm motorola srecord output module 1.1 (c) 2015 Joseph Zatarski";

static uint8_t data[32];  /* acts as a buffer for data portion of a record */
static size_t data_size;  /* indicates current size of data[] */

/*
 * holds address for the current record
 * should only be changed after writing the current record by calling
 * write_data_buffer (which will also change srec_pc automatically after writing
 * a record, based on the size of the record just written)
 */
static unsigned long long srec_pc;

/*
 * holds address which is updated for each byte written, rather than updated
 * after writing each record. This is needed for addralign() for example.
 */
static unsigned long long pc;

/* holds address for the start record
 * default of 0 is OK for not having a start address */
static unsigned long long start_addr = 0;

#define S19 1
#define S28 2
#define S37 3
static int srecfmt = S37;

static char *start_sym; /* points to name of the start symbol, or NULL if not
                           using the execution address in the termination
                           record */

static char *default_start="start"; /* name of default execution address symbol
                                       for termination record */

/* gbm modifications 06'21 */
static void write_newline(FILE *f)
{
  if (!asciiout)
    fw8(f, '\r');
  fw8(f, '\n');
}

static char tohex(uint8_t v)
{
  return "0123456789ABCDEF"[v & 0xf];
}

static void write_hex_byte(FILE *f, uint8_t byte)
/* write a pair of ASCII characters to represent the byte in hex */
{
  fw8(f, tohex(byte >> 4));
  fw8(f, tohex(byte));
}
/* end of gbm mods */


static void write_data_buffer(FILE *f, uint8_t type)
/*
 * writes the data buffer to the output file in a record
 * then updates srec_pc for next record
 * accepts type for number of srecord desired. Currently ignores unsupported
 * types, although that should never happen.
 */
{
  uint8_t checksum;
  int i;
  
  if(data_size == 0 && type != 0) /* allow S0 record to have data size of 0 */
    return; /* nothing to write */

  /* check if address is out of range for this record type and error */
  if(type > 0 && ((srec_pc >> ((type + 1) * 8)) != 0))
    output_error(11,srec_pc);
  
  if(type > 3)
    return; /* ignore types we don't handle, but this shouldn't ever happen */

  fw8(f, 'S');
  fw8(f, type + '0');
  
  checksum = data_size + 2 + (type ? type : 1);  /* at least 2 byte address */

  write_hex_byte(f, checksum); /* count: 4 bytes for address + checksum */

  if(type > 2)
  {
    uint8_t b = srec_pc >> 24;
    write_hex_byte(f, b);
    checksum += b;
  }
  if(type > 1)
  {
    uint8_t b = srec_pc >> 16;
    write_hex_byte(f, b);
    checksum += b;
  }
  if(type > 0)
  {
    uint8_t b = srec_pc >> 8;
    write_hex_byte(f, b);
    checksum += b;
    write_hex_byte(f, srec_pc & 0xff);
    checksum += srec_pc & 0xff;
  }  
  else /* type must be 0 */
  {
    write_hex_byte(f, 0);
    write_hex_byte(f, 0);
  }
  
  for(i = 0; i < data_size; i++)
  {
    write_hex_byte(f, data[i]);
    checksum += data[i];
  }
  
  write_hex_byte(f, checksum ^ 0xff);

  write_newline(f);
  
  srec_pc += data_size;
  data_size = 0;
}

static void write_termination_record(FILE *f)
/* writes termination record, S7/8/9 depending on whether we're in S19/S28/S37
 * mode */
{
  uint8_t checksum = 0;
  
  /* check if address is out of range for this record type and error */
  if(srecfmt > 0 && ((start_addr >> ((srecfmt + 1) * 8)) != 0))
    output_error(11, start_addr);
  
  fw8(f, 'S');
  fw8(f, (10 - srecfmt) + '0'); /* writes S header depending on S19/S28/S37 */
  
  if(srecfmt == S37)
  {
    write_hex_byte(f, 5); /* count: 4 bytes for address + checksum */
    checksum += 5;
  
  }
  else if(srecfmt == S28)
  {
    write_hex_byte(f, 4); /* count: 3 bytes for address + checksum */
    checksum += 4;
  }
  else if(srecfmt == S19)
  {
    write_hex_byte(f, 3); /* count: 2 bytes for address + checksum */
    checksum += 3;
  }

  if(srecfmt >= S37)
  {
    write_hex_byte(f, (start_addr & 0xff000000) >> 24);
    checksum += (start_addr & 0xff000000) >> 24;
  }  
  if(srecfmt >= S28)
  {
    write_hex_byte(f, (start_addr & 0xff0000) >> 16);
    checksum += (start_addr & 0xff0000) >> 16;
  }  
  write_hex_byte(f, (start_addr & 0xff00) >> 8);
  checksum += (start_addr & 0xff00) >> 8;
  write_hex_byte(f, start_addr & 0xff);
  checksum += start_addr & 0xff;

  write_hex_byte(f, checksum ^ 0xff);

  write_newline(f);
}

static void put_byte_in_buffer(FILE *f, uint8_t byte)
/* puts a byte in the data buffer and flushes the buffer if we fill it up */
/* also updates pc */
{
  data[data_size] = byte;
  data_size++;
  
  if(data_size >= 32)
    write_data_buffer(f, srecfmt);
    
  pc++;
}

static void addralign(FILE *f,atom *a,section *sec)
/* modified from fwpcalign() in supp.c */
{
  int align_warning = 0;
  taddr n = balign(pc,a->align);
  taddr patlen;
  uint8_t *pat;

  if (n == 0)
    return;

  if (a->type==SPACE && a->content.sb->space==0) {  /* space align atom */
    if (a->content.sb->maxalignbytes!=0 &&  n>a->content.sb->maxalignbytes)
      return;
    pat = a->content.sb->fill;
    patlen = a->content.sb->size;
  }
  else {
    pat = sec->pad;
    patlen = sec->padbytes;
  }

  /*pc += n; */ /*done in put_byte_in_buffer()*/

  while (n % patlen) {
    if (!align_warning) {
      align_warning = 1;
      /*output_error(9,sec->name,(unsigned long)n,(unsigned long)patlen,
                   ULLTADDR(pc));*/
    }
    put_byte_in_buffer(f,0);
    n--;
  }

  /* write alignment pattern */
  while (n >= patlen) {
    int i;
    for(i = 0; i < patlen; i++)
    {
      put_byte_in_buffer(f, *(pat + i));
    }
    n -= patlen;
  }

  while (n--) {
    if (!align_warning) {
      align_warning = 1;
      /*output_error(9,sec->name,(unsigned long)n,(unsigned long)patlen,
                   ULLTADDR(pc));*/
    }
    put_byte_in_buffer(f, 0);
  }
}

static void write_output(FILE *f,section *sec,symbol *sym)
{
  section *s;
  atom *p;
  unsigned long long i, j;

  if (!sec)
    return;

  for (; sym; sym=sym->next) {
    if (sym->type == IMPORT)
      output_error(6,sym->name);  /* undefined symbol */
    if (start_sym != NULL && !strcmp(sym->name, start_sym)) /* look for start symbol */
    {
      start_addr = sym->pc;
      start_sym = NULL; /* we use this to indicate that we found the symbol */
    }
  }
  
  if (start_sym != NULL) /* if start_name is not NULL, then it means we set it
                             but we were unable to find it */
    output_error(6, start_sym);

  for (s=sec; s!=NULL; s=s->next)	/* iterate through sections */
  {
    for (data_size = 0; (*(s->name + data_size) != '\0') && (data_size < 32); data_size++)
    /* loop loads name of section into data and sets data_size properly */
    {
      data[data_size] = *(s->name + data_size);
    }
    write_data_buffer(f, 0); /* record type is S0 for header */
    
    pc = ULLTADDR(s->org);	/* start at the org address */
    srec_pc = pc;		/* need to update both */
    for (p=s->first; p; p=p->next)	/* iterate through atoms */
    {
      addralign(f,p,s);
      if(p->type == DATA)
        for (i = 0; i < p->content.db->size; i++)
          put_byte_in_buffer(f,(uint8_t)p->content.db->data[i]);
      else if (p->type == SPACE)
      {
        for (i = 0; i < p->content.sb->space; i++)
        {
          for (j = 0; j < p->content.sb->size; j++)
          {
            put_byte_in_buffer(f,p->content.sb->fill[j]);
          }
        }
      } 
    }
    
    write_data_buffer(f, srecfmt); /* now that we're done iterating through atoms */
    /* flush buffer before moving on to next section */
  }
  
  /* after all sections finished, write terminating record */
  write_termination_record(f);
}


static int output_args(char *p)
{
  if (!strcmp(p, "-s19"))
  {
    srecfmt = S19;
    return 1;
  }
  else if (!strcmp(p, "-s28"))
  {
    srecfmt = S28;
    return 1;
  }
  else if (!strcmp(p, "-s37"))
  {
    srecfmt = S37;
    return 1;
  }
  
  else if (!strcmp(p, "-exec"))
  {
    start_sym = default_start;
    return 1;
  }
  
  else if (!strncmp(p, "-exec=", 6))
  {
    start_sym = p + 6;
    return 1;
  }

  else if (!strcmp(p, "-crlf"))
  {
    asciiout = 0;
    return 1;
  }
  return 0;	
}


int init_output_srec(char **cp,void (**wo)(FILE *,section *,symbol *),int (**oa)(char *))
{
  *cp = copyright;
  *wo = write_output;
  *oa = output_args;
  asciiout=1;
  return 1;
}

#else

int init_output_srec(char **cp,void (**wo)(FILE *,section *,symbol *),int (**oa)(char *))
{
  return 0;
}
#endif
