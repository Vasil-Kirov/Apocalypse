#include <Parser.h>
#include <Analyzer.h>
#include <Memory.h>

static b32 reached_eof;
static Token_Iden last_read_token;
static const char *expected;
static const Token_Iden invalid_token = {};

#define parse_expect(func, str) \
	func();                     \
	expected = str
#define tok_expect(token_id, exp_tok, expect)                 \
	{                                                         \
		if (token_id.type != exp_tok)                         \
		{                                                     \
			raise_parsing_unexpected_token(expect, token_id); \
		}                                                     \
	}
#define tok_not_expect(token_id, nexp_tok, expect)           \
	{                                                        \
		if (token_id.type == nexp_tok)                       \
		{                                                    \
			raise_parsing_unexpected_token(expect, token_id) \
		}                                                    \
	}

Ast_Node *
alloc_node()
{
	Ast_Node *result = (Ast_Node *)AllocateCompileMemory(sizeof(Ast_Node));
	memset(result, 0, sizeof(Ast_Node));
	return result;
}

Ast_Node *
ast_cast(Token_Iden token, Type_Info type, Ast_Node *expression)
{
	Ast_Node *result = alloc_node();
	result->type = type_cast;
	result->cast.token = token;
	result->cast.type = type;
	result->cast.expression = expression;
	return result;
}

Ast_Node *
ast_unary_expr(Token_Iden token, Ast_Node *expr)
{
	Ast_Node *result = alloc_node();
	result->type = type_unary_expr;
	result->unary_expr.op = token;
   	result->unary_expr.expression = expr;
	return result;
}

Ast_Node *
ast_identifier(File_Contents *f, Token_Iden identifier_token)
{
	if(identifier_token.type != tok_identifier)
		raise_parsing_unexpected_token("identifier", f);
	
	Ast_Node *result = alloc_node();
	result->type = type_identifier;
	result->identifier.token = identifier_token;
	result->identifier.name = identifier_token.identifier;
	return result; 
}

Ast_Identifier
pure_identifier(Token_Iden token)
{
	Ast_Identifier result = {
		.token = token,
		.name = token.identifier,
	};
	return result;
}

Ast_Node *
ast_variable(Type_Info type, Ast_Identifier identifier, b32 is_const)
{
	Ast_Node *result = alloc_node();
	result->type = type_var;
	result->variable.type = type;
	result->variable.identifier = identifier;
	result->variable.is_const = is_const;
	return result;
}

Ast_Node *
ast_assignment_from_decl(Ast_Node *lhs, Ast_Node *rhs, Type_Info decl_type, Token_Iden *error_token, b32 is_const)
{
	Ast_Node *result = alloc_node();
	result->type = type_assignment;
	result->assignment.is_declaration = true;
	result->assignment.lhs = lhs;
	result->assignment.rhs = rhs;
	result->assignment.token = *error_token;
	result->assignment.assign_type = (Token)'=';
	result->assignment.is_const = is_const;
	result->assignment.decl_type = decl_type;
	return result;
}

Ast_Node *
ast_indexing(Token_Iden token, Ast_Node *operand, Ast_Node *expression)
{
	Ast_Node *result = alloc_node();
	result->type = type_index;
	result->index.expression = expression;
	result->index.token = token;
	result->index.operand = operand;
	return result;
}

Ast_Node *
ast_assignment(Ast_Node *lhs, Ast_Node *rhs, Token op, Token_Iden *error_token)
{
	Ast_Node *result = alloc_node();
	
	result->type = type_assignment;
	result->assignment.is_declaration = false;
	result->assignment.lhs = lhs;
	result->assignment.rhs = rhs;
	result->assignment.token = *error_token;
	result->assignment.assign_type = op;
	// @Note: only here if is_declaration is true
	result->assignment.decl_type = (Type_Info){.type = T_INVALID};
	return result;
}

Ast_Node *
ast_struct(Ast_Identifier id, Ast_Variable *members, int  member_count)
{
	Ast_Node *result = alloc_node();
	result->type = type_struct;
	result->structure.struct_id = id;
	result->structure.member_count = member_count;
	memcpy(result->structure.members, members, sizeof(Ast_Variable) * REASONABLE_MAXIMUM);
	
	return result;
}

