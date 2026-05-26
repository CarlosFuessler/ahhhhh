#include "parser.h"
#include "buffer/buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_indent(int depth, FILE *out) {
  for (int i = 0; i < depth; i++) {
    fputs("  ", out);
  }
}

void ast_lvalue_free(AstLvalueExpression *node) {
  switch (node->kind) {
  case AST_LVALUE_IDENTIFIER:
    free(node->expr.identifier);
    break;
  case AST_LVALUE_BRACKETS:
    if (node->expr.brackets.base != NULL) {
      ast_expression_free(node->expr.brackets.base);
      free(node->expr.brackets.base);
    }
    if (node->expr.brackets.index != NULL) {
      ast_expression_free(node->expr.brackets.index);
      free(node->expr.brackets.index);
    }
    break;
  case AST_LVALUE_DOT:
    if (node->expr.dot.base != NULL) {
      ast_expression_free(node->expr.dot.base);
      free(node->expr.dot.base);
    }
    free(node->expr.dot.identifier);
    break;
  }
}

void ast_switch_clause_free(AstSwitchClause *clause) {
  if (clause->value != NULL) {
    ast_expression_free(clause->value);
    free(clause->value);
  }
  for (size_t i = 0; i < clause->len; i++) {
    ast_statement_free(&clause->statements[i]);
  }
  free(clause->statements);
  clause->value = NULL;
  clause->statements = NULL;
  clause->len = 0;
}

void ast_expression_free(AstExpression *node) {
  switch (node->kind) {
  case AST_EXPR_LITERAL:
    if (node->expr.literal.kind == AST_LITERAL_STRING) {
      free(node->expr.literal.expr.string.val);
    }
    break;
  case AST_EXPR_LVALUE:
    ast_lvalue_free(&node->expr.lvalue);
    break;
  case AST_EXPR_ASSIGNMENT:
    if (node->expr.assignment.left != NULL) {
      ast_lvalue_free(node->expr.assignment.left);
      free(node->expr.assignment.left);
    }
    if (node->expr.assignment.right != NULL) {
      ast_expression_free(node->expr.assignment.right);
      free(node->expr.assignment.right);
    }
    break;
  case AST_EXPR_COMPOUND_ASSIGN:
    if (node->expr.compound_assignment.left != NULL) {
      ast_lvalue_free(node->expr.compound_assignment.left);
      free(node->expr.compound_assignment.left);
    }
    if (node->expr.compound_assignment.right != NULL) {
      ast_expression_free(node->expr.compound_assignment.right);
      free(node->expr.compound_assignment.right);
    }
    break;
  case AST_EXPR_FUNCTION_CALL:
    if (node->expr.function_call.function != NULL) {
      ast_expression_free(node->expr.function_call.function);
      free(node->expr.function_call.function);
    }
    for (size_t i = 0; i < node->expr.function_call.arg_count; i++) {
      ast_expression_free(&node->expr.function_call.args[i]);
    }
    free(node->expr.function_call.args);
    if (node->expr.function_call.arg_names != NULL) {
      for (size_t i = 0; i < node->expr.function_call.arg_count; i++) {
        free(node->expr.function_call.arg_names[i]);
      }
      free(node->expr.function_call.arg_names);
    }
    break;
  case AST_EXPR_GROUP:
    if (node->expr.group.expr != NULL) {
      ast_expression_free(node->expr.group.expr);
      free(node->expr.group.expr);
    }
    break;
  case AST_EXPR_UNARY:
    free(node->expr.unary.keyword);
    if (node->expr.unary.operand != NULL) {
      ast_expression_free(node->expr.unary.operand);
      free(node->expr.unary.operand);
    }
    break;
  case AST_EXPR_BINARY:
    if (node->expr.binary.left != NULL) {
      ast_expression_free(node->expr.binary.left);
      free(node->expr.binary.left);
    }
    if (node->expr.binary.right != NULL) {
      ast_expression_free(node->expr.binary.right);
      free(node->expr.binary.right);
    }
    break;
  case AST_EXPR_ARRAY_LITERAL:
    for (size_t i = 0; i < node->expr.array_literal.element_count; i++) {
      ast_expression_free(&node->expr.array_literal.elements[i]);
    }
    free(node->expr.array_literal.elements);
    break;
  }
}

