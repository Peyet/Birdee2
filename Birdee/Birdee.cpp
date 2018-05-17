// Birdee.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include "Tokenizer.h"
#include <iostream>

using namespace Birdee;

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (aka Parse Tree)
//===----------------------------------------------------------------------===//

Tokenizer tokenizer(fopen("test.txt", "r"));

namespace Birdee{

	void formatprint(int level) {
		for (int i = 0; i < level; i++)
			std::cout << "\t";
	}

	/// StatementAST - Base class for all expression nodes.
	class StatementAST {
	public:
		virtual ~StatementAST() = default;
		virtual void print(int level) {
			for (int i = 0; i < level; i++)
				std::cout << "\t";
		}
	};

	class ExprAST : public StatementAST {
	public:
		virtual ~ExprAST() = default;
	};

	/// NumberExprAST - Expression class for numeric literals like "1.0".
	class NumberExprAST : public ExprAST {
		NumberLiteral Val;
	public:
		NumberExprAST(const NumberLiteral& Val) : Val(Val) {}
		void print(int level) {
			ExprAST::print(level); 
			switch (Val.type)
			{
			case const_int:
				std::cout << "const int "<<Val.v_int << "\n";
				break;
			case const_long:
				std::cout << "const long " << Val.v_long << "\n";
				break;
			case const_uint:
				std::cout << "const uint " << Val.v_uint << "\n";
				break;
			case const_ulong:
				std::cout << "const ulong " << Val.v_ulong << "\n";
				break;
			case const_float:
				std::cout << "const float " << Val.v_double << "\n";
				break;
			case const_double:
				std::cout << "const double " << Val.v_double << "\n";
				break;
			}
		}
	};


	class StringLiteralAST : public ExprAST {
		std::string Val;
	public:
		StringLiteralAST(const std::string& Val) : Val(Val) {}
		void print(int level) { ExprAST::print(level); std::cout << "\"" <<Val << "\"\n"; }
	};

	/// IdentifierExprAST - Expression class for referencing a variable, like "a".
	class IdentifierExprAST : public ExprAST {
		std::string Name;
	public:
		IdentifierExprAST(const std::string &Name) : Name(Name) {}
		void print(int level) { ExprAST::print(level); std::cout << "Identifier:" << Name << "\n"; }
	};

	/// BinaryExprAST - Expression class for a binary operator.
	class BinaryExprAST : public ExprAST {
		Token Op;
		std::unique_ptr<ExprAST> LHS, RHS;
	public:
		BinaryExprAST(Token Op, std::unique_ptr<ExprAST>&& LHS,
			std::unique_ptr<ExprAST>&& RHS)
			: Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
		void print(int level) {
			ExprAST::print(level);
			std::cout << "OP:" << Op << "\n";
			LHS->print(level + 1);
			ExprAST::print(level+1); std::cout << "----------------\n";
			RHS->print(level + 1);
		}
	};
	/// BinaryExprAST - Expression class for a binary operator.
	class IndexExprAST : public ExprAST {
		std::unique_ptr<ExprAST> Expr, Index;
	public:
		IndexExprAST(std::unique_ptr<ExprAST>&& Expr,
			std::unique_ptr<ExprAST>&& Index)
			:  Expr(std::move(Expr)), Index(std::move(Index)) {}
		void print(int level) {
			ExprAST::print(level);
			std::cout << "Index\n";
			Expr->print(level + 1);
			ExprAST::print(level + 1); std::cout << "----------------\n";
			Index->print(level + 1);
		}
	};
	/// CallExprAST - Expression class for function calls.
	class CallExprAST : public ExprAST {
		std::string Callee;
		std::vector<std::unique_ptr<ExprAST>> Args;

	public:
		CallExprAST(const std::string &Callee,
			std::vector<std::unique_ptr<ExprAST>> Args)
			: Callee(Callee), Args(std::move(Args)) {}
	};



	class Type {
		
	public:
		Token type;
		int index_level = 0;
		virtual ~Type() = default;
		Type(Token _type):type(_type){}
		virtual void print(int level)
		{
			formatprint(level);
			std::cout << "Type " << type << " index: "<<index_level;
		}
	};

	class IdentifierType :public Type {
		std::string name;
	public:
		IdentifierType(const std::string&_name):Type(tok_identifier), name(_name){}
		void print(int level)
		{
			Type::print(level);
			std::cout << " Name: " << name;
		}
	};

	class VariableDefAST : public StatementAST {
	};