Ast_Node *
ast_selector(Token_Iden dot_token, Ast_Node *operand, Ast_Node *identifier)
{
	Assert(identifier->type == type_identifier);
	Ast_Node *result = alloc_node();
	result->type = type_selector;
	result->selector.dot_token = dot_token;
	result->selector.operand = operand;
	result->selector.identifier = identifier;
	return result;
}

Ast_Node *
ast_postfix(Token_Iden postfix, Ast_Node *operand)
{
	Ast_Node *result = alloc_node();
	result->type = type_postfix;
	result->postfix.operand = operand;
	result->postfix.token = postfix;
	return result;
}


Ast_Node *
node_to_ptr(Ast_Node node)
{
	Ast_Node *result = alloc_node();
	memcpy(result, &node, sizeof(Ast_Node));
	return result;
}

Ast_Node *
parse(File_Contents *f)
{
	Ast_Node *root = alloc_node();
	root->type = type_root;
	// NOTE(Vasko): nothing special about root, it doesn't contain data

	Token_Iden *info_tok = f->curr_token;

	Scope_Info scope_info = {false, (unsigned int)info_tok->line, 0, info_tok->file, NULL};
	push_scope(f, scope_info);
	root = parse_file_level_statement(f);
	if(!reached_eof)
	{
		raise_parsing_unexpected_token("end of file", f);
	}
	pop_scope(f, *f->prev_token);
	if(!is_scope_stack_empty(f))
		raise_semantic_error(f, "not all scopes closed by eof", *f->prev_token);
	return root;
}

Ast_Node *
parse_file_level_statement(File_Contents *f)
{
	Ast_Node *result = alloc_node();
	switch ((int)f->curr_token->type)
	{
	case (Token)'$':
	{
	} break;
	case tok_struct:
	{
		result = parse_struct(f);
		result->left = parse_file_level_statement(f);
	} break;
	case tok_func:
	{
		result = parse_func(f);
		if (result == NULL)
		{
			raise_parsing_unexpected_token(expected, f);
		}
	} break;
	case tok_eof:
	{
		reached_eof = true;
		return NULL;
	}break;
	default:
	{
		raise_parsing_unexpected_token("top level statement", f);
	} break;
	}
	return result;
}

Ast_Node *
parse_body(File_Contents *f, b32 is_func, Token_Iden opt_tok)
{
	Ast_Node *result = alloc_node();
	result->type = type_scope_start;
	
	if(opt_tok.file == NULL) {
		result->scope_desc.token = *f->curr_token;
		parser_eat(f, (Token)'{');
	} else {
		result->scope_desc.token = opt_tok;
	}
	Scope_Info new_scope = {false, (unsigned int)f->curr_token->line, 0, (char *)f->path, NULL};
	push_scope(f, new_scope);
	result->right = parse_statement(f);
	if(is_func)
		result->left = parse_file_level_statement(f);
	else
		result->left = parse_statement(f);
	return result;
}

Token_Iden *
find_identifier(Ast_Node *expr)
{
	if(expr->type == type_identifier)
	{
		return &expr->identifier.token;
	}
	else if(expr->type == type_unary_expr)
	{
		return find_identifier(expr->unary_expr.expression);
	}
	else if(expr->type == type_index)
	{
		return find_identifier(expr->index.expression);
	}
	return NULL;
}

Ast_Node *
parse_identifier_statement(File_Contents *f)
{
	Ast_Node *lhs = parse_expression(f, NO_EXPECT, true);
	Token_Iden *identifier_token = find_identifier(lhs);
	if(!identifier_token)
	{
		raise_parsing_unexpected_token("identifier", f);
	}
	b32 is_const = false;
	switch((int)f->curr_token->type)
	{
		case tok_equals:
		case tok_plus_equals:
		case tok_minus_equals:
		case tok_mult_equals:
		case tok_div_equals:
		case tok_mod_equals:
		case tok_and_equals:
		case tok_or_equals:
		case tok_xor_equals:
		case tok_lshift_equals:
		case tok_rshift_equals:
		{
			Token_Iden assign_type = advance_token(f);
			Ast_Node *rhs = parse_expression(f, (Token)';', false);
			return ast_assignment(lhs, rhs, assign_type.type, identifier_token);
		} break;
		case tok_const:
		is_const = true;
		// fallthrough
		case ':':
		{
			advance_token(f);
			Type_Info assign_type = {};
			if (f->curr_token->type == tok_equals)
				assign_type.type = T_DETECT;	
			else
				assign_type = parse_type(f);
			parser_eat(f, (Token)'=');
			Ast_Node *rhs = parse_expression(f, (Token)';', false);
			return ast_assignment_from_decl(lhs, rhs, assign_type, identifier_token, is_const);
		} break;
		default:
		{
			advance_token(f);
			raise_parsing_unexpected_token("declaration or assignment", f);
		} break;
	}
	return NULL;
}

