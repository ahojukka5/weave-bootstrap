# Weave Syntax Guidelines

This document describes the coding style and syntax conventions for the Weave language.

## Parenthesis Formatting

### Rule: One Closing Parenthesis Per Line

Every closing parenthesis MUST be on its own line, aligned with the indentation level of its corresponding opening parenthesis, and include a comment indicating what it closes.

**Reasoning:** Long strings of closing parentheses (e.g., `)))))))`) are difficult for both humans and LLMs to parse and count correctly. Proper formatting makes the code structure clear and maintainable.

### ❌ Incorrect Style

```lisp
(tests
  (test compile-all-empty
    (doc "Test compiling empty program")
    (tags unit)
    (body
      (let a (ptr Arena) (arena-create 4096))
      (let root Int32 (parse-string a "()"))
      (let out Buffer (buffer-create 256))
      (let str-counter-var Int32 0)
      (let result Int32 (compile-all-fns a root (array-string-create) (array-string-create) (array-string-create) (buffer-create 0) (addr-of Int32 str-counter-var) out (array-string-create) (array-int32-create)))
      (expect-eq result 0))))
```

### ✅ Correct Style

```lisp
(tests
  (test compile-all-empty
    (doc "Test compiling empty program")
    (tags unit)
    (body
      (let a (ptr Arena) (arena-create 4096))
      (let root Int32 (parse-string a "()"))
      (let out Buffer (buffer-create 256))
      (let str-counter-var Int32 0)
      (let result Int32 (compile-all-fns a root (array-string-create) (array-string-create) (array-string-create) (buffer-create 0) (addr-of Int32 str-counter-var) out (array-string-create) (array-int32-create)))
      (expect-eq result 0)
    ) ; body
  ) ; test
) ; tests
```

### Key Principles

1. **Alignment:** Each closing parenthesis is indented to match its opening parenthesis
2. **Clarity:** Comments (e.g., `; body`, `; test`, `; tests`) make it immediately clear what each parenthesis closes
3. **Consistency:** Apply this rule uniformly across all code

### Error Patterns

- Multiple closing parentheses on the same line (e.g., `))))`), or compact
  closings like `))` at the end of a block, are near-certain syntax errors.
- Fix by splitting into one closing parenthesis per line and add an end-of-line
  comment indicating what is being closed.

❌ Incorrect:

```lisp
      (expect-eq result 0))))
```

✅ Correct:

```lisp
      (expect-eq result 0)
    ) ; body
  ) ; test
) ; tests
```

### Exceptions

Short, simple expressions on a single line are acceptable:

```lisp
(do 0)
(return 1)
(+ x y)
```

However, any structure with multiple nested levels should follow the one-per-line rule.

## Function Structure

Functions must follow this exact order:

```lisp
(fn function-name
  (doc "Description of what the function does")
  (params
    (param-name ParamType)
  )
  (returns ReturnType)
  (body
    ; function implementation
  )
  (tests
    (test test-name
      (doc "Test description")
      (tags tag1 tag2)
      (body
        ; test implementation
      )
    )
  )
)
```

**Important:** The `(tests ...)` section MUST come AFTER the `(body ...)` section, not before.

### Section Spacing

- Always insert a blank line between the closing of the `body` section and the opening of the `tests` section.
- This visual separation improves readability and helps avoid accidental multi-parenthesis runs.

## Indentation

- Use 2 spaces per indentation level
- Never use tabs
- Align nested structures consistently

## Comments

- Use `;;` for line comments within code
- Use `;` for end-of-line comments (especially for closing parentheses)
- Document complex logic with inline comments
- Every function must have a `(doc "...")` string

## File Size Limits

To encourage modular design and maintainability, file sizes are limited:

- **Soft limit: 256 lines** - Compiler warns but continues. Files exceeding this should be considered for refactoring.
- **Hard limit: 512 lines** - Compiler fails. Files must be split or refactored.

### Guidelines

- Files under 256 lines are ideal
- Files between 256-512 lines should have a clear reason (e.g., multiple related functions that logically belong together)
- Files over 512 lines indicate a need for refactoring:
  - **Single massive function**: Extract helper functions for logical sub-tasks
  - **Multiple unrelated things**: Split into separate modules with clear responsibilities
  
### Anti-patterns

❌ **Mechanical splitting**: Don't create `file_a.weave` and `file_b.weave` just to bypass the limit
✅ **Logical organization**: Split by responsibility (e.g., `compile_expr.weave`, `compile_stmt.weave`, `compile_types.weave`)

## Naming Conventions

- Function names: `kebab-case` (e.g., `compile-fn-body`)
- Variable names: `kebab-case` (e.g., `temp-var`)
- Struct names: `PascalCase` (e.g., `Arena`, `Buffer`)
- Constants: `SCREAMING_SNAKE_CASE` (if applicable)
