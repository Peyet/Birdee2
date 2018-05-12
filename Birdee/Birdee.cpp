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

namespace Birdee{



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
		double Val;
	public:
		NumberExprAST(double Val) : Val(Val) {}
		void print(int level) { ExprAST::print(level); std::cout << Val << "\n"; }
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

	/// CallExprAST - Expression class for function calls.
	class CallExprAST : public ExprAST {
		std::string Callee;
		std::vector<std::unique_ptr<ExprAST>> Args;

	public:
		CallExprAST(const std::string &Callee,
			std::vector<std::unique_ptr<ExprAST>> Args)
			: Callee(Callee), Args(std::move(Args)) {}
	};

	/// PrototypeAST - This class represents the "prototype" for a function,
	/// which captures its name, and its argument names (thus implicitly the number
	/// of arguments the function takes).
	class PrototypeAST {
		std::string Name;
		std::vector<std::string> Args;

	public:
		PrototypeAST(const std::string &Name, std::vector<std::string> Args)
			: Name(Name), Args(std::move(Args)) {}

		const std::string &getName() const { return Name; }
	};

	/// FunctionAST - This class represents a function definition itself.
	class FunctionAST {
		std::unique_ptr<PrototypeAST> Proto;
		std::unique_ptr<ExprAST> Body;

	public:
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
			std::unique_ptr<ExprAST> Body)
			: Proto(std::move(Proto)), Body(std::move(Body)) {}
	};

	class Type {
		Token type;	
	public:
		virtual ~Type() = default;
		Type(Token _type):type(_type){}
	};

	class IdentifierType :public Type {
		std::string name;
	public:
		IdentifierType(const std::string&_name):Type(tok_identifier), name(_name){}
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
			std::cout << "Variable:" << name << "\n";
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

	class CompileError {
		int linenumber;
		int pos;
		std::string msg;
	public:
		CompileError(int _linenumber, int _pos, const std::string& _msg): linenumber(_linenumber),pos(_pos),msg(_msg){}
		void print()
		{
			printf("Compile Error at line %d, postion %d : %s", linenumber, pos, msg.c_str());
		}
	};
}



Tokenizer tokenizer(fopen("test.txt", "r"));
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
////////////////////////////////////////////////////////////////////////////////////


std::unique_ptr<VariableSingleDefAST> ParseSingleDim()
{
	std::string identifier = tokenizer.IdentifierStr;
	CompileExpect(tok_identifier, "Expected an identifier to define a variable");
	CompileExpect(tok_as, "Expected \'as\'");
	std::unique_ptr<Type> type;
	switch (tokenizer.CurTok)
	{
	case tok_identifier:
		type = std::make_unique<IdentifierType>(identifier);
		break;
	case tok_int:
		type = std::make_unique<Type>(tok_int);
		break;
	default:
		throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), "Expected an identifier or basic type name");
	}
	tokenizer.GetNextToken();
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
			tokenizer.GetNextToken();
		case tok_eof:
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
	case tok_left_bracket:
		tokenizer.GetNextToken();
		firstexpr = ParseExpressionUnknown();
		CompileAssert(tok_right_bracket == tokenizer.CurTok, "Expected \')\'");
		break;
	case tok_eof:
		return nullptr;
		break;
	default:
		throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), "Unknown Token");
	}
	Token nexttok=tokenizer.GetNextToken(); //eat token
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
	case tok_comma:
		return std::move(firstexpr);
		break;
	default:
		throw CompileError(tokenizer.GetLine(), tokenizer.GetPos(), "Unknown Token");
	}
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
			case tok_dim:
				tokenizer.GetNextToken(); //eat dim
				out.push_back(std::move(ParseDim()));
				break;
			case tok_eof:
				return 0;
				break;
			default:
				firstexpr = ParseExpressionUnknown();
				CompileExpect({ tok_eof,tok_newline }, "Expect a new line after expression");
				if (!firstexpr)
					break;
				out.push_back(std::move(firstexpr));
			}
		}
	}
	catch (CompileError e)
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

