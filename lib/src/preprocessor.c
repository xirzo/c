#include "preprocessor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include "utils.h"
#include "stb_ds.h"

static String PreprocessorProcessFile(const String *content,
                                      const IncludePathList *include_paths,
                                      MacroTable *macro_table, int depth);

static bool StringEqualsCStr(const String *s, const char *cstr);
static bool StringEqualsCstrLen(const char *a, size_t a_len, const char *b);

static bool StringEqualsCStr(const String *s, const char *cstr) {
    size_t len = strlen(cstr);
    if (s->length != len) return false;
    return memcmp(s->data, cstr, len) == 0;
}

static String PreprocessorSpliceLines(const String *content);

static int64_t EvalExpression(const String *expr, size_t *pos,
                              const MacroTable *macro_table);

static void SkipWhitespace(const String *expr, size_t *pos) {
    while (*pos < expr->length &&
           (expr->data[*pos] == ' ' || expr->data[*pos] == '\t'))
        (*pos)++;
}

static int64_t ParseNumber(const String *expr, size_t *pos) {
    int base = 10;
    int64_t value = 0;
    if (*pos + 2 < expr->length && expr->data[*pos] == '0' &&
        (expr->data[*pos + 1] == 'x' || expr->data[*pos + 1] == 'X')) {
        base = 16;
        *pos += 2;
    } else if (*pos + 1 < expr->length && expr->data[*pos] == '0') {
        base = 8;
        (*pos)++;
    }
    while (*pos < expr->length) {
        char c = expr->data[*pos];
        int digit = -1;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (base == 16 && c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else if (base == 16 && c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        if (digit < 0 || digit >= base)
            break;
        value = value * base + digit;
        (*pos)++;
        if (base == 8 && digit >= 8) {
            (*pos)--;
            break;
        }
    }
    return value;
}

static void SkipIdentifier(const String *expr, size_t *pos) {
    while (*pos < expr->length &&
           (isalnum(expr->data[*pos]) || expr->data[*pos] == '_'))
        (*pos)++;
}

static int64_t EvalPrimary(const String *expr, size_t *pos,
                           const MacroTable *macro_table) {
    SkipWhitespace(expr, pos);
    if (*pos >= expr->length)
        return 0;

    char c = expr->data[*pos];

    if (c == '(') {
        (*pos)++;
        int64_t val = EvalExpression(expr, pos, macro_table);
        SkipWhitespace(expr, pos);
        if (*pos < expr->length && expr->data[*pos] == ')')
            (*pos)++;
        return val;
    }

    if (c == '!') {
        (*pos)++;
        return !EvalPrimary(expr, pos, macro_table);
    }

    if (c == '~') {
        (*pos)++;
        return ~EvalPrimary(expr, pos, macro_table);
    }

    if (c == '+' && *pos + 1 < expr->length && expr->data[*pos + 1] == '+') {
        // preprocessor never sees ++, but skip for safety
        (*pos) += 2;
        return EvalPrimary(expr, pos, macro_table);
    }

    if (c == '-' && *pos + 1 < expr->length && expr->data[*pos + 1] == '-') {
        (*pos) += 2;
        return -EvalPrimary(expr, pos, macro_table);
    }

    if (c == '+' && !isdigit(expr->data[*pos + 1])) {
        (*pos)++;
        return EvalPrimary(expr, pos, macro_table);
    }

    if (c == '-' && !isdigit(expr->data[*pos + 1])) {
        (*pos)++;
        return -EvalPrimary(expr, pos, macro_table);
    }

    if (isdigit(c))
        return ParseNumber(expr, pos);

    if (isalpha(c) || c == '_') {
        size_t start = *pos;
        SkipIdentifier(expr, pos);
        String id = StringSubstring(expr, start, *pos - start);

        // Check for defined keyword
        if (StringEqualsCStr(&id, "defined")) {
            SkipWhitespace(expr, pos);
            bool has_paren = (*pos < expr->length && expr->data[*pos] == '(');
            if (has_paren)
                (*pos)++;
            SkipWhitespace(expr, pos);
            size_t macro_start = *pos;
            SkipIdentifier(expr, pos);
            String macro_name =
                StringSubstring(expr, macro_start, *pos - macro_start);
            int idx = MacroTableFind((MacroTable *)macro_table, &macro_name);
            if (has_paren) {
                SkipWhitespace(expr, pos);
                if (*pos < expr->length && expr->data[*pos] == ')')
                    (*pos)++;
            }
            StringFree(&macro_name);
            StringFree(&id);
            return (idx >= 0) ? 1 : 0;
        }

        // Identifier — look up in macro table
        int idx = MacroTableFind((MacroTable *)macro_table, &id);
        int64_t result;
        if (idx >= 0) {
            String expanded = MacroTableExpand((MacroTable *)macro_table,
                                                &macro_table->macros[idx].value);
            result = PreprocessorEvaluateIf(&expanded, macro_table);
            StringFree(&expanded);
        } else {
            result = 0;
        }
        StringFree(&id);
        return result;
    }

    return 0;
}

static int64_t EvalRelational(const String *expr, size_t *pos,
                              const MacroTable *macro_table) {
    int64_t left = EvalPrimary(expr, pos, macro_table);
    SkipWhitespace(expr, pos);

    while (*pos < expr->length) {
        size_t start = *pos;
        char c = expr->data[*pos];
        int op = 0; // 1:< 2:> 3:<= 4:>=

        if (c == '<') {
            if (*pos + 1 < expr->length && expr->data[*pos + 1] == '=') {
                op = 3;
                *pos += 2;
            } else {
                op = 1;
                (*pos)++;
            }
        } else if (c == '>') {
            if (*pos + 1 < expr->length && expr->data[*pos + 1] == '=') {
                op = 4;
                *pos += 2;
            } else {
                op = 2;
                (*pos)++;
            }
        }

        if (op == 0)
            break;

        int64_t right = EvalPrimary(expr, pos, macro_table);
        switch (op) {
            case 1: left = left < right ? 1 : 0; break;
            case 2: left = left > right ? 1 : 0; break;
            case 3: left = left <= right ? 1 : 0; break;
            case 4: left = left >= right ? 1 : 0; break;
        }
        SkipWhitespace(expr, pos);
    }

    return left;
}

static int64_t EvalEquality(const String *expr, size_t *pos,
                            const MacroTable *macro_table) {
    int64_t left = EvalRelational(expr, pos, macro_table);
    SkipWhitespace(expr, pos);

    while (*pos < expr->length) {
        int op = 0; // 1:== 2:!=
        if (expr->data[*pos] == '=' && *pos + 1 < expr->length &&
            expr->data[*pos + 1] == '=') {
            op = 1;
            *pos += 2;
        } else if (expr->data[*pos] == '!' && *pos + 1 < expr->length &&
                   expr->data[*pos + 1] == '=') {
            op = 2;
            *pos += 2;
        }
        if (op == 0)
            break;

        int64_t right = EvalRelational(expr, pos, macro_table);
        switch (op) {
            case 1: left = (left == right) ? 1 : 0; break;
            case 2: left = (left != right) ? 1 : 0; break;
        }
        SkipWhitespace(expr, pos);
    }

    return left;
}

static int64_t EvalLogicalAnd(const String *expr, size_t *pos,
                              const MacroTable *macro_table) {
    int64_t left = EvalEquality(expr, pos, macro_table);
    SkipWhitespace(expr, pos);

    while (*pos + 1 < expr->length && expr->data[*pos] == '&' &&
           expr->data[*pos + 1] == '&') {
        *pos += 2;
        if (!left) {
            // short-circuit: need to parse RHS for position tracking only
            // but we don't need it — skip to next && or end
            size_t saved = *pos;
            EvalEquality(expr, pos, macro_table);
            SkipWhitespace(expr, pos);
            if (*pos + 1 < expr->length && expr->data[*pos] == '&' &&
                expr->data[*pos + 1] == '&') {
                // more &&, reset and continue from here
                *pos = saved;
            }
            left = 0;
        } else {
            int64_t right = EvalEquality(expr, pos, macro_table);
            left = left && right;
        }
        SkipWhitespace(expr, pos);
    }

    return left;
}

static int64_t EvalBitwiseOr(const String *expr, size_t *pos,
                             const MacroTable *macro_table) {
    int64_t left = EvalLogicalAnd(expr, pos, macro_table);
    SkipWhitespace(expr, pos);

    while (*pos + 1 < expr->length && expr->data[*pos] == '|' &&
           expr->data[*pos + 1] == '|') {
        *pos += 2;
        if (left) {
            size_t saved = *pos;
            EvalLogicalAnd(expr, pos, macro_table);
            SkipWhitespace(expr, pos);
            if (*pos + 1 < expr->length && expr->data[*pos] == '|' &&
                expr->data[*pos + 1] == '|') {
                *pos = saved;
            }
            left = 1;
        } else {
            int64_t right = EvalLogicalAnd(expr, pos, macro_table);
            left = left || right;
        }
        SkipWhitespace(expr, pos);
    }

    return left;
}

static int64_t EvalLogOr(const String *expr, size_t *pos,
                         const MacroTable *macro_table) {
    int64_t left = EvalBitwiseOr(expr, pos, macro_table);
    SkipWhitespace(expr, pos);

    while (*pos < expr->length && expr->data[*pos] == '|') {
        size_t next = *pos + 1;
        if (next < expr->length && expr->data[next] == '|') {
            // Already handled by EvalLogicalAnd via || short-circuit
            break;
        }
        // Bitwise OR |
        (*pos)++;
        int64_t right = EvalBitwiseOr(expr, pos, macro_table);
        left = left | right;
        SkipWhitespace(expr, pos);
    }

    return left;
}

static int64_t EvalExpression(const String *expr, size_t *pos,
                              const MacroTable *macro_table) {
    return EvalLogOr(expr, pos, macro_table);
}

int64_t PreprocessorEvaluateIf(const String *expr,
                                const MacroTable *macro_table) {
    size_t pos = 0;
    return EvalExpression(expr, &pos, macro_table);
}

static String ReadFile(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return StringCreateEmpty(0);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(file);
        return StringCreateEmpty(0);
    }

    String result = StringCreateEmpty(file_size + 1);
    if (!result.data) {
        fclose(file);
        return result;
    }

    size_t read = fread(result.data, 1, file_size, file);
    result.length = read;
    result.data[read] = '\0';

    fclose(file);
    return result;
}

static bool FileExists(const char *path) {
    FILE *file = fopen(path, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

static String FindIncludeFile(const String *include_path,
                              const IncludePathList *search_paths) {
    if (FileExists(include_path->data)) {
        return ReadFile(include_path->data);
    }

    for (size_t i = 0; i < search_paths->count; i++) {
        String *path = search_paths->paths[i];
        String full_path =
            StringCreateEmpty(path->length + include_path->length + 2);
        if (!full_path.data)
            continue;

        StringAppend(&full_path, path);
        if (StringGetChar(&full_path, full_path.length - 1) != '/') {
            StringAppendChar(&full_path, '/');
        }
        StringAppend(&full_path, include_path);

        if (FileExists(full_path.data)) {
            String content = ReadFile(full_path.data);
            StringFree(&full_path);
            return content;
        }

        StringFree(&full_path);
    }

    return StringCreateEmpty(0);
}

static String ExtractIncludePath(const String *line, bool *is_system) {
    String line_copy = StringDuplicate(line);
    StringTrim(&line_copy);

    size_t i = 0;
    while (i < line_copy.length && StringGetChar(&line_copy, i) != '#') {
        i++;
    }
    if (i >= line_copy.length) {
        StringFree(&line_copy);
        return StringCreateEmpty(0);
    }
    i++;

    while (i < line_copy.length &&
           (StringGetChar(&line_copy, i) == ' ' ||
            StringGetChar(&line_copy, i) == '\t')) {
        i++;
    }

    const char *include_str = "include";
    size_t j = 0;
    while (j < 7 && i + j < line_copy.length &&
           StringGetChar(&line_copy, i + j) == include_str[j]) {
        j++;
    }
    if (j < 7) {
        StringFree(&line_copy);
        return StringCreateEmpty(0);
    }
    i += 7;

    while (i < line_copy.length &&
           (StringGetChar(&line_copy, i) == ' ' ||
            StringGetChar(&line_copy, i) == '\t')) {
        i++;
    }

    if (i >= line_copy.length) {
        StringFree(&line_copy);
        return StringCreateEmpty(0);
    }

    char first = StringGetChar(&line_copy, i);
    String result = StringCreateEmpty(0);

    if (first == '<') {
        *is_system = true;
        i++;
        size_t start = i;
        while (i < line_copy.length &&
               StringGetChar(&line_copy, i) != '>') {
            i++;
        }
        StringFree(&result);
        result = StringSubstring(&line_copy, start, i - start);
    } else if (first == '"') {
        i++;
        size_t start = i;
        while (i < line_copy.length &&
               StringGetChar(&line_copy, i) != '"') {
            i++;
        }
        StringFree(&result);
        result = StringSubstring(&line_copy, start, i - start);
    }

    StringFree(&line_copy);
    return result;
}

static bool IsIncludeDirective(const String *line) {
    String line_copy = StringDuplicate(line);
    StringTrim(&line_copy);

    bool result = false;
    if (line_copy.length >= 8) {
        result = StringGetChar(&line_copy, 0) == '#' &&
                 StringGetChar(&line_copy, 1) == 'i' &&
                 StringGetChar(&line_copy, 2) == 'n' &&
                 StringGetChar(&line_copy, 3) == 'c' &&
                 StringGetChar(&line_copy, 4) == 'l' &&
                 StringGetChar(&line_copy, 5) == 'u' &&
                 StringGetChar(&line_copy, 6) == 'd' &&
                 StringGetChar(&line_copy, 7) == 'e';
    }

    StringFree(&line_copy);
    return result;
}

String PreprocessorHandleIncludes(const String *content,
                                  const IncludePathList *include_paths) {
    MacroTable table = MacroTableCreate();
    String result = PreprocessorProcessFile(content, include_paths, &table, 0);
    MacroTableFree(&table);
    return result;
}

static void ParseDefineLine(const String *line, size_t dir_end,
                            String *name, String *value) {
    size_t i = dir_end;
    while (i < line->length &&
           (line->data[i] == ' ' || line->data[i] == '\t'))
        i++;

    size_t name_start = i;
    while (i < line->length &&
           (isalnum(line->data[i]) || line->data[i] == '_'))
        i++;

    *name = StringSubstring(line, name_start, i - name_start);

    while (i < line->length &&
           (line->data[i] == ' ' || line->data[i] == '\t'))
        i++;

    *value = StringSubstring(line, i, line->length - i);
}

static int64_t EvaluateIfDirective(const String *line, size_t dir_end,
                                   const MacroTable *macro_table) {
    size_t i = dir_end;
    while (i < line->length &&
           (line->data[i] == ' ' || line->data[i] == '\t'))
        i++;

    String condition = StringSubstring(line, i, line->length - i);
    int64_t result = PreprocessorEvaluateIf(&condition, macro_table);
    StringFree(&condition);
    return result;
}

static bool EvaluateIfdefDirective(const String *line, size_t dir_end,
                                   const MacroTable *macro_table) {
    size_t i = dir_end;
    while (i < line->length &&
           (line->data[i] == ' ' || line->data[i] == '\t'))
        i++;

    size_t start = i;
    while (i < line->length &&
           (isalnum(line->data[i]) || line->data[i] == '_'))
        i++;

    String name = StringSubstring(line, start, i - start);
    int idx = MacroTableFind((MacroTable *)macro_table, &name);
    StringFree(&name);
    return idx >= 0;
}

static void SetupPredefinedMacros(MacroTable *table) {
    MacroTableAdd(table, "__STDC__", "1");
    MacroTableAdd(table, "__STDC_HOSTED__", "1");
    MacroTableAdd(table, "__GNUC__", "4");
    MacroTableAdd(table, "__GNUC_MINOR__", "9");
    MacroTableAdd(table, "__GNUC_PATCHLEVEL__", "0");
    MacroTableAdd(table, "__linux__", "1");
    MacroTableAdd(table, "__unix__", "1");
    MacroTableAdd(table, "__x86_64__", "1");
    MacroTableAdd(table, "__LP64__", "1");
    MacroTableAdd(table, "__ELF__", "1");
    MacroTableAdd(table, "__INT_MAX__", "2147483647");
    MacroTableAdd(table, "__LONG_MAX__", "9223372036854775807");
    MacroTableAdd(table, "__SIZE_MAX__", "18446744073709551615");
    MacroTableAdd(table, "__PTRDIFF_MAX__", "9223372036854775807");
    MacroTableAdd(table, "__INCLUDE_LEVEL__", "0");

    MacroTableAdd(table, "__extension__", "");
    MacroTableAdd(table, "__inline", "");
    MacroTableAdd(table, "__inline__", "");
    MacroTableAdd(table, "__const", "");
    MacroTableAdd(table, "__const__", "");
    MacroTableAdd(table, "__volatile", "");
    MacroTableAdd(table, "__volatile__", "");
    MacroTableAdd(table, "__restrict", "");
    MacroTableAdd(table, "__restrict__", "");
    MacroTableAdd(table, "__attribute__", "");
    MacroTableAdd(table, "__attribute_deprecated__", "");
    MacroTableAdd(table, "__attribute_format_arg__", "");
    MacroTableAdd(table, "__attribute_malloc__", "");
    MacroTableAdd(table, "__attribute_pure__", "");
    MacroTableAdd(table, "__attribute_warn_unused_result__", "");
    MacroTableAdd(table, "__attribute_alloc_size__", "");
    MacroTableAdd(table, "__attribute_artificial__", "");
    MacroTableAdd(table, "__attribute_used__", "");
    MacroTableAdd(table, "__attribute_noinline__", "");
    MacroTableAdd(table, "__attribute_const__", "");
    MacroTableAdd(table, "__attribute_deprecated_msg__", "");
    MacroTableAdd(table, "__wur", "");
    MacroTableAdd(table, "__THROW", "");
    MacroTableAdd(table, "__THROWNL", "");
    MacroTableAdd(table, "__NTH", "");
    MacroTableAdd(table, "__NTHNL", "");
    MacroTableAdd(table, "__nonnull", "");
    MacroTableAdd(table, "__nonnull__", "");
    MacroTableAdd(table, "__returns_nonnull", "");
    MacroTableAdd(table, "__returns_nonnull__", "");
    MacroTableAdd(table, "__extern_inline", "");
    MacroTableAdd(table, "__header_inline", "");
    MacroTableAdd(table, "__BEGIN_DECLS", "");
    MacroTableAdd(table, "__END_DECLS", "");
    MacroTableAdd(table, "__ASMNAME", "");
    MacroTableAdd(table, "__asm__", "");
    MacroTableAdd(table, "__asm", "");
    MacroTableAdd(table, "__typeof__", "");
    MacroTableAdd(table, "__typeof", "");
    MacroTableAdd(table, "__alignof__", "1");
    MacroTableAdd(table, "__alignof", "1");
    MacroTableAdd(table, "__builtin_va_list", "void*");
    MacroTableAdd(table, "__builtin_va_start", "");
    MacroTableAdd(table, "__builtin_va_arg", "");
    MacroTableAdd(table, "__builtin_va_end", "");
    MacroTableAdd(table, "__builtin_offsetof", "0");
    MacroTableAdd(table, "__integer_constant", "");
    MacroTableAdd(table, "__glibc_macro_warning", "");
    MacroTableAdd(table, "__glibc_macro_warning__", "");
}

int MacroTableFind(MacroTable *table, const String *name) {
    if (!table || !name)
        return -1;
    for (size_t i = 0; i < table->count; i++) {
        if (StringEquals(name, &table->macros[i].name))
            return (int)i;
    }
    return -1;
}

bool MacroTableRemove(MacroTable *table, const String *name) {
    int idx = MacroTableFind(table, name);
    if (idx < 0)
        return false;
    StringFree(&table->macros[idx].name);
    StringFree(&table->macros[idx].value);
    for (size_t i = (size_t)idx; i < table->count - 1; i++) {
        table->macros[i] = table->macros[i + 1];
    }
    table->count--;
    return true;
}

static const char *DirectiveName(const String *line, size_t hash_pos,
                                 size_t *dir_end) {
    size_t i = hash_pos + 1;
    while (i < line->length &&
           (line->data[i] == ' ' || line->data[i] == '\t'))
        i++;

    size_t start = i;
    while (i < line->length && isalpha(line->data[i]))
        i++;

    *dir_end = i;

    // null-terminate substring by returning offset into source
    // Instead, just use string compare with length
    // Return a pointer to the start of the name within the line
    // But we can't null-terminate easily. Let me use a different approach.
    // Do the comparison in the caller using StringSubstring.
    // To avoid allocation, the caller can use line->data + start with length.
    // We'll return a static string for known directives.
    // Actually, just return the data pointer, caller uses length.
    return NULL; // not used
}

// Small helper: extract the directive name from a line starting at hash_pos
// Returns the name by writing to *name_start and *name_len
// Returns the position after the directive name in *dir_end
static void GetDirectiveInfo(const String *line, size_t hash_pos,
                             size_t *name_start, size_t *name_len,
                             size_t *dir_end) {
    size_t i = hash_pos + 1;
    while (i < line->length &&
           (line->data[i] == ' ' || line->data[i] == '\t'))
        i++;
    *name_start = i;
    while (i < line->length && isalpha(line->data[i]))
        i++;
    *name_len = i - *name_start;
    *dir_end = i;
}

static bool StringEqualsCstrLen(const char *a, size_t a_len, const char *b) {
    size_t b_len = strlen(b);
    if (a_len != b_len)
        return false;
    return memcmp(a, b, b_len) == 0;
}



static bool IsOnIfStack(PreprocessorIfBranchState *stack) {
    return arrlen(stack) > 0;
}

static bool IsStackActive(PreprocessorIfBranchState *stack) {
    return arrlen(stack) == 0 ||
           stack[arrlen(stack) - 1] == PREPROC_IF_ACTIVE;
}

String PreprocessorProcessWithIncludes(const String *content,
                                        const char *include_paths[]) {
    if (!content || content->length == 0) {
        return StringCreateEmpty(0);
    }

    IncludePathList paths = IncludePathListCreate();

    IncludePathListAdd(&paths, ".");

    if (include_paths) {
        for (size_t i = 0; include_paths[i] != NULL; i++) {
            IncludePathListAdd(&paths, include_paths[i]);
        }
    }

    MacroTable macro_table = MacroTableCreate();
    SetupPredefinedMacros(&macro_table);

    String result =
        PreprocessorProcessFile(content, &paths, &macro_table, 0);

    MacroTableFree(&macro_table);
    IncludePathListFree(&paths);

    return result;
}

static bool IsEndOfLine(char c) {
    return c == '\n' || c == '\r';
}

static bool IsWhitespaceOrTab(char c) {
    return c == ' ' || c == '\t';
}

static String PreprocessorProcessFile(const String *content,
                                      const IncludePathList *include_paths,
                                      MacroTable *macro_table, int depth) {
    if (depth > 64) {
        fprintf(stderr,
                "ERROR: Maximum include depth exceeded (possible circular "
                "includes)\n");
        return StringCreateEmpty(0);
    }

    if (!content || content->length == 0) {
        return StringCreateEmpty(0);
    }

    String spliced = PreprocessorSpliceLines(content);
    if (!spliced.data) {
        return spliced;
    }

    String no_comments = PreprocessorStripComments(&spliced);
    StringFree(&spliced);
    if (!no_comments.data) {
        return no_comments;
    }

    String result = StringCreateEmpty(no_comments.length * 2);
    if (!result.data) {
        StringFree(&no_comments);
        return result;
    }

    PreprocessorIfBranchState *if_stack = NULL;

    size_t line_start = 0;
    for (size_t i = 0; i <= no_comments.length; i++) {
        char c =
            (i < no_comments.length) ? StringGetChar(&no_comments, i) : '\n';

        if (!IsEndOfLine(c) && i < no_comments.length)
            continue;

        String line =
            StringSubstring(&no_comments, line_start, i - line_start);

        // Trim leading whitespace for directive detection
        size_t first = 0;
        while (first < line.length && IsWhitespaceOrTab(line.data[first]))
            first++;

        bool is_skipping = arrlen(if_stack) > 0 &&
                           if_stack[arrlen(if_stack) - 1] != PREPROC_IF_ACTIVE;

        if (first < line.length && line.data[first] == '#') {
            size_t name_start, name_len, dir_end;
            GetDirectiveInfo(&line, first, &name_start, &name_len, &dir_end);

            if (name_len > 0) {
                const char *name_ptr = line.data + name_start;

                if (StringEqualsCstrLen(name_ptr, name_len, "include")) {
                    if (!is_skipping) {
                        bool is_system = false;
                        String include_path =
                            ExtractIncludePath(&line, &is_system);

                        if (include_path.data && include_path.length > 0) {
                            String included_content =
                                FindIncludeFile(&include_path, include_paths);

                            if (included_content.data &&
                                included_content.length > 0) {
                                String processed_included =
                                    PreprocessorProcessFile(
                                        &included_content, include_paths,
                                        macro_table, depth + 1);
                                StringAppend(&result, &processed_included);
                                StringFree(&processed_included);

                                if (result.length > 0 &&
                                    StringGetChar(&result, result.length - 1) !=
                                        '\n') {
                                    StringAppendChar(&result, '\n');
                                }
                            } else {
                                LOG_DEBUG(
                                    "Warning: Include file not found: %s\n",
                                    include_path.data
                                        ? include_path.data
                                        : "(null)");
                            }

                            StringFree(&included_content);
                        }
                        StringFree(&include_path);
                    }

                } else if (StringEqualsCstrLen(name_ptr, name_len, "define")) {
                    if (!is_skipping) {
                        String name, value;
                        ParseDefineLine(&line, dir_end, &name, &value);
                        if (name.data && name.length > 0) {
                            // Expand macros in the value
                            String expanded =
                                MacroTableExpand(macro_table, &value);
                            MacroTableAddString(macro_table, &name, &expanded);
                            StringFree(&expanded);
                        }
                        StringFree(&name);
                        StringFree(&value);
                    }

                } else if (StringEqualsCstrLen(name_ptr, name_len, "undef")) {
                    if (!is_skipping) {
                        size_t iu = dir_end;
                        while (iu < line.length &&
                               IsWhitespaceOrTab(line.data[iu]))
                            iu++;
                        size_t us = iu;
                        while (iu < line.length &&
                               (isalnum(line.data[iu]) ||
                                line.data[iu] == '_'))
                            iu++;
                        String uname =
                            StringSubstring(&line, us, iu - us);
                        MacroTableRemove(macro_table, &uname);
                        StringFree(&uname);
                    }

                } else if (StringEqualsCstrLen(name_ptr, name_len, "if") &&
                           !StringEqualsCstrLen(name_ptr, name_len, "ifdef") &&
                           !StringEqualsCstrLen(name_ptr, name_len,
                                                "ifndef")) {
                    // #if
                    if (is_skipping) {
                        arrput(if_stack, PREPROC_IF_DONE);
                    } else {
                        int64_t cond =
                            EvaluateIfDirective(&line, dir_end, macro_table);
                        arrput(if_stack, cond ? PREPROC_IF_ACTIVE
                                              : PREPROC_IF_SKIPPING);
                    }

                } else if (StringEqualsCstrLen(name_ptr, name_len, "ifdef")) {
                    if (is_skipping) {
                        arrput(if_stack, PREPROC_IF_DONE);
                    } else {
                        bool defined = EvaluateIfdefDirective(&line, dir_end,
                                                               macro_table);
                        arrput(if_stack, defined ? PREPROC_IF_ACTIVE
                                                 : PREPROC_IF_SKIPPING);
                    }

                } else if (StringEqualsCstrLen(name_ptr, name_len, "ifndef")) {
                    if (is_skipping) {
                        arrput(if_stack, PREPROC_IF_DONE);
                    } else {
                        bool defined = EvaluateIfdefDirective(&line, dir_end,
                                                               macro_table);
                        arrput(if_stack, defined ? PREPROC_IF_SKIPPING
                                                 : PREPROC_IF_ACTIVE);
                    }

                } else if (StringEqualsCstrLen(name_ptr, name_len, "else")) {
                    if (arrlen(if_stack) > 0) {
                        PreprocessorIfBranchState top =
                            if_stack[arrlen(if_stack) - 1];
                        if (top == PREPROC_IF_ACTIVE)
                            if_stack[arrlen(if_stack) - 1] = PREPROC_IF_DONE;
                        else if (top == PREPROC_IF_SKIPPING)
                            if_stack[arrlen(if_stack) - 1] =
                                PREPROC_IF_ACTIVE;
                    }

                } else if (StringEqualsCstrLen(name_ptr, name_len, "elif")) {
                    if (arrlen(if_stack) > 0) {
                        PreprocessorIfBranchState top =
                            if_stack[arrlen(if_stack) - 1];
                        if (top == PREPROC_IF_ACTIVE)
                            if_stack[arrlen(if_stack) - 1] = PREPROC_IF_DONE;
                        else if (top == PREPROC_IF_SKIPPING) {
                            int64_t cond = EvaluateIfDirective(
                                &line, dir_end, macro_table);
                            if_stack[arrlen(if_stack) - 1] =
                                cond ? PREPROC_IF_ACTIVE
                                     : PREPROC_IF_SKIPPING;
                        }
                    }

                } else if (StringEqualsCstrLen(name_ptr, name_len, "endif")) {
                    if (arrlen(if_stack) > 0) {
                        arrdel(if_stack, arrlen(if_stack) - 1);
                    }

                } else if (StringEqualsCstrLen(name_ptr, name_len, "pragma") ||
                           StringEqualsCstrLen(name_ptr, name_len, "line") ||
                           StringEqualsCstrLen(name_ptr, name_len, "error") ||
                           StringEqualsCstrLen(name_ptr, name_len,
                                               "warning")) {
                    // silently ignore
                }
            }

        } else if (!is_skipping) {
            // Non-directive line — expand macros and emit
            String expanded = MacroTableExpand(macro_table, &line);
            StringAppend(&result, &expanded);
            if (i < no_comments.length) {
                StringAppendChar(&result, '\n');
            }
            StringFree(&expanded);
        }

        StringFree(&line);
        line_start = i + 1;
    }

    arrfree(if_stack);
    StringFree(&no_comments);

    // Trim trailing newlines
    while (result.length > 0 &&
           StringGetChar(&result, result.length - 1) == '\n') {
        result.data[result.length - 1] = '\0';
        result.length--;
    }
    if (result.length > 0) {
        StringAppendChar(&result, '\n');
    }
    result.data[result.length] = '\0';

    return result;
}

String PreprocessorProcess(const String *content) {
    if (!content || content->length == 0) {
        return StringCreateEmpty(0);
    }

    return PreprocessorStripComments(content);
}

MacroTable MacroTableCreate(void) {
    MacroTable table = {0};
    table.capacity = 16;
    table.macros = malloc(sizeof(Macro) * table.capacity);
    if (!table.macros) {
        table.capacity = 0;
    }
    return table;
}

void MacroTableFree(MacroTable *table) {
    if (!table)
        return;

    for (size_t i = 0; i < table->count; i++) {
        StringFree(&table->macros[i].name);
        StringFree(&table->macros[i].value);
    }
    free(table->macros);
    table->macros = NULL;
    table->count = 0;
    table->capacity = 0;
}

bool MacroTableAdd(MacroTable *table, const char *name, const char *value) {
    if (!table || !name || !value) {
        return false;
    }

    for (size_t i = 0; i < table->count; i++) {
        if (strcmp(table->macros[i].name.data, name) == 0) {
            StringFree(&table->macros[i].value);
            table->macros[i].value = StringCreate(value);
            return table->macros[i].value.data != NULL;
        }
    }

    if (table->count >= table->capacity) {
        size_t new_capacity = table->capacity * 2;
        Macro *new_macros = realloc(table->macros,
                                     sizeof(Macro) * new_capacity);
        if (!new_macros) {
            return false;
        }
        table->macros = new_macros;
        table->capacity = new_capacity;
    }

    table->macros[table->count].name = StringCreate(name);
    table->macros[table->count].value = StringCreate(value);

    if (!table->macros[table->count].name.data ||
        !table->macros[table->count].value.data) {
        StringFree(&table->macros[table->count].name);
        StringFree(&table->macros[table->count].value);
        return false;
    }

    table->count++;
    return true;
}

bool MacroTableAddString(MacroTable *table, const String *name,
                          const String *value) {
    if (!table || !name || !value) {
        return false;
    }

    char *name_cstr = malloc(name->length + 1);
    char *value_cstr = malloc(value->length + 1);

    if (!name_cstr || !value_cstr) {
        free(name_cstr);
        free(value_cstr);
        return false;
    }

    memcpy(name_cstr, name->data, name->length);
    name_cstr[name->length] = '\0';

    memcpy(value_cstr, value->data, value->length);
    value_cstr[value->length] = '\0';

    bool result = MacroTableAdd(table, name_cstr, value_cstr);

    free(name_cstr);
    free(value_cstr);

    return result;
}

String MacroTableExpand(MacroTable *table, const String *content) {
    if (!table || !content || content->length == 0) {
        return StringCreateEmpty(0);
    }

    String result = StringCreateEmpty(content->length * 2);
    if (!result.data) {
        return result;
    }

    size_t i = 0;
    while (i < content->length) {
        char c = StringGetChar(content, i);

        if (c == '"' || c == '\'') {
            char quote = c;
            StringAppendChar(&result, c);
            i++;
            while (i < content->length) {
                char ch = StringGetChar(content, i);
                StringAppendChar(&result, ch);
                if (ch == quote &&
                    (i == 0 || StringGetChar(content, i - 1) != '\\')) {
                    break;
                }
                i++;
            }
            i++;
            continue;
        }

        if (isalpha(c) || c == '_') {
            size_t start = i;
            while (i < content->length &&
                   (isalnum(StringGetChar(content, i)) ||
                    StringGetChar(content, i) == '_')) {
                i++;
            }

            String identifier =
                StringSubstring(content, start, i - start);

            bool found = false;

            // Check for function-like macro FOO(
            if (i < content->length &&
                StringGetChar(content, i) == '(') {
                // Check if this macro name exists and is function-like
                int idx = MacroTableFind(table, &identifier);
                if (idx >= 0) {
                    // For now, function-like macros are not supported.
                    // Emit as-is.
                    StringAppend(&result, &identifier);
                    found = true;
                }
            }

            if (!found) {
                for (size_t j = 0; j < table->count; j++) {
                    if (StringEquals(&identifier,
                                     &table->macros[j].name)) {
                        StringAppend(&result,
                                     &table->macros[j].value);
                        found = true;
                        break;
                    }
                }
            }

            if (!found) {
                StringAppend(&result, &identifier);
            }

            StringFree(&identifier);
            continue;
        }

        StringAppendChar(&result, c);
        i++;
    }

    return result;
}

static String PreprocessorSpliceLines(const String *content) {
    if (!content || content->length == 0) {
        return StringCreateEmpty(0);
    }
    String result = StringCreateEmpty(content->length);
    if (!result.data) return result;

    for (size_t i = 0; i < content->length; i++) {
        char c = StringGetChar(content, i);
        if (c == '\\' && i + 1 < content->length &&
            StringGetChar(content, i + 1) == '\n') {
            i++;
            continue;
        }
        StringAppendChar(&result, c);
    }
    return result;
}

String PreprocessorStripComments(const String *content) {
    if (!content || content->length == 0) {
        return StringCreateEmpty(0);
    }

    String result = StringCreateEmpty(content->length);
    if (!result.data) {
        return result;
    }

    bool in_single_comment = false;
    bool in_multiline_comment = false;
    bool in_string_literal = false;
    bool in_char_literal = false;

    for (size_t i = 0; i < content->length; i++) {
        char c = StringGetChar(content, i);

        if (c == '"' &&
            (i == 0 || StringGetChar(content, i - 1) != '\\')) {
            if (!in_single_comment && !in_multiline_comment) {
                in_string_literal = !in_string_literal;
                StringAppendChar(&result, c);
                continue;
            }
        }

        if (c == '\'' &&
            (i == 0 || StringGetChar(content, i - 1) != '\\')) {
            if (!in_single_comment && !in_multiline_comment) {
                in_char_literal = !in_char_literal;
                StringAppendChar(&result, c);
                continue;
            }
        }

        if (in_string_literal || in_char_literal) {
            StringAppendChar(&result, c);
            continue;
        }

        if (c == '/' && i + 1 < content->length &&
            StringGetChar(content, i + 1) == '/' &&
            !in_multiline_comment) {
            in_single_comment = true;
            i++;
            continue;
        }

        if (c == '/' && i + 1 < content->length &&
            StringGetChar(content, i + 1) == '*' &&
            !in_single_comment) {
            in_multiline_comment = true;
            i++;
            continue;
        }

        if (in_multiline_comment && c == '*' &&
            i + 1 < content->length &&
            StringGetChar(content, i + 1) == '/') {
            in_multiline_comment = false;
            i++;
            continue;
        }

        if (c == '\n') {
            in_single_comment = false;
            if (!in_multiline_comment) {
                StringAppendChar(&result, c);
            }
            continue;
        }

        if (!in_single_comment && !in_multiline_comment) {
            StringAppendChar(&result, c);
        }
    }

    return result;
}

bool PreprocessorIsCommentLine(const String *content, size_t line) {
    if (!content || content->length == 0 || line == 0) {
        return false;
    }

    size_t i = 0;
    size_t current_line = 1;
    size_t line_start = 0;

    while (i < content->length && current_line < line) {
        if (StringGetChar(content, i) == '\n') {
            current_line++;
            line_start = i + 1;
        }
        i++;
    }

    if (current_line != line || line_start >= content->length) {
        return false;
    }

    size_t pos = line_start;
    while (pos < content->length &&
           (StringGetChar(content, pos) == ' ' ||
            StringGetChar(content, pos) == '\t')) {
        pos++;
    }

    if (pos + 1 < content->length &&
        StringGetChar(content, pos) == '/' &&
        StringGetChar(content, pos + 1) == '/') {
        return true;
    }

    return false;
}

IncludePathList IncludePathListCreate(void) {
    IncludePathList list = {0};
    list.capacity = 8;
    list.paths = malloc(sizeof(String *) * list.capacity);
    return list;
}

void IncludePathListFree(IncludePathList *list) {
    if (!list)
        return;

    for (size_t i = 0; i < list->count; i++) {
        StringFree(list->paths[i]);
        free(list->paths[i]);
    }
    free(list->paths);
    list->paths = NULL;
    list->count = 0;
    list->capacity = 0;
}

bool IncludePathListAdd(IncludePathList *list, const char *path) {
    if (!list || !path)
        return false;

    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        String **new_paths =
            realloc(list->paths, sizeof(String *) * new_capacity);
        if (!new_paths)
            return false;
        list->paths = new_paths;
        list->capacity = new_capacity;
    }

    String *path_str = malloc(sizeof(String));
    if (!path_str)
        return false;

    *path_str = StringCreate(path);
    if (!path_str->data) {
        free(path_str);
        return false;
    }

    list->paths[list->count++] = path_str;
    return true;
}

String PreprocessorResolveInclude(const String *include_path,
                                   const IncludePathList *search_paths) {
    if (!include_path || !search_paths) {
        return StringCreateEmpty(0);
    }

    for (size_t i = 0; i < search_paths->count; i++) {
        String *path = search_paths->paths[i];
        String full_path =
            StringCreateEmpty(path->length + include_path->length + 2);
        if (!full_path.data)
            continue;

        StringAppend(&full_path, path);
        if (StringGetChar(&full_path, full_path.length - 1) != '/') {
            StringAppendChar(&full_path, '/');
        }
        StringAppend(&full_path, include_path);

        if (FileExists(full_path.data)) {
            String content = ReadFile(full_path.data);
            StringFree(&full_path);
            return content;
        }

        StringFree(&full_path);
    }

    return StringCreateEmpty(0);
}

String PreprocessorTrimWhitespace(const String *str) {
    if (!str || str->length == 0) {
        return StringCreateEmpty(0);
    }

    String result = StringDuplicate(str);
    if (result.data) {
        StringTrim(&result);
    }
    return result;
}

String PreprocessorCollapseWhitespace(const String *str) {
    if (!str || str->length == 0) {
        return StringCreateEmpty(0);
    }

    String result = StringCreateEmpty(str->length);
    if (!result.data) {
        return result;
    }

    bool in_whitespace = false;

    for (size_t i = 0; i < str->length; i++) {
        char c = StringGetChar(str, i);

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!in_whitespace) {
                StringAppendChar(&result, ' ');
                in_whitespace = true;
            }
        } else {
            StringAppendChar(&result, c);
            in_whitespace = false;
        }
    }

    return result;
}

