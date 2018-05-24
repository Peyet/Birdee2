#pragma once
#include <map>
#include <string>
//#include <sstream>      // std::stringbuf
#include "SourcePos.h"
#include "TokenDef.h"
//modified based on LLVM tutorial
//https://llvm.org/docs/tutorial/LangImpl02.html



namespace Birdee
{


	class TokenizerError {
		int linenumber;
		int pos;
		std::string msg;
	public:
		TokenizerError(int _linenumber, int _pos, const std::string& _msg) : linenumber(_linenumber), pos(_pos), msg(_msg) {}
		void print()
		{
			printf("Compile Error at line %d, postion %d : %s", linenumber, pos, msg.c_str());
		}
	};



	class Tokenizer
	{

		FILE * f;
		int line;
		int pos;
		int LastChar = ' ';
		


		int GetChar()
		{
			int c = getc(f);
			if (c == '\n')
			{
				line++;
				pos = 0;
			}
			pos++;
			return c;
		}


		void ParseString()
		{
			IdentifierStr = "";
			LastChar = GetChar();
			while (true)
			{
				switch (LastChar)
				{
				case '\n':
					throw TokenizerError(line, pos, "New line is not allowed in simple string literal");
					break;
				case  EOF:
					throw TokenizerError(line, pos, "Unexpected end of file when parsing a string literal: expected \"");
					break;
				case '\"':
					LastChar = GetChar();
					return;
					break;
				case '\\':
					LastChar = GetChar();
					switch (LastChar)
					{
					case 'n':
						IdentifierStr += '\n';
						break;
					case '\\':
						IdentifierStr += '\\';
						break;
					case '\'':
						IdentifierStr += '\'';
						break;
					case '\"':
						IdentifierStr += '\"';
						break;
					case '?':
						IdentifierStr += '\?';
						break;
					case 'a':
						IdentifierStr += '\a';
						break;
					case 'b':
						IdentifierStr += '\b';
						break;
					case 'f':
						IdentifierStr += '\f';
						break;
					case 'r':
						IdentifierStr += '\r';
						break;
					case 't':
						IdentifierStr += '\t';
						break;
					case 'v':
						IdentifierStr += '\v';
						break;
					case '0':
						IdentifierStr += '\0';
						break;
					case EOF:
						throw TokenizerError(line, pos, "Unexpected end of file");
						break;
					default:
						throw TokenizerError(line, pos, "Unknown escape sequence \\"+(char)LastChar);
					}
					break;
				default:
					IdentifierStr += (char)LastChar;
				}
				LastChar = GetChar();
			}
		}
	public:

		SourcePos GetSourcePos()
		{
			return SourcePos(line, pos);
		}

		int GetLine() { return line; }
		int GetPos() { return pos; }
		Token CurTok =tok_add;
		Token GetNextToken() { return CurTok = gettok(); }

		Tokenizer(FILE* file) { f = file; line = 1; pos = 1; }
		NumberLiteral NumVal;		// Filled in if tok_number

		std::map<int, Token> single_operator_map = {
		{ '+',tok_add },
		{'-',tok_minus},
		{ '=',tok_assign },
		{ '*',tok_mul },
		{ '/',tok_div },
		{ '%',tok_mod },
		{ '>',tok_gt },
		{ '<',tok_lt },
		{ '&',tok_and },
		{ '|',tok_or },
		{ '!',tok_not },
		{ '^',tok_xor },
		};
		std::map<int, Token> single_token_map={
		{'(',tok_left_bracket },
		{')',tok_right_bracket },
		{ '[',tok_left_index },
		{ ']',tok_right_index },
		{'\n',tok_newline},
		{ ',',tok_comma },
		{'.',tok_dot},
		};
		std::map < std::string , Token > token_map = {
			{ "dim",tok_dim },
		{ "as",tok_as },
		{ "int",tok_int },
		{"function",tok_func},
		{"end",tok_end},
		{"class",tok_class},
		{"private",tok_private},
		{"public",tok_public},
		{"if",tok_if},
		{"then",tok_then},
		{"else",tok_else},
		{"return",tok_return},
		{"for",tok_for},
		{"==",tok_equal},
		{ "!=",tok_ne },
		{"===",tok_cmp_equal},
		{">=",tok_ge},
		{"<=",tok_le},
		{"&&",tok_logic_and},
		{"||",tok_logic_or},
		};
		std::string IdentifierStr; // Filled in if tok_identifier



