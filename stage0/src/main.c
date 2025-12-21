#include "common.h"
#include "fs.h"
#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* Output mode */
typedef enum {
    OUTPUT_EXECUTABLE,  /* Default: produce binary */
    OUTPUT_LLVM_IR,     /* -S or --emit-llvm: produce .ll */
    OUTPUT_OBJECT       /* -c: produce .o */
} OutputMode;

static const char *get_arg_value(int argc, char **argv, const char *name) {
    int i;
    size_t nlen = strlen(name);
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (a[0] == '-' && a[1] == '-') {
            const char *p = a + 2;
            if (strncmp(p, name, nlen) == 0) {
                const char *rest = p + nlen;
                if (*rest == '=' && rest[1] != '\0') return rest + 1;
                if (*rest == '\0' && i + 1 < argc) return argv[i + 1];
            }
        }
    }
    return NULL;
}

static void parse_include_dirs(int argc, char **argv, StrList *out_dirs) {
    int i;
    sl_init(out_dirs);
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (a[0] == '-' && a[1] == '-' && strcmp(a + 2, "include-dir") == 0) {
            if (i + 1 < argc) sl_push(out_dirs, argv[i + 1]);
        } else if (a[0] == '-' && a[1] == '-' && strncmp(a + 2, "include-dir=", 12) == 0) {
            sl_push(out_dirs, a + 2 + 12);
        } else if (a[0] == '-' && a[1] == 'I') {
            /* gcc/clang style -Ipath */
            if (a[2] != '\0') {
                sl_push(out_dirs, a + 2);
            } else if (i + 1 < argc) {
                sl_push(out_dirs, argv[i + 1]);
            }
        }
    }
    if (out_dirs->len == 0) {
        sl_push(out_dirs, ".");
    }
}

static char *compute_base_dir(const char *path) {
    const char *last = NULL;
    const char *p = path;
    while (*p) {
        if (*p == '/' || *p == '\\') last = p;
        p++;
    }
    if (!last) return xstrdup(".");
    {
        size_t n = (size_t)(last - path);
        char *out = (char *)xmalloc(n + 1);
        memcpy(out, path, n);
        out[n] = '\0';
        if (n == 0) {
            free(out);
            return xstrdup(".");
        }
        return out;
    }
}

static void list_tests_in(Node *form) {
    Node *head = list_nth(form, 0);
    int i;
    if (!form || form->kind != N_LIST || !head || head->kind != N_ATOM) return;
    if (is_atom(head, "module") || is_atom(head, "program")) {
        for (i = 1; i < form->count; i++) list_tests_in(list_nth(form, i));
        return;
    }
    if (is_atom(head, "fn")) {
        for (i = 1; i < form->count; i++) {
            Node *extra = list_nth(form, i);
            Node *eh = list_nth(extra, 0);
            int ti;
            if (!eh || eh->kind != N_ATOM) continue;
            if (!is_atom(eh, "tests")) continue;
            for (ti = 1; ti < extra->count; ti++) {
                Node *tform = list_nth(extra, ti);
                Node *th = list_nth(tform, 0);
                if (!th || th->kind != N_ATOM || !is_atom(th, "test")) continue;
                const char *tname = atom_text(list_nth(tform, 1));
                if (tname && *tname) fprintf(stdout, "%s\n", tname);
            }
        }
    }
}

