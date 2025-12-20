# Commit Changes Instructions

## Overview
When committing changes to this repository, use semantic commit messages following the Conventional Commits specification. Commit files one at a time for clarity.

## Semantic Commit Message Format

```
<type>(<scope>): <subject>

<body>
```

### Type
Must be one of:
- **feat**: A new feature
- **fix**: A bug fix
- **test**: Adding or updating tests
- **docs**: Documentation changes
- **refactor**: Code refactoring without changing functionality
- **perf**: Performance improvements
- **style**: Code style changes (formatting, whitespace, etc.)
- **build**: Build system or dependency changes
- **ci**: CI/CD configuration changes
- **chore**: Other changes that don't modify src or test files

### Scope
The scope indicates what part of the codebase is affected:
- `stage0` - C-based bootstrap compiler
- `stage1` - Self-hosted Weave compiler
- `stage2` - Future stage
- `docs` - Documentation
- `build` - Build system
- etc.

### Subject
- Use imperative mood ("add" not "added" or "adds")
- Don't capitalize first letter
- No period at the end
- Maximum 72 characters

### Body
- Separate from subject with blank line
- Explain WHAT and WHY, not HOW
- Include details like test counts, function names, etc.
- Wrap at 72 characters

## Workflow

### 1. Review Changes
First, check what files have been modified:
```bash
git status
git diff <file>
```

### 2. Commit One File at a Time
For each modified file:

```bash
git add <file>
git commit -m "<type>(<scope>): <subject>

<detailed body explaining the change>"
```

### 3. Examples

**Feature commit:**
```bash
git add stage0/src/program.c
git commit -m "feat(stage0): enforce tests section requirement for all functions

Add compile-time validation that requires every regular function to have
a non-empty (tests ...) section. Entry points are exempted as they are
application entry points rather than testable units.

- Detect entry points by checking form head for 'entry' keyword
- Validate presence of (tests ...) section for regular functions
- Validate tests section contains at least one test
- Exit with descriptive error message on validation failure"
```

**Test commit:**
```bash
git add stage1/src/arena.weave
git commit -m "test(stage1): add embedded tests for arena allocator functions

Add comprehensive test coverage for 10 arena data structure functions:
- node-kind-atom/list: enum constant value verification
- arena-new: empty array initialization
- arena-add-node: first and second node ID verification
- arena-kind/value: retrieval validation
- arena-first-child/next-sibling: default value checks
- arena-set-first-child/next-sibling: mutation verification

Total: 14 tests covering arena tree storage operations"
```

**Documentation commit:**
```bash
git add docs/embedded_tests.md
git commit -m "docs: add embedded testing documentation

Document the embedded testing system including syntax, usage,
test selection, and examples for the Weave compiler."
```

**Fix commit:**
```bash
git add stage0/src/lexer.c
git commit -m "fix(stage0): handle escaped quotes in string literals

String tokenizer now correctly processes \" escape sequences
within quoted strings, preventing premature string termination."
```

## Best Practices

1. **Inspect before committing**: Use `git diff <file>` to review exactly what changed
2. **One logical change per commit**: Don't mix unrelated changes
3. **Keep commits atomic**: Each commit should leave the codebase in a working state
4. **Be descriptive**: Future you (or others) should understand why this change was made
5. **Include metrics**: For test commits, mention how many tests were added
6. **List affected functions**: Helps reviewers understand scope of changes

## Common Patterns for This Project

### Adding Tests
```
test(stage1): add embedded tests for <module> functions

Add comprehensive test coverage for N <category> functions:
- function1: what it tests
- function2: what it tests
- ...

Total: X tests covering <what they cover>
```

### Compiler Features
```
feat(stage0): <what the feature does>

<Why this feature is needed>

- Implementation detail 1
- Implementation detail 2
- Edge cases handled
```

### Bug Fixes
```
fix(<scope>): <what was broken>

<Describe the bug and its impact>

Solution: <how it was fixed>
```

## Verification

After committing, verify with:
```bash
git log --oneline -5  # See recent commits
git show             # See last commit in detail
```

## Anti-Patterns to Avoid

❌ Don't: Vague messages
```
git commit -m "fix stuff"
git commit -m "update files"
```

❌ Don't: Committing multiple unrelated files together
```
git add stage0/src/*.c stage1/src/*.weave
git commit -m "various changes"
```

❌ Don't: Missing type/scope
```
git commit -m "added tests"
```

✅ Do: Clear, structured commits
```
git commit -m "test(stage1): add embedded tests for tokenizer functions"
```
