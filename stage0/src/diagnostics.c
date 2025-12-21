#include "diagnostics.h"
#include <stdio.h>
#include <stdlib.h>

static const char *severity_str(DiagSeverity sev) {
    switch (sev) {
    case DIAG_ERROR: return "error";
    case DIAG_WARNING: return "warning";
    case DIAG_NOTE: return "note";
    default: return "diagnostic";
    }
}

void diag_report(const char *filename, int line, int col,
                 DiagSeverity severity,
                 const char *code,
                 const char *message,
                 const char *detail) {
    /* Format: filename:line:col: severity: [code] message */
    if (filename && line > 0 && col > 0) {
        fprintf(stderr, "%s:%d:%d: %s", filename, line, col, severity_str(severity));
    } else if (filename && line > 0) {
        fprintf(stderr, "%s:%d: %s", filename, line, severity_str(severity));
    } else if (filename) {
        fprintf(stderr, "%s: %s", filename, severity_str(severity));
    } else {
        fprintf(stderr, "%s", severity_str(severity));
    }
    
    if (code && *code) {
        fprintf(stderr, ": [%s]", code);
    }
    
    if (message && *message) {
        fprintf(stderr, ": %s", message);
    }
    
    fprintf(stderr, "\n");
    
    if (detail && *detail) {
        fprintf(stderr, "  note: %s\n", detail);
    }
}

void diag_error(const char *filename, int line, int col,
                const char *code, const char *message, const char *detail) {
    diag_report(filename, line, col, DIAG_ERROR, code, message, detail);
}

void diag_warn(const char *filename, int line, int col,
               const char *code, const char *message, const char *detail) {
    diag_report(filename, line, col, DIAG_WARNING, code, message, detail);
}

void diag_note(const char *filename, int line, int col,
               const char *message) {
    diag_report(filename, line, col, DIAG_NOTE, NULL, message, NULL);
}

void diag_fatal(const char *filename, int line, int col,
                const char *code, const char *message, const char *detail) {
    diag_error(filename, line, col, code, message, detail);
    exit(1);
}