Ast_Node *
parse_for_statement(File_Contents *f)
{
	// @TODO: implement
	return NULL;
}

Ast_Node *
parse_statement(File_Contents *f)
{
	Ast_Node *result = alloc_node();
	
	switch((int)f->curr_token->type)
	{
	case '$':
	{
		LG_FATAL("$ has not been implemented");
	}break;
	case '{':
	{
		return parse_body(f, false, invalid_token);
	} break;
	case tok_if:
	{
		advance_token(f);
		result->type = type_if;
		result->condition = parse_expression(f, (Token)'{', false);
		result->left = parse_body(f, false, last_read_token);
	} break;
	case tok_for:
	{
		result = parse_for_statement(f);
	} break;
	case tok_star:
	case tok_identifier:
	{
		result = parse_identifier_statement(f);
		result->left = parse_statement(f);
	} break;
	case tok_arrow:
	{
		advance_token(f);
		result->type = type_return;
		result->holder.token = *f->curr_token;
		result->right = parse_expression(f, (Token)';', false);
		result->left = parse_statement(f);
	} break;
	case tok_break:
	{
		advance_token(f);
		result->type = type_break;
		result->left = parse_statement(f);
	}
	case '}':
	{
		advance_token(f);
		pop_scope(f, *f->curr_token);
		result->type = type_scope_end;
		result->scope_desc.token = *f->curr_token;
		return result;
	} break;
	default:
	{
		raise_parsing_unexpected_token("[ '}' ]", f);
	}
	}
	return result;
}

// @NOTE: parser should check for errors
// SDArray
Ast_Node **
delimited(File_Contents *f, char start, char stop, char seperator, Ast_Node *(*parser)(File_Contents *))
{
	Ast_Node **result = SDCreate(Ast_Node *);
	
	b32 has_args = false;
	parser_eat(f, (Token)start);
	
	while (true)
	{
		if(f->curr_token->type == stop)
		{
			if(has_args)
				raise_parsing_unexpected_token("expression", f);
			break;
		}
		Ast_Node *item = parser(f);
		if(item)
			SDPush(result, item);
		if(f->curr_token->type == stop)
			break;
		parser_eat(f, (Token)seperator);
	}
	parser_eat(f, (Token)stop);
	return result;
}

int
get_precedence(Token op, b32 is_lhs)
{
	switch ((int)op)
	{
	case tok_plusplus:
	case tok_minusminus:
	case '(':
	case '[':
		return is_lhs ? 35 : 34;

	case '*':
	case '/':
	case '%':
		return is_lhs ? 33 : 32;

	case '+':
	case '-':
		return is_lhs ? 31 : 30;

	case tok_bits_lshift:
	case tok_bits_rshift:
		return is_lhs ? 29 : 28;

	case '>':
	case '<':
	case tok_logical_gequal:
	case tok_logical_lequal:
		return is_lhs ? 27 : 26;

	case tok_logical_is:
	case tok_logical_isnot:
		return is_lhs ? 25 : 24;

	case tok_bits_and:
		return is_lhs ? 23 : 22;
	case tok_bits_xor:
		return is_lhs ? 21 : 20;
	case tok_bits_or:
		return is_lhs ? 19 : 18;
	case tok_logical_and:
		return is_lhs ? 17 : 16;
	case tok_logical_or:
		return is_lhs ? 15 : 14;

// @TODO: look more into if and how these should be handled here
/*
	case tok_equals:
	case tok_plus_equals:
	case tok_minus_equals:
	case tok_mult_equals:
	case tok_div_equals:
	case tok_mod_equals:
	case tok_lshift_equals:
	case tok_rshift_equals:
	case tok_and_equals:
	case tok_xor_equals:
	case tok_or_equals:
		return is_lhs ? 10 : 11;
*/
	}
	return 0;
}


