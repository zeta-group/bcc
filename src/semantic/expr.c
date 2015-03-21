#include "phase.h"

struct operand {
   struct func* func;
   struct dim* dim;
   struct region* region;
   struct type* type;
   int value;
   bool complete;
   bool usable;
   bool assignable;
   bool folded;
   bool in_paren;
};

struct call_test {
   struct call* call;
   struct func* func;
   list_iter_t* i;
   int num_args;
};

static void test_expr( struct semantic* phase, struct expr_test*, struct expr*,
   struct operand* );
static void init_operand( struct operand* );
static void use_object( struct semantic* phase, struct expr_test*, struct operand*,
   struct object* );
static void test_node( struct semantic* phase, struct expr_test*, struct operand*,
   struct node* );
static void test_literal( struct semantic* phase, struct operand*, struct literal* );
static void test_string_usage( struct semantic* phase, struct expr_test*,
   struct operand*, struct indexed_string_usage* );
static void test_boolean( struct semantic* phase, struct operand*, struct boolean* );
static void test_name_usage( struct semantic* phase, struct expr_test*, struct operand*,
   struct name_usage* );
static struct object* find_usage_object( struct semantic* phase, struct name_usage* );
static void test_unary( struct semantic* phase, struct expr_test*, struct operand*,
   struct unary* );
static void test_subscript( struct semantic* phase, struct expr_test*, struct operand*,
   struct subscript* );
static void test_call( struct semantic* phase, struct expr_test*, struct operand*,
   struct call* );
static void test_call_args( struct semantic* phase, struct expr_test*,
   struct call_test* );
static void test_call_first_arg( struct semantic* phase, struct expr_test*,
   struct call_test* );
static void test_call_format_arg( struct semantic* phase, struct expr_test*,
   struct call_test* );
static void test_format_item( struct semantic* phase, struct stmt_test* stmt_test,
   struct expr_test* test, struct block* format_block,
   struct format_item* item );
static void test_array_format_item( struct semantic* phase,
   struct stmt_test* stmt_test, struct expr_test* test,
   struct block* format_block, struct format_item* item );
static void test_binary( struct semantic* phase, struct expr_test*, struct operand*,
   struct binary* );
static void test_assign( struct semantic* phase, struct expr_test*, struct operand*,
   struct assign* );
static void test_access( struct semantic* phase, struct expr_test*, struct operand*,
   struct access* );
static void test_region_access( struct semantic* phase, struct expr_test* test,
   struct operand* operand, struct access* access );
static void test_struct_access( struct semantic* phase, struct expr_test* test,
   struct operand* operand, struct access* access );

void s_init_expr_test( struct expr_test* test, struct stmt_test* stmt_test,
   struct block* format_block, bool result_required, bool undef_err,
   bool suggest_paren_assign ) {
   test->stmt_test = stmt_test;
   test->format_block = format_block;
   test->format_block_usage = NULL;
   test->result_required = result_required;
   test->has_string = false;
   test->undef_err = undef_err;
   test->undef_erred = false;
   test->accept_array = false;
   test->suggest_paren_assign = suggest_paren_assign;
}

void init_operand( struct operand* operand ) {
   operand->func = NULL;
   operand->dim = NULL;
   operand->type = NULL;
   operand->region = NULL;
   operand->value = 0;
   operand->complete = false;
   operand->usable = false;
   operand->assignable = false;
   operand->folded = false;
   operand->in_paren = false;
}

void s_test_expr( struct semantic* phase, struct expr_test* test,
   struct expr* expr ) {
   struct operand operand;
   init_operand( &operand );
   test_expr( phase, test, expr, &operand );
}

