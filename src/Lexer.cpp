#include "includes/Lexer.hpp"
#include "proto.hpp"

#include <algorithm>

bool Lexer::isAtEnd() const {
    if (m_current >= m_src.length()) {
        return true;
    }
    return false;
}

char Lexer::advance() {
    return m_src.at(m_current++);
}

void Lexer::addToken(TokenType type, LiteralType ltype) {
    auto lexeme = m_src.substr(m_start, m_current - m_start);
    if (ltype == LiteralType::STR) {
        //! Make the lexeme the value of the str; remove the quotes
        lexeme = lexeme.substr(1, lexeme.length() - 2);
        
        //! Facilitate escape sequences. Only \n, \t, \" and \\ are supported.
        for (std::size_t i = 0; i < lexeme.length(); i++) {
            if (lexeme.at(i) == '\\') {
                if (i + 1 < lexeme.length()) {
                    switch (lexeme.at(i + 1)) {
                    case 'n':
                        lexeme = lexeme.substr(0, i) + '\n' + lexeme.substr(i + 2);
                        break;
                    case 't':
                        lexeme = lexeme.substr(0, i) + '\t' + lexeme.substr(i + 2);
                        break;
                    case '"':
                        lexeme = lexeme.substr(0, i) + '"' + lexeme.substr(i + 2);
                        break;
                    case '\\':
                        lexeme = lexeme.substr(0, i) + '\\' + lexeme.substr(i + 2);
                        break;
                    }
                }
            }
        }
        
    }
    m_tokens.push_back(Token(type, lexeme, m_line, ltype));
}

bool Lexer::isNext(char c) {
    if (isAtEnd()) {
        return false;
    }
    if (m_src.at(m_current) != c) {
        return false;
    }

    //consume the character only if it is what we're looking for
    m_current++;
    return true;
}

bool Lexer::isDigit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::isAlphaOrUnderscore(char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Lexer::isAlphaNumeric(char c) const {
    return isDigit(c) || isAlphaOrUnderscore(c);
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return m_src.at(m_current);
}

char Lexer::peekby2() const {
    if (m_current + 1 >= m_src.length()) return '\0';
    return m_src.at(m_current + 1);
}

void Lexer::string(Proto& p) {
    while (!isAtEnd()) {
        if (peek() != '"') {
            if (peek() == '\n') m_line++;
            advance();
        }
        else if (m_src.at(m_current - 1) == '\\' && m_src.at(m_current - 2) != '\\') {
            advance();
        }
        else break;
    }

    ///Error if string is unterminated
    if (isAtEnd()) {
        p.error(m_line, "Unterminated String. Expected a \".");
        return;
    }

    ///Found the closing quote
    advance();

    addToken(TokenType::STRING, LiteralType::STR);
}

void Lexer::number(Proto& p) {
    //Consume all the digits...
    while (isDigit(peek())) advance();
    //...until the fractional part, and then consume again
    if (peek() == '.' && isDigit(peekby2())) {
        advance();
        while (isDigit(peek())) advance();
    }
    if (peek() == 'e' && (isDigit(peekby2()) || peekby2() == '+' || peekby2() == '-')) {
        advance();
        if ((peek() == '+' || peek() == '-') && isDigit(peekby2())) advance();
        while (isDigit(peek())) advance();
    }
    addToken(TokenType::NUMBER, LiteralType::NUM);
}

void Lexer::identifierOrKeyword() {
    while (isAlphaNumeric(peek())) advance();
    auto str = m_src.substr(m_start, m_current - m_start);

    if (keywords.find(str)!=keywords.end()) {
        if (str == "nix") {
            addToken(keywords.at(str), LiteralType::NIX);
            return;
        }
        if (str == "true") {
            addToken(keywords.at(str), LiteralType::TRUE);
            return;
        }
        if (str == "false") {
            addToken(keywords.at(str), LiteralType::FALSE);
            return;
        }
        
        addToken(keywords.at(str));
        return;
    }
    addToken(TokenType::IDENTIFIER);
}

void Lexer::scanToken(Proto& p) {
    auto c = advance();
    switch (c) {
    case '(':
        addToken(TokenType::LPAREN);
        break;
    case ')':
        addToken(TokenType::RPAREN);
        break;
    case '{':
        addToken(TokenType::LBRACE);
        break;
    case '}':
        addToken(TokenType::RBRACE);
        break;
    case '[':
        addToken(TokenType::LSQRBRKT);
        break;
    case ']':
        addToken(TokenType::RSQRBRKT);
        break;
    case ',':
        addToken(TokenType::COMMA);
        break;
    case '+':
        addToken(isNext('=') ? TokenType::PLUS_EQUAL : TokenType::PLUS);
        break;
    case '-':
        addToken(isNext('=') ? TokenType::MINUS_EQUAL : TokenType::MINUS);
        break;
    case '*':
        addToken(isNext('=')? TokenType::PROD_EQUAL : TokenType::PRODUCT);
        break;
    case ';':
        addToken(TokenType::SEMICOLON);
        break;
    case '^':
        addToken(TokenType::EXPONENTATION);
        break;
    case '!':
        addToken(isNext('=') ? TokenType::NOT_EQUAL : TokenType::NOT);
        break;
    case '=':
        addToken(isNext('=') ? TokenType::EQ_EQUAL : TokenType::EQUAL);
        break;
    case '`':
        isNext('=') ? addToken(TokenType::BT_EQUAL) : p.error(m_line, "Unexpected character: ", std::string(1, c));
        break;
    case '>':
        addToken(isNext('=') ? TokenType::GT_EQUAL : TokenType::GREATER);
        break;
    case '<':
        addToken(isNext('=') ? TokenType::LT_EQUAL : TokenType::LESS);
        break;
    case '/':
        if (isNext('/')) { 
            //! Skip the whole line because this is a comment
            while (peek() != '\n' && !isAtEnd()) advance(); 
        } 
        else if (isNext('[')) { /// multi-line comment: discard until ]/
            while (!(isNext(']') && isNext('/')) && !isAtEnd()) {
                if (peek() == '\n') m_line++; 
                ///multiline comments contribute to lines. 
                /// Helps to report errors with the proper line numbers
                advance();
            }
        }
        else addToken(isNext('=') ? TokenType::DIV_EQUAL : TokenType::DIVISON);
        break;
    case ' ': 
    case '\t':
    case '\r':
        break;
    case '\n':
        m_line++;
        break;
    case '"':
        string(p);
        break;
    default:
        if (isDigit(c) || (c == '.' && isDigit(peek()))) {
            number(p);
        }
        else if (c == '.' && isNext('.')) addToken(TokenType::DOT_DOT);
        else if (isAlphaOrUnderscore(c)) {
            identifierOrKeyword();
        }
        else p.error(m_line, "Unexpected character: ", std::string(1, c));
    }
}

std::vector<Token>& Lexer::scanTokens(Proto& p) {

    while (!isAtEnd()) {
        m_start = m_current;
        scanToken(p);
    }

    //We hit EOF and hence we stopped
    m_tokens.push_back(Token(TokenType::EOF_, "", m_line, LiteralType::NONE));
    return m_tokens;
}