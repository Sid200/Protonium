#include "includes/Statements.hpp"

Expression::Expression(Expr_ptr expr) : m_expr(expr) {

}

void Expression::accept(StmtVisitor* visitor) const {
	visitor->visit(*this);
}

Print::Print(Expr_ptr expr) : m_expr(expr) {

}

void Print::accept(StmtVisitor* visitor) const {
	visitor->visit(*this);
}

Var::Var(Token name, Expr_ptr init) : m_name(name), m_initializer(init) {

}

void Var::accept(StmtVisitor* visitor) const {
	visitor->visit(*this);
}

Block::Block(std::vector<Stmt_ptr> stmts) : m_stmts(stmts) {

}

void Block::accept(StmtVisitor* visitor) const {
	return visitor->visit(*this);
}

If::If(Expr_ptr condition, Stmt_ptr thenBranch, Stmt_ptr elseBranch) : m_condition(condition), m_thenBranch(thenBranch), m_elseBranch(elseBranch) {

}

void If::accept(StmtVisitor* visitor) const {
	visitor->visit(*this);
}