void test_expr( struct semantic* phase, struct expr_test* test, struct expr* expr,
   struct operand* operand ) {
   if ( setjmp( test->bail ) == 0 ) {
      test_node( phase, test, operand, expr->root );
      if ( ! operand->complete ) {
         s_diag( phase, DIAG_POS_ERR, &expr->pos,
            "expression incomplete" );
         s_bail( phase );
      }
      if ( test->result_required && ! operand->usable ) {
         s_diag( phase, DIAG_POS_ERR, &expr->pos,
            "expression does not produce a value" );
         s_bail( phase );
      }
      expr->folded = operand->folded;
      expr->value = operand->value;
      test->pos = expr->pos;
   }
}

void test_node( struct semantic* phase, struct expr_test* test,
   struct operand* operand, struct node* node ) {
   switch ( node->type ) {
   case NODE_LITERAL:
      test_literal( phase, operand, ( struct literal* ) node );
      break;
   case NODE_INDEXED_STRING_USAGE:
      test_string_usage( phase, test, operand,
         ( struct indexed_string_usage* ) node );
      break;
   case NODE_BOOLEAN:
      test_boolean( phase, operand, ( struct boolean* ) node );
      break;
   case NODE_REGION_HOST:
      operand->region = phase->region;
      break;
   case NODE_REGION_UPMOST:
      operand->region = phase->task->region_upmost;
      break;
   case NODE_NAME_USAGE:
      test_name_usage( phase, test, operand, ( struct name_usage* ) node );
      break;
   case NODE_UNARY:
      test_unary( phase, test, operand, ( struct unary* ) node );
      break;
   case NODE_SUBSCRIPT:
      test_subscript( phase, test, operand, ( struct subscript* ) node );
      break;
   case NODE_CALL:
      test_call( phase, test, operand, ( struct call* ) node );
      break;
   case NODE_BINARY:
      test_binary( phase, test, operand, ( struct binary* ) node );
      break;
   case NODE_ASSIGN:
      test_assign( phase, test, operand, ( struct assign* ) node );
      break;
   case NODE_ACCESS:
      test_access( phase, test, operand, ( struct access* ) node );
      break;
   case NODE_PAREN:
      {
         operand->in_paren = true;
         struct paren* paren = ( struct paren* ) node;
         test_node( phase, test, operand, paren->inside );
         operand->in_paren = false;
      }
      break;
   default:
      break;
   }
}

void test_literal( struct semantic* phase, struct operand* operand,
   struct literal* literal ) {
   operand->value = literal->value;
   operand->folded = true;
   operand->complete = true;
   operand->usable = true;
   operand->type = phase->task->type_int;
}

void test_string_usage( struct semantic* phase, struct expr_test* test,
   struct operand* operand, struct indexed_string_usage* usage ) {
   operand->value = usage->string->index;
   operand->folded = true;
   operand->complete = true;
   operand->usable = true;
   operand->type = phase->task->type_str;
   test->has_string = true;
}

void test_boolean( struct semantic* phase, struct operand* operand,
   struct boolean* boolean ) {
   operand->value = boolean->value;
   operand->folded = true;
   operand->complete = true;
   operand->usable = true;
   operand->type = phase->task->type_bool;
}

void test_name_usage( struct semantic* phase, struct expr_test* test,
   struct operand* operand, struct name_usage* usage ) {
   struct object* object = find_usage_object( phase, usage );
   if ( object && object->resolved ) {
      use_object( phase, test, operand, object );
      usage->object = &object->node;
   }
   // Object not found or isn't valid.
   else {
      if ( test->undef_err ) {
         if ( object ) {
            s_diag( phase, DIAG_POS_ERR, &usage->pos,
               "`%s` undefined", usage->text );
         }
         else {
            s_diag( phase, DIAG_POS_ERR, &usage->pos,
               "`%s` not found", usage->text );
         }
         s_bail( phase );
      }
      else {
         test->undef_erred = true;
         longjmp( test->bail, 1 );
      }
   }
}