		/// gettok - Return the next token from standard input.
		Token gettok() {

			// Skip any whitespace.
			while (isspace(LastChar)&& LastChar!='\n')
				LastChar = GetChar();
			auto single_token = single_token_map.find(LastChar);
			if (single_token != single_token_map.end())
			{
				LastChar = GetChar();
				return single_token->second;
			}
			single_token = single_operator_map.find(LastChar);
			if (single_token != single_operator_map.end())
			{
				IdentifierStr = LastChar;
				while (single_operator_map.find(LastChar=GetChar())!= single_operator_map.end())
					IdentifierStr += LastChar;
				if (IdentifierStr.size() > 1)
				{
					auto moperator = token_map.find(IdentifierStr);
					if (moperator != token_map.end())
						return moperator->second;
					else
						throw TokenizerError(line, pos, "Bad token");
				}
				else
				{
					return single_token->second;
				}
			}
			if (isalpha(LastChar)|| LastChar=='_') { // identifier: [a-zA-Z][a-zA-Z0-9]*
				IdentifierStr = LastChar;
				while (isalnum((LastChar = GetChar())))
					IdentifierStr += LastChar;

				auto single_token = token_map.find(IdentifierStr);
				if (single_token != token_map.end())
				{
					return single_token->second;
				}
				return tok_identifier;
			}

			if (isdigit(LastChar) || LastChar == '.') { // Number: [0-9.]+
				std::string NumStr;
				bool isfloat = (LastChar == '.');
				for(;;)
				{
					NumStr += LastChar;
					LastChar = GetChar();
					if (!isdigit(LastChar))
					{
						if (LastChar == '.')
						{
							if (isfloat)
								throw TokenizerError(line, pos, "Bad number literal");
							isfloat = true; //we only allow one dot in number literal
							continue;
						}
						break;
					}
				} 
				switch (LastChar)
				{
				case 'L':
					if(isfloat)
						throw TokenizerError(line, pos, "Should not have \'L\' after a float literal");
					NumVal.type = const_long;
					NumVal.v_long = std::stoll(NumStr);
					LastChar = GetChar();
					break;
				case 'U':
					if (isfloat)
						throw TokenizerError(line, pos, "Should not have \'U\' after a float literal");
					NumVal.type = const_ulong;
					NumVal.v_ulong = std::stoull(NumStr);
					LastChar = GetChar();
					break;
				case 'u':
					if (isfloat)
						throw TokenizerError(line, pos, "Should not have \'u\' after a float literal");
					NumVal.type = const_uint;
					NumVal.v_uint = std::stoul(NumStr);
					LastChar = GetChar();
					break;
				case 'f':
					NumVal.type = const_float;
					NumVal.v_double = std::stof(NumStr);
					LastChar = GetChar();
					break;
				case 'd':
					NumVal.type = const_double;
					NumVal.v_double = std::stod(NumStr);
					LastChar = GetChar();
					break;
				default:
					if (isfloat)
					{
						NumVal.type = const_float;
						NumVal.v_double = std::stod(NumStr);
					}
					else
					{
						NumVal.type = const_int;
						NumVal.v_int = std::stoi(NumStr);
					}
				}
				return tok_number;
			}

			if (LastChar == '\"') {
				ParseString();
				return tok_string_literal;
			}

			if (LastChar == '#') {
				// Comment until end of line.
				LastChar = GetChar();
				if (LastChar == '#')
				{
					int cnt_sharp = 0;
					while (LastChar != EOF)
					{
						LastChar = GetChar();
						if (LastChar == '#')
						{
							cnt_sharp++;
							if (cnt_sharp == 2)
								break;
						}
						else
							cnt_sharp = 0;
					}
					if (LastChar == EOF)
						throw TokenizerError(line, pos, "Unexpected end of file: expected ##");
				}
				else
				{
					while (LastChar != EOF && LastChar != '\n' && LastChar != '\r')
						LastChar = GetChar();
				}

				

				if (LastChar != EOF)
					return gettok();
			}

			// Check for end of file.  Don't eat the EOF.
			if (LastChar == EOF)
				return tok_eof;

			// Otherwise, just return the character as its ascii value.
			int ThisChar = LastChar;
			LastChar = GetChar();
			return tok_error;
		}

	};
}