void ast_block_free(AstBlock *node) {
  for (size_t i = 0; i < node->len; i++) {
    ast_statement_free(&node->statements[i]);
  }
  free(node->statements);
  node->statements = NULL;
  node->len = 0;
}

void ast_statement_free(AstStatement *node) {
  switch (node->kind) {
  case AST_STMT_VAR_DECL:
    free(node->stmt.var_decl.name);
    free(node->stmt.var_decl.type_name);
    if (node->stmt.var_decl.initializer != NULL) {
      ast_expression_free(node->stmt.var_decl.initializer);
      free(node->stmt.var_decl.initializer);
    }
    break;
  case AST_STMT_RETURN:
    if (node->stmt.return_stmt.value != NULL) {
      ast_expression_free(node->stmt.return_stmt.value);
      free(node->stmt.return_stmt.value);
    }
    break;
  case AST_STMT_IF:
    if (node->stmt.if_stmt.condition != NULL) {
      ast_expression_free(node->stmt.if_stmt.condition);
      free(node->stmt.if_stmt.condition);
    }
    if (node->stmt.if_stmt.then_block != NULL) {
      ast_block_free(node->stmt.if_stmt.then_block);
      free(node->stmt.if_stmt.then_block);
    }
    if (node->stmt.if_stmt.else_branch != NULL) {
      ast_statement_free(node->stmt.if_stmt.else_branch);
      free(node->stmt.if_stmt.else_branch);
    }
    break;
  case AST_STMT_WHILE:
    if (node->stmt.while_stmt.condition != NULL) {
      ast_expression_free(node->stmt.while_stmt.condition);
      free(node->stmt.while_stmt.condition);
    }
    if (node->stmt.while_stmt.body != NULL) {
      ast_block_free(node->stmt.while_stmt.body);
      free(node->stmt.while_stmt.body);
    }
    break;
  case AST_STMT_FOR_RANGE:
    free(node->stmt.for_stmt.name);
    if (node->stmt.for_stmt.start != NULL) {
      ast_expression_free(node->stmt.for_stmt.start);
      free(node->stmt.for_stmt.start);
    }
    if (node->stmt.for_stmt.end != NULL) {
      ast_expression_free(node->stmt.for_stmt.end);
      free(node->stmt.for_stmt.end);
    }
    if (node->stmt.for_stmt.body != NULL) {
      ast_block_free(node->stmt.for_stmt.body);
      free(node->stmt.for_stmt.body);
    }
    break;
  case AST_STMT_SWITCH:
    if (node->stmt.switch_stmt.subject != NULL) {
      ast_expression_free(node->stmt.switch_stmt.subject);
      free(node->stmt.switch_stmt.subject);
    }
    for (size_t i = 0; i < node->stmt.switch_stmt.clause_count; i++) {
      ast_switch_clause_free(&node->stmt.switch_stmt.clauses[i]);
    }
    free(node->stmt.switch_stmt.clauses);
    break;
  case AST_STMT_EXPR:
    if (node->stmt.expr_stmt.expr != NULL) {
      ast_expression_free(node->stmt.expr_stmt.expr);
      free(node->stmt.expr_stmt.expr);
    }
    break;
  case AST_STMT_BLOCK:
    ast_block_free(&node->stmt.block);
    break;
  case AST_STMT_FN_DECL:
    free(node->stmt.fn_decl.name);
    for (size_t i = 0; i < node->stmt.fn_decl.param_count; i++) {
      free(node->stmt.fn_decl.params[i]);
      if (node->stmt.fn_decl.param_types && node->stmt.fn_decl.param_types[i])
        free(node->stmt.fn_decl.param_types[i]);
    }
    free(node->stmt.fn_decl.params);
    if (node->stmt.fn_decl.param_types)
      free(node->stmt.fn_decl.param_types);
    free(node->stmt.fn_decl.return_type);
    if (node->stmt.fn_decl.body != NULL) {
      ast_block_free(node->stmt.fn_decl.body);
      free(node->stmt.fn_decl.body);
    }
    break;
  case AST_STMT_TYPE_DECL:
    for (size_t i = 0; i < node->stmt.type_decl.field_count; i++) {
      free(node->stmt.type_decl.fields[i]);
      if (node->stmt.type_decl.field_types && node->stmt.type_decl.field_types[i])
          free(node->stmt.type_decl.field_types[i]);
    }
    free(node->stmt.type_decl.fields);
    if (node->stmt.type_decl.field_types) free(node->stmt.type_decl.field_types);
    free(node->stmt.type_decl.name);
    break;
  case AST_STMT_ANNOTATION:
    free(node->stmt.annotation.value);
    if (node->stmt.annotation.alias != NULL) {
      free(node->stmt.annotation.alias);
    }
    break;
  case AST_STMT_BREAK:
  case AST_STMT_CONTINUE:
    break;
  }
}