struct object* find_usage_object( struct semantic* phase,
   struct name_usage* usage ) {
   struct object* object = NULL;
   struct name* name;
   // Try searching in the hidden compartment of the library.
   if ( usage->lib_id ) {
      list_iter_t i;
      list_iter_init( &i, &phase->task->libraries );
      struct library* lib;
      while ( true ) {
         lib = list_data( &i );
         if ( lib->id == usage->lib_id ) {
            break;
         }
         list_next( &i );
      }
      name = t_make_name( phase->task, usage->text, lib->hidden_names );
      if ( name->object ) {
         object = name->object;
         goto done;
      }
   }
   // Try searching in the current scope.
   name = t_make_name( phase->task, usage->text, phase->region->body );
   if ( name->object ) {
      object = name->object;
      goto done;
   }
   // Try searching in any of the linked regions.
   struct region_link* link = phase->region->link;
   while ( link && ! object ) {
      name = t_make_name( phase->task, usage->text, link->region->body );
      object = t_get_region_object( phase->task, link->region, name );
      link = link->next;
   }
   // Object could not be found.
   if ( ! object ) {
      goto done;
   }
   // If an object is found through a region link, make sure no other object
   // with the same name can be found using any remaining region link. We can
   // use the first object found, but I'd rather we disallow the usage when
   // multiple objects are visible.
   int dup = 0;
   while ( link ) {
      name = t_make_name( phase->task, usage->text, link->region->body );
      if ( name->object ) {
         if ( ! dup ) {
            s_diag( phase, DIAG_POS_ERR, &usage->pos,
               "multiple objects with name `%s`", usage->text );
            s_diag( phase, DIAG_FILE | DIAG_LINE | DIAG_COLUMN, &object->pos,
               "object found here" );
         }
         s_diag( phase, DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
            &name->object->pos, "object found here" );
         ++dup;
      }
      link = link->next;
   }
   if ( dup ) {
      t_bail( phase->task );
   }
   done:
   return object;
}

void use_object( struct semantic* phase, struct expr_test* test,
   struct operand* operand, struct object* object ) {
   if ( object->node.type == NODE_REGION ) {
      operand->region = ( struct region* ) object;
   }
   else if ( object->node.type == NODE_CONSTANT ) {
      struct constant* constant = ( struct constant* ) object;
      operand->type = phase->task->type_int;
      operand->value = constant->value;
      operand->folded = true;
      operand->complete = true;
      operand->usable = true;
   }
   else if ( object->node.type == NODE_VAR ) {
      struct var* var = ( struct var* ) object;
      operand->type = var->type;
      if ( var->dim ) {
         operand->dim = var->dim;
         // NOTE: I'm not too happy with the idea of this field. It is
         // contagious. Blame the Hypnotoad.
         if ( test->accept_array ) {
            operand->complete = true;
         }
      }
      else if ( var->type->primitive ) {
         operand->complete = true;
         operand->usable = true;
         operand->assignable = true;
      }
      var->used = true;
   }
   else if ( object->node.type == NODE_TYPE_MEMBER ) {
      struct type_member* member = ( struct type_member* ) object;
      operand->type = member->type;
      if ( member->dim ) {
         operand->dim = member->dim;
         if ( test->accept_array ) {
            operand->complete = true;
         }
      }
      else if ( member->type->primitive ) {
         operand->complete = true;
         operand->usable = true;
         operand->assignable = true;
      }
   }
   else if ( object->node.type == NODE_FUNC ) {
      operand->func = ( struct func* ) object;
      if ( operand->func->type == FUNC_USER ) {
         struct func_user* impl = operand->func->impl;
         ++impl->usage;
      }
      // When using just the name of an action special, a value is produced,
      // which is the ID of the action special.
      else if ( operand->func->type == FUNC_ASPEC ) {
         operand->complete = true;
         operand->usable = true;
         struct func_aspec* impl = operand->func->impl;
         operand->value = impl->id;
         operand->folded = true;
      }
   }
   else if ( object->node.type == NODE_PARAM ) {
      struct param* param = ( struct param* ) object;
      operand->type = param->type;
      operand->complete = true;
      operand->usable = true;
      operand->assignable = true;
   }
   else if ( object->node.type == NODE_ALIAS ) {
      struct alias* alias = ( struct alias* ) object;
      use_object( phase, test, operand, alias->target );
   }
}

