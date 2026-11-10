#include "codegen_llvm.h"

/*
 * Primeiro back-end LLVM da Macaronica.
 *
 * O Checkpoint 16 integra a DSL vetorial ao programa escalar:
 * - o backend LLVM emite declaracoes/chamadas para os kernels gerados em MLIR;
 * - memrefs estaticos usam a ABI expandida do MLIR 18
 *   (allocated, aligned, offset, size, stride);
 * - llvm-link resolve as declaracoes contra as definicoes dos kernels.
 *
 * A IR escalar continua sem target triple/datalayout e o lowering vetorial
 * permanece responsabilidade do backend MLIR.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CODEGEN_INITIAL_CAPACITY 16U
#define LLVM_OPERAND_CAPACITY 160U

typedef struct {
    const char *name;
    LangType type;
    int global;
    const AstNode *declaration;
    char llvm_name[LLVM_OPERAND_CAPACITY];
    int active;
    size_t scope_depth;
    int is_vector;
    size_t vector_size;
    const char *record_type_name;
} Storage;

typedef struct {
    const AstNode *node;
    size_t id;
    size_t byte_length;
} StringLiteral;

typedef struct {
    LangType type;
    char operand[LLVM_OPERAND_CAPACITY];
    int valid;
} LlvmValue;

typedef struct {
    LangType type;
    char pointer[LLVM_OPERAND_CAPACITY];
    int valid;
} LlvmAddress;

typedef struct {
    const AstNode **items;
    size_t count;
    size_t capacity;
} KernelArgList;

typedef struct {
    FILE *out;
    FILE *err;
    const AstNode *program;
    size_t error_count;
    int memory_error;
    size_t next_temp;
    size_t next_label;
    size_t scope_depth;
    char current_block[LLVM_OPERAND_CAPACITY];

    Storage *locals;
    size_t local_count;
    size_t local_capacity;

    Storage *globals;
    size_t global_count;
    size_t global_capacity;

    StringLiteral *strings;
    size_t string_count;
    size_t string_capacity;

    LangType current_return_type;
    int block_terminated;
} CodegenContext;

static void codegen_error(CodegenContext *context, const AstNode *node,
                          const char *format, ...) {
    va_list args;

    if (context == NULL) return;
    ++context->error_count;

    if (node != NULL)
        fprintf(context->err,
                "[ERRO CODEGEN LLVM] linha %d, coluna %d: ",
                node->line, node->column);
    else
        fprintf(context->err, "[ERRO CODEGEN LLVM] ");

    va_start(args, format);
    vfprintf(context->err, format, args);
    va_end(args);
    fputc('\n', context->err);
}

static void codegen_memory_error(CodegenContext *context) {
    if (context == NULL || context->memory_error) return;
    context->memory_error = 1;
    fprintf(context->err,
            "[ERRO INTERNO] memoria insuficiente durante a geracao LLVM.\n");
}

static const char *llvm_type(LangType type) {
    switch (type) {
        case LANG_TYPE_BOOL: return "i1";
        case LANG_TYPE_INTEIRA: return "i32";
        case LANG_TYPE_FLUT: return "float";
        case LANG_TYPE_DUPLOCARPADO: return "double";
        case LANG_TYPE_PALAVRA: return "ptr";
        case LANG_TYPE_VAZIO: return "void";
        default: return NULL;
    }
}

static void new_temp(CodegenContext *context,
                     char buffer[LLVM_OPERAND_CAPACITY]);
static void new_label(CodegenContext *context, const char *prefix,
                      char buffer[LLVM_OPERAND_CAPACITY]);
static void emit_label(CodegenContext *context, const char *label);
static void emit_cond_branch(CodegenContext *context, const LlvmValue condition,
                             const char *when_true, const char *when_false);
static LlvmValue make_value(LangType type, const char *operand);

static int parse_integer_literal_magnitude(const AstNode *node,
                                           unsigned long long *out_value) {
    char *end = NULL;
    unsigned long long value;

    if (node == NULL || node->kind != AST_INTEGER_LITERAL ||
        node->lexeme == NULL || node->lexeme[0] == '\0')
        return 0;

    value = strtoull(node->lexeme, &end, 10);
    if (end == node->lexeme || *end != '\0')
        return 0;

    if (out_value != NULL) *out_value = value;
    return 1;
}

static int expression_constant_number(const AstNode *node,
                                      double *out_value) {
    char *end = NULL;
    double value;
    double sign = 1.0;

    if (node == NULL) return 0;

    if (node->kind == AST_UNARY_EXPR && node->child_count == 1U &&
        (node->token_type == TOKEN_PLUS || node->token_type == TOKEN_MINUS)) {
        if (!expression_constant_number(node->children[0], &value))
            return 0;
        if (node->token_type == TOKEN_MINUS) sign = -1.0;
        if (out_value != NULL) *out_value = sign * value;
        return 1;
    }

    if ((node->kind != AST_INTEGER_LITERAL &&
         node->kind != AST_REAL_LITERAL) ||
        node->lexeme == NULL)
        return 0;

    value = strtod(node->lexeme, &end);
    if (end == node->lexeme || *end != '\0') return 0;

    if (out_value != NULL) *out_value = value;
    return 1;
}

static int expression_is_known_zero(const AstNode *node) {
    double value = 0.0;
    return expression_constant_number(node, &value) && value == 0.0;
}

static int expression_is_known_nonzero(const AstNode *node) {
    double value = 0.0;
    return expression_constant_number(node, &value) && value != 0.0;
}

static int expression_is_known_integer(const AstNode *node,
                                       long long *out_value) {
    unsigned long long magnitude = 0ULL;
    long long value;
    int negative = 0;
    const AstNode *literal = node;

    if (node == NULL) return 0;

    if (node->kind == AST_UNARY_EXPR && node->child_count == 1U &&
        (node->token_type == TOKEN_PLUS || node->token_type == TOKEN_MINUS)) {
        negative = node->token_type == TOKEN_MINUS;
        literal = node->children[0];
    }

    if (!parse_integer_literal_magnitude(literal, &magnitude) ||
        magnitude > 2147483648ULL)
        return 0;

    if (negative) {
        if (magnitude == 2147483648ULL)
            value = -2147483647LL - 1LL;
        else
            value = -(long long)magnitude;
    } else {
        if (magnitude > 2147483647ULL) return 0;
        value = (long long)magnitude;
    }

    if (out_value != NULL) *out_value = value;
    return 1;
}

static LlvmValue emit_division_zero_guard(CodegenContext *context,
                                          const AstNode *node,
                                          LlvmValue divisor,
                                          LangType type) {
    char is_zero[LLVM_OPERAND_CAPACITY];
    char ok_label[LLVM_OPERAND_CAPACITY];
    char error_label[LLVM_OPERAND_CAPACITY];

    new_temp(context, is_zero);
    if (type == LANG_TYPE_INTEIRA)
        fprintf(context->out, "  %s = icmp eq i32 %s, 0\n",
                is_zero, divisor.operand);
    else
        fprintf(context->out, "  %s = fcmp oeq %s %s, 0.000000e+00\n",
                is_zero, llvm_type(type), divisor.operand);

    new_label(context, "div.nonzero", ok_label);
    new_label(context, "div.zero", error_label);
    emit_cond_branch(context, make_value(LANG_TYPE_BOOL, is_zero),
                     error_label, ok_label);

    emit_label(context, error_label);
    fprintf(context->out,
            "  call void @zl_erro_execucao(i32 2, i32 %d) nounwind\n",
            node->line);
    fprintf(context->out, "  unreachable\n");
    context->block_terminated = 1;

    emit_label(context, ok_label);
    return divisor;
}

static int emit_sdiv_overflow_guard(CodegenContext *context,
                                    const AstNode *node,
                                    LlvmValue dividend,
                                    LlvmValue divisor) {
    char is_min[LLVM_OPERAND_CAPACITY];
    char is_neg_one[LLVM_OPERAND_CAPACITY];
    char overflow[LLVM_OPERAND_CAPACITY];
    char ok_label[LLVM_OPERAND_CAPACITY];
    char error_label[LLVM_OPERAND_CAPACITY];

    new_temp(context, is_min);
    new_temp(context, is_neg_one);
    new_temp(context, overflow);
    fprintf(context->out,
            "  %s = icmp eq i32 %s, -2147483648\n",
            is_min, dividend.operand);
    fprintf(context->out,
            "  %s = icmp eq i32 %s, -1\n",
            is_neg_one, divisor.operand);
    fprintf(context->out,
            "  %s = and i1 %s, %s\n",
            overflow, is_min, is_neg_one);

    new_label(context, "sdiv.ok", ok_label);
    new_label(context, "sdiv.overflow", error_label);
    emit_cond_branch(context, make_value(LANG_TYPE_BOOL, overflow),
                     error_label, ok_label);

    emit_label(context, error_label);
    fprintf(context->out,
            "  call void @zl_erro_execucao(i32 6, i32 %d) nounwind\n",
            node->line);
    fprintf(context->out, "  unreachable\n");
    context->block_terminated = 1;

    emit_label(context, ok_label);
    return 1;
}

static size_t vector_size_from_declaration(const AstNode *node) {
    char *end = NULL;
    unsigned long long value;

    if (node == NULL || node->kind != AST_VECTOR_DECL ||
        node->child_count == 0U || node->children[0] == NULL ||
        node->children[0]->lexeme == NULL)
        return 0U;

    value = strtoull(node->children[0]->lexeme, &end, 10);
    if (end == node->children[0]->lexeme || *end != '\0')
        return 0U;

    return (size_t)value;
}

static size_t declaration_first_identifier(const AstNode *node) {
    return node != NULL && node->kind == AST_VECTOR_DECL ? 1U : 0U;
}

static const AstNode *find_record_definition(const CodegenContext *context,
                                             const char *name) {
    size_t i;

    if (context == NULL || context->program == NULL || name == NULL)
        return NULL;

    for (i = 0U; i < context->program->child_count; ++i) {
        const AstNode *node = context->program->children[i];
        if (node != NULL && node->kind == AST_RECORD &&
            node->lexeme != NULL && strcmp(node->lexeme, name) == 0)
            return node;
    }

    return NULL;
}

static int record_field_info(const CodegenContext *context,
                             const char *record_name,
                             const char *field_name,
                             size_t *out_index,
                             LangType *out_type,
                             size_t *out_vector_size) {
    const AstNode *record;
    size_t field_index = 0U;
    size_t i;

    record = find_record_definition(context, record_name);
    if (record == NULL || field_name == NULL) return 0;

    for (i = 0U; i < record->child_count; ++i) {
        const AstNode *declaration = record->children[i];
        LangType type;
        size_t first;
        size_t j;
        size_t vector_size = 0U;

        if (declaration == NULL ||
            (declaration->kind != AST_VAR_DECL &&
             declaration->kind != AST_VECTOR_DECL))
            continue;

        type = lang_type_from_token(declaration->token_type);
        first = declaration_first_identifier(declaration);
        if (declaration->kind == AST_VECTOR_DECL)
            vector_size = vector_size_from_declaration(declaration);

        for (j = first; j < declaration->child_count; ++j) {
            const AstNode *identifier = declaration->children[j];
            if (identifier == NULL || identifier->kind != AST_IDENTIFIER)
                continue;

            if (identifier->lexeme != NULL &&
                strcmp(identifier->lexeme, field_name) == 0) {
                if (out_index != NULL) *out_index = field_index;
                if (out_type != NULL) *out_type = type;
                if (out_vector_size != NULL) *out_vector_size = vector_size;
                return 1;
            }
            ++field_index;
        }
    }

    return 0;
}

static int emit_record_type(CodegenContext *context,
                            const AstNode *record) {
    size_t i;
    int first_field = 1;

    if (record == NULL || record->kind != AST_RECORD ||
        record->lexeme == NULL)
        return 0;

    fprintf(context->out, "%%struct.%s = type { ", record->lexeme);

    for (i = 0U; i < record->child_count; ++i) {
        const AstNode *declaration = record->children[i];
        LangType type;
        const char *type_name;
        size_t first;
        size_t j;
        size_t vector_size = 0U;

        if (declaration == NULL ||
            (declaration->kind != AST_VAR_DECL &&
             declaration->kind != AST_VECTOR_DECL))
            continue;

        type = lang_type_from_token(declaration->token_type);
        type_name = llvm_type(type);
        if (type_name == NULL) {
            codegen_error(context, declaration,
                          "campo de registro possui tipo LLVM nao suportado");
            return 0;
        }

        first = declaration_first_identifier(declaration);
        if (declaration->kind == AST_VECTOR_DECL)
            vector_size = vector_size_from_declaration(declaration);

        for (j = first; j < declaration->child_count; ++j) {
            const AstNode *identifier = declaration->children[j];
            if (identifier == NULL || identifier->kind != AST_IDENTIFIER)
                continue;

            if (!first_field) fprintf(context->out, ", ");
            if (declaration->kind == AST_VECTOR_DECL)
                fprintf(context->out, "[%zu x %s]", vector_size, type_name);
            else
                fprintf(context->out, "%s", type_name);
            first_field = 0;
        }
    }

    fprintf(context->out, " }\n");
    return 1;
}

static int emit_record_types(CodegenContext *context,
                             const AstNode *program) {
    size_t i;
    int emitted = 0;

    for (i = 0U; i < program->child_count; ++i) {
        const AstNode *node = program->children[i];
        if (node != NULL && node->kind == AST_RECORD) {
            if (!emit_record_type(context, node)) return 0;
            emitted = 1;
        }
    }

    if (emitted) fputc('\n', context->out);
    return 1;
}

static int grow_storage(Storage **items, size_t *capacity, size_t count) {
    size_t new_capacity;
    Storage *grown;

    if (count < *capacity) return 1;
    new_capacity = *capacity == 0U ? CODEGEN_INITIAL_CAPACITY
                                   : *capacity * 2U;
    grown = realloc(*items, new_capacity * sizeof(*grown));
    if (grown == NULL) return 0;
    *items = grown;
    *capacity = new_capacity;
    return 1;
}

static int add_storage(CodegenContext *context, int global,
                       const char *name, LangType type,
                       const AstNode *declaration) {
    Storage **items = global ? &context->globals : &context->locals;
    size_t *count = global ? &context->global_count : &context->local_count;
    size_t *capacity =
        global ? &context->global_capacity : &context->local_capacity;
    Storage *storage;
    size_t same_name_count = 0U;
    size_t i;

    if (!grow_storage(items, capacity, *count)) {
        codegen_memory_error(context);
        return 0;
    }

    if (!global) {
        for (i = 0U; i < *count; ++i)
            if (strcmp((*items)[i].name, name) == 0)
                ++same_name_count;
    }

    storage = &(*items)[*count];
    memset(storage, 0, sizeof(*storage));
    storage->name = name;
    storage->type = type;
    storage->global = global;
    storage->declaration = declaration;
    storage->active = global;

    if (global)
        snprintf(storage->llvm_name, sizeof(storage->llvm_name), "%s", name);
    else if (same_name_count == 0U)
        snprintf(storage->llvm_name, sizeof(storage->llvm_name),
                 "%s.addr", name);
    else
        snprintf(storage->llvm_name, sizeof(storage->llvm_name),
                 "%s.%zu.addr", name, same_name_count);

    ++(*count);
    return 1;
}

static Storage *find_storage_by_declaration(CodegenContext *context,
                                            const AstNode *declaration) {
    size_t i;
    if (context == NULL || declaration == NULL) return NULL;

    for (i = 0U; i < context->local_count; ++i)
        if (context->locals[i].declaration == declaration)
            return &context->locals[i];

    return NULL;
}

static const Storage *lookup_storage(const CodegenContext *context,
                                     const char *name) {
    size_t i;

    if (context == NULL || name == NULL) return NULL;

    for (i = context->local_count; i > 0U; --i) {
        const Storage *storage = &context->locals[i - 1U];
        if (storage->active && strcmp(storage->name, name) == 0)
            return storage;
    }

    for (i = 0U; i < context->global_count; ++i) {
        if (strcmp(context->globals[i].name, name) == 0)
            return &context->globals[i];
    }

    return NULL;
}

static void storage_pointer(const Storage *storage,
                            char buffer[LLVM_OPERAND_CAPACITY]) {
    size_t length = strlen(storage->llvm_name);
    if (length > LLVM_OPERAND_CAPACITY - 2U)
        length = LLVM_OPERAND_CAPACITY - 2U;

    buffer[0] = storage->global ? '@' : '%';
    memcpy(buffer + 1U, storage->llvm_name, length);
    buffer[length + 1U] = '\0';
}

static int grow_strings(CodegenContext *context) {
    size_t new_capacity;
    StringLiteral *grown;

    if (context->string_count < context->string_capacity) return 1;

    new_capacity = context->string_capacity == 0U
                       ? CODEGEN_INITIAL_CAPACITY
                       : context->string_capacity * 2U;
    grown = realloc(context->strings, new_capacity * sizeof(*grown));
    if (grown == NULL) {
        codegen_memory_error(context);
        return 0;
    }

    context->strings = grown;
    context->string_capacity = new_capacity;
    return 1;
}

static void collect_strings(CodegenContext *context, const AstNode *node) {
    size_t i;

    if (node == NULL || context->memory_error) return;

    if (node->kind == AST_STRING_LITERAL && node->lexeme != NULL) {
        size_t length = strlen(node->lexeme);
        if (!grow_strings(context)) return;

        context->strings[context->string_count].node = node;
        context->strings[context->string_count].id = context->string_count;
        context->strings[context->string_count].byte_length =
            length >= 2U ? length - 2U : 0U;
        ++context->string_count;
    }

    for (i = 0U; i < node->child_count; ++i)
        collect_strings(context, node->children[i]);
}

static const StringLiteral *find_string(const CodegenContext *context,
                                        const AstNode *node) {
    size_t i;

    for (i = 0U; i < context->string_count; ++i)
        if (context->strings[i].node == node) return &context->strings[i];

    return NULL;
}

static void emit_escaped_string(FILE *out, const char *lexeme) {
    size_t i;
    size_t length;

    if (lexeme == NULL) return;
    length = strlen(lexeme);

    if (length < 2U) return;

    for (i = 1U; i + 1U < length; ++i) {
        unsigned char c = (unsigned char)lexeme[i];

        if (c >= 32U && c <= 126U && c != '"' && c != '\\')
            fputc((int)c, out);
        else
            fprintf(out, "\\%02X", (unsigned int)c);
    }
}

static void emit_string_globals(CodegenContext *context) {
    size_t i;

    for (i = 0U; i < context->string_count; ++i) {
        const StringLiteral *literal = &context->strings[i];
        fprintf(context->out,
                "@.str.%zu = private unnamed_addr constant [%zu x i8] c\"",
                literal->id, literal->byte_length + 1U);
        emit_escaped_string(context->out, literal->node->lexeme);
        fprintf(context->out, "\\00\"\n");
    }

    if (context->string_count > 0U) fputc('\n', context->out);
}

static void emit_runtime_declarations(CodegenContext *context) {
    fprintf(context->out,
            "declare void @zl_imprime_int(i32) nounwind\n"
            "declare void @zl_imprime_bool(i32) nounwind\n"
            "declare void @zl_imprime_flut(float) nounwind\n"
            "declare void @zl_imprime_duplo(double) nounwind\n"
            "declare void @zl_imprime_texto(ptr) nounwind\n"
            "declare void @zl_imprime_nl() nounwind\n"
            "declare i32 @zl_le_int() nounwind\n"
            "declare void @zl_erro_execucao(i32, i32) nounwind\n"
            "declare i32 @zl_pow_i32(i32, i32, i32) nounwind\n"
            "declare float @zl_pow_flut(float, float) nounwind\n"
            "declare double @zl_pow_duplo(double, double) nounwind\n"
            "declare float @zl_raiz_flut(float, i32) nounwind\n"
            "declare double @zl_raiz_duplo(double, i32) nounwind\n\n");
}

static void new_temp(CodegenContext *context,
                     char buffer[LLVM_OPERAND_CAPACITY]) {
    snprintf(buffer, LLVM_OPERAND_CAPACITY, "%%t%zu", context->next_temp++);
}

static void new_label(CodegenContext *context, const char *prefix,
                      char buffer[LLVM_OPERAND_CAPACITY]) {
    snprintf(buffer, LLVM_OPERAND_CAPACITY, "%s.%zu",
             prefix, context->next_label++);
}

static void emit_label(CodegenContext *context, const char *label) {
    fprintf(context->out, "%s:\n", label);
    snprintf(context->current_block, sizeof(context->current_block),
             "%s", label);
    context->block_terminated = 0;
}

static void emit_branch(CodegenContext *context, const char *label) {
    fprintf(context->out, "  br label %%%s\n", label);
    context->block_terminated = 1;
}

static void emit_cond_branch(CodegenContext *context, const LlvmValue condition,
                             const char *when_true, const char *when_false) {
    fprintf(context->out,
            "  br i1 %s, label %%%s, label %%%s\n",
            condition.operand, when_true, when_false);
    context->block_terminated = 1;
}

static LlvmValue invalid_value(void) {
    LlvmValue value;
    memset(&value, 0, sizeof(value));
    value.type = LANG_TYPE_ERROR;
    value.valid = 0;
    return value;
}

static LlvmValue make_value(LangType type, const char *operand) {
    LlvmValue value;
    memset(&value, 0, sizeof(value));
    value.type = type;
    value.valid = 1;
    snprintf(value.operand, sizeof(value.operand), "%s", operand);
    return value;
}

static LlvmAddress invalid_address(void) {
    LlvmAddress address;
    memset(&address, 0, sizeof(address));
    address.type = LANG_TYPE_ERROR;
    address.valid = 0;
    return address;
}

static LlvmAddress make_address(LangType type, const char *pointer) {
    LlvmAddress address;
    memset(&address, 0, sizeof(address));
    address.type = type;
    address.valid = 1;
    snprintf(address.pointer, sizeof(address.pointer), "%s", pointer);
    return address;
}

static const AstNode *find_callable(const CodegenContext *context,
                                    const char *name) {
    size_t i;

    if (context == NULL || context->program == NULL || name == NULL)
        return NULL;

    for (i = 0U; i < context->program->child_count; ++i) {
        const AstNode *node = context->program->children[i];
        if (node == NULL ||
            (node->kind != AST_FUNCTION && node->kind != AST_PRINCIPAL) ||
            node->lexeme == NULL)
            continue;

        if (strcmp(node->lexeme, name) == 0)
            return node;
    }

    return NULL;
}

static const AstNode *function_parameter_at(const AstNode *function,
                                            size_t index) {
    size_t i;
    size_t current = 0U;

    if (function == NULL) return NULL;

    for (i = 0U; i < function->child_count; ++i) {
        const AstNode *child = function->children[i];
        if (child == NULL || child->kind != AST_PARAMETER)
            continue;

        if (current == index) return child;
        ++current;
    }

    return NULL;
}

static const char *llvm_callable_name(const AstNode *function) {
    if (function != NULL && function->kind == AST_PRINCIPAL)
        return "zl_principal";
    return function != NULL ? function->lexeme : NULL;
}

static LlvmValue convert_value(CodegenContext *context, const AstNode *node,
                               LlvmValue value, LangType target) {
    char temp[LLVM_OPERAND_CAPACITY];

    if (!value.valid || value.type == target) return value;

    if (value.type == LANG_TYPE_INTEIRA && target == LANG_TYPE_FLUT) {
        new_temp(context, temp);
        fprintf(context->out, "  %s = sitofp i32 %s to float\n",
                temp, value.operand);
        return make_value(LANG_TYPE_FLUT, temp);
    }

    if (value.type == LANG_TYPE_INTEIRA &&
        target == LANG_TYPE_DUPLOCARPADO) {
        new_temp(context, temp);
        fprintf(context->out, "  %s = sitofp i32 %s to double\n",
                temp, value.operand);
        return make_value(LANG_TYPE_DUPLOCARPADO, temp);
    }

    if (value.type == LANG_TYPE_FLUT &&
        target == LANG_TYPE_DUPLOCARPADO) {
        new_temp(context, temp);
        fprintf(context->out, "  %s = fpext float %s to double\n",
                temp, value.operand);
        return make_value(LANG_TYPE_DUPLOCARPADO, temp);
    }

    codegen_error(context, node,
                  "conversao LLVM ainda nao suportada de %s para %s",
                  lang_type_name(value.type), lang_type_name(target));
    return invalid_value();
}

static LlvmValue generate_expression(CodegenContext *context,
                                     const AstNode *node);
static LlvmValue generate_user_call(CodegenContext *context,
                                    const AstNode *node);

static LlvmValue generate_short_circuit(CodegenContext *context,
                                        const AstNode *node) {
    LlvmValue left;
    LlvmValue right;
    char rhs_label[LLVM_OPERAND_CAPACITY];
    char end_label[LLVM_OPERAND_CAPACITY];
    char left_predecessor[LLVM_OPERAND_CAPACITY];
    char right_predecessor[LLVM_OPERAND_CAPACITY];
    char result[LLVM_OPERAND_CAPACITY];
    const int is_and = node->token_type == TOKEN_E;

    left = generate_expression(context, node->children[0]);
    if (!left.valid || left.type != LANG_TYPE_BOOL) {
        codegen_error(context, node,
                      "curto-circuito exige operando esquerdo bool");
        return invalid_value();
    }

    snprintf(left_predecessor, sizeof(left_predecessor), "%s",
             context->current_block);
    new_label(context, is_and ? "and.rhs" : "or.rhs", rhs_label);
    new_label(context, is_and ? "and.end" : "or.end", end_label);

    if (is_and)
        emit_cond_branch(context, left, rhs_label, end_label);
    else
        emit_cond_branch(context, left, end_label, rhs_label);

    emit_label(context, rhs_label);
    right = generate_expression(context, node->children[1]);
    if (!right.valid || right.type != LANG_TYPE_BOOL) {
        codegen_error(context, node,
                      "curto-circuito exige operando direito bool");
        return invalid_value();
    }

    snprintf(right_predecessor, sizeof(right_predecessor), "%s",
             context->current_block);
    emit_branch(context, end_label);

    emit_label(context, end_label);
    new_temp(context, result);
    fprintf(context->out,
            "  %s = phi i1 [ %s, %%%s ], [ %s, %%%s ]\n",
            result,
            is_and ? "false" : "true", left_predecessor,
            right.operand, right_predecessor);

    return make_value(LANG_TYPE_BOOL, result);
}

static LlvmAddress generate_vector_address(CodegenContext *context,
                                                const AstNode *node) {
    const Storage *storage;
    LlvmValue index;
    const char *element_type;
    char base_pointer[LLVM_OPERAND_CAPACITY];
    char lower[LLVM_OPERAND_CAPACITY];
    char upper[LLVM_OPERAND_CAPACITY];
    char in_bounds[LLVM_OPERAND_CAPACITY];
    char ok_label[LLVM_OPERAND_CAPACITY];
    char error_label[LLVM_OPERAND_CAPACITY];
    char element_pointer[LLVM_OPERAND_CAPACITY];

    if (node == NULL || node->child_count == 0U)
        return invalid_address();

    storage = lookup_storage(context, node->lexeme);
    if (storage == NULL || !storage->is_vector) {
        codegen_error(context, node,
                      "'%s' nao possui armazenamento vetorial LLVM",
                      node->lexeme != NULL ? node->lexeme : "<sem-nome>");
        return invalid_address();
    }

    index = generate_expression(context, node->children[0]);
    if (!index.valid || index.type != LANG_TYPE_INTEIRA) {
        codegen_error(context, node,
                      "indice vetorial LLVM deve produzir i32");
        return invalid_address();
    }

    new_temp(context, lower);
    new_temp(context, upper);
    new_temp(context, in_bounds);
    fprintf(context->out, "  %s = icmp sge i32 %s, 0\n",
            lower, index.operand);
    fprintf(context->out, "  %s = icmp slt i32 %s, %zu\n",
            upper, index.operand, storage->vector_size);
    fprintf(context->out, "  %s = and i1 %s, %s\n",
            in_bounds, lower, upper);

    new_label(context, "bounds.ok", ok_label);
    new_label(context, "bounds.error", error_label);
    emit_cond_branch(context, make_value(LANG_TYPE_BOOL, in_bounds),
                     ok_label, error_label);

    emit_label(context, error_label);
    fprintf(context->out,
            "  call void @zl_erro_execucao(i32 1, i32 %d) nounwind\n",
            node->line);
    fprintf(context->out, "  unreachable\n");
    context->block_terminated = 1;

    emit_label(context, ok_label);
    storage_pointer(storage, base_pointer);
    element_type = llvm_type(storage->type);
    if (element_type == NULL) {
        codegen_error(context, node,
                      "elemento vetorial do tipo %s nao possui representacao LLVM",
                      lang_type_name(storage->type));
        return invalid_address();
    }

    new_temp(context, element_pointer);
    fprintf(context->out,
            "  %s = getelementptr [%zu x %s], ptr %s, i32 0, i32 %s\n",
            element_pointer, storage->vector_size, element_type,
            base_pointer, index.operand);

    return make_address(storage->type, element_pointer);
}

static LlvmAddress generate_field_address(CodegenContext *context,
                                           const AstNode *node) {
    const Storage *storage;
    const AstNode *field;
    size_t field_index = 0U;
    size_t field_vector_size = 0U;
    LangType field_type = LANG_TYPE_ERROR;
    char base_pointer[LLVM_OPERAND_CAPACITY];
    char field_pointer[LLVM_OPERAND_CAPACITY];

    if (node == NULL || node->child_count == 0U ||
        node->children[0] == NULL)
        return invalid_address();

    storage = lookup_storage(context, node->lexeme);
    if (storage == NULL || storage->type != LANG_TYPE_RECORD ||
        storage->record_type_name == NULL) {
        codegen_error(context, node,
                      "'%s' nao possui armazenamento de registro LLVM",
                      node->lexeme != NULL ? node->lexeme : "<sem-nome>");
        return invalid_address();
    }

    field = node->children[0];
    if (!record_field_info(context, storage->record_type_name,
                           field->lexeme, &field_index, &field_type,
                           &field_vector_size)) {
        codegen_error(context, field,
                      "campo '%s.%s' nao encontrado no layout LLVM",
                      storage->name,
                      field->lexeme != NULL ? field->lexeme : "<sem-nome>");
        return invalid_address();
    }

    if (field_vector_size != 0U) {
        codegen_error(context, field,
                      "campo vetorial de registro ainda nao possui sintaxe de acesso na linguagem");
        return invalid_address();
    }

    storage_pointer(storage, base_pointer);
    new_temp(context, field_pointer);
    fprintf(context->out,
            "  %s = getelementptr %%struct.%s, ptr %s, i32 0, i32 %zu\n",
            field_pointer, storage->record_type_name,
            base_pointer, field_index);

    return make_address(field_type, field_pointer);
}

static LlvmAddress generate_lvalue_address(CodegenContext *context,
                                            const AstNode *node) {
    const Storage *storage;
    char pointer[LLVM_OPERAND_CAPACITY];

    if (node == NULL) return invalid_address();

    if (node->kind == AST_VECTOR_ACCESS)
        return generate_vector_address(context, node);
    if (node->kind == AST_FIELD_ACCESS)
        return generate_field_address(context, node);

    if (node->kind != AST_IDENTIFIER) {
        codegen_error(context, node,
                      "no %s nao e um lvalue LLVM",
                      ast_node_kind_name(node->kind));
        return invalid_address();
    }

    storage = lookup_storage(context, node->lexeme);
    if (storage == NULL) {
        codegen_error(context, node,
                      "armazenamento LLVM de '%s' nao encontrado",
                      node->lexeme != NULL ? node->lexeme : "<sem-nome>");
        return invalid_address();
    }

    if (storage->is_vector || storage->type == LANG_TYPE_RECORD) {
        codegen_error(context, node,
                      "'%s' exige acesso a elemento/campo",
                      storage->name);
        return invalid_address();
    }

    storage_pointer(storage, pointer);
    return make_address(storage->type, pointer);
}

static LlvmValue load_address(CodegenContext *context,
                              const AstNode *node,
                              LlvmAddress address) {
    const char *type_name;
    char temp[LLVM_OPERAND_CAPACITY];

    if (!address.valid) return invalid_value();

    type_name = llvm_type(address.type);
    if (type_name == NULL) {
        codegen_error(context, node,
                      "tipo %s nao pode ser carregado como valor escalar",
                      lang_type_name(address.type));
        return invalid_value();
    }

    new_temp(context, temp);
    fprintf(context->out, "  %s = load %s, ptr %s\n",
            temp, type_name, address.pointer);
    return make_value(address.type, temp);
}

static LlvmValue generate_identifier(CodegenContext *context,
                                     const AstNode *node) {
    const Storage *storage;
    const char *type_name;
    char pointer[LLVM_OPERAND_CAPACITY];
    char temp[LLVM_OPERAND_CAPACITY];

    storage = lookup_storage(context, node->lexeme);
    if (storage == NULL) {
        codegen_error(context, node,
                      "armazenamento LLVM de '%s' nao foi encontrado",
                      node->lexeme != NULL ? node->lexeme : "<sem-nome>");
        return invalid_value();
    }

    if (storage->is_vector || storage->type == LANG_TYPE_RECORD) {
        codegen_error(context, node,
                      "'%s' exige acesso a elemento/campo",
                      storage->name);
        return invalid_value();
    }

    type_name = llvm_type(storage->type);
    if (type_name == NULL) {
        codegen_error(context, node,
                      "tipo %s ainda nao possui representacao escalar neste checkpoint",
                      lang_type_name(storage->type));
        return invalid_value();
    }

    storage_pointer(storage, pointer);
    new_temp(context, temp);
    fprintf(context->out, "  %s = load %s, ptr %s\n",
            temp, type_name, pointer);
    return make_value(storage->type, temp);
}

static LlvmValue generate_unary(CodegenContext *context,
                                const AstNode *node) {
    LlvmValue operand;
    const char *type_name;
    char temp[LLVM_OPERAND_CAPACITY];

    if (node->child_count == 0U) return invalid_value();

    /*
     * 2147483648 nao cabe em i32 positivo, mas -2147483648 e o limite
     * inferior valido. Reconhecemos essa forma antes de gerar o filho.
     */
    if (node->token_type == TOKEN_MINUS &&
        node->children[0] != NULL &&
        node->children[0]->kind == AST_INTEGER_LITERAL) {
        unsigned long long magnitude = 0ULL;
        if (parse_integer_literal_magnitude(node->children[0], &magnitude) &&
            magnitude == 2147483648ULL)
            return make_value(LANG_TYPE_INTEIRA, "-2147483648");
    }

    operand = generate_expression(context, node->children[0]);
    if (!operand.valid) return operand;

    if (node->token_type == TOKEN_PLUS) return operand;

    if (node->token_type == TOKEN_NAO) {
        new_temp(context, temp);
        fprintf(context->out, "  %s = xor i1 %s, true\n",
                temp, operand.operand);
        return make_value(LANG_TYPE_BOOL, temp);
    }

    if (node->token_type != TOKEN_MINUS) {
        codegen_error(context, node,
                      "operador unario '%s' ainda nao suportado pelo LLVM",
                      node->lexeme != NULL ? node->lexeme : "<desconhecido>");
        return invalid_value();
    }

    type_name = llvm_type(operand.type);
    if (operand.type == LANG_TYPE_INTEIRA) {
        new_temp(context, temp);
        fprintf(context->out, "  %s = sub i32 0, %s\n",
                temp, operand.operand);
        return make_value(operand.type, temp);
    }

    if (operand.type == LANG_TYPE_FLUT ||
        operand.type == LANG_TYPE_DUPLOCARPADO) {
        new_temp(context, temp);
        fprintf(context->out, "  %s = fneg %s %s\n",
                temp, type_name, operand.operand);
        return make_value(operand.type, temp);
    }

    codegen_error(context, node,
                  "negacao numerica nao suportada para %s",
                  lang_type_name(operand.type));
    return invalid_value();
}