	class VariableSingleDefAST : public VariableDefAST {
		std::string name;
		std::unique_ptr<Type> type;
		std::unique_ptr<ExprAST> val;
	public:
		VariableSingleDefAST(const std::string& _name, std::unique_ptr<Type>&& _type, std::unique_ptr<ExprAST>&& _val) : name(_name),type(std::move(_type)), val(std::move(_val)){}
		void print(int level) {
			VariableDefAST::print(level);
			std::cout << "Variable:" << name << " ";
			type->print(0);
			std::cout << "\n";
			if(val)
				val->print(level + 1);
		}
	};

	class VariableMultiDefAST : public VariableDefAST {
		std::vector<std::unique_ptr<VariableSingleDefAST>> lst;
	public:
		VariableMultiDefAST(std::vector<std::unique_ptr<VariableSingleDefAST>>&& vec) :lst(std::move(vec)) {}

		void print(int level) {
			//VariableDefAST::print(level);
			for (auto& a : lst)
				a->print(level);
		}
	};


	/// PrototypeAST - This class represents the "prototype" for a function,
	/// which captures its name, and its argument names (thus implicitly the number
	/// of arguments the function takes).
	class PrototypeAST {
	protected:
		std::string Name;
		std::unique_ptr<VariableDefAST> Args;
		std::unique_ptr<Type> RetType;

	public:
		PrototypeAST(const std::string &Name, std::unique_ptr<VariableDefAST>&& Args, std::unique_ptr<Type>&& RetType)
			: Name(Name), Args(std::move(Args)), RetType(std::move(RetType)) {}

		const std::string &getName() const { return Name; }
		void print(int level)
		{
			formatprint(level);
			std::cout << "Function Proto: " << Name << std::endl;
			if(Args)
				Args->print(level+1);
			else
			{
				formatprint(level + 1); std::cout << "No arg\n";
			}
			RetType->print(level + 1); std::cout << "\n";
		}
	};

	/// FunctionAST - This class represents a function definition itself.
	class FunctionAST : public ExprAST {
		std::unique_ptr<PrototypeAST> Proto;
		std::vector<std::unique_ptr<StatementAST>> Body;

	public:
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
			std::vector<std::unique_ptr<StatementAST>>&& Body)
			: Proto(std::move(Proto)), Body(std::move(Body)) {}
		void print(int level)
		{
			ExprAST::print(level);
			std::cout << "Function def\n";
			Proto->print(level+1);
			for (auto&& node : Body)
			{
				node->print(level+1);
			}
		}
	};

	class CompileError {
		int linenumber;
		int pos;
		std::string msg;
	public:
		CompileError(int _linenumber, int _pos, const std::string& _msg): linenumber(_linenumber),pos(_pos),msg(_msg){}
		CompileError(const std::string& _msg) : linenumber(tokenizer.GetLine()), pos(tokenizer.GetPos()), msg(_msg) {}
		void print()
		{
			printf("Compile Error at line %d, postion %d : %s", linenumber, pos, msg.c_str());
		}
	};
}




inline void CompileExpect(Token expected_tok, const std::string& msg)
{
	if (tokenizer.CurTok != expected_tok)
	{
		throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), msg);
	}

	tokenizer.GetNextToken();
}


inline void CompileAssert(bool a, const std::string& msg)
{
	if (!a)
	{
		throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), msg);
	}
}

inline void CompileExpect(std::initializer_list<Token> expected_tok, const std::string& msg)
{

	for(Token t :expected_tok)
	{
		if (t == tokenizer.CurTok)
		{
			tokenizer.GetNextToken();
			return;
		}
	}
	throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), msg);
}

template<typename T>
T CompileExpectNotNull(T&& p, const std::string& msg)
{
	if(!p)
		throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), msg);
	return std::move(p);
}

/////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<ExprAST> ParseExpression(std::unique_ptr<ExprAST>&& firstexpr);
std::unique_ptr<ExprAST> ParseExpressionUnknown();
std::unique_ptr<ExprAST> ParseFunction();
////////////////////////////////////////////////////////////////////////////////////


std::unique_ptr<Type> ParseType()
{
	//CompileExpect(tok_as, "Expected \'as\'");
	if(tokenizer.CurTok!= tok_as)
		return std::make_unique<Type>(tok_auto);
	tokenizer.GetNextToken(); //eat as
	std::unique_ptr<Type> type;
	switch (tokenizer.CurTok)
	{
	case tok_identifier:
		type = std::make_unique<IdentifierType>(tokenizer.IdentifierStr);
		break;
	case tok_int:
		type = std::make_unique<Type>(tok_int);
		break;
	default:
		throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), "Expected an identifier or basic type name");
	}
	tokenizer.GetNextToken();
	while (tokenizer.CurTok == tok_left_index)
	{
		type->index_level++;
		tokenizer.GetNextToken();//eat [
		CompileExpect(tok_right_index, "Expected  \']\'");
	}
	return std::move(type);
}