void test_unary( struct semantic* phase, struct expr_test* test,
   struct operand* operand, struct unary* unary ) {
   struct operand target;
   init_operand( &target );
   if ( unary->op == UOP_PRE_INC || unary->op == UOP_PRE_DEC ||
      unary->op == UOP_POST_INC || unary->op == UOP_POST_DEC ) {
      test_node( phase, test, &target, unary->operand );
      // Only an l-value can be incremented.
      if ( ! target.assignable ) {
         const char* action = "incremented";
         if ( unary->op == UOP_PRE_DEC || unary->op == UOP_POST_DEC ) {
            action = "decremented";
         }
         s_diag( phase, DIAG_POS_ERR, &unary->pos, "operand cannot be %s",
            action );
         s_bail( phase );
      }
   }
   else {
      test_node( phase, test, &target, unary->operand );
      // Remaining operations require a value to work on.
      if ( ! target.usable ) {
         s_diag( phase, DIAG_POS_ERR, &unary->pos,
            "operand of unary operation not a value" );
         s_bail( phase );
      }
   }
   // Compile-time evaluation.
   if ( target.folded ) {
      switch ( unary->op ) {
      case UOP_MINUS:
         operand->value = ( - target.value );
         break;
      case UOP_PLUS:
         operand->value = target.value;
         break;
      case UOP_LOG_NOT:
         operand->value = ( ! target.value );
         break;
      case UOP_BIT_NOT:
         operand->value = ( ~ target.value );
         break;
      default:
         break;
      }
      operand->folded = true;
   }
   operand->complete = true;
   operand->usable = true;
   // Type of the result.
   if ( unary->op == UOP_LOG_NOT ) {
      operand->type = phase->task->type_bool;
   }
   else {
      operand->type = phase->task->type_int;
   }
}

void test_subscript( struct semantic* phase, struct expr_test* test,
   struct operand* operand, struct subscript* subscript ) {
   struct operand lside;
   init_operand( &lside );
   test_node( phase, test, &lside, subscript->lside );
   if ( ! lside.dim ) {
      s_diag( phase, DIAG_POS_ERR, &subscript->pos,
         "operand not an array" );
      s_bail( phase );
   }
   struct expr_test index;
   s_init_expr_test( &index, test->stmt_test, test->format_block, true,
      test->undef_err, false );
   s_test_expr( phase, &index, subscript->index );
   if ( index.undef_erred ) {
      test->undef_erred = true;
      longjmp( test->bail, 1 );
   }
   // Out-of-bounds warning for a constant index.
   if ( lside.dim->size && subscript->index->folded && (
      subscript->index->value < 0 ||
      subscript->index->value >= lside.dim->size ) ) {
      s_diag( phase, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &subscript->index->pos, "array index out-of-bounds" );
   }
   operand->type = lside.type;
   operand->dim = lside.dim->next;
   if ( operand->dim ) {
      if ( test->accept_array ) {
         operand->complete = true;
      }
   }
   else {
      if ( operand->type->primitive ) {
         operand->complete = true;
         operand->usable = true;
         operand->assignable = true;
      }
   }
}