static LlvmValue generate_binary(CodegenContext *context,
                                 const AstNode *node) {
    LlvmValue left;
    LlvmValue right;
    LangType operation_type;
    const char *type_name;
    const char *instruction = NULL;
    int divisor_known_nonzero = 0;
    int sdiv_overflow_impossible = 0;
    char temp[LLVM_OPERAND_CAPACITY];

    if (node->child_count < 2U) return invalid_value();

    if (node->inferred_is_vector) {
        codegen_error(context, node,
                      "operacao vetorial DSL requer lowering via MLIR (PjBL 4 Parte B)");
        return invalid_value();
    }

    if (node->token_type == TOKEN_E || node->token_type == TOKEN_OU)
        return generate_short_circuit(context, node);

    if ((node->token_type == TOKEN_INTEGER_DIV ||
         node->token_type == TOKEN_SLASH) &&
        expression_is_known_zero(node->children[1])) {
        codegen_error(context, node,
                      "divisao por zero comprovada estaticamente");
        return invalid_value();
    }

    if (node->token_type == TOKEN_INTEGER_DIV ||
        node->token_type == TOKEN_SLASH)
        divisor_known_nonzero =
            expression_is_known_nonzero(node->children[1]);

    if (node->token_type == TOKEN_INTEGER_DIV) {
        long long left_constant = 0LL;
        long long right_constant = 0LL;
        int left_known =
            expression_is_known_integer(node->children[0], &left_constant);
        int right_known =
            expression_is_known_integer(node->children[1], &right_constant);

        if ((right_known && right_constant != -1LL) ||
            (left_known && left_constant != (-2147483647LL - 1LL)))
            sdiv_overflow_impossible = 1;
    }

    left = generate_expression(context, node->children[0]);
    right = generate_expression(context, node->children[1]);
    if (!left.valid || !right.valid) return invalid_value();

    if (node->token_type == TOKEN_CARET) {
        operation_type = node->inferred_type;
        left = convert_value(context, node, left, operation_type);
        right = convert_value(context, node, right, operation_type);
        if (!left.valid || !right.valid) return invalid_value();

        new_temp(context, temp);
        if (operation_type == LANG_TYPE_INTEIRA) {
            fprintf(context->out,
                    "  %s = call i32 @zl_pow_i32(i32 %s, i32 %s, i32 %d) nounwind\n",
                    temp, left.operand, right.operand, node->line);
        } else if (operation_type == LANG_TYPE_FLUT) {
            fprintf(context->out,
                    "  %s = call float @zl_pow_flut(float %s, float %s) nounwind\n",
                    temp, left.operand, right.operand);
        } else if (operation_type == LANG_TYPE_DUPLOCARPADO) {
            fprintf(context->out,
                    "  %s = call double @zl_pow_duplo(double %s, double %s) nounwind\n",
                    temp, left.operand, right.operand);
        } else {
            codegen_error(context, node,
                          "potencia nao suportada para tipo %s",
                          lang_type_name(operation_type));
            return invalid_value();
        }

        return make_value(operation_type, temp);
    }

    if (node->token_type == TOKEN_XOR) {
        new_temp(context, temp);
        fprintf(context->out, "  %s = xor i1 %s, %s\n",
                temp, left.operand, right.operand);
        return make_value(LANG_TYPE_BOOL, temp);
    }

    if (node->token_type == TOKEN_EQUAL_EQUAL ||
        node->token_type == TOKEN_BANG ||
        node->token_type == TOKEN_LESS ||
        node->token_type == TOKEN_GREATER) {
        if (lang_type_is_numeric(left.type) &&
            lang_type_is_numeric(right.type)) {
            operation_type = lang_type_common_numeric(left.type, right.type);
            left = convert_value(context, node, left, operation_type);
            right = convert_value(context, node, right, operation_type);
            if (!left.valid || !right.valid) return invalid_value();

            new_temp(context, temp);
            if (operation_type == LANG_TYPE_INTEIRA) {
                if (node->token_type == TOKEN_EQUAL_EQUAL)
                    instruction = "icmp eq";
                else if (node->token_type == TOKEN_BANG)
                    instruction = "icmp ne";
                else if (node->token_type == TOKEN_LESS)
                    instruction = "icmp slt";
                else
                    instruction = "icmp sgt";

                fprintf(context->out, "  %s = %s i32 %s, %s\n",
                        temp, instruction, left.operand, right.operand);
            } else {
                if (node->token_type == TOKEN_EQUAL_EQUAL)
                    instruction = "fcmp oeq";
                else if (node->token_type == TOKEN_BANG)
                    instruction = "fcmp one";
                else if (node->token_type == TOKEN_LESS)
                    instruction = "fcmp olt";
                else
                    instruction = "fcmp ogt";

                fprintf(context->out, "  %s = %s %s %s, %s\n",
                        temp, instruction, llvm_type(operation_type),
                        left.operand, right.operand);
            }
            return make_value(LANG_TYPE_BOOL, temp);
        }

        if (left.type == LANG_TYPE_BOOL && right.type == LANG_TYPE_BOOL &&
            (node->token_type == TOKEN_EQUAL_EQUAL ||
             node->token_type == TOKEN_BANG)) {
            new_temp(context, temp);
            fprintf(context->out, "  %s = icmp %s i1 %s, %s\n",
                    temp,
                    node->token_type == TOKEN_EQUAL_EQUAL ? "eq" : "ne",
                    left.operand, right.operand);
            return make_value(LANG_TYPE_BOOL, temp);
        }

        codegen_error(context, node,
                      "comparacao LLVM ainda nao suportada para %s e %s",
                      lang_type_name(left.type), lang_type_name(right.type));
        return invalid_value();
    }

    operation_type = node->inferred_type;
    type_name = llvm_type(operation_type);
    if (type_name == NULL) {
        codegen_error(context, node,
                      "tipo resultante %s nao e escalar LLVM neste checkpoint",
                      lang_type_name(operation_type));
        return invalid_value();
    }

    left = convert_value(context, node, left, operation_type);
    right = convert_value(context, node, right, operation_type);
    if (!left.valid || !right.valid) return invalid_value();

    if ((node->token_type == TOKEN_INTEGER_DIV ||
         node->token_type == TOKEN_SLASH) &&
        !divisor_known_nonzero) {
        right = emit_division_zero_guard(context, node, right, operation_type);
        if (!right.valid) return invalid_value();
    }

    if (node->token_type == TOKEN_INTEGER_DIV &&
        !sdiv_overflow_impossible &&
        !emit_sdiv_overflow_guard(context, node, left, right))
        return invalid_value();

    if (operation_type == LANG_TYPE_INTEIRA) {
        if (node->token_type == TOKEN_PLUS)
            instruction = "add";
        else if (node->token_type == TOKEN_MINUS)
            instruction = "sub";
        else if (node->token_type == TOKEN_STAR)
            instruction = "mul";
        else if (node->token_type == TOKEN_INTEGER_DIV)
            instruction = "sdiv";
    } else if (operation_type == LANG_TYPE_FLUT ||
               operation_type == LANG_TYPE_DUPLOCARPADO) {
        if (node->token_type == TOKEN_PLUS)
            instruction = "fadd";
        else if (node->token_type == TOKEN_MINUS)
            instruction = "fsub";
        else if (node->token_type == TOKEN_STAR)
            instruction = "fmul";
        else if (node->token_type == TOKEN_SLASH)
            instruction = "fdiv";
    }

    if (instruction == NULL) {
        codegen_error(context, node,
                      "operador '%s' ainda nao possui lowering LLVM neste checkpoint",
                      node->lexeme != NULL ? node->lexeme : "<desconhecido>");
        return invalid_value();
    }

    new_temp(context, temp);
    fprintf(context->out, "  %s = %s %s %s, %s\n",
            temp, instruction, type_name, left.operand, right.operand);
    return make_value(operation_type, temp);
}