int main(int argc, char **argv) {
    const char *input = get_arg_value(argc, argv, "input");
    const char *output = get_arg_value(argc, argv, "output");
    const char *runtime_path = getenv("WEAVE_RUNTIME");
    OutputMode mode = OUTPUT_EXECUTABLE;
    int use_static = 0;
    int optimize = 0;
    int generate_tests_mode = 0;
    int list_tests_only = 0;
    StrList selected_test_names;
    StrList selected_tags;
    sl_init(&selected_test_names);
    sl_init(&selected_tags);
    /* clang-style: we accept -I dir, -Idir, -o outfile, and positional input. */
    int i;
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-o") == 0 && i + 1 < argc) {
            output = argv[i + 1];
            i++;
        } else if (strncmp(a, "-o", 2) == 0 && a[2] != '\0') {
            output = a + 2;
        } else if (strcmp(a, "--output") == 0 && i + 1 < argc) {
            output = argv[i + 1];
            i++;
        } else if (strncmp(a, "--output=", 9) == 0) {
            output = a + 9;
        } else if (strcmp(a, "--input") == 0 && i + 1 < argc) {
            input = argv[i + 1];
            i++;
        } else if (strncmp(a, "--input=", 8) == 0) {
            input = a + 8;
        } else if (strcmp(a, "-S") == 0 || strcmp(a, "--emit-llvm") == 0 || strcmp(a, "-emit-llvm") == 0) {
            mode = OUTPUT_LLVM_IR;
        } else if (strcmp(a, "-c") == 0) {
            mode = OUTPUT_OBJECT;
        } else if (strcmp(a, "--static") == 0) {
            use_static = 1;
        } else if (strcmp(a, "-O") == 0 || strcmp(a, "-O2") == 0 || strcmp(a, "--optimize") == 0) {
            optimize = 1;
        } else if ((strcmp(a, "--runtime") == 0 || strcmp(a, "-runtime") == 0) && i + 1 < argc) {
            runtime_path = argv[i + 1];
            i++;
        } else if (strncmp(a, "--runtime=", 10) == 0) {
            runtime_path = a + 10;
        } else if (strcmp(a, "--run-tests") == 0 || strcmp(a, "-run-tests") == 0 || strcmp(a, "-generate-tests") == 0) {
            generate_tests_mode = 1;
            /* In test generation mode, default to executable output to run tests. */
            mode = OUTPUT_EXECUTABLE;
        } else if (strcmp(a, "--list-tests") == 0 || strcmp(a, "-list-tests") == 0) {
            list_tests_only = 1;
        } else if (strcmp(a, "-test") == 0 && i + 1 < argc) {
            sl_push(&selected_test_names, argv[i + 1]);
            i++;
        } else if (strncmp(a, "-test=", 6) == 0) {
            sl_push(&selected_test_names, a + 6);
        } else if (strcmp(a, "-tag") == 0 && i + 1 < argc) {
            sl_push(&selected_tags, argv[i + 1]);
            i++;
        } else if (strncmp(a, "-tag=", 5) == 0) {
            sl_push(&selected_tags, a + 5);
        } else if (a[0] != '-') {
            input = a;
        }
    }
    char *src;
    Node *top;
    StrList included;
    StrList include_dirs;
    char *base_dir;
    StrBuf ir;
    FILE *f;

    if (!input) {
        fprintf(stderr, "Usage: weavec [options] INPUT\n");
        fprintf(stderr, "       weavec [options] -o OUTPUT INPUT\n");
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -o <file>         Output file (default: a.out)\n");
        fprintf(stderr, "  -S, -emit-llvm    Emit LLVM IR instead of executable\n");
        fprintf(stderr, "  -c                Emit object file\n");
        fprintf(stderr, "  -O, --optimize    Enable optimizations\n");
        fprintf(stderr, "  --static          Produce static executable\n");
        fprintf(stderr, "  --runtime PATH    Path to runtime.c (or set WEAVE_RUNTIME env var)\n");
        fprintf(stderr, "  -generate-tests   Generate & run embedded tests (emit synthetic main)\n");
        fprintf(stderr, "  -run-tests        Alias for -generate-tests\n");
        fprintf(stderr, "  -list-tests       List embedded tests by name (one per line)\n");
        fprintf(stderr, "  -test NAME        Select test(s) by name (repeatable)\n");
        fprintf(stderr, "  -tag TAG          Select test(s) by tag (repeatable)\n");
        fprintf(stderr, "  -I<dir>           Add include directory\n");
        fprintf(stderr, "\nEnvironment:\n");
        fprintf(stderr, "  WEAVE_RUNTIME     Default path to runtime.c\n");
        return 2;
    }
    
    /* Default output to a.out if not specified */
    if (!output) {
        output = "a.out";
    }

    src = read_file_all(input);
    top = parse_top(src, input);
    free(src);

    sl_init(&included);
    parse_include_dirs(argc, argv, &include_dirs);
    base_dir = compute_base_dir(input);
    merge_includes(top, &included, base_dir, &include_dirs, input);
    free(base_dir);

    sb_init(&ir);
    if (list_tests_only) {
        int i;
        Node *decls = top;
        for (i = 0; decls && i < decls->count; i++) list_tests_in(list_nth(decls, i));
        /* No IR generation in list mode */
    } else {
        compile_to_llvm_ir(top, &ir, generate_tests_mode, &selected_test_names, &selected_tags);
    }

    if (list_tests_only) {
        /* Listing mode prints to stdout only */
        return 0;
    } else if (mode == OUTPUT_LLVM_IR) {
        /* Just write the IR */
        f = fopen(output, "wb");
        if (!f) {
            fprintf(stderr, "weavec: cannot write output: %s\n", output);
            return 1;
        }
        fwrite(ir.data ? ir.data : "", 1, ir.len, f);
        fclose(f);
    } else {
        /* Compile to object or executable using clang */
        char ll_tmp[256];
        pid_t pid;
        int status;
        
        /* Check runtime for executable output */
        if (mode == OUTPUT_EXECUTABLE && !runtime_path) {
            fprintf(stderr, "weavec: runtime path required for executable output\n");
            fprintf(stderr, "  Use --runtime PATH or set WEAVE_RUNTIME environment variable\n");
            return 1;
        }
        
        /* Write IR to temp file */
        snprintf(ll_tmp, sizeof(ll_tmp), "/tmp/weavec_%d.ll", getpid());
        f = fopen(ll_tmp, "wb");
        if (!f) {
            fprintf(stderr, "weavec: cannot write temp file: %s\n", ll_tmp);
            return 1;
        }
        fwrite(ir.data ? ir.data : "", 1, ir.len, f);
        fclose(f);
        
        /* Build clang command */
        pid = fork();
        if (pid == 0) {
            /* Child process - exec clang */
            const char *clang_args[24];
            int arg_idx = 0;
            
            clang_args[arg_idx++] = "clang";
            const char *use_asan_env = getenv("WEAVE_ASAN");
            int use_asan = (use_asan_env && use_asan_env[0] == '1');
            if (optimize) {
                clang_args[arg_idx++] = "-O2";
            }
            /* Silence external runtime null-char literal warnings */
            clang_args[arg_idx++] = "-Wno-null-character";
            if (use_asan) {
                clang_args[arg_idx++] = "-fsanitize=address";
                clang_args[arg_idx++] = "-fno-omit-frame-pointer";
            }
            if (mode == OUTPUT_OBJECT) {
                clang_args[arg_idx++] = "-c";
            }
            if (use_static && mode == OUTPUT_EXECUTABLE) {
                clang_args[arg_idx++] = "-static";
            }
            clang_args[arg_idx++] = "-o";
            clang_args[arg_idx++] = output;
            clang_args[arg_idx++] = ll_tmp;
            if (mode == OUTPUT_EXECUTABLE && runtime_path) {
                clang_args[arg_idx++] = runtime_path;
            }
            clang_args[arg_idx++] = "-lm";
            clang_args[arg_idx] = NULL;
            
            execvp("clang", (char *const *)clang_args);
            fprintf(stderr, "weavec: failed to execute clang\n");
            _exit(1);
        } else if (pid > 0) {
            /* Parent - wait for clang */
            waitpid(pid, &status, 0);
            unlink(ll_tmp);  /* Clean up temp file */
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                fprintf(stderr, "weavec: clang failed\n");
                return 1;
            }
        } else {
            fprintf(stderr, "weavec: fork failed\n");
            return 1;
        }
    }

    return 0;
}