void ast_program_free(AstProgram *node) {
  for (size_t i = 0; i < node->len; i++) {
    ast_statement_free(&node->statements[i]);
  }
  free(node->statements);
  node->statements = NULL;
  node->len = 0;
}

void ast_lvalue_print(AstLvalueExpression *node, int depth, FILE *out);

void ast_expression_print(AstExpression *node, int depth, FILE *out) {
  switch (node->kind) {
  case AST_EXPR_LITERAL:
    print_indent(depth, out);
    if (node->expr.literal.kind == AST_LITERAL_NUMBER) {
      fprintf(out, "Literal(Number %.15g)\n", node->expr.literal.expr.number.val);
    } else if (node->expr.literal.kind == AST_LITERAL_STRING) {
      fprintf(out, "Literal(String \"%s\")\n",
              node->expr.literal.expr.string.val == NULL
                  ? ""
                  : node->expr.literal.expr.string.val);
    } else if (node->expr.literal.kind == AST_LITERAL_BOOL) {
      fprintf(out, "Literal(Bool %s)\n",
              node->expr.literal.expr.boolean.val ? "true" : "false");
    } else {
      fprintf(out, "Literal(Null)\n");
    }
    break;
  case AST_EXPR_LVALUE:
    ast_lvalue_print(&node->expr.lvalue, depth, out);
    break;
  case AST_EXPR_ASSIGNMENT:
    print_indent(depth, out);
    fprintf(out, "Assignment\n");
    print_indent(depth + 1, out);
    fprintf(out, "Left:\n");
    ast_lvalue_print(node->expr.assignment.left, depth + 2, out);
    print_indent(depth + 1, out);
    fprintf(out, "Right:\n");
    ast_expression_print(node->expr.assignment.right, depth + 2, out);
    break;
  case AST_EXPR_COMPOUND_ASSIGN:
    print_indent(depth, out);
    fprintf(out, "CompoundAssignment(%s)\n",
            token_type_name(node->expr.compound_assignment.op));
    print_indent(depth + 1, out);
    fprintf(out, "Left:\n");
    ast_lvalue_print(node->expr.compound_assignment.left, depth + 2, out);
    print_indent(depth + 1, out);
    fprintf(out, "Right:\n");
    ast_expression_print(node->expr.compound_assignment.right, depth + 2, out);
    break;
  case AST_EXPR_FUNCTION_CALL:
    print_indent(depth, out);
    fprintf(out, "FunctionCall\n");
    print_indent(depth + 1, out);
    fprintf(out, "Function:\n");
    ast_expression_print(node->expr.function_call.function, depth + 2, out);
    print_indent(depth + 1, out);
    fprintf(out, "Args(%zu):\n", node->expr.function_call.arg_count);
    for (size_t i = 0; i < node->expr.function_call.arg_count; i++) {
      if (node->expr.function_call.arg_names != NULL &&
          node->expr.function_call.arg_names[i] != NULL) {
        print_indent(depth + 2, out);
        fprintf(out, "Field(%s):\n", node->expr.function_call.arg_names[i]);
        ast_expression_print(&node->expr.function_call.args[i], depth + 3, out);
      } else {
        ast_expression_print(&node->expr.function_call.args[i], depth + 2, out);
      }
    }
    break;
  case AST_EXPR_GROUP:
    print_indent(depth, out);
    fprintf(out, "Group\n");
    ast_expression_print(node->expr.group.expr, depth + 1, out);
    break;
  case AST_EXPR_UNARY:
    print_indent(depth, out);
    if (node->expr.unary.op == TOKEN_KEYWORD) {
      fprintf(out, "Unary(%s)\n",
              node->expr.unary.keyword == NULL ? "keyword" : node->expr.unary.keyword);
    } else {
      fprintf(out, "Unary(%s)\n", token_type_name(node->expr.unary.op));
    }
    ast_expression_print(node->expr.unary.operand, depth + 1, out);
    break;
  case AST_EXPR_BINARY:
    print_indent(depth, out);
    fprintf(out, "Binary(%s)\n", token_type_name(node->expr.binary.op));
    ast_expression_print(node->expr.binary.left, depth + 1, out);
    ast_expression_print(node->expr.binary.right, depth + 1, out);
    break;
  case AST_EXPR_ARRAY_LITERAL:
    print_indent(depth, out);
    fprintf(out, "ArrayLiteral(%zu elements)\n", node->expr.array_literal.element_count);
    for (size_t i = 0; i < node->expr.array_literal.element_count; i++) {
      ast_expression_print(&node->expr.array_literal.elements[i], depth + 1, out);
    }
    break;
  }
}