static LlvmValue generate_root_expression(CodegenContext *context,
                                               const AstNode *node) {
    LlvmValue argument;
    LangType result_type;
    char temp[LLVM_OPERAND_CAPACITY];

    if (node == NULL || node->child_count == 0U)
        return invalid_value();

    argument = generate_expression(context, node->children[0]);
    if (!argument.valid) return argument;

    result_type = node->inferred_type;
    argument = convert_value(context, node, argument, result_type);
    if (!argument.valid) return invalid_value();

    new_temp(context, temp);
    if (result_type == LANG_TYPE_FLUT) {
        fprintf(context->out,
                "  %s = call float @zl_raiz_flut(float %s, i32 %d) nounwind\n",
                temp, argument.operand, node->line);
    } else if (result_type == LANG_TYPE_DUPLOCARPADO) {
        fprintf(context->out,
                "  %s = call double @zl_raiz_duplo(double %s, i32 %d) nounwind\n",
                temp, argument.operand, node->line);
    } else {
        codegen_error(context, node,
                      "raiz() nao suportada para tipo %s",
                      lang_type_name(result_type));
        return invalid_value();
    }

    return make_value(result_type, temp);
}

static LlvmValue generate_user_call(CodegenContext *context,
                                    const AstNode *node) {
    const AstNode *function;
    const char *function_name;
    LangType return_type;
    LlvmValue *arguments = NULL;
    size_t count;
    size_t i;
    char temp[LLVM_OPERAND_CAPACITY];

    if (node == NULL || node->lexeme == NULL)
        return invalid_value();

    function = find_callable(context, node->lexeme);
    if (function == NULL) {
        codegen_error(context, node,
                      "assinatura LLVM da funcao '%s' nao foi encontrada",
                      node->lexeme);
        return invalid_value();
    }

    count = node->child_count;
    if (count > 0U) {
        arguments = calloc(count, sizeof(*arguments));
        if (arguments == NULL) {
            codegen_memory_error(context);
            return invalid_value();
        }
    }

    for (i = 0U; i < count; ++i) {
        const AstNode *parameter = function_parameter_at(function, i);
        LangType target_type;

        if (parameter == NULL) {
            free(arguments);
            codegen_error(context, node,
                          "assinatura de '%s' possui menos parametros que a chamada",
                          node->lexeme);
            return invalid_value();
        }

        target_type = lang_type_from_token(parameter->token_type);
        arguments[i] = generate_expression(context, node->children[i]);
        arguments[i] =
            convert_value(context, node->children[i], arguments[i], target_type);

        if (!arguments[i].valid) {
            free(arguments);
            return invalid_value();
        }
    }

    function_name = llvm_callable_name(function);
    return_type = lang_type_from_token(function->token_type);

    if (return_type != LANG_TYPE_VAZIO) {
        new_temp(context, temp);
        fprintf(context->out, "  %s = call %s @%s(",
                temp, llvm_type(return_type), function_name);
    } else {
        fprintf(context->out, "  call void @%s(", function_name);
    }

    for (i = 0U; i < count; ++i) {
        const AstNode *parameter = function_parameter_at(function, i);
        LangType parameter_type = lang_type_from_token(parameter->token_type);

        if (i > 0U) fprintf(context->out, ", ");
        fprintf(context->out, "%s %s",
                llvm_type(parameter_type), arguments[i].operand);
    }

    fprintf(context->out, ") nounwind ; linha %d\n", node->line);
    free(arguments);

    if (return_type == LANG_TYPE_VAZIO)
        return make_value(LANG_TYPE_VAZIO, "");

    return make_value(return_type, temp);
}

