#pragma once
#include "Expressions.hpp"
#include "Statements.hpp"
#include "Environment.hpp"
#include "Callable.hpp"
#include "ProtoFunc.hpp"

class RuntimeError : std::exception {
private:
	std::string m_error;
	Token m_tok;
public:
	RuntimeError() = delete;
	RuntimeError(Token t, const std::string& err);
	virtual const char* what() const noexcept override;
	Token getToken() const;
};

class BreakThrow {};
class ContinueThrow {};

class Interpreter : public ExprVisitor, public StmtVisitor {
private:
	Value m_val;
	Env_ptr m_env;	//the current environment
	Env_ptr m_global;	//the global environment of course
	std::unordered_map<const Expr*, std::size_t> m_locals;
private:
	bool isNum(const Value& val);
	bool isNix(const Value& val);
	bool isStr(const Value& val);
	bool isBool(const Value& val);
	bool isTrue(const Value& val);
	bool isCallable(const Value& val);
	bool isList(const Value& val);
	bool isEqual(const Value& left, const Value& right);
	bool isEqual(long double left, long double right);

	void execute(Stmt_ptr stmt);
	void executeBlock(Stmts stmts, Env_ptr env);

	Value& lookUpVariable(const Expr& e, const Token& t);

	void verifyIndices(list_ptr list, const Value& index, Token indexOp);

	Interpreter();
	friend ProtoFunction;
public:
	static Interpreter& getInstance();
	std::string stringify(const Value& value, const char* strContainer = "");
	void resolve(const Expr& expr, std::size_t depth);
	Interpreter(const Interpreter&) = delete;
	void operator=(const Interpreter&) = delete;
	
	virtual void visit(const Binary& bin) override;
	virtual void visit(const Unary& un) override;
	virtual void visit(const ParenGroup& group) override;
	virtual void visit(const Literal& lit) override;
	virtual void visit(const Variable& var) override;
	virtual void visit(const Logical& log) override;
	virtual void visit(const Assign& expr) override;
	virtual void visit(const Call& expr) override;
	virtual void visit(const Lambda& expr) override;
	virtual void visit(const ListExpr& expr) override;
	virtual void visit(const Index& expr) override;
	virtual void visit(const RangeExpr& expr) override;
	virtual void visit(const IndexAssign& expr) override;
	virtual void visit(const InExpr& expr) override;

	//Statements

	virtual void visit(const Expression& expr) override;
	virtual void visit(const Block& block) override;
	virtual void visit(const If& ifStmt) override;
	virtual void visit(const While& whilestmt) override;
	virtual void visit(const For& forstmt) override;
	virtual void visit(const RangedFor& rforstmt) override;
	virtual void visit(const Break& breakstmt) override;
	virtual void visit(const Continue& contstmt) override;
	virtual void visit(const Func& func) override;
	virtual void visit(const Return& stmt);

	void interpret(const Stmts& stmts);
	std::string interpret(Expr_ptr expr);
};