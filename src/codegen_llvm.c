#include "codegen_llvm.h"

/*
 * Primeiro back-end LLVM da Macaronica.
 *
 * Escopo do Checkpoint 9:
 * - principal inteira sem parametros -> @zl_principal;
 * - variaveis escalares locais e globais;
 * - literais inteira/flut/bool/palavra;
 * - atribuicao a identificador;
 * - expressoes aritmeticas escalares, comparacoes, XOR e unarios;
 * - saida(...) e entrada(...) para os casos suportados pelo runtime POSIX;
 * - respost.
 *
 * Controle de fluxo, funcoes de usuario, vetores, registros, ^ e raiz()
 * entram nos checkpoints seguintes. Quando um programa valido usa uma
 * construcao ainda nao rebaixada, o gerador falha de forma controlada e o
 * arquivo de saida e removido pelo chamador.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define CODEGEN_INITIAL_CAPACITY 16U
#define LLVM_OPERAND_CAPACITY 160U

typedef struct {
    const char *name;
    LangType type;
    int global;
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
    FILE *out;
    FILE *err;
    size_t error_count;
    int memory_error;
    size_t next_temp;

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
                       const char *name, LangType type) {
    Storage **items = global ? &context->globals : &context->locals;
    size_t *count = global ? &context->global_count : &context->local_count;
    size_t *capacity =
        global ? &context->global_capacity : &context->local_capacity;

    if (!grow_storage(items, capacity, *count)) {
        codegen_memory_error(context);
        return 0;
    }

    (*items)[*count].name = name;
    (*items)[*count].type = type;
    (*items)[*count].global = global;
    ++(*count);
    return 1;
}

static const Storage *lookup_storage(const CodegenContext *context,
                                     const char *name) {
    size_t i;

    if (context == NULL || name == NULL) return NULL;

    for (i = context->local_count; i > 0U; --i) {
        if (strcmp(context->locals[i - 1U].name, name) == 0)
            return &context->locals[i - 1U];
    }

    for (i = 0U; i < context->global_count; ++i) {
        if (strcmp(context->globals[i].name, name) == 0)
            return &context->globals[i];
    }

    return NULL;
}

static void storage_pointer(const Storage *storage,
                            char buffer[LLVM_OPERAND_CAPACITY]) {
    if (storage->global)
        snprintf(buffer, LLVM_OPERAND_CAPACITY, "@%s", storage->name);
    else
        snprintf(buffer, LLVM_OPERAND_CAPACITY, "%%%s.addr", storage->name);
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
            "declare void @zl_erro_execucao(i32, i32) nounwind\n\n");
}

static void new_temp(CodegenContext *context,
                     char buffer[LLVM_OPERAND_CAPACITY]) {
    snprintf(buffer, LLVM_OPERAND_CAPACITY, "%%t%zu", context->next_temp++);
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
    char temp[LLVM_OPERAND_CAPACITY];

    if (node->child_count < 2U) return invalid_value();

    if (node->token_type == TOKEN_E || node->token_type == TOKEN_OU) {
        codegen_error(
            context, node,
            "E/OU exigem curto-circuito e serao rebaixados com controle de fluxo no proximo checkpoint");
        return invalid_value();
    }

    if (node->token_type == TOKEN_CARET) {
        codegen_error(context, node,
                      "operador '^' sera rebaixado no checkpoint de funcoes/intrinsecos");
        return invalid_value();
    }

    left = generate_expression(context, node->children[0]);
    right = generate_expression(context, node->children[1]);
    if (!left.valid || !right.valid) return invalid_value();

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

static LlvmValue generate_expression(CodegenContext *context,
                                     const AstNode *node) {
    char literal[LLVM_OPERAND_CAPACITY];
    char *end = NULL;
    double number;

    if (node == NULL) return invalid_value();

    switch (node->kind) {
        case AST_INTEGER_LITERAL:
            return make_value(LANG_TYPE_INTEIRA,
                              node->lexeme != NULL ? node->lexeme : "0");

        case AST_REAL_LITERAL:
            number = strtod(node->lexeme != NULL ? node->lexeme : "0",
                            &end);
            if (end == node->lexeme) {
                codegen_error(context, node,
                              "literal real invalido para LLVM");
                return invalid_value();
            }
            snprintf(literal, sizeof(literal), "%.9e", number);
            return make_value(LANG_TYPE_FLUT, literal);

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

        case AST_BINARY_EXPR:
            return generate_binary(context, node);

        case AST_UNARY_EXPR:
            return generate_unary(context, node);

        case AST_ROOT_EXPR:
            codegen_error(context, node,
                          "raiz() sera rebaixada com intrinseco/helper em checkpoint posterior");
            return invalid_value();

        case AST_CALL:
            codegen_error(context, node,
                          "chamada '%s' nao pode ser usada como expressao neste checkpoint",
                          node->lexeme != NULL ? node->lexeme : "<sem-nome>");
            return invalid_value();

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
    size_t i;

    if (node->kind != AST_VAR_DECL || node->token_type == TOKEN_IDENTIFIER) {
        codegen_error(context, node,
                      "somente globais escalares de tipo primitivo sao suportadas neste checkpoint");
        return 0;
    }

    type = lang_type_from_token(node->token_type);
    type_name = llvm_type(type);
    if (type_name == NULL || type == LANG_TYPE_VAZIO) {
        codegen_error(context, node,
                      "tipo global %s ainda nao suportado",
                      lang_type_name(type));
        return 0;
    }

    for (i = 0U; i < node->child_count; ++i) {
        const AstNode *identifier = node->children[i];
        if (identifier == NULL || identifier->kind != AST_IDENTIFIER)
            continue;

        if (!add_storage(context, 1, identifier->lexeme, type)) return 0;
        fprintf(context->out, "@%s = global %s zeroinitializer\n",
                identifier->lexeme, type_name);
    }

    return 1;
}

static int predeclare_locals(CodegenContext *context, const AstNode *body) {
    size_t i;
    size_t j;

    for (i = 0U; i < body->child_count; ++i) {
        const AstNode *statement = body->children[i];
        LangType type;
        const char *type_name;

        if (statement == NULL || statement->kind != AST_VAR_DECL)
            continue;

        if (statement->token_type == TOKEN_IDENTIFIER) {
            codegen_error(context, statement,
                          "variavel local de registro entra no checkpoint de registros");
            return 0;
        }

        type = lang_type_from_token(statement->token_type);
        type_name = llvm_type(type);
        if (type_name == NULL || type == LANG_TYPE_VAZIO) {
            codegen_error(context, statement,
                          "tipo local %s ainda nao suportado",
                          lang_type_name(type));
            return 0;
        }

        for (j = 0U; j < statement->child_count; ++j) {
            const AstNode *identifier = statement->children[j];
            if (identifier == NULL || identifier->kind != AST_IDENTIFIER)
                continue;

            if (!add_storage(context, 0, identifier->lexeme, type))
                return 0;

            fprintf(context->out, "  %%%s.addr = alloca %s ; linha %d\n",
                    identifier->lexeme, type_name, identifier->line);
        }
    }

    return 1;
}

static int generate_assignment(CodegenContext *context,
                               const AstNode *node) {
    const AstNode *target;
    const Storage *storage;
    const char *type_name;
    char pointer[LLVM_OPERAND_CAPACITY];
    LlvmValue value;

    if (node->child_count < 2U) return 0;
    target = node->children[0];

    if (target == NULL || target->kind != AST_IDENTIFIER) {
        codegen_error(context, target != NULL ? target : node,
                      "checkpoint 9 suporta atribuicao apenas a identificador escalar");
        return 0;
    }

    storage = lookup_storage(context, target->lexeme);
    if (storage == NULL) {
        codegen_error(context, target,
                      "armazenamento LLVM de '%s' nao encontrado",
                      target->lexeme);
        return 0;
    }

    value = generate_expression(context, node->children[1]);
    value = convert_value(context, node, value, storage->type);
    if (!value.valid) return 0;

    type_name = llvm_type(storage->type);
    storage_pointer(storage, pointer);
    fprintf(context->out, "  store %s %s, ptr %s ; linha %d\n",
            type_name, value.operand, pointer, node->line);
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
        const Storage *storage;
        char pointer[LLVM_OPERAND_CAPACITY];
        char temp[LLVM_OPERAND_CAPACITY];

        if (node->child_count != 1U) return 0;
        target = node->children[0];

        if (target == NULL || target->kind != AST_IDENTIFIER) {
            codegen_error(context, node,
                          "entrada no checkpoint 9 suporta apenas identificador inteira");
            return 0;
        }

        storage = lookup_storage(context, target->lexeme);
        if (storage == NULL || storage->type != LANG_TYPE_INTEIRA) {
            codegen_error(context, target,
                          "entrada no checkpoint 9 requer destino inteira");
            return 0;
        }

        new_temp(context, temp);
        fprintf(context->out, "  %s = call i32 @zl_le_int()\n", temp);
        storage_pointer(storage, pointer);
        fprintf(context->out,
                "  store i32 %s, ptr %s ; linha %d\n",
                temp, pointer, node->line);
        return 1;
    }

    codegen_error(context, node,
                  "chamada de funcao '%s' entra no checkpoint de funcoes",
                  node->lexeme != NULL ? node->lexeme : "<sem-nome>");
    return 0;
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
                              const AstNode *node) {
    if (context->block_terminated || node == NULL) return 1;

    switch (node->kind) {
        case AST_VAR_DECL:
            return 1;
        case AST_ASSIGNMENT:
            return generate_assignment(context, node);
        case AST_CALL:
            return generate_builtin_call(context, node);
        case AST_RETURN:
            return generate_return(context, node);
        default:
            codegen_error(context, node,
                          "comando %s sera rebaixado em checkpoint posterior",
                          ast_node_kind_name(node->kind));
            return 0;
    }
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

static int generate_principal(CodegenContext *context,
                              const AstNode *function) {
    const AstNode *body;
    size_t i;

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

    context->local_count = 0U;
    context->current_return_type = LANG_TYPE_INTEIRA;
    context->block_terminated = 0;
    context->next_temp = 0U;

    fprintf(context->out,
            "define i32 @zl_principal() nounwind {\n"
            "entry:\n");

    if (!predeclare_locals(context, body)) return 0;

    for (i = 0U; i < body->child_count && !context->memory_error; ++i)
        if (!generate_statement(context, body->children[i])) return 0;

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

    fprintf(context->out,
            "; LLVM IR gerado pelo compilador Macaronica\n"
            "; IR portavel: sem target triple e sem target datalayout\n\n");

    collect_strings(context, program);
    if (context->memory_error) return 0;
    emit_string_globals(context);
    emit_runtime_declarations(context);

    for (i = 0U; i < program->child_count; ++i) {
        const AstNode *node = program->children[i];

        if (node == NULL) continue;
        if (node->kind == AST_VAR_DECL) {
            if (!declare_global(context, node)) return 0;
        } else if (node->kind == AST_PRINCIPAL) {
            principal = node;
        } else if (node->kind == AST_FUNCTION ||
                   node->kind == AST_RECORD ||
                   node->kind == AST_VECTOR_DECL) {
            codegen_error(context, node,
                          "%s sera suportado em checkpoint posterior",
                          ast_node_kind_name(node->kind));
            return 0;
        }
    }

    if (context->global_count > 0U) fputc('\n', context->out);

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