static LlvmValue generate_expression(CodegenContext *context,
                                     const AstNode *node) {
    char literal[LLVM_OPERAND_CAPACITY];
    char *end = NULL;
    double number;

    if (node == NULL) return invalid_value();

    switch (node->kind) {
        case AST_INTEGER_LITERAL: {
            unsigned long long magnitude = 0ULL;

            if (!parse_integer_literal_magnitude(node, &magnitude) ||
                magnitude > 2147483647ULL) {
                codegen_error(
                    context, node,
                    "literal inteira '%s' nao cabe em i32 (-2147483648..2147483647)",
                    node->lexeme != NULL ? node->lexeme : "<invalido>");
                return invalid_value();
            }

            return make_value(LANG_TYPE_INTEIRA, node->lexeme);
        }

        case AST_REAL_LITERAL: {
            float rounded;
            double exact;
            uint64_t bits;
            number = strtod(node->lexeme != NULL ? node->lexeme : "0",
                            &end);
            if (end == node->lexeme) {
                codegen_error(context, node,
                              "literal real invalido para LLVM");
                return invalid_value();
            }

            /*
             * LLVM 18 aceita a forma legada 0x + 16 hexadecimais para
             * float/double. Para um literal Macaronica (flut), primeiro
             * arredondamos para binary32 e depois promovemos esse valor exato
             * a double apenas para serializar o bitstring exigido pela IR.
             */
            rounded = (float)number;
            exact = (double)rounded;
            memcpy(&bits, &exact, sizeof(bits));
            snprintf(literal, sizeof(literal), "0x%016llX",
                     (unsigned long long)bits);
            return make_value(LANG_TYPE_FLUT, literal);
        }

        case AST_BOOL_LITERAL:
            return make_value(
                LANG_TYPE_BOOL,
                node->lexeme != NULL && strcmp(node->lexeme, "VER") == 0
                    ? "true"
                    : "false");

        case AST_STRING_LITERAL: {
            const StringLiteral *string = find_string(context, node);
            if (string == NULL) {
                codegen_error(context, node,
                              "literal de palavra nao foi registrado");
                return invalid_value();
            }
            snprintf(literal, sizeof(literal), "@.str.%zu", string->id);
            return make_value(LANG_TYPE_PALAVRA, literal);
        }

        case AST_IDENTIFIER:
            return generate_identifier(context, node);

        case AST_VECTOR_ACCESS:
            return load_address(context, node,
                                generate_vector_address(context, node));

        case AST_FIELD_ACCESS:
            return load_address(context, node,
                                generate_field_address(context, node));

        case AST_BINARY_EXPR:
            return generate_binary(context, node);

        case AST_UNARY_EXPR:
            return generate_unary(context, node);

        case AST_ROOT_EXPR:
            return generate_root_expression(context, node);

        case AST_VECTOR_REDUCTION:
            codegen_error(context, node,
                          "reducao vetorial DSL requer lowering via MLIR (PjBL 4 Parte B)");
            return invalid_value();

        case AST_CALL:
            if (node->lexeme != NULL &&
                (strcmp(node->lexeme, "entrada") == 0 ||
                 strcmp(node->lexeme, "saida") == 0)) {
                codegen_error(context, node,
                              "builtin '%s' nao pode ser usado como expressao de valor",
                              node->lexeme);
                return invalid_value();
            }
            return generate_user_call(context, node);

        default:
            codegen_error(context, node,
                          "no %s ainda nao pode ser gerado como expressao LLVM",
                          ast_node_kind_name(node->kind));
            return invalid_value();
    }
}

