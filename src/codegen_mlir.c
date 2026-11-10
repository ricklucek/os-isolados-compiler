#include "codegen_mlir.h"

/*
 * Backend MLIR da DSL vetorial (PjBL 4).
 *
 * Estrategia hibrida:
 * - o programa escalar permanece no backend LLVM existente;
 * - cada atribuicao vetorial/reducao vira uma func.func independente;
 * - elemento a elemento usa affine.for;
 * - reducoes usam scf.for com iter_args;
 * - os kernels usam apenas func/arith/memref/affine/scf nesta etapa.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define MLIR_NAME_CAPACITY 128U
#define MLIR_INITIAL_ARGS 8U

typedef struct {
    const char *name;
    LangType type;
    int is_vector;
    size_t vector_size;
} MlirArg;

typedef struct {
    LangType type;
    char name[MLIR_NAME_CAPACITY];
    int valid;
} MlirValue;

typedef struct {
    FILE *out;
    FILE *err;
    size_t error_count;
    int memory_error;
    size_t kernel_count;
    size_t next_value;
    MlirArg *args;
    size_t arg_count;
    size_t arg_capacity;
    int affine_loads;
} MlirContext;

static void mlir_error(MlirContext *context, const AstNode *node,
                       const char *format, ...) {
    va_list args;

    if (context == NULL) return;
    ++context->error_count;

    if (node != NULL)
        fprintf(context->err,
                "[ERRO CODEGEN MLIR] linha %d, coluna %d: ",
                node->line, node->column);
    else
        fprintf(context->err, "[ERRO CODEGEN MLIR] ");

    va_start(args, format);
    vfprintf(context->err, format, args);
    va_end(args);
    fputc('\n', context->err);
}

static void mlir_memory_error(MlirContext *context) {
    if (context == NULL || context->memory_error) return;
    context->memory_error = 1;
    fprintf(context->err,
            "[ERRO INTERNO] memoria insuficiente durante a geracao MLIR.\n");
}

static const char *mlir_type(LangType type) {
    switch (type) {
        case LANG_TYPE_INTEIRA: return "i32";
        case LANG_TYPE_FLUT: return "f32";
        case LANG_TYPE_DUPLOCARPADO: return "f64";
        default: return NULL;
    }
}

static MlirValue invalid_value(void) {
    MlirValue value;
    memset(&value, 0, sizeof(value));
    value.type = LANG_TYPE_ERROR;
    return value;
}

static MlirValue make_value(LangType type, const char *name) {
    MlirValue value;
    memset(&value, 0, sizeof(value));
    value.type = type;
    value.valid = 1;
    snprintf(value.name, sizeof(value.name), "%s", name);
    return value;
}

static void new_value(MlirContext *context,
                      char buffer[MLIR_NAME_CAPACITY]) {
    snprintf(buffer, MLIR_NAME_CAPACITY, "%%v%zu", context->next_value++);
}

static int ensure_arg_capacity(MlirContext *context) {
    size_t new_capacity;
    MlirArg *grown;

    if (context->arg_count < context->arg_capacity) return 1;

    new_capacity = context->arg_capacity == 0U
                       ? MLIR_INITIAL_ARGS
                       : context->arg_capacity * 2U;
    grown = realloc(context->args, new_capacity * sizeof(*grown));
    if (grown == NULL) {
        mlir_memory_error(context);
        return 0;
    }

    context->args = grown;
    context->arg_capacity = new_capacity;
    return 1;
}

static int add_arg(MlirContext *context, const AstNode *node) {
    size_t i;
    MlirArg *arg;

    if (node == NULL || node->kind != AST_IDENTIFIER ||
        node->lexeme == NULL)
        return 1;

    for (i = 0U; i < context->arg_count; ++i)
        if (context->args[i].is_vector == node->inferred_is_vector &&
            strcmp(context->args[i].name, node->lexeme) == 0)
            return 1;

    if (!ensure_arg_capacity(context)) return 0;

    arg = &context->args[context->arg_count++];
    arg->name = node->lexeme;
    arg->type = node->inferred_type;
    arg->is_vector = node->inferred_is_vector;
    arg->vector_size = node->inferred_vector_size;
    return 1;
}

static int collect_args(MlirContext *context, const AstNode *node) {
    size_t i;

    if (node == NULL || context->memory_error) return !context->memory_error;

    if (node->kind == AST_IDENTIFIER)
        return add_arg(context, node);

    for (i = 0U; i < node->child_count; ++i)
        if (!collect_args(context, node->children[i]))
            return 0;

    return 1;
}

static void print_memref_type(FILE *out, const MlirArg *arg) {
    fprintf(out, "memref<%zux%s>",
            arg->vector_size, mlir_type(arg->type));
}

static const MlirArg *find_arg(const MlirContext *context,
                               const AstNode *node) {
    size_t i;

    if (context == NULL || node == NULL || node->lexeme == NULL)
        return NULL;

    for (i = 0U; i < context->arg_count; ++i)
        if (context->args[i].is_vector == node->inferred_is_vector &&
            strcmp(context->args[i].name, node->lexeme) == 0)
            return &context->args[i];

    return NULL;
}

static MlirValue convert_value(MlirContext *context,
                               const AstNode *node,
                               MlirValue value,
                               LangType target) {
    char temp[MLIR_NAME_CAPACITY];

    if (!value.valid || value.type == target) return value;

    new_value(context, temp);

    if (value.type == LANG_TYPE_INTEIRA && target == LANG_TYPE_FLUT) {
        fprintf(context->out,
                "      %s = arith.sitofp %s : i32 to f32\n",
                temp, value.name);
        return make_value(target, temp);
    }

    if (value.type == LANG_TYPE_INTEIRA &&
        target == LANG_TYPE_DUPLOCARPADO) {
        fprintf(context->out,
                "      %s = arith.sitofp %s : i32 to f64\n",
                temp, value.name);
        return make_value(target, temp);
    }

    if (value.type == LANG_TYPE_FLUT &&
        target == LANG_TYPE_DUPLOCARPADO) {
        fprintf(context->out,
                "      %s = arith.extf %s : f32 to f64\n",
                temp, value.name);
        return make_value(target, temp);
    }

    mlir_error(context, node,
               "conversao MLIR nao suportada de %s para %s",
               lang_type_name(value.type), lang_type_name(target));
    return invalid_value();
}

static MlirValue emit_element_expression(MlirContext *context,
                                         const AstNode *node,
                                         const char *index_name);

static MlirValue emit_identifier(MlirContext *context,
                                 const AstNode *node,
                                 const char *index_name) {
    const MlirArg *arg;
    char temp[MLIR_NAME_CAPACITY];

    arg = find_arg(context, node);
    if (arg == NULL) {
        mlir_error(context, node,
                   "operando '%s' nao foi registrado no kernel",
                   node->lexeme != NULL ? node->lexeme : "<sem-nome>");
        return invalid_value();
    }

    if (!arg->is_vector) {
        snprintf(temp, sizeof(temp), "%%s_%s", arg->name);
        return make_value(arg->type, temp);
    }

    new_value(context, temp);
    if (context->affine_loads) {
        fprintf(context->out,
                "      %s = affine.load %%v_%s[%s] : ",
                temp, arg->name, index_name);
    } else {
        fprintf(context->out,
                "      %s = memref.load %%v_%s[%s] : ",
                temp, arg->name, index_name);
    }
    print_memref_type(context->out, arg);
    fputc('\n', context->out);

    return make_value(arg->type, temp);
}

static MlirValue emit_literal(MlirContext *context,
                              const AstNode *node) {
    char temp[MLIR_NAME_CAPACITY];
    const char *type_name = mlir_type(node->inferred_type);

    if (type_name == NULL) {
        mlir_error(context, node,
                   "literal nao numerico em kernel MLIR");
        return invalid_value();
    }

    new_value(context, temp);
    fprintf(context->out,
            "      %s = arith.constant %s : %s\n",
            temp,
            node->lexeme != NULL ? node->lexeme : "0",
            type_name);
    return make_value(node->inferred_type, temp);
}

static MlirValue emit_binary(MlirContext *context,
                             const AstNode *node,
                             const char *index_name) {
    MlirValue left;
    MlirValue right;
    LangType result_type;
    const char *op = NULL;
    char temp[MLIR_NAME_CAPACITY];

    if (node == NULL || node->child_count < 2U)
        return invalid_value();

    left = emit_element_expression(context, node->children[0], index_name);
    right = emit_element_expression(context, node->children[1], index_name);
    if (!left.valid || !right.valid) return invalid_value();

    result_type = node->inferred_type;
    left = convert_value(context, node, left, result_type);
    right = convert_value(context, node, right, result_type);
    if (!left.valid || !right.valid) return invalid_value();

    if (result_type == LANG_TYPE_INTEIRA) {
        if (node->token_type == TOKEN_PLUS) op = "arith.addi";
        else if (node->token_type == TOKEN_MINUS) op = "arith.subi";
        else if (node->token_type == TOKEN_STAR) op = "arith.muli";
    } else {
        if (node->token_type == TOKEN_PLUS) op = "arith.addf";
        else if (node->token_type == TOKEN_MINUS) op = "arith.subf";
        else if (node->token_type == TOKEN_STAR) op = "arith.mulf";
    }

    if (op == NULL) {
        mlir_error(context, node,
                   "operador '%s' nao pertence ao subconjunto vetorial MLIR",
                   node->lexeme != NULL ? node->lexeme : "<desconhecido>");
        return invalid_value();
    }

    new_value(context, temp);
    fprintf(context->out,
            "      %s = %s %s, %s : %s\n",
            temp, op, left.name, right.name, mlir_type(result_type));
    return make_value(result_type, temp);
}

static MlirValue emit_element_expression(MlirContext *context,
                                         const AstNode *node,
                                         const char *index_name) {
    if (node == NULL) return invalid_value();

    switch (node->kind) {
        case AST_IDENTIFIER:
            return emit_identifier(context, node, index_name);
        case AST_INTEGER_LITERAL:
        case AST_REAL_LITERAL:
            return emit_literal(context, node);
        case AST_BINARY_EXPR:
            return emit_binary(context, node, index_name);
        default:
            mlir_error(context, node,
                       "no %s ainda nao pode compor kernel vetorial",
                       ast_node_kind_name(node->kind));
            return invalid_value();
    }
}

static void print_kernel_args(MlirContext *context,
                              const AstNode *target,
                              int include_output) {
    size_t i;
    int first = 1;

    for (i = 0U; i < context->arg_count; ++i) {
        const MlirArg *arg = &context->args[i];

        if (!first) fprintf(context->out, ", ");
        if (arg->is_vector) {
            fprintf(context->out, "%%v_%s: ", arg->name);
            print_memref_type(context->out, arg);
        } else {
            fprintf(context->out, "%%s_%s: %s",
                    arg->name, mlir_type(arg->type));
        }
        first = 0;
    }

    if (include_output) {
        if (!first) fprintf(context->out, ", ");
        fprintf(context->out, "%%out: memref<%zux%s>",
                target->inferred_vector_size,
                mlir_type(target->inferred_type));
    }
}

static int emit_elementwise_kernel(MlirContext *context,
                                   const AstNode *assignment) {
    const AstNode *target;
    const AstNode *expression;
    MlirValue result;
    size_t kernel_id;

    if (assignment == NULL || assignment->child_count < 2U)
        return 0;

    target = assignment->children[0];
    expression = assignment->children[1];

    context->arg_count = 0U;
    context->next_value = 0U;
    context->affine_loads = 1;

    if (!collect_args(context, expression)) return 0;

    kernel_id = context->kernel_count++;
    fprintf(context->out,
            "  // kernel %zu: atribuicao vetorial da linha %d\n",
            kernel_id, assignment->line);
    fprintf(context->out,
            "  func.func @zl_mlir_kernel_%zu_l%d(",
            kernel_id, assignment->line);
    print_kernel_args(context, target, 1);
    fprintf(context->out, ") {\n");
    fprintf(context->out,
            "    affine.for %%i = 0 to %zu {\n",
            target->inferred_vector_size);

    result = emit_element_expression(context, expression, "%i");
    if (!result.valid) return 0;
    result = convert_value(context, expression, result,
                           target->inferred_type);
    if (!result.valid) return 0;

    fprintf(context->out,
            "      affine.store %s, %%out[%%i] : memref<%zux%s>\n",
            result.name,
            target->inferred_vector_size,
            mlir_type(target->inferred_type));
    fprintf(context->out, "    }\n");
    fprintf(context->out, "    return\n");
    fprintf(context->out, "  }\n\n");
    return 1;
}

static MlirValue emit_zero(MlirContext *context, LangType type) {
    char temp[MLIR_NAME_CAPACITY];

    new_value(context, temp);
    if (type == LANG_TYPE_INTEIRA)
        fprintf(context->out,
                "    %s = arith.constant 0 : i32\n", temp);
    else
        fprintf(context->out,
                "    %s = arith.constant 0.0 : %s\n",
                temp, mlir_type(type));
    return make_value(type, temp);
}

static int emit_sum_kernel(MlirContext *context,
                           const AstNode *assignment,
                           const AstNode *reduction) {
    const AstNode *expression = reduction->children[0];
    LangType result_type = reduction->inferred_type;
    MlirValue zero;
    MlirValue element;
    MlirValue next;
    char add_name[MLIR_NAME_CAPACITY];
    size_t kernel_id;

    context->arg_count = 0U;
    context->next_value = 0U;
    context->affine_loads = 0;
    if (!collect_args(context, expression)) return 0;

    kernel_id = context->kernel_count++;
    fprintf(context->out,
            "  // kernel %zu: somavet da linha %d\n",
            kernel_id, assignment->line);
    fprintf(context->out,
            "  func.func @zl_mlir_kernel_%zu_l%d(",
            kernel_id, assignment->line);
    print_kernel_args(context, NULL, 0);
    fprintf(context->out, ") -> %s {\n", mlir_type(result_type));
    fprintf(context->out,
            "    %%c0 = arith.constant 0 : index\n"
            "    %%cN = arith.constant %zu : index\n"
            "    %%c1 = arith.constant 1 : index\n",
            reduction->children[0]->inferred_vector_size);

    zero = emit_zero(context, result_type);
    fprintf(context->out,
            "    %%result = scf.for %%i = %%c0 to %%cN step %%c1 "
            "iter_args(%%acc = %s) -> (%s) {\n",
            zero.name, mlir_type(result_type));

    element = emit_element_expression(context, expression, "%i");
    element = convert_value(context, expression, element, result_type);
    if (!element.valid) return 0;

    new_value(context, add_name);
    if (result_type == LANG_TYPE_INTEIRA)
        fprintf(context->out,
                "      %s = arith.addi %%acc, %s : i32\n",
                add_name, element.name);
    else
        fprintf(context->out,
                "      %s = arith.addf %%acc, %s : %s\n",
                add_name, element.name, mlir_type(result_type));
    next = make_value(result_type, add_name);

    fprintf(context->out,
            "      scf.yield %s : %s\n",
            next.name, mlir_type(result_type));
    fprintf(context->out, "    }\n");
    fprintf(context->out,
            "    return %%result : %s\n", mlir_type(result_type));
    fprintf(context->out, "  }\n\n");
    return 1;
}

static int emit_max_kernel(MlirContext *context,
                           const AstNode *assignment,
                           const AstNode *reduction) {
    const AstNode *expression = reduction->children[0];
    LangType result_type = reduction->inferred_type;
    MlirValue init;
    MlirValue element;
    char cmp[MLIR_NAME_CAPACITY];
    char selected[MLIR_NAME_CAPACITY];
    size_t kernel_id;
    size_t size = expression->inferred_vector_size;

    context->arg_count = 0U;
    context->next_value = 0U;
    context->affine_loads = 0;
    if (!collect_args(context, expression)) return 0;

    kernel_id = context->kernel_count++;
    fprintf(context->out,
            "  // kernel %zu: maxvet da linha %d\n",
            kernel_id, assignment->line);
    fprintf(context->out,
            "  func.func @zl_mlir_kernel_%zu_l%d(",
            kernel_id, assignment->line);
    print_kernel_args(context, NULL, 0);
    fprintf(context->out, ") -> %s {\n", mlir_type(result_type));
    fprintf(context->out,
            "    %%c0 = arith.constant 0 : index\n"
            "    %%c1 = arith.constant 1 : index\n"
            "    %%cN = arith.constant %zu : index\n",
            size);

    init = emit_element_expression(context, expression, "%c0");
    init = convert_value(context, expression, init, result_type);
    if (!init.valid) return 0;

    fprintf(context->out,
            "    %%result = scf.for %%i = %%c1 to %%cN step %%c1 "
            "iter_args(%%acc = %s) -> (%s) {\n",
            init.name, mlir_type(result_type));

    element = emit_element_expression(context, expression, "%i");
    element = convert_value(context, expression, element, result_type);
    if (!element.valid) return 0;

    new_value(context, cmp);
    if (result_type == LANG_TYPE_INTEIRA)
        fprintf(context->out,
                "      %s = arith.cmpi sgt, %s, %%acc : i32\n",
                cmp, element.name);
    else
        fprintf(context->out,
                "      %s = arith.cmpf ogt, %s, %%acc : %s\n",
                cmp, element.name, mlir_type(result_type));

    new_value(context, selected);
    fprintf(context->out,
            "      %s = arith.select %s, %s, %%acc : %s\n",
            selected, cmp, element.name, mlir_type(result_type));
    fprintf(context->out,
            "      scf.yield %s : %s\n",
            selected, mlir_type(result_type));
    fprintf(context->out, "    }\n");
    fprintf(context->out,
            "    return %%result : %s\n", mlir_type(result_type));
    fprintf(context->out, "  }\n\n");
    return 1;
}

static int emit_dot_kernel(MlirContext *context,
                           const AstNode *assignment,
                           const AstNode *reduction) {
    const AstNode *left_expr = reduction->children[0];
    const AstNode *right_expr = reduction->children[1];
    LangType result_type = reduction->inferred_type;
    MlirValue zero;
    MlirValue left;
    MlirValue right;
    char product[MLIR_NAME_CAPACITY];
    char sum[MLIR_NAME_CAPACITY];
    size_t kernel_id;
    size_t size = left_expr->inferred_vector_size;

    context->arg_count = 0U;
    context->next_value = 0U;
    context->affine_loads = 0;
    if (!collect_args(context, left_expr) ||
        !collect_args(context, right_expr))
        return 0;

    kernel_id = context->kernel_count++;
    fprintf(context->out,
            "  // kernel %zu: prodesc da linha %d\n",
            kernel_id, assignment->line);
    fprintf(context->out,
            "  func.func @zl_mlir_kernel_%zu_l%d(",
            kernel_id, assignment->line);
    print_kernel_args(context, NULL, 0);
    fprintf(context->out, ") -> %s {\n", mlir_type(result_type));
    fprintf(context->out,
            "    %%c0 = arith.constant 0 : index\n"
            "    %%cN = arith.constant %zu : index\n"
            "    %%c1 = arith.constant 1 : index\n",
            size);

    zero = emit_zero(context, result_type);
    fprintf(context->out,
            "    %%result = scf.for %%i = %%c0 to %%cN step %%c1 "
            "iter_args(%%acc = %s) -> (%s) {\n",
            zero.name, mlir_type(result_type));

    left = emit_element_expression(context, left_expr, "%i");
    right = emit_element_expression(context, right_expr, "%i");
    left = convert_value(context, left_expr, left, result_type);
    right = convert_value(context, right_expr, right, result_type);
    if (!left.valid || !right.valid) return 0;

    new_value(context, product);
    if (result_type == LANG_TYPE_INTEIRA)
        fprintf(context->out,
                "      %s = arith.muli %s, %s : i32\n",
                product, left.name, right.name);
    else
        fprintf(context->out,
                "      %s = arith.mulf %s, %s : %s\n",
                product, left.name, right.name, mlir_type(result_type));

    new_value(context, sum);
    if (result_type == LANG_TYPE_INTEIRA)
        fprintf(context->out,
                "      %s = arith.addi %%acc, %s : i32\n",
                sum, product);
    else
        fprintf(context->out,
                "      %s = arith.addf %%acc, %s : %s\n",
                sum, product, mlir_type(result_type));

    fprintf(context->out,
            "      scf.yield %s : %s\n",
            sum, mlir_type(result_type));
    fprintf(context->out, "    }\n");
    fprintf(context->out,
            "    return %%result : %s\n", mlir_type(result_type));
    fprintf(context->out, "  }\n\n");
    return 1;
}

static int emit_reduction_kernel(MlirContext *context,
                                 const AstNode *assignment,
                                 const AstNode *reduction) {
    switch (reduction->token_type) {
        case TOKEN_SOMAVET:
            return emit_sum_kernel(context, assignment, reduction);
        case TOKEN_MAXVET:
            return emit_max_kernel(context, assignment, reduction);
        case TOKEN_PRODESC:
            return emit_dot_kernel(context, assignment, reduction);
        default:
            mlir_error(context, reduction,
                       "reducao vetorial desconhecida");
            return 0;
    }
}

static int visit_node(MlirContext *context, const AstNode *node) {
    size_t i;

    if (node == NULL || context->memory_error) return !context->memory_error;

    if (node->kind == AST_ASSIGNMENT && node->child_count >= 2U) {
        const AstNode *target = node->children[0];
        const AstNode *value = node->children[1];

        if (target != NULL && target->inferred_is_vector &&
            value != NULL && value->inferred_is_vector) {
            if (!emit_elementwise_kernel(context, node)) return 0;
            return 1;
        }

        if (value != NULL && value->kind == AST_VECTOR_REDUCTION) {
            if (!emit_reduction_kernel(context, node, value)) return 0;
            return 1;
        }
    }

    for (i = 0U; i < node->child_count; ++i)
        if (!visit_node(context, node->children[i]))
            return 0;

    return 1;
}

CodegenMlirResult codegen_mlir_generate(const AstNode *ast,
                                        FILE *output,
                                        FILE *error_stream) {
    MlirContext context;
    CodegenMlirResult result;

    memset(&context, 0, sizeof(context));
    context.out = output;
    context.err = error_stream != NULL ? error_stream : stderr;

    if (ast == NULL || output == NULL) {
        result.status = CODEGEN_MLIR_HAS_ERRORS;
        result.error_count = 1U;
        result.kernel_count = 0U;
        fprintf(context.err,
                "[ERRO CODEGEN MLIR] AST/saida nao fornecida.\n");
        return result;
    }

    fprintf(output,
            "// MLIR gerado pela DSL vetorial Macaronica\n"
            "// estrategia hibrida: apenas kernels vetoriais\n"
            "module {\n\n");

    (void)visit_node(&context, ast);

    fprintf(output, "}\n");

    result.error_count = context.error_count;
    result.kernel_count = context.kernel_count;
    if (context.memory_error)
        result.status = CODEGEN_MLIR_MEMORY_ERROR;
    else if (context.error_count > 0U)
        result.status = CODEGEN_MLIR_HAS_ERRORS;
    else
        result.status = CODEGEN_MLIR_OK;

    free(context.args);
    return result;
}
