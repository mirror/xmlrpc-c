#ifndef DATA_H_INCLUDED
#define DATA_H_INCLUDED


typedef struct 
{
    char *name,*value;
    uint16_t hash;
} TTableItem;

typedef struct
{
    TTableItem *item;
    uint16_t size,maxsize;
} TTable;

void
TableInit(TTable * const t);

void
TableFree(TTable * const t);

abyss_bool
TableAdd(TTable *     const t,
         const char * const name,
         const char * const value);

abyss_bool
TableAddReplace(TTable *     const t,
                const char * const name,
                const char * const value);

abyss_bool
TableFindIndex(TTable *     const t,
               const char * const name,
               uint16_t *   const index);

char *
TableFind(TTable *     const t,
          const char * const name);


#endif