std::unique_ptr<VariableSingleDefAST> ParseSingleDim()
{
	std::string identifier = tokenizer.IdentifierStr;
	CompileExpect(tok_identifier, "Expected an identifier to define a variable");
	std::unique_ptr<Type> type= ParseType();
	std::unique_ptr<ExprAST> val;
	if (tokenizer.CurTok == tok_assign)
	{
		tokenizer.GetNextToken();
		val = ParseExpressionUnknown();
	}
	return std::make_unique<VariableSingleDefAST>(identifier, std::move(type), std::move(val));
}

std::unique_ptr<VariableDefAST> ParseDim()
{
	std::vector<std::unique_ptr<VariableSingleDefAST>> defs;
	auto def = ParseSingleDim();
	defs.push_back(std::move(def));
	for (;;)
	{
		
		switch (tokenizer.CurTok)
		{
		case tok_comma:
			tokenizer.GetNextToken();
			def = ParseSingleDim();
			defs.push_back(std::move(def));
			break;
		case tok_newline:
		case tok_eof:
		case tok_right_bracket:
			goto done;
			break;
		default:
			throw CompileError(tokenizer.GetLine(),tokenizer.GetPos(), "Expected a new line after variable definition");
		}
		
	}
done:
	if (defs.size() == 1)
		return std::move(defs[0]);
	else 
		return std::make_unique<VariableMultiDefAST>(std::move(defs));
}


std::unique_ptr<ExprAST> ParsePrimaryExpression()
{
	std::unique_ptr<ExprAST> firstexpr;
	switch (tokenizer.CurTok)
	{
	case tok_identifier:
		firstexpr = std::make_unique<IdentifierExprAST>(tokenizer.IdentifierStr);
		break;
	case tok_number:
		firstexpr = std::make_unique<NumberExprAST>(tokenizer.NumVal);
		break;
	case tok_string_literal:
		firstexpr = std::make_unique<StringLiteralAST>(tokenizer.IdentifierStr);
		break;
	case tok_left_bracket:
		tokenizer.GetNextToken();
		firstexpr = ParseExpressionUnknown();
		CompileAssert(tok_right_bracket == tokenizer.CurTok, "Expected \')\'");
		break;
	case tok_func:
		tokenizer.GetNextToken(); //eat function
		firstexpr = ParseFunction();
		CompileAssert(tok_newline == tokenizer.CurTok, "Expected newline after function definition");
		return firstexpr; //don't eat the newline token!
		break;
	case tok_eof:
		return nullptr;
		break;
	default:
		throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), "Expected an expression");
	}
	tokenizer.GetNextToken(); //eat token
	while (tokenizer.CurTok == tok_left_index) //check for index
	{
		tokenizer.GetNextToken();//eat [
		firstexpr = std::make_unique<IndexExprAST>(std::move(firstexpr), CompileExpectNotNull(ParseExpressionUnknown(), "Expected an expression for index"));
		CompileExpect(tok_right_index, "Expected  \']\'");
	}
	return firstexpr;
}

/*
A term is a an expression of "*", "/" or "%"
e.g. "1*2 + 5*5" has two terms: 1*2 and 5*5 
*/
std::unique_ptr<ExprAST> ParseTerm(std::unique_ptr<ExprAST>&& firstexpr)
{
	Token nexttok = tokenizer.CurTok;
	std::unique_ptr<ExprAST> current;
	switch (nexttok)
	{
	case tok_mul:
	case tok_div:
	case tok_mod:
		tokenizer.GetNextToken();//eat the operator
		current = std::make_unique<BinaryExprAST>(nexttok, std::move(firstexpr),
			CompileExpectNotNull(ParsePrimaryExpression(), "Expect an expression after binary expression"));
		return ParseTerm(std::move(current));
	}
	return std::move(firstexpr);
}

std::unique_ptr<ExprAST> GetNextTerm()
{
	std::unique_ptr<ExprAST> next_primary = CompileExpectNotNull(ParsePrimaryExpression(), "Expect an expression");
	return CompileExpectNotNull(ParseTerm(std::move(next_primary)), "Expect an expression");
}

/*
Parse an expression when we don't know what kind of it is. Maybe an identifier? A function def?
*/
std::unique_ptr<ExprAST> ParseExpressionUnknown()
{
	return ParseExpression(GetNextTerm());
}

