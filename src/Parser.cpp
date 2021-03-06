#include <string_view>

#include "includes/Parser.hpp"
#include "includes/Lambda.hpp"
#include "proto.hpp"

Parser::Parser(std::vector<Token>& tokens, bool parseRepl) : m_tokens(tokens), m_current(0), m_allowExpr(parseRepl), m_foundExpr(false), m_loopDepth(0){

}

std::variant<Stmts, Expr_ptr> Parser::parse() {
	std::vector<Stmt_ptr> statements;
	while (!isAtEnd()) {
		statements.push_back(statement());

		if (m_foundExpr) {
			if (auto a = std::static_pointer_cast<Expression>(statements.back())) {
				return a->m_expr;
			}
		}
		m_allowExpr = false;
	}
	return statements;
}

bool Parser::isAtEnd() {
	return peek().getType() == TokenType::EOF_;
}

Token Parser::peek() {
	return m_tokens.at(m_current);
}

Token Parser::previous() {
	return m_tokens.at(m_current - 1);
}

bool Parser::isNextType(TokenType type) {
	if (isAtEnd()) return false;
	return peek().getType() == type;
}

bool Parser::isNextNextType(TokenType type) {
	if (isAtEnd()) return false;
	if (m_tokens.at(m_current + 1).getType() == TokenType::EOF_) return false;
	return m_tokens.at(m_current + 1).getType() == type;
}

Token Parser::advance() { 
	if (!isAtEnd()) m_current++;
	return previous();
}

bool Parser::match(const std::list<TokenType>& types) {
	for (auto type : types) {
		if (isNextType(type)) {
			advance();
			return true;
		}
	}

	return false;
}

bool Parser::match(TokenType type) {
	if (isNextType(type)) {
		advance();
		return true;
	}
	return false;
}

ParseError Parser::error(Token t, std::string_view msg){
	Proto::getInstance().error(t.getLine(), msg);
	return ParseError{ msg };
}

bool Parser::matchWithErr(TokenType type, std::string_view err) {
	if (isNextType(type)) {
		advance();
		return true;
	}

	throw error(peek(), err);
}

void Parser::sync() {
	advance(); //consume the erroneous token 

	while (!isAtEnd()) {
		//We consume all tokens when encountered, but we make sure 
		//that we didn't consume a semi-colon. If we did, that's it 
		//for the consumption marathon.

		if (previous().getType() == TokenType::SEMICOLON) break;

		switch (peek().getType()) {
		case TokenType::CLASS:
		case TokenType::IF:
		case TokenType::WHILE:
		case TokenType::FOR:
		case TokenType::FUNCTION:
		case TokenType::RETURN:
			return;
		}

		advance();
	}
}

//! Production rules

Stmt_ptr Parser::statement() {
	try {
		if (match( TokenType::RETURN )) {
			return returnstmt();
		}
		if (isNextType(TokenType::FUNCTION) && isNextNextType(TokenType::IDENTIFIER)) {
			advance();	//consume the fn
			return fndefn();
		}
		if (match(TokenType::LBRACE)) {
			return std::make_shared<Block>(block());
		}
		if (match(TokenType::IF)) {
			return ifstmt();
		}
		if (match(TokenType::WHILE)) {
			return whilestmt();
		}
		if (match(TokenType::FOR)) {
			return forstmt();
		}
		if (match(TokenType::BREAK)) {
			return breakstmt();
		}
		if (match(TokenType::CONTINUE)) {
			return contstmt();
		}
		return exprstmt();
	}
	catch (const ParseError&) {
		sync();
		return nullptr;
	}
}

Stmts Parser::block() {
	std::vector<Stmt_ptr> statements;
	while (!isNextType(TokenType::RBRACE) && !isAtEnd()) {
		statements.push_back(statement());
	}

	matchWithErr(TokenType::RBRACE, "Expected a '}' at the end of the block.");
	return statements;
}

Stmt_ptr Parser::ifstmt() {
	matchWithErr(TokenType::LPAREN, "Expected a '(' after 'if'.");
	auto condition = expression();
	matchWithErr(TokenType::RPAREN, "Expected a ')' after if condition.");

	auto then = statement();
	Stmt_ptr elseBranch{ nullptr };
	if (match(TokenType::ELSE)) {
		elseBranch = statement();
	}

	return std::make_shared<If>(condition, then, elseBranch);
}