String PreprocessorRemoveEmptyLines(const String *str) {
    if (!str || str->length == 0) {
        return StringCreateEmpty(0);
    }

    String result = StringCreateEmpty(str->length);
    if (!result.data) {
        return result;
    }

    bool last_was_newline = true;
    bool has_content = false;

    for (size_t i = 0; i < str->length; i++) {
        char c = StringGetChar(str, i);

        if (c == '\n') {
            if (has_content) {
                StringAppendChar(&result, c);
                has_content = false;
            }
            last_was_newline = true;
        } else if (c != ' ' && c != '\t') {
            if (last_was_newline && has_content) {
                StringAppendChar(&result, c);
            } else if (last_was_newline) {
                StringAppendChar(&result, c);
                has_content = true;
            } else {
                StringAppendChar(&result, c);
                has_content = true;
            }
            last_was_newline = false;
        } else {
            if (has_content) {
                StringAppendChar(&result, c);
            }
        }
    }

    return result;
}

String PreprocessorNormalizeNewlines(const String *str) {
    if (!str || str->length == 0) {
        return StringCreateEmpty(0);
    }

    String result = StringCreateEmpty(str->length);
    if (!result.data) {
        return result;
    }

    for (size_t i = 0; i < str->length; i++) {
        char c = StringGetChar(str, i);

        if (c == '\r') {
            if (i + 1 < str->length &&
                StringGetChar(str, i + 1) == '\n') {
                StringAppendChar(&result, '\n');
                i++;
            } else {
                StringAppendChar(&result, '\n');
            }
        } else {
            StringAppendChar(&result, c);
        }
    }

    return result;
}