void test_call( struct semantic* phase, struct expr_test* expr_test,
   struct operand* operand, struct call* call ) {
   struct call_test test = {
      .call = call,
      .func = NULL,
      .i = NULL,
      .num_args = 0
   };
   struct operand callee;
   init_operand( &callee );
   test_node( phase, expr_test, &callee, call->operand );
   if ( ! callee.func ) {
      s_diag( phase, DIAG_POS_ERR, &call->pos,
         "operand not a function" );
      s_bail( phase );
   }
   test.func = callee.func;
   test_call_args( phase, expr_test, &test );
   // Some action-specials cannot be called from a script.
   if ( test.func->type == FUNC_ASPEC ) {
      struct func_aspec* impl = test.func->impl;
      if ( ! impl->script_callable ) {
         struct str str;
         str_init( &str );
         t_copy_name( test.func->name, false, &str );
         s_diag( phase, DIAG_POS_ERR, &call->pos,
            "action-special `%s` called from script", str.value );
         s_bail( phase );
      }
   }
   // Latent function cannot be called in a function or a format block.
   if ( test.func->type == FUNC_DED ) {
      struct func_ded* impl = test.func->impl;
      if ( impl->latent ) {
         bool erred = false;
         struct stmt_test* stmt = expr_test->stmt_test;
         while ( stmt && ! stmt->format_block ) {
            stmt = stmt->parent;
         }
         if ( stmt || phase->func_test->func ) {
            s_diag( phase, DIAG_POS_ERR, &call->pos,
               "calling latent function inside a %s",
               stmt ? "format block" : "function" );
            // Show educational note to user.
            if ( phase->func_test->func ) {
               struct str str;
               str_init( &str );
               t_copy_name( test.func->name, false, &str );
               s_diag( phase, DIAG_FILE, &call->pos,
                  "waiting functions like `%s` can only be called inside a "
                  "script", str.value );
            }
            s_bail( phase );
         }
      }
   }
   call->func = test.func;
   operand->type = test.func->return_type;
   operand->complete = true;
   if ( test.func->return_type ) {
      operand->usable = true;
   }
   if ( call->func->type == FUNC_USER ) {
      struct func_user* impl = call->func->impl;
      if ( impl->nested ) {
         struct nested_call* nested = mem_alloc( sizeof( *nested ) );
         nested->next = impl->nested_calls;
         nested->id = 0;
         nested->enter_pos = 0;
         nested->leave_pos = 0;
         impl->nested_calls = call;
         call->nested_call = nested;
      }
   }
}

void test_call_args( struct semantic* phase, struct expr_test* expr_test,
   struct call_test* test ) {
   struct call* call = test->call;
   struct func* func = test->func;
   list_iter_t i;
   list_iter_init( &i, &call->args );
   test->i = &i;
   test_call_first_arg( phase, expr_test, test );
   // Test remaining arguments.
   while ( ! list_end( &i ) ) {
      struct expr_test arg;
      s_init_expr_test( &arg, expr_test->stmt_test, expr_test->format_block,
         true, expr_test->undef_err, false );
      s_test_expr( phase, &arg, list_data( &i ) );
      if ( arg.undef_erred ) {
         expr_test->undef_erred = true;
         longjmp( expr_test->bail, 1 );
      }
      ++test->num_args;
      list_next( &i );
   }
   // Number of arguments must be correct.
   if ( test->num_args < func->min_param ) {
      s_diag( phase, DIAG_POS_ERR, &call->pos,
         "not enough arguments in function call" );
      struct str str;
      str_init( &str );
      t_copy_name( func->name, false, &str );
      s_diag( phase, DIAG_FILE, &call->pos,
         "function `%s` needs %s%d argument%s", str.value,
         func->min_param != func->max_param ? "at least " : "",
         func->min_param, func->min_param != 1 ? "s" : "" );
      s_bail( phase );
   }
   if ( test->num_args > func->max_param ) {
      s_diag( phase, DIAG_POS_ERR, &call->pos,
         "too many arguments in function call" );
      struct str str;
      str_init( &str );
      t_copy_name( func->name, false, &str );
      s_diag( phase, DIAG_FILE, &call->pos,
         "function `%s` takes %s%d argument%s", str.value,
         func->min_param != func->max_param ? "up_to " : "",
         func->max_param, func->max_param != 1 ? "s" : "" );
      s_bail( phase );
   }
}

