#include "symbol_table.h"

#include <stdlib.h>
#include <string.h>

#define SYMBOL_INITIAL_CAPACITY 8U

static char *copy_string(const char *text) {
    size_t length;
    char *copy;
    if (text == NULL) return NULL;
    length = strlen(text);
    copy = malloc(length + 1U);
    if (copy == NULL) return NULL;
    memcpy(copy, text, length + 1U);
    return copy;
}

static void symbol_free(Symbol *symbol) {
    if (symbol == NULL) return;
    free(symbol->name);
    free(symbol->parameter_types);
    memset(symbol, 0, sizeof(*symbol));
}

static SymbolScope *scope_create(const char *name, size_t depth,
                                 SymbolScope *parent) {
    SymbolScope *scope = calloc(1U, sizeof(*scope));
    if (scope == NULL) return NULL;
    scope->name = copy_string(name != NULL ? name : "scope");
    if (scope->name == NULL) {
        free(scope);
        return NULL;
    }
    scope->depth = depth;
    scope->parent = parent;
    return scope;
}

static void scope_free(SymbolScope *scope) {
    size_t i;
    if (scope == NULL) return;
    for (i = 0U; i < scope->symbol_count; ++i)
        symbol_free(&scope->symbols[i]);
    free(scope->symbols);
    free(scope->name);
    free(scope);
}

LangType lang_type_from_token(TokenType token_type) {
    switch (token_type) {
        case TOKEN_BOOL: return LANG_TYPE_BOOL;
        case TOKEN_INTEIRA: return LANG_TYPE_INTEIRA;
        case TOKEN_FLUT: return LANG_TYPE_FLUT;
        case TOKEN_PALAVRA: return LANG_TYPE_PALAVRA;
        case TOKEN_DUPLOCARPADO: return LANG_TYPE_DUPLOCARPADO;
        case TOKEN_VAZIO: return LANG_TYPE_VAZIO;
        default: return LANG_TYPE_ERROR;
    }
}

const char *lang_type_name(LangType type) {
    switch (type) {
        case LANG_TYPE_BOOL: return "bool";
        case LANG_TYPE_INTEIRA: return "inteira";
        case LANG_TYPE_FLUT: return "flut";
        case LANG_TYPE_PALAVRA: return "palavra";
        case LANG_TYPE_DUPLOCARPADO: return "duplocarpado";
        case LANG_TYPE_VAZIO: return "vazio";
        case LANG_TYPE_ERROR: return "<erro>";
        default: return "<tipo-desconhecido>";
    }
}

const char *symbol_kind_name(SymbolKind kind) {
    switch (kind) {
        case SYMBOL_VARIABLE: return "variavel";
        case SYMBOL_VECTOR: return "vetor";
        case SYMBOL_PARAMETER: return "parametro";
        case SYMBOL_FUNCTION: return "funcao";
        case SYMBOL_PRINCIPAL: return "principal";
        case SYMBOL_RECORD: return "registro";
        default: return "simbolo";
    }
}

int lang_type_is_numeric(LangType type) {
    return type == LANG_TYPE_INTEIRA || type == LANG_TYPE_FLUT ||
           type == LANG_TYPE_DUPLOCARPADO;
}

static int numeric_rank(LangType type) {
    switch (type) {
        case LANG_TYPE_INTEIRA: return 1;
        case LANG_TYPE_FLUT: return 2;
        case LANG_TYPE_DUPLOCARPADO: return 3;
        default: return 0;
    }
}

int lang_type_can_assign(LangType target, LangType source) {
    if (target == LANG_TYPE_ERROR || source == LANG_TYPE_ERROR) return 0;
    if (target == source) return 1;
    if (lang_type_is_numeric(target) && lang_type_is_numeric(source))
        return numeric_rank(source) <= numeric_rank(target);
    return 0;
}

LangType lang_type_common_numeric(LangType left, LangType right) {
    if (!lang_type_is_numeric(left) || !lang_type_is_numeric(right))
        return LANG_TYPE_ERROR;
    return numeric_rank(left) >= numeric_rank(right) ? left : right;
}

int symbol_table_init(SymbolTable *table) {
    if (table == NULL) return 0;
    table->global = scope_create("global", 0U, NULL);
    if (table->global == NULL) {
        table->current = NULL;
        return 0;
    }
    table->current = table->global;
    return 1;
}

