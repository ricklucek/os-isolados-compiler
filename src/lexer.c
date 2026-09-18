#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *source;
    size_t length;
    size_t current;
    int line;
    int column;
    size_t lexical_errors;
    TokenList *tokens;
    FILE *error_stream;
} Lexer;

typedef struct {
    const char *lexeme;
    TokenType type;
} Keyword;

static const Keyword KEYWORDS[] = {
    {"receba", TOKEN_RECEBA}, {"repete", TOKEN_REPETE},
    {"durante", TOKEN_DURANTE}, {"cond", TOKEN_COND},
    {"casocontrario", TOKEN_CASOCONTRARIO}, {"func", TOKEN_FUNC},
    {"bool", TOKEN_BOOL}, {"inteira", TOKEN_INTEIRA},
    {"flut", TOKEN_FLUT}, {"palavra", TOKEN_PALAVRA},
    {"duplocarpado", TOKEN_DUPLOCARPADO}, {"principal", TOKEN_PRINCIPAL},
    {"vazio", TOKEN_VAZIO}, {"respost", TOKEN_RESPOST},
    {"registro", TOKEN_REGISTRO}, {"outrafuncao", TOKEN_OUTRAFUNCAO},
    {"edai", TOKEN_EDAI}, {"vet", TOKEN_VET},
    {"VER", TOKEN_VER}, {"FAL", TOKEN_FAL}, {"OU", TOKEN_OU},
    {"E", TOKEN_E}, {"XOR", TOKEN_XOR}, {"raiz", TOKEN_RAIZ}
};

static int is_ascii_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int is_ascii_digit(char c) { return c >= '0' && c <= '9'; }
static int is_ascii_alnum(char c) { return is_ascii_alpha(c) || is_ascii_digit(c); }
static int lexer_is_at_end(const Lexer *lexer) { return lexer->current >= lexer->length; }

static char lexer_peek(const Lexer *lexer) {
    return lexer_is_at_end(lexer) ? '\0' : lexer->source[lexer->current];
}

static char lexer_peek_next(const Lexer *lexer) {
    return lexer->current + 1U >= lexer->length ? '\0' : lexer->source[lexer->current + 1U];
}

static char lexer_advance(Lexer *lexer) {
    char c = lexer->source[lexer->current++];
    if (c == '\n') {
        ++lexer->line;
        lexer->column = 1;
    } else {
        ++lexer->column;
    }
    return c;
}

static int lexer_add_token(Lexer *lexer, TokenType type, size_t start,
                           size_t length, int line, int column) {
    return token_list_append(lexer->tokens, type, lexer->source + start,
                             length, line, column);
}

static TokenType keyword_type(const char *start, size_t length) {
    size_t i;
    for (i = 0U; i < sizeof(KEYWORDS) / sizeof(KEYWORDS[0]); ++i) {
        size_t keyword_length = strlen(KEYWORDS[i].lexeme);
        if (keyword_length == length && memcmp(start, KEYWORDS[i].lexeme, length) == 0)
            return KEYWORDS[i].type;
    }
    return TOKEN_IDENTIFIER;
}

static int starts_with_nao(const Lexer *lexer) {
    static const char NAO_UTF8[] = "NÃO";
    const size_t length = sizeof(NAO_UTF8) - 1U;
    size_t after;
    if (lexer->current + length > lexer->length) return 0;
    if (memcmp(lexer->source + lexer->current, NAO_UTF8, length) != 0) return 0;
    after = lexer->current + length;
    return after == lexer->length || !is_ascii_alnum(lexer->source[after]);
}

static int scan_nao(Lexer *lexer) {
    static const char NAO_UTF8[] = "NÃO";
    const size_t byte_length = sizeof(NAO_UTF8) - 1U;
    size_t start = lexer->current;
    int line = lexer->line, column = lexer->column;
    lexer->current += byte_length;
    lexer->column += 3;
    return lexer_add_token(lexer, TOKEN_NAO, start, byte_length, line, column);
}

static int scan_identifier(Lexer *lexer) {
    size_t start = lexer->current;
    int line = lexer->line, column = lexer->column;
    TokenType type;
    lexer_advance(lexer);
    while (is_ascii_alnum(lexer_peek(lexer))) lexer_advance(lexer);
    type = keyword_type(lexer->source + start, lexer->current - start);
    return lexer_add_token(lexer, type, start, lexer->current - start, line, column);
}

static int scan_number(Lexer *lexer) {
    size_t start = lexer->current;
    int line = lexer->line, column = lexer->column;
    TokenType type = TOKEN_INTEGER_LITERAL;
    while (is_ascii_digit(lexer_peek(lexer))) lexer_advance(lexer);
    if (lexer_peek(lexer) == '.' && is_ascii_digit(lexer_peek_next(lexer))) {
        type = TOKEN_REAL_LITERAL;
        lexer_advance(lexer);
        while (is_ascii_digit(lexer_peek(lexer))) lexer_advance(lexer);
    }
    return lexer_add_token(lexer, type, start, lexer->current - start, line, column);
}

static int scan_string(Lexer *lexer) {
    size_t start = lexer->current;
    int line = lexer->line, column = lexer->column;

    lexer_advance(lexer);

    while (!lexer_is_at_end(lexer) && lexer_peek(lexer) != '"' &&
           lexer_peek(lexer) != '\n' && lexer_peek(lexer) != '\r') {
        lexer_advance(lexer);
    }

    if (lexer_is_at_end(lexer) || lexer_peek(lexer) == '\n' ||
        lexer_peek(lexer) == '\r') {
        fprintf(lexer->error_stream,
                "[ERRO LEXICO] linha %d, coluna %d: literal de palavra nao terminado; esperado fechamento com aspas duplas.\n",
                line, column);
        ++lexer->lexical_errors;
        return lexer_add_token(lexer, TOKEN_INVALID, start,
                               lexer->current - start, line, column);
    }

    lexer_advance(lexer);
    return lexer_add_token(lexer, TOKEN_STRING_LITERAL, start,
                           lexer->current - start, line, column);
}