void ast_lvalue_print(AstLvalueExpression *node, int depth, FILE *out) {
  switch (node->kind) {
  case AST_LVALUE_IDENTIFIER:
    print_indent(depth, out);
    fprintf(out, "LValue(Identifier %s)\n",
            node->expr.identifier == NULL ? "" : node->expr.identifier);
    break;
  case AST_LVALUE_BRACKETS:
    print_indent(depth, out);
    fprintf(out, "LValue(Index)\n");
    print_indent(depth + 1, out);
    fprintf(out, "Base:\n");
    ast_expression_print(node->expr.brackets.base, depth + 2, out);
    print_indent(depth + 1, out);
    fprintf(out, "Index:\n");
    ast_expression_print(node->expr.brackets.index, depth + 2, out);
    break;
  case AST_LVALUE_DOT:
    print_indent(depth, out);
    fprintf(out, "LValue(Dot %s)\n",
            node->expr.dot.identifier == NULL ? "" : node->expr.dot.identifier);
    ast_expression_print(node->expr.dot.base, depth + 1, out);
    break;
  }
}

void ast_block_print(AstBlock *node, int depth, FILE *out) {
  print_indent(depth, out);
  fprintf(out, "Block(%zu)\n", node->len);
  for (size_t i = 0; i < node->len; i++) {
    ast_statement_print(&node->statements[i], depth + 1, out);
  }
}