void symbol_table_leave_scope(SymbolTable *table) {
    SymbolScope *scope;
    if (table == NULL || table->current == NULL ||
        table->current == table->global)
        return;
    scope = table->current;
    table->current = scope->parent;
    scope_free(scope);
}

void symbol_table_free(SymbolTable *table) {
    if (table == NULL) return;
    while (table->current != NULL && table->current != table->global)
        symbol_table_leave_scope(table);
    scope_free(table->global);
    table->global = NULL;
    table->current = NULL;
}

int symbol_table_enter_scope(SymbolTable *table, const char *name) {
    SymbolScope *scope;
    if (table == NULL || table->current == NULL) return 0;
    scope = scope_create(name, table->current->depth + 1U, table->current);
    if (scope == NULL) return 0;
    table->current = scope;
    return 1;
}

static const Symbol *scope_lookup(const SymbolScope *scope,
                                  const char *name) {
    size_t i;
    if (scope == NULL || name == NULL) return NULL;
    for (i = 0U; i < scope->symbol_count; ++i) {
        if (strcmp(scope->symbols[i].name, name) == 0)
            return &scope->symbols[i];
    }
    return NULL;
}

const Symbol *symbol_table_lookup_current(const SymbolTable *table,
                                          const char *name) {
    if (table == NULL) return NULL;
    return scope_lookup(table->current, name);
}

const Symbol *symbol_table_lookup_global(const SymbolTable *table,
                                         const char *name) {
    if (table == NULL) return NULL;
    return scope_lookup(table->global, name);
}

const Symbol *symbol_table_lookup(const SymbolTable *table,
                                  const char *name) {
    const SymbolScope *scope;
    const Symbol *symbol;
    if (table == NULL || name == NULL) return NULL;
    for (scope = table->current; scope != NULL; scope = scope->parent) {
        symbol = scope_lookup(scope, name);
        if (symbol != NULL) return symbol;
    }
    return NULL;
}

static int scope_grow(SymbolScope *scope) {
    size_t new_capacity;
    Symbol *new_symbols;
    if (scope == NULL) return 0;
    new_capacity = scope->symbol_capacity == 0U
                       ? SYMBOL_INITIAL_CAPACITY
                       : scope->symbol_capacity * 2U;
    new_symbols = realloc(scope->symbols,
                          new_capacity * sizeof(*new_symbols));
    if (new_symbols == NULL) return 0;
    scope->symbols = new_symbols;
    scope->symbol_capacity = new_capacity;
    return 1;
}

SymbolDeclareStatus symbol_table_declare(
    SymbolTable *table, const char *name, SymbolKind kind, LangType type,
    size_t vector_size, const LangType *parameter_types, size_t parameter_count,
    int line, int column, const Symbol **out_symbol) {
    SymbolScope *scope;
    Symbol *symbol;

    if (out_symbol != NULL) *out_symbol = NULL;
    if (table == NULL || table->current == NULL || name == NULL)
        return SYMBOL_DECLARE_MEMORY_ERROR;

    if (symbol_table_lookup_current(table, name) != NULL)
        return SYMBOL_DECLARE_DUPLICATE;

    scope = table->current;
    if (scope->symbol_count == scope->symbol_capacity && !scope_grow(scope))
        return SYMBOL_DECLARE_MEMORY_ERROR;

    symbol = &scope->symbols[scope->symbol_count];
    memset(symbol, 0, sizeof(*symbol));
    symbol->name = copy_string(name);
    if (symbol->name == NULL) return SYMBOL_DECLARE_MEMORY_ERROR;

    if (parameter_count > 0U) {
        symbol->parameter_types =
            malloc(parameter_count * sizeof(*symbol->parameter_types));
        if (symbol->parameter_types == NULL) {
            free(symbol->name);
            symbol->name = NULL;
            return SYMBOL_DECLARE_MEMORY_ERROR;
        }
        memcpy(symbol->parameter_types, parameter_types,
               parameter_count * sizeof(*symbol->parameter_types));
    }

    symbol->kind = kind;
    symbol->type = type;
    symbol->vector_size = vector_size;
    symbol->parameter_count = parameter_count;
    symbol->line = line;
    symbol->column = column;
    ++scope->symbol_count;

    if (out_symbol != NULL) *out_symbol = symbol;
    return SYMBOL_DECLARE_OK;
}
