#include <Basic.h>
#include <Log.h>
#include <Lexer.h>
#include <Analyzer.h>

u8 *get_error_segment(File_Contents *f, Token_Iden error_token)
{

	u8 *scanner = f->file_data;
	i32 row = 1;
	while(true)
	{
		if(*scanner == 0)
			return (u8 *)"";

		scanner++;
		if(*scanner == '\n')
		{
			row++;
		}
		if(row == error_token.line - 1)
			break;
	}
	scanner++;
	u8 *start = scanner;
	i32 counter = 0;
	i32 last_line_columns = 0;
	char *white_space = SDCreate(char);
	char tab = '\t';
	char space = ' ';
	while(true)
	{
		if (*scanner == 0)
		{
			scanner--;
			if(counter != 0)
				break;
			return (u8 *)"";
		}
		
		if(counter == 1) {
			if(*scanner == '\t')
				SDPush(white_space, tab);
			else
				SDPush(white_space, space);
		}

		if(counter == 2) {
			last_line_columns++;
		}
		if(*scanner == '\n')
			counter++;

		if(counter == 3)
			break;
		scanner++;
	}
	u8 *end = scanner;
	size_t len = end - start + 1;
	
	// @NOTE: + 10 for safety
	u8 *result = (u8 *)AllocateCompileMemory(len + last_line_columns + 10);
	memcpy(result, start, len);
	*(result + len) = '\n';
	scanner = result + len + 1;
	i32 tick_counter = 0;
	i32 white_space_it = 0;
	while(tick_counter != error_token.column + 1)
	{
		if(white_space_it == SDCount(white_space))
			white_space_it = 0;
		*scanner = white_space[white_space_it++];
		scanner++;
		tick_counter++;
	}
	*(scanner - 2) = '^';
	*(scanner - 1) = '^';
	*(scanner - 0) = '^';

	return result;
}

void raise_semantic_error(File_Contents *f, const char *error_msg, struct _Token_Iden token)
{
	u8 *error_location = get_error_segment(f, token);
	LG_FATAL("%s (%d, %d):\n\tSemantic error: %s\n\n%s",
			 token.file, token.line, token.column, error_msg, error_location);
}

void raise_token_syntax_error(File_Contents *f, const char *error_msg, char *file, u64 line,
							  u64 column)
{
	Token_Iden new_token = {};
	new_token.file = file;
	new_token.column = column;
	new_token.line = line;
	u8 *error_location = get_error_segment(f, new_token);
	LG_FATAL("%s (%d, %d):\n\tAn error occured while tokenizing: %s\n\n%s", file, line, column, error_msg, error_location);
}

void raise_parsing_unexpected_token(const char *expected_tok, File_Contents *f)
{
	Token_Iden token;
	
	if(f->prev_token)
		token = *f->prev_token;
	else
		token = *f->curr_token;
	
	u8 *error_location = get_error_segment(f, token);
	if(token.type == tok_identifier)
	{
		LG_FATAL("%s (%d, %d):\n\tFound unexpected token %s, expected %s, got [ \"%s\" ]\n\n%s",
				 token.file, token.line, token.column, token_to_str(token.type),
				 expected_tok, token.identifier, error_location);
	}
	else
		LG_FATAL("%s (%d, %d):\n\tFound unexpected token %s, expected %s\n\n%s",
			 token.file, token.line, token.column, token_to_str(token.type), expected_tok, error_location);
}

