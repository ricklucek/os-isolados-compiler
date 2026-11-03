#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stddef.h>

#include "language_types.h"

typedef enum {
    SYMBOL_VARIABLE = 0,
    SYMBOL_VECTOR,
    SYMBOL_PARAMETER,
    SYMBOL_FUNCTION,
    SYMBOL_PRINCIPAL,
    SYMBOL_RECORD
} SymbolKind;

typedef struct {
    char *name;
    SymbolKind kind;
    LangType type;
    size_t vector_size;
    LangType *parameter_types;
    size_t parameter_count;
    char *record_type_name;
    int line;
    int column;
} Symbol;

typedef struct SymbolScope {
    char *name;
    size_t depth;
    struct SymbolScope *parent;
    Symbol *symbols;
    size_t symbol_count;
    size_t symbol_capacity;
} SymbolScope;

typedef struct {
    SymbolScope *global;
    SymbolScope *current;
} SymbolTable;

typedef enum {
    SYMBOL_DECLARE_OK = 0,
    SYMBOL_DECLARE_DUPLICATE,
    SYMBOL_DECLARE_MEMORY_ERROR
} SymbolDeclareStatus;

const char *symbol_kind_name(SymbolKind kind);

int symbol_table_init(SymbolTable *table);
void symbol_table_free(SymbolTable *table);
int symbol_table_enter_scope(SymbolTable *table, const char *name);
void symbol_table_leave_scope(SymbolTable *table);

const Symbol *symbol_table_lookup(const SymbolTable *table, const char *name);
const Symbol *symbol_table_lookup_current(const SymbolTable *table, const char *name);
const Symbol *symbol_table_lookup_global(const SymbolTable *table, const char *name);

SymbolDeclareStatus symbol_table_declare(
    SymbolTable *table, const char *name, SymbolKind kind, LangType type,
    size_t vector_size, const LangType *parameter_types, size_t parameter_count,
    const char *record_type_name, int line, int column,
    const Symbol **out_symbol);

#endif