static int declare_global(CodegenContext *context, const AstNode *node) {
    LangType type;
    const char *type_name;
    const char *record_type_name = NULL;
    size_t vector_size = 0U;
    size_t first_identifier;
    size_t i;

    if (node == NULL ||
        (node->kind != AST_VAR_DECL && node->kind != AST_VECTOR_DECL))
        return 0;

    if (node->token_type == TOKEN_IDENTIFIER) {
        type = LANG_TYPE_RECORD;
        record_type_name = node->lexeme;
        type_name = NULL;
    } else {
        type = lang_type_from_token(node->token_type);
        type_name = llvm_type(type);
    }

    if (node->kind == AST_VECTOR_DECL)
        vector_size = vector_size_from_declaration(node);

    first_identifier = declaration_first_identifier(node);

    for (i = first_identifier; i < node->child_count; ++i) {
        const AstNode *identifier = node->children[i];
        Storage *storage;

        if (identifier == NULL || identifier->kind != AST_IDENTIFIER)
            continue;

        if (!add_storage(context, 1, identifier->lexeme, type, identifier))
            return 0;

        storage = &context->globals[context->global_count - 1U];
        storage->is_vector = node->kind == AST_VECTOR_DECL;
        storage->vector_size = vector_size;
        storage->record_type_name = record_type_name;

        if (storage->is_vector) {
            if (type_name == NULL) {
                codegen_error(context, node,
                              "vetor global possui tipo de elemento invalido");
                return 0;
            }
            fprintf(context->out,
                    "@%s = global [%zu x %s] zeroinitializer\n",
                    identifier->lexeme, vector_size, type_name);
        } else if (type == LANG_TYPE_RECORD) {
            if (record_type_name == NULL ||
                find_record_definition(context, record_type_name) == NULL) {
                codegen_error(context, node,
                              "tipo de registro global '%s' nao encontrado",
                              record_type_name != NULL
                                  ? record_type_name : "<sem-nome>");
                return 0;
            }
            fprintf(context->out,
                    "@%s = global %%struct.%s zeroinitializer\n",
                    identifier->lexeme, record_type_name);
        } else {
            if (type_name == NULL || type == LANG_TYPE_VAZIO) {
                codegen_error(context, node,
                              "tipo global %s ainda nao suportado",
                              lang_type_name(type));
                return 0;
            }
            fprintf(context->out, "@%s = global %s zeroinitializer\n",
                    identifier->lexeme, type_name);
        }
    }

    return 1;
}