Stmt_ptr Parser::whilestmt() {
	matchWithErr(TokenType::LPAREN, "Expected a '(' after 'while'.");
	auto condition = expression();
	matchWithErr(TokenType::RPAREN, "Expected a ')' after while condition.");

	m_loopDepth++;
	auto body = statement();
	m_loopDepth--;
	return std::make_shared<While>(condition, body);
}

Stmt_ptr Parser::forstmt() {
	matchWithErr(TokenType::LPAREN, "Expected a '(' after 'for'.");
	
	Expr_ptr init;
	if (match(TokenType::SEMICOLON)) {
		init = nullptr;
	}
	else {
		init = expression();

		if (auto in = std::dynamic_pointer_cast<InExpr>(init)) {
			//we have a range based for loop

			//! make sure to consume the right paren
			matchWithErr(TokenType::RPAREN, "Expected a ')' after the ranged for loop clause.");

			m_loopDepth++;
			Stmt_ptr body = statement();
			m_loopDepth--;
			return std::make_shared<RangedFor>(in, body);
		}

		matchWithErr(TokenType::SEMICOLON, "Expected a ';' after for-loop initialization clause.");
	}

	Expr_ptr condition{ nullptr };
	if (!isNextType(TokenType::SEMICOLON)) {
		//if the condition clause hasn't been omitted.
		condition = expression();
	}
	matchWithErr(TokenType::SEMICOLON, "Expected a ';' after for-loop condition.");

	Expr_ptr increment{ nullptr };
	if (!isNextType(TokenType::RPAREN)) {
		//if the increment clause hasn't been omitted.
		increment = expression();
	}
	matchWithErr(TokenType::RPAREN, "Expected a ')' after for-loop clauses.");

	m_loopDepth++;
	Stmt_ptr body = statement();
	m_loopDepth--;

	if (condition == nullptr) condition = std::make_shared<Literal>(Token(TokenType::TRUE, "true", 0, LiteralType::TRUE));

	return std::make_shared<For>(init, condition, increment, body);
}

Stmt_ptr Parser::breakstmt() {
	if (m_loopDepth == 0) {
		error(previous(), "Cannot use 'break' outside of a loop.");
	}
	matchWithErr(TokenType::SEMICOLON, "Expected a ';' after 'break'.");
	return std::make_shared<Break>();
}

Stmt_ptr Parser::contstmt() {
	if (m_loopDepth == 0) {
		error(previous(), "Cannot use 'continue' outside of a loop.");
	}
	matchWithErr(TokenType::SEMICOLON, "Expected a ';' after 'continue'.");
	return std::make_shared<Continue>();
}

Stmt_ptr Parser::exprstmt() {
	auto expr = expression();
	if (m_allowExpr && isAtEnd()) m_foundExpr = true;
	else matchWithErr(TokenType::SEMICOLON, "Invalid Syntax. Did you miss a ';' after the expression?");
	return std::make_shared<Expression>(expr);
}

Stmt_ptr Parser::fndefn() {
	matchWithErr(TokenType::IDENTIFIER, "A function name was expected.");
	auto name = previous();
	std::vector<Token> params;
	matchWithErr(TokenType::LPAREN, "Expected a '(' after function name in definition.");
	if (!isNextType(TokenType::RPAREN)) {
		do {
			if (params.size() >= 127)
				Proto::getInstance().error(peek().getLine(), "Cannot have more than 127 parameters in a function.");
			matchWithErr(TokenType::IDENTIFIER, "Expected a parameter name after ','.");
			params.push_back(previous());
		} 
		while (match(TokenType::COMMA));
	}

	matchWithErr(TokenType::RPAREN, "Expected a ')' after function parameters.");
	matchWithErr(TokenType::LBRACE, "Expected a '{' before function body.");
	return std::make_shared<Func>(name, params, block());
}

Stmt_ptr Parser::returnstmt() {
	auto keyword = previous();
	Expr_ptr val{ nullptr };
	if (!isNextType(TokenType::SEMICOLON)) {
		val = expression();
	}
	matchWithErr(TokenType::SEMICOLON, "Expected a ';' after return value.");
	return std::make_shared<Return>(keyword, val);
}

