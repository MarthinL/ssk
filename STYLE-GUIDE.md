# Project Coding Style Guide

This document defines the coding conventions for this project. All contributors, maintainers, and automated tools (e.g., formatters) must follow these rules to ensure consistency and readability across the codebase.

## Multi-Line Expression Formatting Rules

1. **Logical Operator Alignment**:
   - Break the line **before logical operators** (e.g., `&&`, `||`).
   - Align the logical operator directly to the **right of the opening parenthesis** on the previous line.

2. **Comparison Operator Alignment**:
   - Align comparison operators (e.g., `>`, `<`, `==`, `!=`) **vertically across lines** to improve clarity.

3. **Operand Alignment**:
   - Ensure that operands in expressions are aligned vertically for better readability.

4. **Spacing Inside Parentheses**:
   - Do **not** add spaces after an opening parenthesis and the first parameter or condition, **unless such space is necessary for alignment of multi-line statements or conditions**.

5. **Opening Brace Placement**:
   - Place the opening brace `{` on the **same line** as the closing parenthesis `)` of the condition.

### Example
```c
if (     (first_expression    >  some_value)
     &&(second_expression  !=  another_value)) {
    do_something();
}
```

---

## General Formatting Rules

### Indentation
- Use **K&R style indentation**, but with the following modification:
  - The opening brace `{` in **function definitions** must appear in **column 1 on a new line**.
- Use **4 spaces per indentation level** (no tabs).

### Line Length
- Lines must not exceed **80 characters**.
- If an expression or statement exceeds the line limit:
  - Break the line **before operators**.
  - Wrap the line according to brackets and/or operator precedence.
  - Use indentation to highlight the nesting structure.

### Function Definition and Call Formatting
- **Function Definitions**:
  - Place the opening brace `{` in **column 1 on a new line**.
  - Example:
    ```c
    void my_function(int arg1, int arg2)
    {
        // Function body
    }
    ```
- **Function Calls**:
  - **No space** between a function name and the opening parenthesis `(`.
    - Correct: `my_function(arg1, arg2);`
    - Incorrect: `my_function (arg1, arg2);`
  - **Always a space** after a comma in argument lists.
    - Correct: `my_function(arg1, arg2);`
    - Incorrect: `my_function(arg1,arg2);`
  - **No space** between the opening parenthesis and the first parameter, or the last parameter and the closing parenthesis.
    - Correct: `my_function(arg1, arg2);`
    - Incorrect: `my_function( arg1, arg2 );`

---

## Naming Conventions


### Variables and Function Names
- Use **lowercase_with_underscores** for variable and function names.

#### Acronyms

- **In `CamelCase` typedefs**: Acronyms must be **fully capitalized** (e.g., `SSKSegment`, not `SskSegment`; `SSKDecoded`, not `SskDecoded`).
- **In `lowercase_with_underscores` function and variable names**: Acronyms must be **fully lowercase** (e.g., `ssk_add`, `buffer_ssk`, `ssk_partition_at`).
  - This applies regardless of whether the acronym appears at the start or elsewhere in the name.
  - The rationale: acronyms in snake_case names follow the general rule that all characters are lowercase, with underscores as separators.

#### Example Table

| Context              | Correct Name        | Incorrect Name      |
|----------------------|-------------------|-------------------|
| Typedef              | SSKSegment        | SskSegment         |
| Typedef              | SSKDecoded        | SskDecoded         |
| Function (acronym first) | ssk_add          | sSK_add            |
| Function (acronym first) | ssk_partition_at | sSK_partition_at   |
| Function (acronym not first) | add_ssk      | add_SSK            |
| Variable             | buffer_ssk        | buffer_SSK         |

### Constants, Macros, and Enum Member Values
- Use **UPPERCASE_WITH_UNDERSCORES** for all constants, macros, and enum member values.

Example:
```c
#define MAX_BUFFER_SIZE 1024

enum ErrorCodes {
    ERROR_NONE,
    ERROR_FILE_NOT_FOUND,
    ERROR_MEMORY_ALLOCATION_FAILED
};
```

### Typedefs
- Use **CamelCase** for `typedef` names.
- If the `typedef` represents a `struct` or `union`, the underlying type must be prefixed with an underscore.

Example:
```c
typedef struct _MyStruct {
    int field1;
    char field2;
} MyStruct;
```

---

## File Organization


### Header Guards
- Use `#ifndef`, `#define`, and `#endif` to guard header files.
- Name header guards in the format `PROJECT_FILENAME_H`, where `PROJECT` is the main acronym or project name, and `FILENAME` is the file name (without extension).
- For the main header file, use only the project acronym if appropriate (e.g., `SSK_H` for `ssk.h`).

Example:
```c
#ifndef SSK_H
#define SSK_H

// Declarations

#endif // SSK_H
```

### Commenting
- Use `/* ... */` for block comments and `//` for single-line comments.
- Document all functions and complex logic with descriptive comments.

Example:
```c
// This function calculates the sum of two integers.
int add_numbers(int a, int b)
{
    return a + b;
}
```
