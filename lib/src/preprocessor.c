#include "preprocessor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include "utils.h"

static String PreprocessorProcessFile(const String *content, const IncludePathList *include_paths, int depth);

IncludePathList IncludePathListCreate(void) {
    IncludePathList list = {0};
    list.capacity = 8;
    list.paths = malloc(sizeof(String*) * list.capacity);
    return list;
}

void IncludePathListFree(IncludePathList *list) {
    if (!list) return;
    
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
    if (!list || !path) return false;
    
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        String **new_paths = realloc(list->paths, sizeof(String*) * new_capacity);
        if (!new_paths) return false;
        list->paths = new_paths;
        list->capacity = new_capacity;
    }
    
    String *path_str = malloc(sizeof(String));
    if (!path_str) return false;
    
    *path_str = StringCreate(path);
    if (!path_str->data) {
        free(path_str);
        return false;
    }
    
    list->paths[list->count++] = path_str;
    return true;
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
        
        if (c == '"' && (i == 0 || StringGetChar(content, i - 1) != '\\')) {
            if (!in_single_comment && !in_multiline_comment) {
                in_string_literal = !in_string_literal;
                StringAppendChar(&result, c);
                continue;
            }
        }
        
        if (c == '\'' && (i == 0 || StringGetChar(content, i - 1) != '\\')) {
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
        
        if (in_multiline_comment && c == '*' && i + 1 < content->length && 
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

static String FindIncludeFile(const String *include_path, const IncludePathList *search_paths) {
    if (FileExists(include_path->data)) {
        return ReadFile(include_path->data);
    }
    
    char *filename = include_path->data;
    if (FileExists(filename)) {
        return ReadFile(filename);
    }
    
    for (size_t i = 0; i < search_paths->count; i++) {
        String *path = search_paths->paths[i];
        String full_path = StringCreateEmpty(path->length + include_path->length + 2);
        if (!full_path.data) continue;
        
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
    *is_system = false;
    
    if (!line || line->length == 0) {
        return StringCreateEmpty(0);
    }
    
    String line_copy = StringDuplicate(line);
    StringTrim(&line_copy);
    
    if (line_copy.length == 0) {
        StringFree(&line_copy);
        return StringCreateEmpty(0);
    }
    
    size_t i = 0;
    while (i < line_copy.length && StringGetChar(&line_copy, i) != '#') {
        i++;
    }
    if (i >= line_copy.length) {
        StringFree(&line_copy);
        return StringCreateEmpty(0);
    }
    i++; 
    
    while (i < line_copy.length && (StringGetChar(&line_copy, i) == ' ' || StringGetChar(&line_copy, i) == '\t')) {
        i++;
    }
    
    const char *include_str = "include";
    size_t j = 0;
    while (j < 7 && i + j < line_copy.length && StringGetChar(&line_copy, i + j) == include_str[j]) {
        j++;
    }
    if (j < 7) {
        StringFree(&line_copy);
        return StringCreateEmpty(0);
    }
    i += 7;
    
    while (i < line_copy.length && (StringGetChar(&line_copy, i) == ' ' || StringGetChar(&line_copy, i) == '\t')) {
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
        while (i < line_copy.length && StringGetChar(&line_copy, i) != '>') {
            i++;
        }
        StringFree(&result);
        result = StringSubstring(&line_copy, start, i - start);
    } else if (first == '"') {
        i++;
        size_t start = i;
        while (i < line_copy.length && StringGetChar(&line_copy, i) != '"') {
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

String PreprocessorHandleIncludes(const String *content, const IncludePathList *include_paths) {
    return PreprocessorProcessFile(content, include_paths, 0);
}

static String PreprocessorProcessFile(const String *content, const IncludePathList *include_paths, int depth) {
    if (depth > 10) {
        fprintf(stderr, "ERROR: Maximum include depth exceeded (possible circular includes)\n");
        return StringCreateEmpty(0);
    }
    
    if (!content || content->length == 0) {
        return StringCreateEmpty(0);
    }
    
    String no_comments = PreprocessorStripComments(content);
    if (!no_comments.data) {
        return no_comments;
    }
    
    String result = StringCreateEmpty(no_comments.length * 2);
    if (!result.data) {
        StringFree(&no_comments);
        return result;
    }
    
    size_t line_start = 0;
    for (size_t i = 0; i <= no_comments.length; i++) {
        char c = (i < no_comments.length) ? StringGetChar(&no_comments, i) : '\n';
        
        if (c == '\n' || i == no_comments.length) {
            String line = StringSubstring(&no_comments, line_start, i - line_start);
            
            if (IsIncludeDirective(&line)) {
                bool is_system = false;
                String include_path = ExtractIncludePath(&line, &is_system);
                
                if (include_path.data && include_path.length > 0) {
                    String included_content = FindIncludeFile(&include_path, include_paths);
                    
                    if (included_content.data && included_content.length > 0) {
                        String processed_included = PreprocessorProcessFile(&included_content, include_paths, depth + 1);
                        StringAppend(&result, &processed_included);
                        StringFree(&processed_included);
                        
                        if (result.length > 0 && StringGetChar(&result, result.length - 1) != '\n') {
                            StringAppendChar(&result, '\n');
                        }
                    } else {
                        LOG_DEBUG("Warning: Include file not found: %s\n", include_path.data ? include_path.data : "(null)");
                    }
                    
                    StringFree(&included_content);
                }
                StringFree(&include_path);
            } else {
                size_t first_non_space = 0;
                while (first_non_space < line.length &&
                       (line.data[first_non_space] == ' ' || line.data[first_non_space] == '\t')) {
                    first_non_space++;
                }
                bool is_directive = (first_non_space < line.length && line.data[first_non_space] == '#');
                if (!is_directive) {
                    StringAppend(&result, &line);
                    if (i < no_comments.length) {
                        StringAppendChar(&result, '\n');
                    }
                }
            }
            
            StringFree(&line);
            line_start = i + 1;
        }
    }
    
    StringFree(&no_comments);
    
    while (result.length > 0 && StringGetChar(&result, result.length - 1) == '\n') {
        result.data[result.length - 1] = '\0';
        result.length--;
    }
    if (result.length > 0) {
        StringAppendChar(&result, '\n');
    }
    result.data[result.length] = '\0';
    
    return result;
}

String PreprocessorResolveInclude(const String *include_path, const IncludePathList *search_paths) {
    if (!include_path || !search_paths) {
        return StringCreateEmpty(0);
    }
    
    for (size_t i = 0; i < search_paths->count; i++) {
        String *path = search_paths->paths[i];
        String full_path = StringCreateEmpty(path->length + include_path->length + 2);
        if (!full_path.data) continue;
        
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
    if (!table) return;
    
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
        Macro *new_macros = realloc(table->macros, sizeof(Macro) * new_capacity);
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

bool MacroTableAddString(MacroTable *table, const String *name, const String *value) {
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
                if (ch == quote && (i == 0 || StringGetChar(content, i - 1) != '\\')) {
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
            
            String identifier = StringSubstring(content, start, i - start);
            
            bool found = false;
            for (size_t j = 0; j < table->count; j++) {
                if (StringEquals(&identifier, &table->macros[j].name)) {
                    StringAppend(&result, &table->macros[j].value);
                    found = true;
                    break;
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



String PreprocessorProcess(const String *content) {
    if (!content || content->length == 0) {
        return StringCreateEmpty(0);
    }
    
    return PreprocessorStripComments(content);
}

String PreprocessorProcessWithIncludes(const String *content, const char *include_paths[]) {
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
    
    String result = PreprocessorHandleIncludes(content, &paths);
    
    IncludePathListFree(&paths);
    
    return result;
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
            if (i + 1 < str->length && StringGetChar(str, i + 1) == '\n') {
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
