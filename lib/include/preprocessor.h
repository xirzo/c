#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include "xi_string.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    String *content;
    size_t line;
    size_t position;
    bool in_single_comment;
    bool in_multiline_comment;
    bool in_string_literal;
    bool in_char_literal;
} PreprocessorContext;

typedef struct {
    String *content;
    size_t count;
    size_t capacity;
} PreprocessorOutput;

typedef enum {
    PREPROC_IF_ACTIVE,
    PREPROC_IF_SKIPPING,
    PREPROC_IF_DONE,
} PreprocessorIfBranchState;

String PreprocessorProcess(const String *content);
String PreprocessorProcessWithIncludes(const String *content, const char *include_paths[]);

String PreprocessorStripComments(const String *content);
bool PreprocessorIsCommentLine(const String *content, size_t line);

typedef struct {
    String **paths;
    size_t count;
    size_t capacity;
} IncludePathList;

IncludePathList IncludePathListCreate(void);
void IncludePathListFree(IncludePathList *list);
bool IncludePathListAdd(IncludePathList *list, const char *path);

String PreprocessorHandleIncludes(const String *content, const IncludePathList *include_paths);
String PreprocessorResolveInclude(const String *include_path, const IncludePathList *search_paths);

typedef struct {
    String name;
    String value;
} Macro;

typedef struct {
    Macro *macros;
    size_t count;
    size_t capacity;
} MacroTable;

MacroTable MacroTableCreate(void);
void MacroTableFree(MacroTable *table);
bool MacroTableAdd(MacroTable *table, const char *name, const char *value);
bool MacroTableAddString(MacroTable *table, const String *name, const String *value);
String MacroTableExpand(MacroTable *table, const String *content);
int MacroTableFind(MacroTable *table, const String *name);
bool MacroTableRemove(MacroTable *table, const String *name);

int64_t PreprocessorEvaluateIf(const String *expr, const MacroTable *macro_table);

String PreprocessorTrimWhitespace(const String *str);
String PreprocessorCollapseWhitespace(const String *str);
String PreprocessorRemoveEmptyLines(const String *str);
String PreprocessorNormalizeNewlines(const String *str);

#endif