static int predeclare_locals(CodegenContext *context,
                             const AstNode *node) {
    size_t i;

    if (node == NULL || context->memory_error) return !context->memory_error;

    if (node->kind == AST_VAR_DECL || node->kind == AST_VECTOR_DECL) {
        LangType type;
        const char *type_name = NULL;
        const char *record_type_name = NULL;
        size_t vector_size = 0U;
        size_t first_identifier = declaration_first_identifier(node);

        if (node->token_type == TOKEN_IDENTIFIER) {
            type = LANG_TYPE_RECORD;
            record_type_name = node->lexeme;
        } else {
            type = lang_type_from_token(node->token_type);
            type_name = llvm_type(type);
        }

        if (node->kind == AST_VECTOR_DECL)
            vector_size = vector_size_from_declaration(node);

        for (i = first_identifier; i < node->child_count; ++i) {
            const AstNode *identifier = node->children[i];
            Storage *storage;

            if (identifier == NULL || identifier->kind != AST_IDENTIFIER)
                continue;

            if (!add_storage(context, 0, identifier->lexeme, type, identifier))
                return 0;

            storage = &context->locals[context->local_count - 1U];
            storage->is_vector = node->kind == AST_VECTOR_DECL;
            storage->vector_size = vector_size;
            storage->record_type_name = record_type_name;

            if (storage->is_vector) {
                if (type_name == NULL) {
                    codegen_error(context, node,
                                  "vetor local possui tipo de elemento invalido");
                    return 0;
                }
                fprintf(context->out,
                        "  %%%s = alloca [%zu x %s] ; linha %d\n",
                        storage->llvm_name, vector_size, type_name,
                        identifier->line);
            } else if (type == LANG_TYPE_RECORD) {
                if (record_type_name == NULL ||
                    find_record_definition(context, record_type_name) == NULL) {
                    codegen_error(context, node,
                                  "tipo de registro local '%s' nao encontrado",
                                  record_type_name != NULL
                                      ? record_type_name : "<sem-nome>");
                    return 0;
                }
                fprintf(context->out,
                        "  %%%s = alloca %%struct.%s ; linha %d\n",
                        storage->llvm_name, record_type_name,
                        identifier->line);
            } else {
                if (type_name == NULL || type == LANG_TYPE_VAZIO) {
                    codegen_error(context, node,
                                  "tipo local %s ainda nao suportado",
                                  lang_type_name(type));
                    return 0;
                }
                fprintf(context->out,
                        "  %%%s = alloca %s ; linha %d\n",
                        storage->llvm_name, type_name, identifier->line);
            }
        }
        return 1;
    }

    for (i = 0U; i < node->child_count; ++i)
        if (!predeclare_locals(context, node->children[i]))
            return 0;

    return 1;
}

static int activate_declaration(CodegenContext *context,
                                const AstNode *node) {
    size_t i;

    if (node == NULL ||
        (node->kind != AST_VAR_DECL && node->kind != AST_VECTOR_DECL))
        return 1;

    for (i = 0U; i < node->child_count; ++i) {
        const AstNode *identifier = node->children[i];
        Storage *storage;

        if (identifier == NULL || identifier->kind != AST_IDENTIFIER)
            continue;

        storage = find_storage_by_declaration(context, identifier);
        if (storage == NULL) {
            codegen_error(context, identifier,
                          "armazenamento prealocado de '%s' nao encontrado",
                          identifier->lexeme);
            return 0;
        }

        storage->active = 1;
        storage->scope_depth = context->scope_depth;
    }

    return 1;
}

static void deactivate_scope(CodegenContext *context, size_t scope_depth) {
    size_t i;

    for (i = 0U; i < context->local_count; ++i)
        if (context->locals[i].active &&
            context->locals[i].scope_depth == scope_depth)
            context->locals[i].active = 0;
}

