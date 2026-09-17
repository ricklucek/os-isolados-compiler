#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stddef.h>

#include "token.h"

typedef enum {
    LANG_TYPE_ERROR = 0,
    LANG_TYPE_BOOL,
    LANG_TYPE_INTEIRA,
    LANG_TYPE_FLUT,
    LANG_TYPE_PALAVRA,
    LANG_TYPE_DUPLOCARPADO,
    LANG_TYPE_VAZIO
} LangType;

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

LangType lang_type_from_token(TokenType token_type);
const char *lang_type_name(LangType type);
const char *symbol_kind_name(SymbolKind kind);
int lang_type_is_numeric(LangType type);
int lang_type_can_assign(LangType target, LangType source);
LangType lang_type_common_numeric(LangType left, LangType right);

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
    int line, int column, const Symbol **out_symbol);

#endif