Ast_Node *
parse_var(File_Contents *f, Token_Iden name_token)
{
	Token_Iden colon = *f->curr_token;
	if(colon.type != ':')
		return NULL;
	
	advance_token(f);
	Type_Info type_info = {};
	
	Token_Iden type_tok = *f->curr_token;
	if(type_tok.type == '=')
	{
		advance_token(f);
		type_info.type = T_DETECT;
		type_info.token = type_tok;
	}
	else if(type_tok.type == tok_identifier || type_tok.type == tok_star)
	{
		type_info = parse_type(f);
	}
	else
	{
		raise_parsing_unexpected_token("Type after ':'." 
									   "In the case of a declaration you can alse use '='", f);
	}
	
	Assert(type_info.token.file != NULL);
	Ast_Node *result = ast_variable(type_info, pure_identifier(name_token), false);
	return result;
}

b32
is_literal(Token_Iden token)
{
	if(token.type == tok_identifier || token.type == tok_const_str || token.type == tok_number)
		return true;
	return false;
}

Ast_Node *
parse_func_call(File_Contents *f, Ast_Node *operand)
{
	Ast_Node *result = alloc_node();
	result->type = type_func_call;
	result->func_call.operand = operand;
	result->func_call.arguments = SDCreate(Ast_Node *);
	result->func_call.token = advance_token(f);

	b32 has_multiple_args = false;
	
	while(true)
	{
		if(f->curr_token->type == ')')
		{
			advance_token(f);
			break;
		}
		Ast_Node *expression = parse_expression(f, NO_EXPECT, false);
		if(expression)
			SDPush(result->func_call.arguments, expression);

		Token_Iden next = advance_token(f);
		if(has_multiple_args && expression == NULL)
			raise_parsing_unexpected_token("expression after [ , ]", f);
		if(next.type == ')')
			break;
		if(next.type != ',')
			raise_parsing_unexpected_token("[ , ] or [ ) ] after expression", f);

		has_multiple_args = true;
	}
	return result;
}

Ast_Node *
parse_struct_initialize(File_Contents *f, Ast_Node *operand)
{
	if (!operand || operand->type != type_identifier)
	{
		raise_parsing_unexpected_token("identifier for struct initialization", f);
	}
	Ast_Node *result = alloc_node();
	result->type = type_struct_init;
	result->struct_init.operand = operand;
	result->struct_init.expressions = SDCreate(Ast_Node *);
	result->struct_init.token = *f->curr_token;

	while(true)
	{
		if(f->curr_token->type == '}')
			break;
		Ast_Node *expression = parse_expression(f, NO_EXPECT, false);
		if(expression)
			SDPush(result->struct_init.expressions, expression);

		Token_Iden next = advance_token(f);
		if(next.type == '}')
			break;
		if(next.type != ',')
			raise_parsing_unexpected_token("[ , ] or [ } ] after expression", f);

	}
	return result;
}


Ast_Node *
parse_atom_expression(File_Contents *f, Ast_Node *operand, char stop_at, b32 is_lhs)
{
	b32 loop = true;
	while(loop)
	{
		if(f->curr_token->type == stop_at)
			return operand;
		
		switch ((int)f->curr_token->type)
		{
			case '(':
			{
				operand = parse_func_call(f, operand);
			} break;
			case '{':
			{
				if(is_lhs)
				{
					raise_parsing_unexpected_token("left-hand side of statement "
												   ", not got struct initialization", f);
				}
				advance_token(f);
				operand = parse_struct_initialize(f, operand);
			} break;
			case '[':
			{
				Token_Iden index_token = advance_token(f);
				operand = ast_indexing(index_token, operand, parse_expression(f, (Token)']', false));
			} break;
			case '.':
			{
				operand = ast_selector(advance_token(f), operand,
									   ast_identifier(f, advance_token(f)));
			} break;
			case tok_plusplus:
			case tok_minusminus:
			{
				operand = ast_postfix(advance_token(f), operand); 
			} break;
			default:
			{
				loop = false;
			} break;
		}
	}
	return operand;
}