void test_call_first_arg( struct semantic* phase, struct expr_test* expr_test,
   struct call_test* test ) {
   if ( test->func->type == FUNC_FORMAT ) {
      test_call_format_arg( phase, expr_test, test );
   }
   else {
      if ( ! list_end( test->i ) ) {
         struct node* node = list_data( test->i );
         if ( node->type == NODE_FORMAT_ITEM ) {
            struct format_item* item = ( struct format_item* ) node;
            s_diag( phase, DIAG_POS_ERR, &item->pos,
               "passing format-item to non-format function" );
            s_bail( phase );
         }
         else if ( node->type == NODE_FORMAT_BLOCK_USAGE ) {
            struct format_block_usage* usage =
               ( struct format_block_usage* ) node;
            s_diag( phase, DIAG_POS_ERR, &usage->pos,
               "passing format-block to non-format function" );
            s_bail( phase );
         }
      }
   }
}

void test_call_format_arg( struct semantic* phase, struct expr_test* expr_test,
   struct call_test* test ) {
   list_iter_t* i = test->i;
   struct node* node = NULL;
   if ( ! list_end( i ) ) {
      node = list_data( i );
   }
   if ( ! node || ( node->type != NODE_FORMAT_ITEM &&
      node->type != NODE_FORMAT_BLOCK_USAGE ) ) {
      s_diag( phase, DIAG_POS_ERR, &test->call->pos,
         "function call missing format argument" );
      s_bail( phase );
   }
   // Format-block:
   if ( node->type == NODE_FORMAT_BLOCK_USAGE ) {
      if ( ! expr_test->format_block ) {
         s_diag( phase, DIAG_POS_ERR, &test->call->pos,
            "function call missing format-block" );
         s_bail( phase );
      }
      struct format_block_usage* usage = ( struct format_block_usage* ) node;
      usage->block = expr_test->format_block;
      // Attach usage to the end of the usage list.
      struct format_block_usage* prev = expr_test->format_block_usage;
      while ( prev && prev->next ) {
         prev = prev->next;
      }
      if ( prev ) {
         prev->next = usage;
      }
      else {
         expr_test->format_block_usage = usage;
      }
      list_next( i );
   }
   // Format-list:
   else {
      while ( ! list_end( i ) ) {
         struct node* node = list_data( i );
         if ( node->type != NODE_FORMAT_ITEM ) {
            break;
         }
         s_test_format_item( phase, ( struct format_item* ) node,
            expr_test->stmt_test, expr_test, expr_test->format_block );
         list_next( i );
      }
   }
   // Both a format-block or a format-list count as a single argument.
   ++test->num_args;
}

// This function handles a format-item found inside a function call,
// and a free format-item, the one found in a format-block.
void s_test_format_item( struct semantic* phase, struct format_item* item,
   struct stmt_test* stmt_test, struct expr_test* test,
   struct block* format_block ) {
   while ( item ) {
      if ( item->cast == FCAST_ARRAY ) {
         test_array_format_item( phase, stmt_test, test, format_block, item );
      }
      else {
         test_format_item( phase, stmt_test, test, format_block, item );
      }
      item = item->next;
   }
}

void test_format_item( struct semantic* phase, struct stmt_test* stmt_test,
   struct expr_test* test, struct block* format_block,
   struct format_item* item ) {
   struct expr_test value;
   s_init_expr_test( &value, stmt_test, format_block, false,
      test ? test->undef_err : true, false );
   s_test_expr( phase, &value, item->value );
   if ( value.undef_erred ) {
      test->undef_erred = true;
      longjmp( test->bail, 1 );
   }
}

void test_array_format_item( struct semantic* phase, struct stmt_test* stmt_test,
   struct expr_test* test, struct block* format_block,
   struct format_item* item ) {
   struct expr_test value;
   s_init_expr_test( &value, stmt_test, format_block, false,
      test ? test->undef_err : true, false );
   // When using the array format cast, accept an array as the result of the
   // expression.
   value.accept_array = true;
   struct operand array;
   init_operand( &array );
   test_expr( phase, &value, item->value, &array );
   if ( value.undef_erred ) {
      test->undef_erred = true;
      longjmp( test->bail, 1 );
   }
   if ( ! array.dim ) {
      s_diag( phase, DIAG_POS_ERR, &item->value->pos,
         "argument not an array" );
      s_bail( phase );
   }
   if ( array.dim->next ) {
      s_diag( phase, DIAG_POS_ERR, &item->value->pos,
         "array argument not of single dimension" );
      s_bail( phase );
   }
   // Test optional fields: offset and length.
   if ( item->extra ) {
      struct format_item_array* extra = item->extra;
      s_init_expr_test( &value, stmt_test, format_block, true, true, false );
      s_test_expr( phase, &value, extra->offset );
      if ( extra->length ) {
         s_init_expr_test( &value, stmt_test, format_block, true, true,
            false );
         s_test_expr( phase, &value, extra->length );
      }
   }
}