static void report_invalid_char(Lexer *lexer, unsigned char c, int line, int column) {
    if (c >= 32U && c <= 126U)
        fprintf(lexer->error_stream,
                "[ERRO LEXICO] linha %d, coluna %d: caractere '%c' nao pertence aos tokens definidos da Macaronica.\n",
                line, column, c);
    else
        fprintf(lexer->error_stream,
                "[ERRO LEXICO] linha %d, coluna %d: byte invalido 0x%02X na entrada.\n",
                line, column, (unsigned int)c);
}

static int scan_symbol(Lexer *lexer) {
    size_t start = lexer->current;
    int line = lexer->line, column = lexer->column;
    char c = lexer_advance(lexer);
    TokenType type;

    switch (c) {
        case '+': type = TOKEN_PLUS; break;
        case '-': type = TOKEN_MINUS; break;
        case '*': type = TOKEN_STAR; break;
        case '^': type = TOKEN_CARET; break;
        case '<': type = TOKEN_LESS; break;
        case '>': type = TOKEN_GREATER; break;
        case '!': type = TOKEN_BANG; break;
        case '[': type = TOKEN_LEFT_BRACKET; break;
        case ']': type = TOKEN_RIGHT_BRACKET; break;
        case '{': type = TOKEN_LEFT_BRACE; break;
        case '}': type = TOKEN_RIGHT_BRACE; break;
        case '(': type = TOKEN_LEFT_PAREN; break;
        case ')': type = TOKEN_RIGHT_PAREN; break;
        case ';': type = TOKEN_SEMICOLON; break;
        case ',': type = TOKEN_COMMA; break;
        case '@': type = TOKEN_AT; break;
        case '/':
            if (lexer_peek(lexer) == '/') {
                lexer_advance(lexer);
                type = TOKEN_INTEGER_DIV;
            } else type = TOKEN_SLASH;
            break;
        case '=':
            if (lexer_peek(lexer) == '=') {
                lexer_advance(lexer);
                type = TOKEN_EQUAL_EQUAL;
            } else {
                fprintf(lexer->error_stream,
                        "[ERRO LEXICO] linha %d, coluna %d: '=' isolado nao e operador da Macaronica; a comparacao definida e '=='.\n",
                        line, column);
                ++lexer->lexical_errors;
                type = TOKEN_INVALID;
            }
            break;
        default:
            report_invalid_char(lexer, (unsigned char)c, line, column);
            ++lexer->lexical_errors;
            type = TOKEN_INVALID;
            break;
    }
    return lexer_add_token(lexer, type, start, lexer->current - start, line, column);
}

static LexerStatus scan_source(const char *source, size_t length,
                               TokenList *tokens, FILE *error_stream) {
    Lexer lexer = {source, length, 0U, 1, 1, 0U, tokens, error_stream};

    while (!lexer_is_at_end(&lexer)) {
        char c = lexer_peek(&lexer);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            lexer_advance(&lexer);
            continue;
        }
        if (c == '"') {
            if (!scan_string(&lexer)) return LEXER_MEMORY_ERROR;
            continue;
        }
        if (starts_with_nao(&lexer)) {
            if (!scan_nao(&lexer)) return LEXER_MEMORY_ERROR;
            continue;
        }
        if (is_ascii_alpha(c)) {
            if (!scan_identifier(&lexer)) return LEXER_MEMORY_ERROR;
            continue;
        }
        if (is_ascii_digit(c)) {
            if (!scan_number(&lexer)) return LEXER_MEMORY_ERROR;
            continue;
        }
        if (!scan_symbol(&lexer)) return LEXER_MEMORY_ERROR;
    }

    if (!token_list_append(tokens, TOKEN_EOF, "", 0U, lexer.line, lexer.column))
        return LEXER_MEMORY_ERROR;
    return lexer.lexical_errors == 0U ? LEXER_OK : LEXER_HAS_ERRORS;
}

LexerStatus lexer_scan_file(const char *path, TokenList *tokens, FILE *error_stream) {
    FILE *file;
    long file_size;
    size_t bytes_read;
    char *buffer;
    LexerStatus status;

    if (path == NULL || tokens == NULL) return LEXER_IO_ERROR;
    if (error_stream == NULL) error_stream = stderr;

    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(error_stream, "Erro: nao foi possivel abrir '%s'.\n", path);
        return LEXER_IO_ERROR;
    }
    if (fseek(file, 0L, SEEK_END) != 0 || (file_size = ftell(file)) < 0L) {
        fprintf(error_stream, "Erro: nao foi possivel medir '%s'.\n", path);
        fclose(file);
        return LEXER_IO_ERROR;
    }
    rewind(file);

    buffer = malloc((size_t)file_size + 1U);
    if (buffer == NULL) {
        fprintf(error_stream, "Erro: memoria insuficiente para ler '%s'.\n", path);
        fclose(file);
        return LEXER_MEMORY_ERROR;
    }
    bytes_read = fread(buffer, 1U, (size_t)file_size, file);
    if (bytes_read != (size_t)file_size && ferror(file)) {
        fprintf(error_stream, "Erro: falha durante a leitura de '%s'.\n", path);
        free(buffer);
        fclose(file);
        return LEXER_IO_ERROR;
    }
    buffer[bytes_read] = '\0';
    fclose(file);
    status = scan_source(buffer, bytes_read, tokens, error_stream);
    free(buffer);
    return status;
}