Ast_Node *
parse_operand(File_Contents *f, char stop_at, b32 is_lhs)
{
	Ast_Node *result = NULL;
	switch((int)f->curr_token->type)
	{
		case tok_identifier:
		{
			result = ast_identifier(f, advance_token(f));
		} break;
		case tok_number:
		{
			if(is_lhs)
			{
				advance_token(f);
				raise_parsing_unexpected_token("left-handside of statement", f);
			}
			result = alloc_node();
			result->type = type_literal;
			result->atom.identifier = pure_identifier(advance_token(f));
		} break;
		case tok_const_str:
		{
			if(is_lhs)
			{
				advance_token(f);
				raise_parsing_unexpected_token("left-handside of statement", f);
			}
			result = alloc_node();
			result->type = type_const_str;
			result->atom.identifier = pure_identifier(advance_token(f));
		} break;
		case '(':
		{
			advance_token(f);
			if(is_lhs)
			{
				raise_parsing_unexpected_token("left-handside of statement", f);
			}
			result = parse_expression(f, (Token)')', false);
		} break;
	}
	return result;
}

Ast_Node *
parse_unary_expression(File_Contents *f, char stop_at, b32 is_lhs)
{
	Ast_Node *result = NULL;
	b32 lhs_unary = false;
	Token_Iden unary_token = *f->curr_token;
	switch((int)unary_token.type)
	{
		case '#':
		{
			Token_Iden token = advance_token(f);
			if(is_lhs)
			{
				raise_parsing_unexpected_token("left-hand side of statement, not cast", f);
			}
			Type_Info type = parse_type(f);
			result = ast_cast(token, type, parse_unary_expression(f, stop_at, false));
			return result;
		} break;
		case '*':
		lhs_unary = true;
		case '@':
		case tok_minus:
		case tok_not:
		case tok_plusplus:
		case tok_minusminus:
		{
			
			Token_Iden token = advance_token(f);
			if(is_lhs && !lhs_unary)
			{
				raise_parsing_unexpected_token("left-hand side of statement, not unary expression", f);
			}
			Ast_Node *expression = parse_unary_expression(f, stop_at, false);
			result = ast_unary_expr(token, expression);
			return result;
		} break;
		
	}

	result = parse_atom_expression(f, parse_operand(f, stop_at, is_lhs), stop_at, is_lhs);
	if (result == NULL)
		raise_parsing_unexpected_token("opperand", f);

	return result;
}



Ast_Node *
parse_binary_expression(File_Contents *f, Token stop_at, int min_bp, b32 is_lhs)
{
	Ast_Node *result = parse_unary_expression(f, stop_at, is_lhs);
	Token_Iden current;
	while(true)
	{
		current = *f->curr_token;
		
		int l_bp = get_precedence(current.type, true);
		int r_bp = get_precedence(current.type, false);

		if(current.type == stop_at || l_bp < min_bp)
			break;		
		advance_token(f);
		
		if(is_lhs)
			raise_parsing_unexpected_token("left-handside of statement, not binary operation", f);
		
		Ast_Node *right = parse_binary_expression(f, stop_at, r_bp, is_lhs);
		Ast_Node *node = alloc_node();
		node->type = type_binary_expr;
		node->binary_expr.op = current.type;
		node->binary_expr.token = current;
		node->right = right;
		node->left = result;
		result = node;
	}
	return result;
}

b32
type_is_invalid(Type_Info type)
{
	if(type.type == T_INVALID)
		return true;
	return false;
}

Ast_Node *
parse_expression(File_Contents *f, Token stop_at, b32 is_lhs)
{
	if(f->curr_token->type == stop_at)
	{
		advance_token(f);
		return NULL;
	}
	Ast_Node *result = parse_binary_expression(f, stop_at, 1, is_lhs);
	if(stop_at != '\x18')
	{
		parser_eat(f, stop_at);
	}
	return result;
}

u8 *
ptr_to_identifier(Type_Info ptr)
{
	if(ptr.type == T_POINTER)
	{
		u8 *result = ptr_to_identifier(*ptr.pointer.type);
		vstd_strcat((char *)result, "*");
		return result;
	}
	else
	{
		Assert(ptr.identifier);
		size_t len = vstd_strlen((char *)ptr.identifier);
		u8 *result = (u8 *)AllocateCompileMemory(len + 64);
		memcpy(result, ptr.identifier, len);
		return result;
	}
}

Type_Info
parse_type(File_Contents *f)
{
	Type_Info result = {};
	Token_Iden pointer_or_type = *f->curr_token;
	result.token = pointer_or_type;
	
	if (pointer_or_type.type == '*')
	{
		advance_token(f);
		result.type = T_POINTER;
		Type_Info pointed = parse_type(f);
		result.pointer.type = (Type_Info *)AllocateCompileMemory(sizeof(Type_Info));
		memcpy(result.pointer.type, &pointed, sizeof(Type_Info));
		result.identifier = ptr_to_identifier(result);
	}
	else if(pointer_or_type.type == tok_identifier)
	{
		// @Note: Invalid types are checked in analyzer
		result = get_type(f, pointer_or_type.identifier);
		advance_token(f);
	}
	else
	{
		result = (Type_Info){.type = T_INVALID};
	}
	result.token = pointer_or_type;
	return result;
}