Expr_ptr Parser::expression() {
	return assignment();
}

Expr_ptr Parser::assignment() {
	auto expr = lor();

	if (match({ TokenType::EQUAL, TokenType::BT_EQUAL })) {
		Token op = previous();
		Expr_ptr val = assignment();

		if (auto var = std::dynamic_pointer_cast<Variable>(expr)) {
			auto name = var->m_name;
			return std::make_shared<Assign>(name, op, val);
		}

		if (auto index = std::dynamic_pointer_cast<Index>(expr)) {
			return std::make_shared<IndexAssign>(index->m_list, index->m_index, index->m_indexOp, op, val);
		}

		error(op, "Invalid assignment location.");
	}

	if (match({ TokenType::PROD_EQUAL, TokenType::DIV_EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL })) {
		Token op = previous();
		auto val = assignment();

		/*
		*	desugaring:
			a += 2 => a `= a + 2
			a -= 2 => a `= a - 2
			a *= 2 => a `= a * 2
			a /= 2 => a `= a / 2
		*/

		if (auto var = std::dynamic_pointer_cast<Variable>(expr)) {
			
			switch (op.getType()) {
			case TokenType::PLUS_EQUAL:
				op = Token(TokenType::PLUS, "+", op.getLine(), LiteralType::NONE);
				val = std::make_shared<Binary>(var, op, val);
				break;
			case TokenType::MINUS_EQUAL:
				op = Token(TokenType::MINUS, "-", op.getLine(), LiteralType::NONE);
				val = std::make_shared<Binary>(var, op, val);
				break;
			case TokenType::PROD_EQUAL:
				op = Token(TokenType::PRODUCT, "*", op.getLine(), LiteralType::NONE);
				val = std::make_shared<Binary>(var, op, val);
				break;
			case TokenType::DIV_EQUAL:
				op = Token(TokenType::DIVISON, "/", op.getLine(), LiteralType::NONE);
				val = std::make_shared<Binary>(var, op, val);
				break;
			}

			op  = Token(TokenType::BT_EQUAL, "`=", op.getLine(), LiteralType::NONE);
			return std::make_shared<Assign>(var->m_name, op, val);
		}
		else {
			error(op, "Invalid assignment location.");
		}
	}

	if (match(TokenType::IN)) {
		Token in = previous();
		Expr_ptr iterable = assignment();
		if (auto var = std::dynamic_pointer_cast<Variable>(expr)) {
			auto name = var->m_name;
			return std::make_shared<InExpr>(name, in, iterable);
		}
		error(in, "Missing identifier for iterating variable.");
	}

	return expr;
}

Expr_ptr Parser::lor() {
	auto expr = land();

	while (match(TokenType::OR)) {
		Token op = previous();
		auto right = land();
		expr = std::make_shared<Logical>(expr, op, right);
	}

	return expr;
}

Expr_ptr Parser::land() {
	auto expr = equality();

	while (match(TokenType::AND)) {
		Token op = previous();
		auto right = equality();
		expr = std::make_shared<Logical>(expr, op, right);
	}

	return expr;
}

Expr_ptr Parser::equality() {
	auto expr = comparision();

	while (match({ TokenType::NOT_EQUAL, TokenType::EQ_EQUAL })) {
		Token op = previous();
		auto right = comparision();
		expr = std::make_shared<Binary>(expr, op, right);
	}
	return expr;
}

Expr_ptr Parser::comparision() {
	auto expr = range();

	while (match({ TokenType::GREATER, TokenType::GT_EQUAL, TokenType::LESS, TokenType::LT_EQUAL })) {
		Token op = previous();
		auto right = range();
		expr = std::make_shared<Binary>(expr, op, right);
	}

	return expr;
}

Expr_ptr Parser::range() {
	auto expr = addition();
	if (match(TokenType::DOT_DOT)) {
		Token op = previous();
		auto expr2 = addition();
		if (match(TokenType::DOT_DOT)) {
			auto expr3 = addition();
			expr = std::make_shared<RangeExpr>(expr, expr2, expr3, op);
		}
		else {
			expr = std::make_shared<RangeExpr>(expr, expr2, op);
		}
	}

	return expr;
}