static void kernel_arg_list_free(KernelArgList *list) {
    if (list == NULL) return;
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static int kernel_arg_list_add(CodegenContext *context,
                               KernelArgList *list,
                               const AstNode *node) {
    const AstNode **grown;
    size_t new_capacity;
    size_t i;

    if (node == NULL || node->kind != AST_IDENTIFIER ||
        node->lexeme == NULL)
        return 1;

    for (i = 0U; i < list->count; ++i) {
        const AstNode *existing = list->items[i];
        if (existing != NULL && existing->lexeme != NULL &&
            existing->inferred_is_vector == node->inferred_is_vector &&
            strcmp(existing->lexeme, node->lexeme) == 0)
            return 1;
    }

    if (list->count == list->capacity) {
        new_capacity = list->capacity == 0U
                           ? CODEGEN_INITIAL_CAPACITY
                           : list->capacity * 2U;
        grown = realloc(list->items,
                        new_capacity * sizeof(*grown));
        if (grown == NULL) {
            codegen_memory_error(context);
            return 0;
        }
        list->items = grown;
        list->capacity = new_capacity;
    }

    list->items[list->count++] = node;
    return 1;
}

static int collect_kernel_args(CodegenContext *context,
                               KernelArgList *list,
                               const AstNode *node) {
    size_t i;

    if (node == NULL || context->memory_error)
        return !context->memory_error;

    if (node->kind == AST_IDENTIFIER)
        return kernel_arg_list_add(context, list, node);

    for (i = 0U; i < node->child_count; ++i)
        if (!collect_kernel_args(context, list, node->children[i]))
            return 0;

    return 1;
}

static void mlir_kernel_name(const AstNode *assignment,
                             char buffer[LLVM_OPERAND_CAPACITY]) {
    snprintf(buffer, LLVM_OPERAND_CAPACITY,
             "zl_mlir_kernel_l%d_c%d",
             assignment->line, assignment->column);
}

static int is_mlir_kernel_assignment(const AstNode *node) {
    const AstNode *target;
    const AstNode *value;

    if (node == NULL || node->kind != AST_ASSIGNMENT ||
        node->child_count < 2U)
        return 0;

    target = node->children[0];
    value = node->children[1];

    return (target != NULL && target->inferred_is_vector &&
            value != NULL && value->inferred_is_vector) ||
           (value != NULL && value->kind == AST_VECTOR_REDUCTION);
}

static void emit_memref_signature(FILE *out) {
    fprintf(out, "ptr, ptr, i64, i64, i64");
}

static void emit_kernel_arg_signature(FILE *out,
                                      const AstNode *arg) {
    if (arg->inferred_is_vector)
        emit_memref_signature(out);
    else
        fprintf(out, "%s", llvm_type(arg->inferred_type));
}

static int emit_kernel_declaration(CodegenContext *context,
                                   const AstNode *assignment) {
    const AstNode *target = assignment->children[0];
    const AstNode *value = assignment->children[1];
    KernelArgList args;
    char name[LLVM_OPERAND_CAPACITY];
    size_t i;

    memset(&args, 0, sizeof(args));
    if (!collect_kernel_args(context, &args, value)) {
        kernel_arg_list_free(&args);
        return 0;
    }

    mlir_kernel_name(assignment, name);

    if (value->kind == AST_VECTOR_REDUCTION)
        fprintf(context->out, "declare %s @%s(",
                llvm_type(value->inferred_type), name);
    else
        fprintf(context->out, "declare void @%s(", name);

    for (i = 0U; i < args.count; ++i) {
        if (i > 0U) fprintf(context->out, ", ");
        emit_kernel_arg_signature(context->out, args.items[i]);
    }

    if (target->inferred_is_vector) {
        if (args.count > 0U) fprintf(context->out, ", ");
        emit_memref_signature(context->out);
    }

    fprintf(context->out, ") nounwind\n");
    kernel_arg_list_free(&args);
    return 1;
}

static int emit_kernel_declarations_recursive(CodegenContext *context,
                                              const AstNode *node) {
    size_t i;

    if (node == NULL || context->memory_error)
        return !context->memory_error;

    if (is_mlir_kernel_assignment(node))
        if (!emit_kernel_declaration(context, node))
            return 0;

    for (i = 0U; i < node->child_count; ++i)
        if (!emit_kernel_declarations_recursive(context,
                                                node->children[i]))
            return 0;

    return 1;
}

static int emit_memref_call_args(CodegenContext *context,
                                 const AstNode *node,
                                 int *need_comma) {
    const Storage *storage;
    char pointer[LLVM_OPERAND_CAPACITY];

    storage = lookup_storage(context, node->lexeme);
    if (storage == NULL || !storage->is_vector) {
        codegen_error(context, node,
                      "vetor '%s' nao possui storage para chamada MLIR",
                      node->lexeme != NULL ? node->lexeme : "<sem-nome>");
        return 0;
    }

    storage_pointer(storage, pointer);
    if (*need_comma) fprintf(context->out, ", ");
    fprintf(context->out,
            "ptr %s, ptr %s, i64 0, i64 %zu, i64 1",
            pointer, pointer, storage->vector_size);
    *need_comma = 1;
    return 1;
}

static LlvmValue *prepare_kernel_scalar_args(
    CodegenContext *context, const KernelArgList *args) {
    LlvmValue *values;
    size_t i;

    values = calloc(args->count == 0U ? 1U : args->count,
                    sizeof(*values));
    if (values == NULL) {
        codegen_memory_error(context);
        return NULL;
    }

    for (i = 0U; i < args->count; ++i) {
        const AstNode *arg = args->items[i];
        if (arg->inferred_is_vector) continue;

        values[i] = generate_identifier(context, arg);
        if (!values[i].valid) {
            free(values);
            return NULL;
        }
    }

    return values;
}

static int emit_kernel_call_args(CodegenContext *context,
                                 const KernelArgList *args,
                                 const LlvmValue *scalar_values,
                                 int *need_comma) {
    size_t i;

    for (i = 0U; i < args->count; ++i) {
        const AstNode *arg = args->items[i];

        if (arg->inferred_is_vector) {
            if (!emit_memref_call_args(context, arg, need_comma))
                return 0;
        } else {
            const LlvmValue value = scalar_values[i];

            if (!value.valid) return 0;
            if (*need_comma) fprintf(context->out, ", ");
            fprintf(context->out, "%s %s",
                    llvm_type(value.type), value.operand);
            *need_comma = 1;
        }
    }

    return 1;
}

static int generate_mlir_kernel_assignment(CodegenContext *context,
                                           const AstNode *node) {
    const AstNode *target = node->children[0];
    const AstNode *expression = node->children[1];
    KernelArgList args;
    LlvmValue *scalar_values = NULL;
    char name[LLVM_OPERAND_CAPACITY];
    int need_comma = 0;

    memset(&args, 0, sizeof(args));
    if (!collect_kernel_args(context, &args, expression)) {
        free(scalar_values);
        kernel_arg_list_free(&args);
        return 0;
    }

    mlir_kernel_name(node, name);

    scalar_values = prepare_kernel_scalar_args(context, &args);
    if (scalar_values == NULL) {
        free(scalar_values);
        kernel_arg_list_free(&args);
        return 0;
    }

    if (expression->kind == AST_VECTOR_REDUCTION) {
        LlvmAddress address;
        LlvmValue value;
        char temp[LLVM_OPERAND_CAPACITY];
        const char *result_type;

        address = generate_lvalue_address(context, target);
        if (!address.valid) {
            free(scalar_values);
            kernel_arg_list_free(&args);
            return 0;
        }

        result_type = llvm_type(expression->inferred_type);
        new_temp(context, temp);
        fprintf(context->out, "  %s = call %s @%s(",
                temp, result_type, name);

        if (!emit_kernel_call_args(context, &args, scalar_values, &need_comma)) {
            free(scalar_values);
            kernel_arg_list_free(&args);
            return 0;
        }

        fprintf(context->out, ") nounwind ; kernel MLIR linha %d\n",
                node->line);

        value = make_value(expression->inferred_type, temp);
        value = convert_value(context, expression, value, address.type);
        if (!value.valid) {
            free(scalar_values);
            kernel_arg_list_free(&args);
            return 0;
        }

        fprintf(context->out,
                "  store %s %s, ptr %s ; resultado kernel MLIR\n",
                llvm_type(address.type), value.operand, address.pointer);
    } else {
        const Storage *output;
        char output_pointer[LLVM_OPERAND_CAPACITY];

        output = lookup_storage(context, target->lexeme);
        if (output == NULL || !output->is_vector) {
            codegen_error(context, target,
                          "destino vetorial '%s' nao possui storage LLVM",
                          target->lexeme != NULL ? target->lexeme : "<sem-nome>");
            free(scalar_values);
            kernel_arg_list_free(&args);
            return 0;
        }

        fprintf(context->out, "  call void @%s(", name);
        if (!emit_kernel_call_args(context, &args, scalar_values, &need_comma)) {
            free(scalar_values);
            kernel_arg_list_free(&args);
            return 0;
        }

        storage_pointer(output, output_pointer);
        if (need_comma) fprintf(context->out, ", ");
        fprintf(context->out,
                "ptr %s, ptr %s, i64 0, i64 %zu, i64 1",
                output_pointer, output_pointer, output->vector_size);
        fprintf(context->out,
                ") nounwind ; kernel MLIR linha %d\n",
                node->line);
    }

    free(scalar_values);
    kernel_arg_list_free(&args);
    return 1;
}

static int generate_assignment(CodegenContext *context,
                               const AstNode *node) {
    const AstNode *target;
    LlvmAddress address;
    LlvmValue value;
    const char *type_name;

    if (node == NULL || node->child_count < 2U) return 0;
    target = node->children[0];

    if (is_mlir_kernel_assignment(node))
        return generate_mlir_kernel_assignment(context, node);

    address = generate_lvalue_address(context, target);
    if (!address.valid) return 0;

    value = generate_expression(context, node->children[1]);
    value = convert_value(context, node, value, address.type);
    if (!value.valid) return 0;

    type_name = llvm_type(address.type);
    if (type_name == NULL) {
        codegen_error(context, target,
                      "destino de atribuicao possui tipo LLVM nao escalar");
        return 0;
    }

    fprintf(context->out, "  store %s %s, ptr %s ; linha %d\n",
            type_name, value.operand, address.pointer, node->line);
    return 1;
}

static int generate_builtin_call(CodegenContext *context,
                                 const AstNode *node) {
    LlvmValue value;

    if (node->lexeme != NULL && strcmp(node->lexeme, "saida") == 0) {
        if (node->child_count != 1U) return 0;
        value = generate_expression(context, node->children[0]);
        if (!value.valid) return 0;

        switch (value.type) {
            case LANG_TYPE_INTEIRA:
                fprintf(context->out,
                        "  call void @zl_imprime_int(i32 %s)\n",
                        value.operand);
                break;
            case LANG_TYPE_BOOL: {
                char temp[LLVM_OPERAND_CAPACITY];
                new_temp(context, temp);
                fprintf(context->out,
                        "  %s = zext i1 %s to i32\n",
                        temp, value.operand);
                fprintf(context->out,
                        "  call void @zl_imprime_bool(i32 %s)\n",
                        temp);
                break;
            }
            case LANG_TYPE_FLUT:
                fprintf(context->out,
                        "  call void @zl_imprime_flut(float %s)\n",
                        value.operand);
                break;
            case LANG_TYPE_DUPLOCARPADO:
                fprintf(context->out,
                        "  call void @zl_imprime_duplo(double %s)\n",
                        value.operand);
                break;
            case LANG_TYPE_PALAVRA:
                fprintf(context->out,
                        "  call void @zl_imprime_texto(ptr %s)\n",
                        value.operand);
                break;
            default:
                codegen_error(context, node,
                              "saida ainda nao suporta tipo %s",
                              lang_type_name(value.type));
                return 0;
        }

        fprintf(context->out, "  call void @zl_imprime_nl() ; linha %d\n",
                node->line);
        return 1;
    }

    if (node->lexeme != NULL && strcmp(node->lexeme, "entrada") == 0) {
        const AstNode *target;
        LlvmAddress address;
        char temp[LLVM_OPERAND_CAPACITY];

        if (node->child_count != 1U) return 0;
        target = node->children[0];

        address = generate_lvalue_address(context, target);
        if (!address.valid) return 0;
        if (address.type != LANG_TYPE_INTEIRA) {
            codegen_error(context, target,
                          "entrada atualmente requer destino inteira");
            return 0;
        }

        new_temp(context, temp);
        fprintf(context->out, "  %s = call i32 @zl_le_int()\n", temp);
        fprintf(context->out,
                "  store i32 %s, ptr %s ; linha %d\n",
                temp, address.pointer, node->line);
        return 1;
    }

    value = generate_user_call(context, node);
    return value.valid;
}

static int generate_return(CodegenContext *context, const AstNode *node) {
    LlvmValue value;
    const char *type_name;

    if (context->current_return_type == LANG_TYPE_VAZIO) {
        if (node->child_count != 0U) return 0;
        fprintf(context->out, "  ret void ; linha %d\n", node->line);
        context->block_terminated = 1;
        return 1;
    }

    if (node->child_count == 0U) {
        codegen_error(context, node,
                      "respost sem valor em funcao nao-vazio");
        return 0;
    }

    value = generate_expression(context, node->children[0]);
    value = convert_value(context, node, value, context->current_return_type);
    if (!value.valid) return 0;

    type_name = llvm_type(context->current_return_type);
    fprintf(context->out, "  ret %s %s ; linha %d\n",
            type_name, value.operand, node->line);
    context->block_terminated = 1;
    return 1;
}

static int generate_statement(CodegenContext *context,
                              const AstNode *node);

static int generate_block(CodegenContext *context, const AstNode *block,
                          int create_scope) {
    size_t i;
    size_t this_scope = context->scope_depth;

    if (block == NULL || block->kind != AST_BLOCK) {
        codegen_error(context, block, "bloco LLVM invalido");
        return 0;
    }

    if (create_scope) {
        ++context->scope_depth;
        this_scope = context->scope_depth;
    }

    for (i = 0U; i < block->child_count && !context->memory_error; ++i) {
        if (context->block_terminated) break;
        if (!generate_statement(context, block->children[i])) {
            if (create_scope) {
                deactivate_scope(context, this_scope);
                --context->scope_depth;
            }
            return 0;
        }
    }

    if (create_scope) {
        deactivate_scope(context, this_scope);
        --context->scope_depth;
    }

    return 1;
}

static int generate_if(CodegenContext *context, const AstNode *node) {
    const AstNode *condition;
    const AstNode *body;
    size_t next_branch = 2U;
    char end_label[LLVM_OPERAND_CAPACITY];
    int reaches_end = 0;

    if (node->child_count < 2U) {
        codegen_error(context, node, "condicional incompleto na AST");
        return 0;
    }

    condition = node->children[0];
    body = node->children[1];
    new_label(context, "if.end", end_label);

    for (;;) {
        LlvmValue value;
        char then_label[LLVM_OPERAND_CAPACITY];
        char false_label[LLVM_OPERAND_CAPACITY];
        const AstNode *next = next_branch < node->child_count
                                  ? node->children[next_branch]
                                  : NULL;

        new_label(context, "if.then", then_label);
        if (next == NULL)
            snprintf(false_label, sizeof(false_label), "%s", end_label);
        else if (next->kind == AST_ELSE)
            new_label(context, "if.else", false_label);
        else
            new_label(context, "if.next", false_label);

        value = generate_expression(context, condition);
        if (!value.valid || value.type != LANG_TYPE_BOOL) {
            codegen_error(context, condition,
                          "condicao LLVM deve produzir i1");
            return 0;
        }

        emit_cond_branch(context, value, then_label, false_label);

        emit_label(context, then_label);
        if (!generate_block(context, body, 1)) return 0;
        if (!context->block_terminated) {
            emit_branch(context, end_label);
            reaches_end = 1;
        }

        if (next == NULL) {
            reaches_end = 1; /* ramo falso cai diretamente no fim */
            break;
        }

        emit_label(context, false_label);

        if (next->kind == AST_ELSE_IF) {
            if (next->child_count < 2U) {
                codegen_error(context, next,
                              "casocontrario cond incompleto na AST");
                return 0;
            }
            condition = next->children[0];
            body = next->children[1];
            ++next_branch;
            continue;
        }

        if (next->kind == AST_ELSE) {
            if (next->child_count < 1U) {
                codegen_error(context, next,
                              "casocontrario incompleto na AST");
                return 0;
            }
            if (!generate_block(context, next->children[0], 1)) return 0;
            if (!context->block_terminated) {
                emit_branch(context, end_label);
                reaches_end = 1;
            }
            ++next_branch;
            break;
        }

        codegen_error(context, next,
                      "ramo inesperado %s no condicional",
                      ast_node_kind_name(next->kind));
        return 0;
    }

    if (reaches_end) {
        emit_label(context, end_label);
        context->block_terminated = 0;
    } else {
        context->block_terminated = 1;
    }

    return 1;
}

static int generate_while(CodegenContext *context, const AstNode *node) {
    LlvmValue condition;
    char cond_label[LLVM_OPERAND_CAPACITY];
    char body_label[LLVM_OPERAND_CAPACITY];
    char end_label[LLVM_OPERAND_CAPACITY];

    if (node->child_count < 2U) {
        codegen_error(context, node, "durante incompleto na AST");
        return 0;
    }

    new_label(context, "while.cond", cond_label);
    new_label(context, "while.body", body_label);
    new_label(context, "while.end", end_label);

    emit_branch(context, cond_label);

    emit_label(context, cond_label);
    condition = generate_expression(context, node->children[0]);
    if (!condition.valid || condition.type != LANG_TYPE_BOOL) {
        codegen_error(context, node->children[0],
                      "condicao de durante deve produzir i1");
        return 0;
    }
    emit_cond_branch(context, condition, body_label, end_label);

    emit_label(context, body_label);
    if (!generate_block(context, node->children[1], 1)) return 0;
    if (!context->block_terminated)
        emit_branch(context, cond_label);

    emit_label(context, end_label);
    return 1;
}

static int generate_for(CodegenContext *context, const AstNode *node) {
    LlvmValue condition;
    char cond_label[LLVM_OPERAND_CAPACITY];
    char body_label[LLVM_OPERAND_CAPACITY];
    char step_label[LLVM_OPERAND_CAPACITY];
    char end_label[LLVM_OPERAND_CAPACITY];

    if (node->child_count < 4U) {
        codegen_error(context, node, "repete incompleto na AST");
        return 0;
    }

    if (!generate_assignment(context, node->children[0])) return 0;

    new_label(context, "for.cond", cond_label);
    new_label(context, "for.body", body_label);
    new_label(context, "for.step", step_label);
    new_label(context, "for.end", end_label);

    emit_branch(context, cond_label);

    emit_label(context, cond_label);
    condition = generate_expression(context, node->children[1]);
    if (!condition.valid || condition.type != LANG_TYPE_BOOL) {
        codegen_error(context, node->children[1],
                      "condicao de repete deve produzir i1");
        return 0;
    }
    emit_cond_branch(context, condition, body_label, end_label);

    emit_label(context, body_label);
    if (!generate_block(context, node->children[3], 1)) return 0;
    if (!context->block_terminated)
        emit_branch(context, step_label);

    emit_label(context, step_label);
    if (!generate_assignment(context, node->children[2])) return 0;
    emit_branch(context, cond_label);

    emit_label(context, end_label);
    return 1;
}

static int generate_statement(CodegenContext *context,
                              const AstNode *node) {
    if (context->block_terminated || node == NULL) return 1;

    switch (node->kind) {
        case AST_VAR_DECL:
        case AST_VECTOR_DECL:
            return activate_declaration(context, node);
        case AST_ASSIGNMENT:
            return generate_assignment(context, node);
        case AST_CALL:
            return generate_builtin_call(context, node);
        case AST_RETURN:
            return generate_return(context, node);
        case AST_IF:
            return generate_if(context, node);
        case AST_WHILE:
            return generate_while(context, node);
        case AST_FOR:
            return generate_for(context, node);
        case AST_BLOCK:
            return generate_block(context, node, 1);
        default:
            codegen_error(context, node,
                          "comando %s sera rebaixado em checkpoint posterior",
                          ast_node_kind_name(node->kind));
            return 0;
    }
}

static size_t parameter_count(const AstNode *function);

static int prepare_parameters(CodegenContext *context,
                              const AstNode *function) {
    size_t count = parameter_count(function);
    size_t i;

    for (i = 0U; i < count; ++i) {
        const AstNode *parameter = function_parameter_at(function, i);
        LangType type;
        Storage *storage;

        if (parameter == NULL) continue;
        type = lang_type_from_token(parameter->token_type);

        if (!add_storage(context, 0, parameter->lexeme, type, parameter))
            return 0;

        storage = &context->locals[context->local_count - 1U];
        storage->active = 1;
        storage->scope_depth = 0U;

        fprintf(context->out,
                "  %%%s = alloca %s ; parametro linha %d\n",
                storage->llvm_name, llvm_type(type), parameter->line);
        fprintf(context->out,
                "  store %s %%arg%zu, ptr %%%s\n",
                llvm_type(type), i, storage->llvm_name);
    }

    return 1;
}

static void reset_function_context(CodegenContext *context,
                                   LangType return_type) {
    context->local_count = 0U;
    context->current_return_type = return_type;
    context->block_terminated = 0;
    context->next_temp = 0U;
    context->next_label = 0U;
    context->scope_depth = 0U;
    snprintf(context->current_block, sizeof(context->current_block), "entry");
}

static const AstNode *function_body(const AstNode *function) {
    size_t i;

    if (function == NULL) return NULL;
    for (i = 0U; i < function->child_count; ++i)
        if (function->children[i] != NULL &&
            function->children[i]->kind == AST_BLOCK)
            return function->children[i];

    return NULL;
}

static size_t parameter_count(const AstNode *function) {
    size_t i;
    size_t count = 0U;

    for (i = 0U; i < function->child_count; ++i)
        if (function->children[i] != NULL &&
            function->children[i]->kind == AST_PARAMETER)
            ++count;

    return count;
}

static int generate_user_function(CodegenContext *context,
                                  const AstNode *function) {
    const AstNode *body;
    LangType return_type;
    const char *return_name;
    size_t count;
    size_t i;

    if (function == NULL || function->kind != AST_FUNCTION)
        return 0;

    body = function_body(function);
    if (body == NULL) {
        codegen_error(context, function,
                      "funcao '%s' nao possui bloco para geracao LLVM",
                      function->lexeme != NULL ? function->lexeme : "<sem-nome>");
        return 0;
    }

    return_type = lang_type_from_token(function->token_type);
    return_name = llvm_type(return_type);
    count = parameter_count(function);

    fprintf(context->out, "define %s @%s(",
            return_name, function->lexeme);
    for (i = 0U; i < count; ++i) {
        const AstNode *parameter = function_parameter_at(function, i);
        LangType type = lang_type_from_token(parameter->token_type);
        if (i > 0U) fprintf(context->out, ", ");
        fprintf(context->out, "%s %%arg%zu", llvm_type(type), i);
    }
    fprintf(context->out, ") nounwind {\nentry:\n");

    reset_function_context(context, return_type);
    if (!prepare_parameters(context, function)) return 0;
    if (!predeclare_locals(context, body)) return 0;
    if (!generate_block(context, body, 0)) return 0;

    if (!context->block_terminated) {
        if (return_type == LANG_TYPE_VAZIO) {
            fprintf(context->out, "  ret void\n");
            context->block_terminated = 1;
        } else {
            codegen_error(context, function,
                          "funcao '%s' terminou sem terminador LLVM",
                          function->lexeme);
            return 0;
        }
    }

    fprintf(context->out, "}\n\n");
    return 1;
}

static int generate_principal(CodegenContext *context,
                              const AstNode *function) {
    const AstNode *body;

    if (function->token_type != TOKEN_INTEIRA) {
        codegen_error(context, function,
                      "neste contrato de runtime, principal deve retornar inteira");
        return 0;
    }

    if (parameter_count(function) != 0U) {
        codegen_error(context, function,
                      "neste contrato de runtime, principal nao recebe parametros");
        return 0;
    }

    body = function_body(function);
    if (body == NULL) {
        codegen_error(context, function,
                      "principal nao possui bloco para geracao LLVM");
        return 0;
    }

    reset_function_context(context, LANG_TYPE_INTEIRA);

    fprintf(context->out,
            "define i32 @zl_principal() nounwind {\n"
            "entry:\n");

    if (!predeclare_locals(context, body)) return 0;
    if (!generate_block(context, body, 0)) return 0;

    if (!context->block_terminated) {
        codegen_error(context, function,
                      "principal terminou sem terminador LLVM");
        return 0;
    }

    fprintf(context->out, "}\n\n");
    return 1;
}

static int generate_program(CodegenContext *context,
                            const AstNode *program) {
    size_t i;
    const AstNode *principal = NULL;

    if (program == NULL || program->kind != AST_PROGRAM) {
        codegen_error(context, program,
                      "raiz do codegen deve ser AST_PROGRAM");
        return 0;
    }

    context->program = program;

    fprintf(context->out,
            "; LLVM IR gerado pelo compilador Macaronica\n"
            "; IR portavel: sem target triple e sem target datalayout\n\n");

    collect_strings(context, program);
    if (context->memory_error) return 0;
    if (!emit_record_types(context, program)) return 0;
    emit_string_globals(context);
    emit_runtime_declarations(context);

    if (!emit_kernel_declarations_recursive(context, program))
        return 0;
    if (context->error_count == 0U)
        fputc('\n', context->out);

    for (i = 0U; i < program->child_count; ++i) {
        const AstNode *node = program->children[i];

        if (node == NULL) continue;
        if (node->kind == AST_VAR_DECL ||
            node->kind == AST_VECTOR_DECL) {
            if (!declare_global(context, node)) return 0;
        } else if (node->kind == AST_PRINCIPAL) {
            principal = node;
        } else if (node->kind == AST_RECORD) {
            /* Tipo nomeado ja foi emitido antes das variaveis globais. */
            continue;
        }
    }

    if (context->global_count > 0U) fputc('\n', context->out);

    /*
     * Definicoes podem chamar funcoes declaradas mais adiante no texto:
     * referencias globais nomeadas do LLVM sao resolvidas no modulo completo.
     */
    for (i = 0U; i < program->child_count; ++i) {
        const AstNode *node = program->children[i];
        if (node != NULL && node->kind == AST_FUNCTION)
            if (!generate_user_function(context, node)) return 0;
    }

    if (principal == NULL) {
        codegen_error(context, program,
                      "principal nao encontrada para geracao LLVM");
        return 0;
    }

    return generate_principal(context, principal);
}

CodegenLlvmResult codegen_llvm_generate(const AstNode *ast,
                                        FILE *output,
                                        FILE *error_stream) {
    CodegenContext context;
    CodegenLlvmResult result;

    memset(&context, 0, sizeof(context));
    context.out = output;
    context.err = error_stream != NULL ? error_stream : stderr;

    if (output == NULL) {
        result.status = CODEGEN_LLVM_HAS_ERRORS;
        result.error_count = 1U;
        fprintf(context.err,
                "[ERRO CODEGEN LLVM] fluxo de saida nao foi fornecido.\n");
        return result;
    }

    (void)generate_program(&context, ast);

    result.error_count = context.error_count;
    if (context.memory_error)
        result.status = CODEGEN_LLVM_MEMORY_ERROR;
    else if (context.error_count > 0U)
        result.status = CODEGEN_LLVM_HAS_ERRORS;
    else
        result.status = CODEGEN_LLVM_OK;

    free(context.locals);
    free(context.globals);
    free(context.strings);
    return result;
}