Ast_Node *
parse_struct(File_Contents *f)
{
	parser_eat(f, tok_struct);
	Token_Iden struct_id = get_next_expecting(f, tok_identifier, "struct name");
	parser_eat(f, (Token)'{');
	Token_Iden curr_tok;
	Ast_Variable members[REASONABLE_MAXIMUM] = {};
	int member_count = 0;
	while(true)
	{
		curr_tok = advance_token(f);
		if(curr_tok.type == '}')
			break;
		if(curr_tok.type == ';')
		{
			curr_tok = advance_token(f);
			if(curr_tok.type == '}')
				break;
		}
		if(curr_tok.type != tok_identifier)
		{
			raise_parsing_unexpected_token("struct member or end of structer '}'", f);
		}
		parser_eat(f, (Token)':');
		Type_Info type = parse_type(f);
		Ast_Variable member = ast_variable(type, ast_identifier(f, curr_tok)->identifier, false)->variable;
		members[member_count++] = member;
	}
	if(member_count == 0)
	{
		raise_parsing_unexpected_token("struct members", f);
	}

	Ast_Node *result = ast_struct(ast_identifier(f, struct_id)->identifier, members, member_count);
	add_type(f, result);
	return result;
}

Ast_Node *
parse_func_arg(File_Contents *f)
{
	// @TODO: fix
	Token_Iden identifier_token = advance_token(f);
	if(identifier_token.type == tok_var_args)
	{
		return ast_variable((Type_Info){.type = T_DETECT}, pure_identifier(identifier_token), true);
	}
	Ast_Node *result = parse_var(f, identifier_token);
	if(result == NULL)
		raise_parsing_unexpected_token("correctly formated argument", f);
	return result;
}

Ast_Node *
parse_func(File_Contents *f)
{
	parser_eat(f, tok_func);

	Ast_Func this_func = {};
	Ast_Node *func_id = ast_identifier(f, advance_token(f));
	this_func.identifier = func_id->identifier;
	this_func.arguments = delimited(f, '(', ')', ',', parse_func_arg);
	parser_eat(f, tok_arrow);

	Type_Info func_type = {};
	Token_Iden maybe_type = *f->curr_token;
	if (maybe_type.type != '{' && maybe_type.type != ';')
	{
		func_type = parse_type(f);
	}
	else
	{
		func_type.type = T_VOID;
		func_type.identifier = (u8 *)"void";
	}
	this_func.type = func_type;

	Token_Iden body = *f->curr_token;

	Ast_Node *result = alloc_node();
	result->type = type_func;
	result->function = this_func;

	{
		Symbol this_symbol = {S_FUNCTION};
		this_symbol.token = func_id->identifier.token;
		this_symbol.node = result;
		this_symbol.identifier = this_func.identifier.name;
		this_symbol.type = func_type;
		add_symbol(f, this_symbol);
	}

	if(body.type == '{')
	{
		// TODO(Vasko): move this to analyzer to add symbols on scope stack
		for(size_t i = 0; i < SDCount(result->function.arguments); ++i)
		{
			Ast_Variable arg = result->function.arguments[i]->variable;
			Symbol arg_symbol = {S_FUNC_ARG};
			arg_symbol.token = func_id->identifier.token;
			arg_symbol.node = result->function.arguments[i];
			arg_symbol.identifier = arg.identifier.name;
			arg_symbol.type = arg.type;
			add_symbol(f, arg_symbol);
		}
		result->left = parse_body(f, true, invalid_token);
	}
	else if(body.type == ';')
		result->left = parse_file_level_statement(f);
	else
		raise_parsing_unexpected_token("'{' or ';'", f);

	return result;
}

void parser_eat(File_Contents *f, Token expected_token)
{
	Token_Iden got_token = advance_token(f);
	if (got_token.type != expected_token)
	{
		raise_parsing_unexpected_token((const char *)token_to_str(expected_token), f);
	}
	last_read_token = got_token;
}