Expr_ptr Parser::addition() {
	auto expr = product();

	while (match({ TokenType::PLUS, TokenType::MINUS })) {
		Token op = previous();
		auto right = product();
		expr = std::make_shared<Binary>(expr, op, right);
	}

	return expr;
}

Expr_ptr Parser::product() {
	auto expr = unary();

	while (match({ TokenType::PRODUCT, TokenType::DIVISON })) {
		Token op = previous();
		auto right = unary();
		expr = std::make_shared<Binary>(expr, op, right);
	}

	return expr;
}

Expr_ptr Parser::unary() {
	if (match({ TokenType::NOT, TokenType::MINUS })) {
		Token op = previous();
		auto right = unary();
		return std::make_shared<Unary>(op, right);
	}

	return exponentiation();
}

Expr_ptr Parser::exponentiation() {
	auto base = index_or_call();
	if (match(TokenType::EXPONENTATION)) {
		Token op = previous();
		auto power = exponentiation();
		base = std::make_shared<Binary>(base, op, power);
	}
	return base;
}

Expr_ptr Parser::index_or_call() {
	Expr_ptr expr = primary();
	while (true) {
		//call
		if (match(TokenType::LPAREN)) {
			std::vector<Expr_ptr> args;
			if (!isNextType(TokenType::RPAREN)) {
				do {
					if (args.size() >= 127)
						Proto::getInstance().error(peek().getLine(), "Cannot have more than 127 arguments.");
					args.push_back(expression());
				} 
				while (match(TokenType::COMMA));
			}

			matchWithErr(TokenType::RPAREN, "Expected a ')' after function arguments.");
			auto paren = previous();
			expr = std::make_shared<Call>(expr, paren, args);
		}

		//indexing
		else if (match(TokenType::LSQRBRKT)) {
			auto tok = previous();
			if (match(TokenType::LSQRBRKT)) {
				//list indexing
				auto listIndex = list();
				expr = std::make_shared<Index>(tok, expr, listIndex);
			}
			else {
				auto index = expression();
				//can be a number or a range (which turns to a list)
				expr = std::make_shared<Index>(tok, expr, index);
			}

			matchWithErr(TokenType::RSQRBRKT, "Expected a ']' after index end.");
		}
		else break;
	}

	return expr;
}

Expr_ptr Parser::primary() {
	if (match({ TokenType::TRUE, TokenType::FALSE, TokenType::NIX, TokenType::NUMBER, TokenType::STRING })) {
		return std::make_shared<Literal>(previous());
	}

	if (match(TokenType::LPAREN)) {
		auto expr = expression();
		matchWithErr(TokenType::RPAREN, "Expected ')' after expression.");
		return std::make_shared<ParenGroup>(expr);
	}

	if (match(TokenType::IDENTIFIER)) {
		return std::make_shared<Variable>(previous());
	}

	if (match(TokenType::FUNCTION)) {
		//lambda
		std::vector<Token> params;
		matchWithErr(TokenType::LPAREN, "Expected a '(' after fn");
		if (!isNextType(TokenType::RPAREN)) {
			do {
				if (params.size() >= 127)
					Proto::getInstance().error(peek().getLine(), "Cannot have more than 127 parameters in a lambda.");
				matchWithErr(TokenType::IDENTIFIER, "Expected a parameter name after ','.");
				params.push_back(previous());
			} while (match(TokenType::COMMA));
		}

		matchWithErr(TokenType::RPAREN, "Expected a ')' after lambda parameters.");
		matchWithErr(TokenType::LBRACE, "Expected a '{' before lambda body.");
		return std::make_shared<Lambda>(params, block());
	}

	if (match(TokenType::LSQRBRKT)) {
		return list();
	}

	throw error(peek(), "Invalid Syntax.");
}

Expr_ptr Parser::list() {
	auto lsqrbrkt = previous();
	std::vector<Expr_ptr> expressions;
	if (!isNextType(TokenType::RSQRBRKT)) {
		do {
			expressions.push_back(expression());
		} 
		while (match(TokenType::COMMA));
	}

	matchWithErr(TokenType::RSQRBRKT, "Expected a ']' after list end.");
	return std::make_shared<ListExpr>(expressions, lsqrbrkt);
}