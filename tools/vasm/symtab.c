/* symtab.c  hashtable file for vasm */
/* (c) in 2002-2004,2008,2011,2014 by Volker Barthelmann and Frank Wille */

#include "vasm.h"

hashtable *new_hashtable(size_t size)
{
  hashtable *new = mymalloc(sizeof(*new));

  new->size = size;
  new->collisions = 0;
  new->entries = mycalloc(size*sizeof(*new->entries));
  return new;
}

size_t hashcode(char *name)
{
  size_t h = 5381;
  int c;

  while (c = (unsigned char)*name++)
    h = ((h << 5) + h) + c;
  return h;
}

size_t hashcodelen(char *name,int len)
{
  size_t h = 5381;

  while (len--)
    h = ((h << 5) + h) + (unsigned char)*name++;
  return h;
}

size_t hashcode_nc(char *name)
{
  size_t h = 5381;
  int c;

  while (c = (unsigned char)*name++)
    h = ((h << 5) + h) + tolower(c);
  return h;
}

size_t hashcodelen_nc(char *name,int len)
{
  size_t h = 5381;

  while (len--)
    h = ((h << 5) + h) + tolower((unsigned char)*name++);
  return h;
}

/* add to hashtable; name must be unique */
void add_hashentry(hashtable *ht,char *name,hashdata data)
{
  size_t i=nocase?(hashcode_nc(name)%ht->size):(hashcode(name)%ht->size);
  hashentry *new=mymalloc(sizeof(*new));
  new->name=name;
  new->data=data;
  if(debug){
    if(ht->entries[i])
      ht->collisions++;
  }
  new->next=ht->entries[i];
  ht->entries[i]=new;
}

/* remove from hashtable; name must be unique */
void rem_hashentry(hashtable *ht,char *name,int no_case)
{
  size_t i=no_case?(hashcode_nc(name)%ht->size):(hashcode(name)%ht->size);
  hashentry *p,*last;

  for(p=ht->entries[i],last=NULL;p;p=p->next){
    if(!strcmp(name,p->name)||(no_case&&!stricmp(name,p->name))){
      if(last==NULL)
        ht->entries[i]=p->next;
      else
        last->next=p->next;
      myfree(p);
      return;
    }
    last=p;
  }
  ierror(0);
}

/* finds unique entry in hashtable */
int find_name(hashtable *ht,char *name,hashdata *result)
{
  if(nocase)
    return find_name_nc(ht,name,result);
  else{
    size_t i=hashcode(name)%ht->size;
    hashentry *p;
    for(p=ht->entries[i];p;p=p->next){
      if(!strcmp(name,p->name)){
        *result=p->data;
        return 1;
      }else
        ht->collisions++;
    }
  }
  return 0;
}

/* same as above, but uses len instead of zero-terminated string */
int find_namelen(hashtable *ht,char *name,int len,hashdata *result)
{
  if(nocase)
    return find_namelen_nc(ht,name,len,result);
  else{
    size_t i=hashcodelen(name,len)%ht->size;
    hashentry *p;
    for(p=ht->entries[i];p;p=p->next){
      if(!strncmp(name,p->name,len)&&p->name[len]==0){
        *result=p->data;
        return 1;
      }else
        ht->collisions++;
    }
  }
  return 0;
}

/* finds unique entry in hashtable - case insensitive */
int find_name_nc(hashtable *ht,char *name,hashdata *result)
{
  size_t i=hashcode_nc(name)%ht->size;
  hashentry *p;
  for(p=ht->entries[i];p;p=p->next){
    if(!stricmp(name,p->name)){
      *result=p->data;
      return 1;
    }else
      ht->collisions++;
  }
  return 0;
}

/* same as above, but uses len instead of zero-terminated string */
int find_namelen_nc(hashtable *ht,char *name,int len,hashdata *result)
{
  size_t i=hashcodelen_nc(name,len)%ht->size;
  hashentry *p;
  for(p=ht->entries[i];p;p=p->next){
    if(!strnicmp(name,p->name,len)&&p->name[len]==0){
      *result=p->data;
      return 1;
    }else
      ht->collisions++;
  }
  return 0;
}