/*
Parse an expression when the first expression(term) is known
*/
std::unique_ptr<ExprAST> ParseExpression(std::unique_ptr<ExprAST>&& firstexpr)
{
	Token tok = tokenizer.CurTok;
	switch (tokenizer.CurTok)
	{
	case tok_assign:
		tokenizer.GetNextToken();//eat assign
		return std::make_unique<BinaryExprAST>(tok_assign, std::move(firstexpr),
			CompileExpectNotNull(ParseExpressionUnknown(),"Expect an expression after assignmeent"));
		break;
	case tok_add:
		tokenizer.GetNextToken();//eat the operator
		return ParseExpression( std::make_unique<BinaryExprAST>(tok, std::move(firstexpr), GetNextTerm()));
		break;
	case tok_eof:
	case tok_newline:
	case tok_right_bracket:
	case tok_right_index:
	case tok_comma:
		return std::move(firstexpr);
		break;
	default:
		throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), "Unknown Token");
	}
}

std::unique_ptr<ExprAST> ParseFunction()
{
	
	std::string name;
	if (tokenizer.CurTok == tok_identifier)
	{
		name = tokenizer.IdentifierStr;
		tokenizer.GetNextToken();
	}
	CompileExpect(tok_left_bracket, "Expected \'(\'");
	std::unique_ptr<VariableDefAST> args;
	if (tokenizer.CurTok!= tok_right_bracket) 
		args = ParseDim();
	CompileExpect(tok_right_bracket, "Expected \')\'");
	auto rettype = ParseType();
	if (rettype->type == tok_auto)
		rettype->type = tok_void;
	auto funcproto = std::make_unique<PrototypeAST>(name,std::move(args),std::move(rettype));

	std::vector<std::unique_ptr<StatementAST>> body;
	std::unique_ptr<ExprAST> firstexpr;
	//parse function body
	while (true)
	{

		switch (tokenizer.CurTok)
		{
		case tok_newline:
			tokenizer.GetNextToken(); //eat newline
			continue;
			break;
		case tok_dim:
			tokenizer.GetNextToken(); //eat dim
			body.push_back(std::move(ParseDim()));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after variable definition");
			break;
		case tok_func:
			tokenizer.GetNextToken(); //eat function
			body.push_back(std::move(ParseFunction()));
			CompileExpect({ tok_newline,tok_eof }, "Expected a new line after function definition");
			break;
		case tok_end:
			tokenizer.GetNextToken(); //eat end
			if(tokenizer.CurTok==tok_func) //optional: end function
				tokenizer.GetNextToken(); //eat function
			goto done;
			break;
		case tok_eof:
			throw CompileError("Unexpected end of file, missing \"end\"");
			break;
		default:
			firstexpr = ParseExpressionUnknown();
			CompileExpect({ tok_eof,tok_newline }, "Expect a new line after expression");
			CompileAssert(firstexpr != nullptr, "Compiler internal error: firstexpr=null");
			body.push_back(std::move(firstexpr));
		}
	}
	done:
	return std::make_unique<FunctionAST>(std::move(funcproto), std::move(body));
}

int ParseTopLevel(std::vector<std::unique_ptr<StatementAST>>& out)
{
	std::unique_ptr<ExprAST> firstexpr;
	tokenizer.GetNextToken();
	try {
		while (tokenizer.CurTok != tok_eof && tokenizer.CurTok != tok_error)
		{
			
			switch (tokenizer.CurTok)
			{
			case tok_newline:
				tokenizer.GetNextToken(); //eat newline
				continue;
				break;
			case tok_dim:
				tokenizer.GetNextToken(); //eat dim
				out.push_back(std::move(ParseDim()));
				CompileExpect({ tok_newline,tok_eof }, "Expected a new line after variable definition");
				break;
			case tok_eof:
				return 0;
				break;
			case tok_func:
				tokenizer.GetNextToken(); //eat function
				out.push_back(std::move(ParseFunction()));
				CompileExpect({ tok_newline,tok_eof }, "Expected a new line after function definition");
				break;
			default:
				firstexpr = ParseExpressionUnknown();
				CompileExpect({ tok_eof,tok_newline }, "Expect a new line after expression");
				//if (!firstexpr)
				//	break;
				CompileAssert(firstexpr != nullptr, "Compiler internal error: firstexpr=null");
				out.push_back(std::move(firstexpr));
			}
		}
	}
	catch (CompileError e)
	{
		e.print();
		return 1;
	}
	catch (TokenizerError e)
	{
		e.print();
		return 1;
	}
	return 0;
}

int main()
{
	std::vector<std::unique_ptr<StatementAST>> s;
	ParseTopLevel(s);
	for (auto&& node : s)
	{
		node->print(0);
	}
    return 0;
}