void ast_statement_print(AstStatement *node, int depth, FILE *out) {
  switch (node->kind) {
  case AST_STMT_VAR_DECL:
    print_indent(depth, out);
    fprintf(out, "%sDecl(%s",
            node->stmt.var_decl.is_const ? "Const" : "Var",
            node->stmt.var_decl.name == NULL ? "" : node->stmt.var_decl.name);
    if (node->stmt.var_decl.type_name != NULL) {
      fprintf(out, ": %s", node->stmt.var_decl.type_name);
    }
    fprintf(out, ")\n");
    if (node->stmt.var_decl.initializer != NULL) {
      ast_expression_print(node->stmt.var_decl.initializer, depth + 1, out);
    }
    break;
  case AST_STMT_RETURN:
    print_indent(depth, out);
    fprintf(out, "Return\n");
    if (node->stmt.return_stmt.has_value && node->stmt.return_stmt.value != NULL) {
      ast_expression_print(node->stmt.return_stmt.value, depth + 1, out);
    }
    break;
  case AST_STMT_IF:
    print_indent(depth, out);
    fprintf(out, "If\n");
    print_indent(depth + 1, out);
    fprintf(out, "Condition:\n");
    ast_expression_print(node->stmt.if_stmt.condition, depth + 2, out);
    print_indent(depth + 1, out);
    fprintf(out, "Then:\n");
    ast_block_print(node->stmt.if_stmt.then_block, depth + 2, out);
    if (node->stmt.if_stmt.else_branch != NULL) {
      print_indent(depth + 1, out);
      fprintf(out, "Else:\n");
      ast_statement_print(node->stmt.if_stmt.else_branch, depth + 2, out);
    }
    break;
  case AST_STMT_WHILE:
    print_indent(depth, out);
    fprintf(out, "While\n");
    ast_expression_print(node->stmt.while_stmt.condition, depth + 1, out);
    ast_block_print(node->stmt.while_stmt.body, depth + 1, out);
    break;
  case AST_STMT_FOR_RANGE:
    print_indent(depth, out);
    fprintf(out, "ForRange(%s)\n",
            node->stmt.for_stmt.name == NULL ? "" : node->stmt.for_stmt.name);
    print_indent(depth + 1, out);
    fprintf(out, "Start:\n");
    ast_expression_print(node->stmt.for_stmt.start, depth + 2, out);
    print_indent(depth + 1, out);
    fprintf(out, "End:\n");
    ast_expression_print(node->stmt.for_stmt.end, depth + 2, out);
    ast_block_print(node->stmt.for_stmt.body, depth + 1, out);
    break;
  case AST_STMT_SWITCH:
    print_indent(depth, out);
    fprintf(out, "Switch\n");
    print_indent(depth + 1, out);
    fprintf(out, "Subject:\n");
    ast_expression_print(node->stmt.switch_stmt.subject, depth + 2, out);
    print_indent(depth + 1, out);
    fprintf(out, "Clauses(%zu):\n", node->stmt.switch_stmt.clause_count);
    for (size_t i = 0; i < node->stmt.switch_stmt.clause_count; i++) {
      AstSwitchClause *clause = &node->stmt.switch_stmt.clauses[i];
      print_indent(depth + 2, out);
      if (clause->is_default) {
        fprintf(out, "Default:\n");
      } else {
        fprintf(out, "Case:\n");
        ast_expression_print(clause->value, depth + 3, out);
      }
      for (size_t j = 0; j < clause->len; j++) {
        ast_statement_print(&clause->statements[j], depth + 3, out);
      }
    }
    break;
  case AST_STMT_BREAK:
    print_indent(depth, out);
    fprintf(out, "Break\n");
    break;
  case AST_STMT_CONTINUE:
    print_indent(depth, out);
    fprintf(out, "Continue\n");
    break;
  case AST_STMT_EXPR:
    print_indent(depth, out);
    fprintf(out, "ExprStmt\n");
    ast_expression_print(node->stmt.expr_stmt.expr, depth + 1, out);
    break;
  case AST_STMT_BLOCK:
    ast_block_print(&node->stmt.block, depth, out);
    break;
  case AST_STMT_FN_DECL:
    print_indent(depth, out);
    fprintf(out, "FnDecl(%s%s)\n", node->stmt.fn_decl.is_export ? "export " : "",
            node->stmt.fn_decl.name == NULL ? "" : node->stmt.fn_decl.name);
    print_indent(depth + 1, out);
    fprintf(out, "Params(%zu):\n", node->stmt.fn_decl.param_count);
    for (size_t i = 0; i < node->stmt.fn_decl.param_count; i++) {
      print_indent(depth + 2, out);
      fprintf(out, "%s\n", node->stmt.fn_decl.params[i]);
    }
    if (node->stmt.fn_decl.return_type != NULL) {
      print_indent(depth + 1, out);
      fprintf(out, "ReturnType: %s\n", node->stmt.fn_decl.return_type);
    }
    ast_block_print(node->stmt.fn_decl.body, depth + 1, out);
    break;
  case AST_STMT_TYPE_DECL:
    print_indent(depth, out);
    fprintf(out, "%sDecl(%s)\n",
            node->stmt.type_decl.is_enum ? "Enum" : "Struct",
            node->stmt.type_decl.name == NULL ? "" : node->stmt.type_decl.name);
    if (!node->stmt.type_decl.is_enum && node->stmt.type_decl.field_count > 0) {
      print_indent(depth + 1, out);
      fprintf(out, "Fields(%zu):\n", node->stmt.type_decl.field_count);
      for (size_t i = 0; i < node->stmt.type_decl.field_count; i++) {
        print_indent(depth + 2, out);
        fprintf(out, "%s", node->stmt.type_decl.fields[i]);
        if (node->stmt.type_decl.field_types && node->stmt.type_decl.field_types[i]) {
            fprintf(out, ": %s", node->stmt.type_decl.field_types[i]);
        }
        fprintf(out, "\n");
      }
    }
    break;
  case AST_STMT_ANNOTATION:
    print_indent(depth, out);
    fprintf(out, "Annotation(@(%s))\n",
            node->stmt.annotation.value == NULL ? "" : node->stmt.annotation.value);
    break;
  }
}

void ast_program_print(AstProgram *node, FILE *out) {
  fprintf(out, "Program(%zu)\n", node->len);
  for (size_t i = 0; i < node->len; i++) {
    ast_statement_print(&node->statements[i], 1, out);
  }
}