void test_binary( struct semantic* phase, struct expr_test* test,
   struct operand* operand, struct binary* binary ) {
   struct operand lside;
   init_operand( &lside );
   test_node( phase, test, &lside, binary->lside );
   if ( ! lside.usable ) {
      s_diag( phase, DIAG_POS_ERR, &binary->pos,
         "operand on left side not a value" );
      s_bail( phase );
   }
   struct operand rside;
   init_operand( &rside );
   test_node( phase, test, &rside, binary->rside );
   if ( ! rside.usable ) {
      s_diag( phase, DIAG_POS_ERR, &binary->pos,
         "operand on right side not a value" );
      s_bail( phase );
   }
   // Compile-time evaluation.
   if ( lside.folded && rside.folded ) {
      int l = lside.value;
      int r = rside.value;
      // Division and modulo get special treatment because of the possibility
      // of a division by zero.
      if ( ( binary->op == BOP_DIV || binary->op == BOP_MOD ) && ! r ) {
         s_diag( phase, DIAG_POS_ERR, &binary->pos, "division by zero" );
         s_bail( phase );
      }
      switch ( binary->op ) {
      case BOP_MOD: l %= r; break;
      case BOP_MUL: l *= r; break;
      case BOP_DIV: l /= r; break;
      case BOP_ADD: l += r; break;
      case BOP_SUB: l -= r; break;
      case BOP_SHIFT_R: l >>= r; break;
      case BOP_SHIFT_L: l <<= r; break;
      case BOP_GTE: l = l >= r; break;
      case BOP_GT: l = l > r; break;
      case BOP_LTE: l = l <= r; break;
      case BOP_LT: l = l < r; break;
      case BOP_NEQ: l = l != r; break;
      case BOP_EQ: l = l == r; break;
      case BOP_BIT_AND: l = l & r; break;
      case BOP_BIT_XOR: l = l ^ r; break;
      case BOP_BIT_OR: l = l | r; break;
      case BOP_LOG_AND: l = l && r; break;
      case BOP_LOG_OR: l = l || r; break;
      default: break;
      }
      operand->value = l;
      operand->folded = true;
   }
   operand->complete = true;
   operand->usable = true;
   // Type of the result.
   switch ( binary->op ) {
   case BOP_NONE:
   case BOP_MOD:
   case BOP_MUL:
   case BOP_DIV:
   case BOP_ADD:
   case BOP_SUB:
   case BOP_SHIFT_L:
   case BOP_SHIFT_R:
   case BOP_BIT_AND:
   case BOP_BIT_XOR:
   case BOP_BIT_OR:
      operand->type = phase->task->type_int;
      break;
   case BOP_GTE:
   case BOP_GT:
   case BOP_LTE:
   case BOP_LT:
   case BOP_NEQ:
   case BOP_EQ:
   case BOP_LOG_AND:
   case BOP_LOG_OR:
      operand->type = phase->task->type_bool;
      break;
   }
}

