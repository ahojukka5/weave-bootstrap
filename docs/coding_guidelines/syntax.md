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

## Naming Conventions

- Function names: `kebab-case` (e.g., `compile-fn-body`)
- Variable names: `kebab-case` (e.g., `temp-var`)
- Struct names: `PascalCase` (e.g., `Arena`, `Buffer`)
- Constants: `SCREAMING_SNAKE_CASE` (if applicable)
