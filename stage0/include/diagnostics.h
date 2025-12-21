#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

/* Centralized diagnostics for consistent error/warning reporting.
 * All compiler phases (parse, typecheck, codegen) should use these
 * instead of direct fprintf/die calls.
 */

/* Diagnostic severity levels */
typedef enum {
    DIAG_ERROR,
    DIAG_WARNING,
    DIAG_NOTE
} DiagSeverity;

/* Report a diagnostic with source location.
 * 
 * filename: source file path (or NULL for no location)
 * line: 1-based line number (or 0 if unknown)
 * col: 1-based column number (or 0 if unknown)
 * severity: error, warning, or note
 * code: short error code/category (e.g., "type-mismatch", "syntax-error")
 * message: primary diagnostic message
 * detail: optional additional detail (can be NULL)
 * 
 * For errors, this function does NOT terminate; caller should handle cleanup/exit.
 */
void diag_report(const char *filename, int line, int col,
                 DiagSeverity severity,
                 const char *code,
                 const char *message,
                 const char *detail);

/* Convenience wrappers */
void diag_error(const char *filename, int line, int col,
                const char *code, const char *message, const char *detail);

void diag_warn(const char *filename, int line, int col,
               const char *code, const char *message, const char *detail);

void diag_note(const char *filename, int line, int col,
               const char *message);

/* Fatal error: report and exit immediately */
void diag_fatal(const char *filename, int line, int col,
                const char *code, const char *message, const char *detail) __attribute__((noreturn));

#endif /* DIAGNOSTICS_H */