void test_assign( struct semantic* phase, struct expr_test* test,
   struct operand* operand, struct assign* assign ) {
   // To avoid the error where the user wanted equality operator but instead
   // typed in the assignment operator, suggest that assignment be wrapped in
   // parentheses.
   if ( test->suggest_paren_assign && ! operand->in_paren ) {
      s_diag( phase, DIAG_WARN | DIAG_FILE | DIAG_LINE | DIAG_COLUMN,
         &assign->pos, "assignment operation not in parentheses" );
   }
   struct operand lside;
   init_operand( &lside );
   test_node( phase, test, &lside, assign->lside );
   if ( ! lside.assignable ) {
      s_diag( phase, DIAG_POS_ERR, &assign->pos,
         "cannot assign to operand on left side" );
      s_bail( phase );
   }
   struct operand rside;
   init_operand( &rside );
   test_node( phase, test, &rside, assign->rside );
   if ( ! rside.usable ) {
      s_diag( phase, DIAG_POS_ERR, &assign->pos,
         "right side of assignment not a value" );
      s_bail( phase );
   }
   operand->complete = true;
   operand->usable = true;
   operand->type = lside.type;
}

void test_access( struct semantic* phase, struct expr_test* test,
   struct operand* operand, struct access* access ) {
   // `::` operator.
   if ( access->is_region ) {
      test_region_access( phase, test, operand, access );
   }
   // `.` operator.
   else {
      test_struct_access( phase, test, operand, access );
   }
}

void test_region_access( struct semantic* phase, struct expr_test* test,
   struct operand* operand, struct access* access ) {
   struct operand lside;
   init_operand( &lside );
   test_node( phase, test, &lside, access->lside );
   if ( ! lside.region ) {
      s_diag( phase, DIAG_POS_ERR, &access->pos,
         "left operand not a region" );
      s_bail( phase );
   }
   struct name* name = t_make_name( phase->task, access->name,
      lside.region->body );
   struct object* object = t_get_region_object( phase->task, lside.region, name );
   if ( ! object ) {
      s_diag( phase, DIAG_POS_ERR, &access->pos,
         "`%s` not found in region", access->name );
      s_bail( phase );
   }
   if ( ! object->resolved ) {
      if ( ! test->undef_err ) {
         test->undef_erred = true;
         longjmp( test->bail, 1 );
      }
      s_diag( phase, DIAG_POS_ERR, &access->pos,
         "right operand `%s` undefined", access->name );
      s_bail( phase );
   }
   use_object( phase, test, operand, object );
   access->rside = ( struct node* ) object;
}

void test_struct_access( struct semantic* phase, struct expr_test* test,
   struct operand* operand, struct access* access ) {
   struct operand lside;
   init_operand( &lside );
   test_node( phase, test, &lside, access->lside );
   if ( ! ( lside.type && ! lside.dim ) ) {
      s_diag( phase, DIAG_POS_ERR, &access->pos,
         "left operand not of struct type" );
      s_bail( phase );
   }
   struct name* name = t_make_name( phase->task, access->name, lside.type->body );
   if ( ! name->object ) {
      // The right operand might be a member of a structure that hasn't been
      // processed yet because the structure appears later in the source code.
      // Leave for now.
      // TODO: The name should already refer to a valid member by this stage.
      // Need to refactor the declaration code, then remove this.
      if ( ! test->undef_err ) {
         test->undef_erred = true;
         longjmp( test->bail, 1 );
      }
      if ( lside.type->anon ) {
         s_diag( phase, DIAG_POS_ERR, &access->pos,
            "`%s` not member of anonymous struct", access->name );
         s_bail( phase );
      }
      else {
         struct str str;
         str_init( &str );
         t_copy_name( lside.type->name, false, &str );
         s_diag( phase, DIAG_POS_ERR, &access->pos,
            "`%s` not member of struct `%s`", access->name,
            str.value );
         s_bail( phase );
      }
   }
   if ( ! name->object->resolved ) {
      if ( test->undef_err ) {
         s_diag( phase, DIAG_POS_ERR, &access->pos,
            "right operand `%s` undefined", access->name );
         s_bail( phase );
      }
      else {
         test->undef_erred = true;
         longjmp( test->bail, 1 );
      }
   }
   use_object( phase, test, operand, name->object );
   access->rside = ( struct node* ) name->object;
}