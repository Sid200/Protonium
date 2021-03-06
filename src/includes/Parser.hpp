#pragma once
#include <list>
#include <vector>
#include <memory>
#include <exception>
#include <string>
#include <string_view>
#include <variant>

#include "Expressions.hpp"
#include "Token.hpp"
#include "Statements.hpp"

class ParseError : std::exception {
private:
	std::string_view m_error;
public:
	ParseError() = delete;
	ParseError(std::string_view err) : m_error(err) {

	}
	virtual const char* what() const noexcept override {
		return m_error.data();
	}
};

class Parser {
private:
	std::vector<Token> m_tokens;
	std::size_t m_current;
	bool m_allowExpr;
	bool m_foundExpr;
	std::size_t m_loopDepth;
public:
	Parser() = delete;
	Parser(std::vector<Token>& tokens, bool parseRepl = false);
	std::variant<Stmts, Expr_ptr> parse();
private:
//! Helpers

	bool isAtEnd();
	Token peek();
	Token previous();
	bool isNextType(TokenType type);
	bool isNextNextType(TokenType type);
	Token advance(); //! Consume the next token
	
	//! Consume the next token if it matches any type of types
	bool match(const std::list<TokenType>& types);
	bool match(TokenType type);
	
	ParseError error(Token t, std::string_view msg);

	//! The next token is expected to be type. If it is not, we have 
	//! an error.
	bool matchWithErr(TokenType type, std::string_view err);

	void sync();

//! Production rules
	Stmt_ptr statement();
	Stmts block();
	Stmt_ptr ifstmt();
	Stmt_ptr whilestmt();
	Stmt_ptr forstmt();
	Stmt_ptr breakstmt();
	Stmt_ptr contstmt();
	Stmt_ptr exprstmt();
	Stmt_ptr fndefn();
	Stmt_ptr returnstmt();


	Expr_ptr expression();
	Expr_ptr assignment();
	Expr_ptr lor();
	Expr_ptr land();
	Expr_ptr equality();
	Expr_ptr comparision();
	Expr_ptr range();
	Expr_ptr addition();
	Expr_ptr product();
	Expr_ptr unary();
	Expr_ptr exponentiation();
	Expr_ptr index_or_call();
	Expr_ptr primary();
	Expr_ptr list();
};

